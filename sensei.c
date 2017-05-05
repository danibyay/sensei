/*** Includes ***/
/*
 * Proyecto final de interfaces
 * Daniela Becerra, Adair Ibarra, Sergio Flores, Luis Rivera
 * Bocina y led de potencia controlados por timers y bluetooth
 */

#include <asf.h>
#include <avr/wdt.h>
#include <avr/io.h>
#include <avr/interrupt.h>

typedef unsigned char u8;

#define BAUDRATE 9600
#define BAUD_PRESCALLER (((F_CPU / (BAUDRATE * 16UL))) - 1)
#define F_CPU 16000000UL

#define PERCENT    0x25
#define AMPERSAND   0x26
#define DESIRED_KOHMS  20
#define PESO_SIGN  0x24
#define ARROBA   0x40
#define SMALLER   0x3c
#define DOS_PUNTOS 0x3a
#define PUNTO_COMA 0x3b
#define DECREASE  35

void SPI_MasterInit(void);
void SPI_MasterTransmit(u8 cData);
void delay(u8 times);
void timer0_init(void);
void ledInit(void);
void calcResist(volatile u8 kohms,volatile u8 * value);
void USART_init(void);
void pwm_init(void);

volatile u8 ohms=DESIRED_KOHMS;
volatile u8 value = 0;
volatile u8 flag_vol_plus=0;
volatile u8 flag_vol_minus=0;
volatile u8 flag_30min=0;
volatile u8 flag_1min=0;
volatile u8 flag_15min=0;
volatile u8 treinta;
volatile u8 uno;
volatile u8 quince;
volatile u8 cycle_value;

/**************************************************Interruptions*****************************************************/
ISR(USART_RX_vect)									//This is our timer interrupt service routine
{
	if((UDR0 == PESO_SIGN) & (OCR1A < 150)){		//more light and set limits
		OCR1A +=4;
	}
	else if((UDR0 == ARROBA) & (OCR1A > 10)){		//less light
		OCR1A -=4;
	}
	else if(UDR0 == AMPERSAND){					    //more volume
		flag_vol_plus=1;
	}
	else if(UDR0 == PERCENT){						//less volume
		flag_vol_minus=1;
	}
	else if(UDR0 == SMALLER){						//temporizador en 1m
		flag_30min=1;
		OCR1A = 149;
		timer0_init();
	}
}
ISR(TIMER0_COMPA_vect){	                            //Every 31.872 ms
	treinta++;
}

/**************************************************Functions*****************************************************/
void USART_init(void){
	UBRR0H = (uint8_t)(BAUD_PRESCALLER>>8);			//12 bit prescaler need 2 registers to fit
	UBRR0L = (uint8_t)(BAUD_PRESCALLER);			//Second part of prescaler
	UCSR0B = (1<<RXEN0)|(1<<TXEN0)|(1<<RXCIE0);		//Enable Tx and Rx and Rx Interrupt
	UCSR0A = 0x22;									//Disable doubling the frec (data sheet wrong)
}

void SPI_MasterInit(void){
	/* Set MOSI and SCK output, all others input */
	PORTB=(1<<PORTB2);                                  //SS = 1
	DDRB |= (1<<PORTB3)|(1<<PORTB5)|(1<<PORTB2);
	/* Enable SPI, Master, set clock rate fck/16=1Mhz */
	SPCR = (1<<SPE)|(1<<MSTR)|(0<<SPR0)|(1<<SPR1)|(0<<CPOL)|(0<<CPHA);
}

void SPI_MasterTransmit(u8 cData){
	PORTB=(0<<PORTB2);
	SPDR = (0x11);									/* Start transmission */
	while(!(SPSR & (1<<SPIF)));						/* Wait for transmission complete */
	SPDR = cData;									/* Start transmission */
	while(!(SPSR & (1<<SPIF)));
	PORTB=(1<<PORTB2);								/* SS = 1 */
}

void calcResist(volatile u8 kohms, volatile u8 * value){
    if(kohms > 100 | kohms < 0){
        temp = 100
    }
    else{
        int temp = 0;
        temp = kohms;
        temp = (temp*255)/100;
        *value =  (u8)temp;
    }
}

void pwm_init(){
	DDRB |= (1 << DDB1);                        // PB1 as output
	OCR1A = 1; 	                                // set PWM for 50% duty cycle at 10bit
	TCCR1A |= (1 << COM1A1);                    // set non-inverting mode
	TCCR1A |= (1 << WGM11) | (1 << WGM10);      // set 10bit phase corrected PWM Mode
	TCCR1B |= (1 << CS11);                      // set prescaler to 8 and starts PWM
}

void ledInit(void){
	PORTB = 1<<PORTB0;						    //dato
	DDRB = 1<<PORTB0;							//direccion
}

void timer0_init(void){
	TCCR0A = (1<<WGM01);				        //Timer in CTC mode
	TCCR0B = ((1<<CS02)|(1<<CS00));		        //1:1024 prescaler
	OCR0A = 249;						        //Value to have an compare at every 1ms
	TIMSK0 = (1<<OCIE0A);				        //Enable timer interrupts
}

int main (void){
	sei();                                          //Activate interruptions
    //timer0_init();
	USART_init();                                   //Rx, Tx and interruptions of Rx
	pwm_init();                                     //Init PWM in 50% and positive changes
	calcResist(DESIRED_KOHMS, &value);              //calculate resistance to send by SPI
	SPI_MasterInit();
	SPI_MasterTransmit(value);	                    //set init value of resistor to 20kohm
	for(;;){
		if(flag_vol_plus){
			ohms+=4;
			calcResist(ohms, &value);
			SPI_MasterTransmit(value);
			flag_vol_plus=0;
		}
		else if(flag_vol_minus){
			ohms-=4;
			calcResist(ohms, &value);
			SPI_MasterTransmit(value);
			flag_vol_minus=0;
		}
		if((treinta>30)){                           //Every 1 second decrement the duty cycle
			if(OCR1A>10){                           //If not minimum value keep decreasing
                OCR1A -=2;                          //37 different PWM values
                treinta =0;                         //reset counter
            }
            else{
                TIMSK0 = (0<<OCIE0A);				//Disable timer interrupts
            }
		}
		wdt_reset();
	}
	return 0;
}
