#include "I2C.h"
#include "Delay.h"
#include "ConstValue.h"
#include "STC15Pins.h"
__bit is_feedback_on;
__bit input_rasing_edge, input_falling_edge;
enum {
  Times_of_1ms_size = 2
};
unsigned char Times_of_1ms[Times_of_1ms_size];
enum {
  Timer_Feedback = 0,
  Timer_Delay = 1
};
enum {
  Times_of_1ms_long_size = 1
};
unsigned int Times_of_1ms_long[Times_of_1ms_long_size];
enum {
  Timer_Long_Press = 0
};
enum {
	Press_level_none = 0x00,
	Press_level_press = 0x01,
	Press_level_1 = 0x02,
	Press_level_2 = 0x03
};
enum {
	Press_level_1_time = 0x1F4, //500
  Press_level_2_time = 0x1F4, //500
	Press_level_shutdown_time = 0xFA0, //4000
	Press_level_restart_time = 0x1388 //5000
};
unsigned char events[1], state_table[1], display_matrix[3], level;
//-Events:
//--0.|LED digital 1 changed|LED digital 2 changed|LED digital 3 changed|Feedback|
//--              7                     6                     5             4          
//--  Trun LED to state 1|Trun LED to state 0|Input: Falling edge|Input: Level changed
//--	          3                  2                 1                   0
//-State:
//--0.|Mode|state
//--   7-4   3-0
//---Mode 0:
//----State 0: All LED off
//----State 1: All LED on
//---Mode 1: Control LED with acceleration
//----State 0: 0x00, 0x00, 0x00
//----State 1: 0x00, 0x33, 0x00
//----State 2: 0x00, 0x00, 0xcc
//----State 3: 0xcc, 0x00, 0x00
//----State 4: 0x00, 0xcc, 0x00
//----State 5: 0x33, 0x00, 0x00
//----State 6: 0x00, 0x00, 0x33
//-Level:
//--|Mode 1:Viscosity of object|Level of press|
//--               7-4               3-0
//
//Send 8 bits
void Send_char_max7219(unsigned char dat) {
	char loop = 0;
	for(; loop<8; ++loop) {
		max7219_clk = OFF;
		max7219_din = (__bit)(dat & The_8th_bit);
		dat <<= 1;
		max7219_clk = ON;
	}
}
//Write a register
void Write_max7219(unsigned char address, unsigned char dat) {
	max7219_load = OFF;
	Send_char_max7219(address);
	Send_char_max7219(dat);
	max7219_load = ON;
}
enum {
  Start_Write_mpu6050_signal = 0xd0,
  Start_Read_mpu6050_signal = 0xd1
};
//Write mpu6050
__bit Write_mpu6050(unsigned char address, unsigned char dat) {
	__bit is_succeeded = Successed;
	I2c_start();
	is_succeeded = I2c_send_char(Start_Write_mpu6050_signal);
	is_succeeded = I2c_send_char(address);
	is_succeeded = I2c_send_char(dat);
	I2c_end();
	return is_succeeded;
}
//Read mpu6050
__bit Read_mpu6050(unsigned char address, unsigned int * dat) {
	__bit is_succeeded = Successed;
	I2c_start();
	is_succeeded = I2c_send_char(Start_Write_mpu6050_signal);
	is_succeeded = I2c_send_char(address);
	I2c_start();
	is_succeeded = I2c_send_char(Start_Read_mpu6050_signal);
	*dat = I2c_receive_char_with_ack(ON) << 8;
	*dat |= I2c_receive_char_with_ack(OFF);
	I2c_end();
	return is_succeeded;
}
//Interrupt-function of button 
void Input_signal_edge() __interrupt 0 {
	if (input_pin == OFF) input_rasing_edge = TRUE;
	else input_falling_edge = TRUE;
}
void Reset_timer() {
  unsigned char i; 
  for (i = 0; i < Times_of_1ms_size; ++i)
    Times_of_1ms[i] = 0x00;
  for (i = 0; i < Times_of_1ms_long_size; ++i)
    Times_of_1ms_long[i] = 0x0000;
}
//Stop 1msTimer
inline void Stop_1ms_timer() {
  AUXR &= ~The_5th_bit;
  Reset_timer();
}
//Start 1msTimer
void Start_1ms_timer()
{
  const unsigned char Times_of_cycle_for_1ms_High = 0xb1;
  const unsigned char Times_of_cycle_for_1ms_Low = 0xe0;
	Stop_1ms_timer();
	T2H = Times_of_cycle_for_1ms_High;
	T2L = Times_of_cycle_for_1ms_Low;
	AUXR |= The_5th_bit;
}
//Interrupt-function of 1msTimer
void Count_1ms() __interrupt 12 {
  unsigned char i;
  for (i = 0; i < Times_of_1ms_size; ++i)
    if (Times_of_1ms[i] != 0x00) --Times_of_1ms[i];
  for (i = 0; i < Times_of_1ms_long_size; ++i)
    if (Times_of_1ms_long[i] != 0x0000) --Times_of_1ms_long[i];
}
//Initialize everything (chips,input and timers)
void Init() {
	//----------Initialize framework----------
	events[0] = 0x00;
	state_table[0] = 0x00;
	display_matrix[0] = 0x00;
	display_matrix[1] = 0x00;
	display_matrix[2] = 0x00;
	events[0] |= 0xe0;//Initialize display
  Reset_timer();
	level = 0;
	//----------Initialize max7219----------
	Write_max7219(0x01, 0x00);//---
	Write_max7219(0x02, 0x00);//Turn off leds
	Write_max7219(0x03, 0x00);//---
	Write_max7219(0x09, 0x00);//No decode
	Write_max7219(0x0a, 0x0f);//Max intensity
	Write_max7219(0x0b, 0x02);//3 line of leds
	Write_max7219(0x0f, 0x00);//Turn off test mode
	//----------Initialize I2c----------
	sda = 1;
	scl = 1;
	//----------Initialize mpu6050----------
	Write_mpu6050(0x6b, 0x48);
	Write_mpu6050(0x19, 0x07);
	Write_mpu6050(0x1A, 0x06);
	Write_mpu6050(0x1B, 0x18);
	Write_mpu6050(0x1C, 0x01);
	//----------Initialize input----------
  P3M1 = 0x04;
	input_rasing_edge = 0;
	input_falling_edge = 0;
	IT0 = 0;
	EX0 = 1;
	//----------Initialize feedback----------
	feedback = 1;
  is_feedback_on = 0;
	//----------Initialize 1ms timer----------
	AUXR |= 0x04;
	IE2 |= 0x04;
	EA = 1;
  //----------Initialize ADC----------
  P1ASF = 0x80;
  ADC_RES = 0x00;
}
//Shutdown Everything
void Shutdown() {
	__bit Restart = 0;
	//Turn everything off
	Write_max7219(0x0c, 0x00);
	Write_mpu6050(0x6b, 0x48);
	input_rasing_edge = 0;
	//Feedback
	feedback = 0;
  Delay_38ms();
  Delay_38ms();
	feedback = 1;
	while (Restart != 1) {
		Stop_1ms_timer();
		PCON = 0x02;
		_nop_();
		if (input_falling_edge == 1) input_falling_edge = 0;
		if (input_rasing_edge == 1) {
			input_rasing_edge = 0;
			Start_1ms_timer();
			Times_of_1ms_long[Timer_Long_Press] = Press_level_restart_time;
			while (input_pin == 0) {
				 if (Times_of_1ms_long[Timer_Long_Press] == 0x0000) {
					 Restart = 1;
					 break;
				 }
			}
		}
	}
	//Feedback
	feedback = 0;
  Delay_38ms();
  Delay_38ms();
	feedback = 1;
	//Return to normal
	Write_max7219(0x0c, 0x01);
	Write_mpu6050(0x6b, 0x00);
}
inline void Start_single_tap_feedback() {
  Times_of_1ms[Timer_Feedback] = 0x46;
  events[0] |= 0x10;
}
void Mode_1_processing(unsigned int critical_value) {
	unsigned int dat, max;
	unsigned char stable_state = 0x01;
	Read_mpu6050(0x3b, &dat);
	if (dat < 0x8000) {
		max = dat;
	}else {
		max = (0xffff - dat);
		stable_state = 0x02;
	}
	Read_mpu6050(0x3d, &dat);
	if (dat < 0x8000) {
		if (dat > max) {
			max = dat;
			stable_state = 0x03;
		}
	}else {
		if ((0xffff - dat) > max) {
			max = (0xffff - dat);
			stable_state = 0x04;
		}
	}
	Read_mpu6050(0x3f, &dat);
	if (dat < 0x8000) {
		if (dat > max) {
			max = dat;
			stable_state = 0x05;
		}
	}else {
		if ((0xffff - dat) > max) {
			max = (0xffff - dat);
			stable_state = 0x06;
		}
	}
	if (max >= critical_value && stable_state != (state_table[0] & 0x0f)) {
    if ((level & 0xf0) == 0x00) level |= 0x10;
		switch (state_table[0] & 0x0f) {
		case 0x03:
		case 0x05:
			display_matrix[0] = 0x00;
			events[0] |= 0x80;
			break;
		case 0x01:
		case 0x04:
			display_matrix[1] = 0x00;
			events[0] |= 0x40;
			break;
		case 0x02:
		case 0x06:
			display_matrix[2] = 0x00;
			events[0] |= 0x20;
			break;
		}
		switch (stable_state) {
		case 0x01:
			display_matrix[1] = 0x33;
			events[0] |= 0x40;
			break;
		case 0x02:
			display_matrix[2] = 0xcc;
			events[0] |= 0x20;
			break;
		case 0x03:
			display_matrix[0] = 0xcc;
			events[0] |= 0x80;
			break;
		case 0x04:
			display_matrix[1] = 0xcc;
			events[0] |= 0x40;
			break;
		case 0x05:
			display_matrix[0] = 0x33;
			events[0] |= 0x80;
			break;
		case 0x06:
			display_matrix[2] = 0x33;
			events[0] |= 0x20;
			break;
		}
		state_table[0] &= 0xf0;
		state_table[0] |= stable_state;
    Start_single_tap_feedback();
  }
}
//Button: Press level changed
void Process_button_press_level_changed_events() {
	if ((level & 0x0f) == Press_level_press) {
		if ((state_table[0] & 0xf0) == 0x00) {
      Start_single_tap_feedback();
		}
	}
	if ((level & 0x0f) == Press_level_1) {
    Start_single_tap_feedback();
	}
	if ((level & 0x0f) == Press_level_2) {
    Start_single_tap_feedback();
	}
}
void Check_power_stat() {
  if (battery_charging == 0) {
    display_matrix[0] = 0x5B, display_matrix[1] = 0xF8, display_matrix[2] = 0x40;
    events[0] |= 0xe0;
  }else if (battery_full == 0) {
    display_matrix[0] = 0xFF, display_matrix[1] = 0xFF, display_matrix[2] = 0xFF;
    events[0] |= 0xe0;
  }else {
    ADC_CONTR = 0xE0;
    Delay_38ms();
    ADC_CONTR = 0xE8;
    _nop_();
    _nop_();
    _nop_();
    _nop_();
    while ((ADC_CONTR & 0x10) == 0);
    ADC_CONTR = 0x00;
    display_matrix[0] = ADC_RES, display_matrix[1] = 0x00, display_matrix[2] = 0x00;
    events[0] |= 0xe0;
  }
}
//Button: Falling edge
void Process_button_falling_edge_events() {
	//Press
	if ((level & 0x0f) == Press_level_press) {
	//When in mode 0
		if ((state_table[0] & 0xf0) == 0x00) {
			if ((state_table[0] & 0x0f) == 0x00) {
				state_table[0] |= 0x01;
				events[0] |= 0x08;
			}else {
				state_table[0] &= 0xf0;
				events[0] |= 0x04;
			}
		}
	}
	//Level 1 press
	if ((level & 0x0f) == Press_level_1) {
	//Change mode
		if ((state_table[0] & 0xf0) == 0x00) {
			state_table[0] = 0x10;
			display_matrix[0] = 0x00, display_matrix[1] = 0x00, display_matrix[2] = 0x00;
			events[0] |= 0xe0;
			level &= 0x0f;
		}else {
			state_table[0] &= 0x0f;
	//Process led state
      if ((state_table[0] & 0x0f) == 0x00) events[0] |= 0x04;
      else events[0] |= 0x08;
    }
	}
  //Level 2 press
  if ((level & 0x0F) == Press_level_2) {
    Check_power_stat();
  }
	level &= 0xf0;
}
void Process_LED_events() {
	//Mode 0
	if ((state_table[0] & 0xf0) == 0x00) {
		if ((events[0] & 0x04) != 0x00) {
			events[0] &= 0xfb;
			display_matrix[0] = 0x00, display_matrix[1] = 0x00, display_matrix[2] = 0x00;
			events[0] |= 0xe0;
		}
		if ((events[0] & 0x08) != 0x00) {
			events[0] &= 0xf7;
			display_matrix[0] = 0xff, display_matrix[1] = 0xff, display_matrix[2] = 0xff;
			events[0] |= 0xe0;
		}
	}
	//Mode 1
	if ((state_table[0] & 0xf0) == 0x10) {
		if (Times_of_1ms[Timer_Delay] == 0x00) {
			Times_of_1ms[Timer_Delay] = 0x0f;
			if ((level & 0xf0) == 0x00) Mode_1_processing(0x0200);
			else Mode_1_processing(0x7ffe);
		}
	}
}
//Events processing
void Process_events() {
	//--Press level changed--
	if ((events[0] & 0x01) != 0x00) {
		events[0] &= 0xfe;
		Process_button_press_level_changed_events();
	}
	//-Falling edge-
	if ((events[0] & 0x02) != 0x00) {
		events[0] &= 0xfd;
		Process_button_falling_edge_events();
	}
	Process_LED_events();
}
//Button pressed
void Process_press_button() {
	//Rasing edge
	if (input_rasing_edge == 1) {
		input_rasing_edge = 0;
		events[0] |= 0x01;//Event: Level changed
		level &= 0xf0;
		level |= Press_level_press;//Button level: Press
		Times_of_1ms_long[Timer_Long_Press] = Press_level_1_time;
	}
	//Level 1
	if ((level & 0x0F) == Press_level_press && Times_of_1ms_long[Timer_Long_Press] == 0x0000) {
		events[0] |= 0x01, //Event: Level changed
		level &= 0xf0;
		level |= Press_level_1;//Button level: Level 1
		Times_of_1ms_long[Timer_Long_Press] = Press_level_2_time;
	}
  //Level 2
	if ((level & 0x0F) == Press_level_1 && Times_of_1ms_long[Timer_Long_Press] == 0x0000) {
		events[0] |= 0x01, //Event: Level changed
		level &= 0xf0;
		level |= Press_level_2;//Button level: Level 2
		Times_of_1ms_long[Timer_Long_Press] = Press_level_shutdown_time;
	}
	//Level shutdown
	if ((level & 0x0F) == Press_level_2 && Times_of_1ms_long[Timer_Long_Press] == 0x0000) {
		level &= 0xf0;//Button level: None
		Shutdown();
	}
}
//Button released
void Process_release_button() {
	//Falling edge
	if (input_falling_edge == 1) {
		events[0] |= 0x02;
		input_falling_edge = 0;
	}
}
void Process_LED_changed() {
	if ((events[0] & 0x80) != 0x00) {
		events[0] &= 0x7f;
		Write_max7219(0x01, display_matrix[0]);
	}
	if ((events[0] & 0x40) != 0x00) {
		events[0] &= 0xbf;
		Write_max7219(0x02, display_matrix[1]);
	}
	if ((events[0] & 0x20) != 0x00) {
		events[0] &= 0xdf;
		Write_max7219(0x03, display_matrix[2]);
	}
}
void Delay_12ms(unsigned int k){
	_nop_();
	_nop_();
  while (--k);
}
void main() {
	Init();
	Write_max7219(0x0c, 0x01);
	Write_mpu6050(0x6b, 0x00);
	//Work with event queue
	Start_1ms_timer();
	while (1==1) {
		//-Hardwares-
		//--Button--
		if (input_pin == 0) Process_press_button();
		if (input_pin == 1) Process_release_button();
		//--LED--
		Process_LED_changed();
		//--Feedback--
		//---Start feedback---
		if ((events[0] & 0x10) != 0x00) {
			events[0] &= 0xef;
			feedback = 0;
      is_feedback_on = 1;
		}
		//---Stop feedback---
		if (is_feedback_on == 1 && Times_of_1ms[Timer_Feedback] == 0x00) feedback = 1, is_feedback_on = 0;
		//-Events-
		Process_events();
	}
}
