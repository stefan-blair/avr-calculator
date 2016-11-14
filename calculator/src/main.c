/*
 * main.c
 *
 * NOTE
 * this program is designed to run on the atmega324p microcontroller
 * usage with any other model may not function properly.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>

// LCD interface

#define CTRL_DDR	DDRD
#define DATA_DDR	DDRA
#define CTRL_PORT	PORTD
#define DATA_PORT	PORTA
#define RS_PIN		PIND3
#define RW_PIN		PIND1
#define E_PIN		PIND0
#define clear_screen	command(0b00000001)

// send LED comomands to start communications
void init_LCD(void);
// send a command to the LED (wrapper around write_bits)
void command(char bits);
// send and print a character on the LED (wrapper around write_bits)
void print(char bits);
// sends a byte of data to the LED
void write_bits(char bits, bool RS);
// pulses the LED's E pin, telling it to read available data
void pulse_E();
// reverses a given byte, returned as a new byte
char reverse_byte(char byte);

// Button Matrix Interface
#define BTN_DDR		DDRB
#define BTN_PORT	PORTB
#define BTN_READ	PINB
#define BTN_CTRL_DDR	DDRB
#define BTN_CTRL_PORT	PORTB
#define BTN_CTRL_PIN	PINB7

// converts a set of 8 bits to an integer index
int bits_to_int(char b);
char read_btns();
// find which button (if any) is pressed at the time of the function call
char get_dwn_btn();
// solve the equation described by the saved symbols
void solve();
// write the answer to the LED screen
void display_answer();

char btn_map[4][4] = {
	{'+','9','8','7'},
	{'-','6','5','4'},
	{'*','3','2','1'},
	{'/','=','0','.'}
};
char entry_buffer[16] = "/100=**2//2";
int entry_buffer_index = 0;
float answer = 200.5;

int main(void)
{

	// Initialize Button Interface

	BTN_DDR = 0x00;
	BTN_PORT = 0xff;

	// Initialize the LCD

	init_LCD();
	command(0b00001111);

	// Initialize interrupts for button pin

	PCICR |= 1 << PCIE1;
	PCMSK1 |= 1 << PCINT15;

	BTN_CTRL_DDR &= ~(1 << BTN_CTRL_PIN);
	BTN_CTRL_PORT |= 1 << BTN_CTRL_PIN;

	sei();

	// Print initial message

	command(0b11000000);
	print('h');
	print('e');
	print('L');
	print('L');
	print('o');
	command(0b10000000);

	while(1){}
}

// interrupt handler for the register handling all button inputs
ISR(PCINT1_vect)
{
	if (bit_is_clear(PINB, 7))
	{
		if (((~PINB) & (1 << 7)) == 0)
		{
			return;
		}

		// get the button after a delay to eliminate button bouncing
		_delay_ms(50);
		char btn = get_dwn_btn();
		if (btn == 'n')
		{
			return;
		}
		// write the button
		entry_buffer[entry_buffer_index] = btn;
		entry_buffer_index++;
		print(btn);
		_delay_ms(80);
		if (btn == '=')
		{
			// if the equals button is pressed, solve the equations
			solve();
			// clear the screen
			clear_screen;
			// display the answer and reset the cursor
			_delay_ms(50);
			display_answer();
			entry_buffer_index = 0;
		}

	}
}

// Button Function Declaration

int bits_to_int(char b)
{
	// check the first four bits.  return the index of the active one
	// since the buttons are layed out in a four by four grid
	for(char i = 0; i < 4; i++)
	{
		if (b & 1 << i)
		{
			return 3-i;
		}
	}

	return 5;
}

char get_dwn_btn(){
	// read the current indexes.  x is in the first four bits and y is in the
	// second four bits (4 by 4 grid connected to one register)
	int x = bits_to_int(~BTN_READ & 0b00001111);
	int y = bits_to_int(((~BTN_READ) >> 4) & 0b00001111);

	// make sure that the button is valid
	if (x == 5 || y == 5)
	{
		return 'n';
	}

	// return the button at the calculated position
	return btn_map[y][x];
}

float char_to_float(char data[], char start, char end)
{
	char buffer[16];

	for (int i = start; i < end; i++)
	{
		buffer[i - start] = data[i];
		buffer[i - start + 1] = '\0';
	}
	return atof(buffer);
}

void solve()
{
	float opend_1 = answer;
	float opend_2;
	char operators[7] = {'+','-','*','/','s','q','='};
	char opend_1_buf[2] = {0, 0};
	char opend_2_buf[2] = {0, 0};
	char* cur_opend = opend_1_buf;
	char op;
	char is_opend_2 = 0;
	bool is_op;

	for (int i = 0; i < 16; i++)
	{
		is_op = false;
		for (int p = 0; p < 7; p++)
		{
			if (entry_buffer[i] == operators[p])
			{
				if (is_opend_2 == 1)
				{
					is_opend_2++;
				}
				else if (entry_buffer[i] == entry_buffer[i+1])
				{
					switch(entry_buffer[i])
					{
						case '*':
							op = 's';
							is_opend_2++;
							i++;
							break;
						case '/':
							op = 'q';
							is_opend_2++;
							i++;
							break;
						default:
							op = entry_buffer[i];
							is_opend_2++;
							i++;
							break;
					}
				}
				else
				{
					op = entry_buffer[i];
					is_opend_2++;
				}
				is_op = true;
				break;
			}
		}
		if (is_op)
		{
			if (is_opend_2 >= 2)
			{
				opend_2 = char_to_float(entry_buffer, opend_2_buf[0], opend_2_buf[1]);
				if (opend_1_buf[1] == 0)
				{
					opend_1 = answer;
				}
				else
				{
					opend_1 = char_to_float(entry_buffer, opend_1_buf[0], opend_1_buf[1]);
				}

				switch(op)
				{
					case '+':
						answer = opend_1 + opend_2;
						break;
					case '-':
						answer = opend_1 - opend_2;
						break;
					case '*':
						answer = opend_1 * opend_2;
						break;
					case '/':
						answer = opend_1 / opend_2;
						break;
					case 's':
						answer = pow(opend_1, opend_2);
						break;
					case 'q':
						answer = pow(opend_1, 1.0 / opend_2);
						break;
					default:
						break;
				}
				if (entry_buffer[i] == '=')
				{
					break;
				}
				else
				{
					opend_1_buf[1] = 0;
					is_opend_2 = 0;
					i--;
				}
			}
			else
			{
				cur_opend = opend_2_buf;
				cur_opend[0] = i + 1;
			}
		}
		else
		{
			cur_opend[1] = i + 1;
		}
	}
}

void display_answer()
{

	command(0b11000000);

	char buffer[16];
	dtostrf(answer, 16, 6, buffer);

	for (int i = 0; i < 16; i++)
	{
		print(buffer[i]);
	}

	command(0b10000000);

}

char read_btns()
{
	char count = 8;
	while(count > 2){
		count = 0;
		for (int i = 0; i < 8; i++)
		{
			if (!bit_is_clear(BTN_READ, i))
			{
				count++;
			}
		}

	}

	return BTN_READ;
}

// LCD Function Declaration

void init_LCD()
{
	/*
	 * these are just a series of commands written to the register designated
	 * for LED screen communication.  the commands come from its manual.
	 * they initialize communication with the LED screen
	 */
	DATA_DDR = 0b11111111;
	CTRL_DDR |= 1 << RS_PIN | 1 << RW_PIN | 1 << E_PIN;
	DATA_DDR = 0b11111111;
	CTRL_PORT = 0;
	_delay_ms(50);
	command(0b00110000);
	_delay_us(4500);
	command(0b00110000);
	_delay_us(150);
	command(0b00110000);
	command(0b00111100);
	command(0b00001100);
	command(0b00000001);
	_delay_us(2000);
	command(0b00000110);
}

char reverse_byte(char byte)
{
	char buffer = 0;
	// store each bit in the buffer starting from the end, instead of beginning
	for (int i = 0; i < 8; i++)
	{
		if (byte & 1 << i)
		{
			buffer |= 1 << (7 - i);
		}
		else
		{
			buffer |= 0 << (7 - i);
		}
	}

	return buffer;

}

/*
 * these functions are for communicating with the
 */

void pulse_E()
{
	/*
	 * pulse the bit corresponding to the LED screen's E pin, notifying it
	 * of new data to be read
	 */

	CTRL_PORT &= ~(1 << E_PIN);
	_delay_us(1);
	CTRL_PORT |= 1 << E_PIN;
	_delay_us(1);
	CTRL_PORT &= ~(1 << E_PIN);
	_delay_us(100);
}

void write_bits(char bits, bool RS)
{
	bits = reverse_byte(bits);
	if (RS)
	{
		// RS pin means display character
		CTRL_PORT |= 1 << RS_PIN;
	}
	else
	{
		// no RS pin means execute command
		CTRL_PORT &= ~(1 << RS_PIN);
	}
	// write bits to the LED screen
	DATA_PORT = bits;
	// tell LED screen to read bits
	pulse_E();
}

// wrapper to print character
void print(char bits)
{
	write_bits(bits, true);
}

// wrapper to execute command
void command(char bits)
{
	write_bits(bits, false);
}
