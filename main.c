/*
 * Custom Glade Automatic Air Freshener
 * Hack using an attiny85 microcontroller to customize the action time.
 * 
 * Due to the limit of ports available on the microcontroller, only two functions are left: manual and automatic (with fixed activation time).
 * 
 * IMPORTANT:
 * - set clock fuse to 128kHz
 * - set RSTDISBL fuse to leave RESET pin as GPIO (You will need a High Voltage Programmer (HVSP) to reprogram it in the future.)
 * 
 * BUILD
 * - avr-gcc -mmcu=attiny85 -DF_CPU=128000UL -Os -o main.elf main.c
 * - avr-objcopy -O ihex -R .eeprom main.elf main.hex
 * - FUSES: lfuse=0x62, hfuse=0x5f, efuse=0xff
 */

#define F_CPU 128000UL  // <<< RELOJ INTERNO DE BAJO CONSUMO
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <util/delay_basic.h>

#define TUNE_DELAY_LOOP_2_COUNT_TO_1MS 256 // Define according to internal clok of each microcontroller, tune manually.
#define TUNE_WDT_REAL_TICK_TIME_IN_SEC 1.02 //Define according to the WDT time of each microcontroller, tune manually.

#define AUTOMATIC_ACTIVATION_TIME 3600/TUNE_WDT_REAL_TICK_TIME_IN_SEC

volatile uint8_t triggered = 0;
volatile uint16_t wdt_counter = 0;

void setup_spray() {
    DDRB |= (1 << PB3) | (1 << PB4); // pin 2 y 3 como salidas
    PORTB |= (1 << PB3) | (1 << PB4); // pull-up

    DDRB |= (1 << PB0) | (1 << PB1); // pin 5 y 6 como salidas
    PORTB &= ~((1 << PB0) | (1 << PB1)); // pull-down
}

void setup_manual_sw() {
    DDRB &= ~(1 << PB2); // pin 7 como entrada
    PORTB |= (1 << PB2); // pull-up
    
    // Configurar INT0 para activarse en flanco de bajada (INT0 esta mapeada a PB2)
    MCUCR |= (1 << ISC01);   // ISC01=1, ISC00=0 => flanco de bajada
    MCUCR &= ~(1 << ISC00);
    
    // Habilitar interrupción externa INT0
    GIMSK |= (1 << INT0);
}

void setup_onoff_sw() {
    DDRB &= ~(1 << PB5); // pin 1 como entrada
    PORTB |= (1 << PB5); // pull-up
    
    // watchdog para spray automatico
    MCUSR &= ~(1 << WDRF); // Limpia el flag de reset por watchdog
    
    // Habilitar cambios temporales al WDT
    WDTCR |= (1 << WDCE) | (1 << WDE);
    
    // Modo interrupción, 1 segundo (WDP2 + WDP1)
    WDTCR = (1 << WDIE) | (1 << WDP2) | (1 << WDP1);
}

// Custom delay in milliseconds (manually tuned for each microcontroller.)
void delay_ms_custom(uint16_t ms) {
    while (ms--) {
        _delay_loop_2(TUNE_DELAY_LOOP_2_COUNT_TO_1MS); // _count x 4 ciclos = ~1 ms a 128 kHz
    }
}

void to_spray() {
    // forward
    PORTB &= ~(1 << PB3);
    PORTB |=  (1 << PB0);
    delay_ms_custom(560);
    PORTB |=  (1 << PB3);
    PORTB &= ~(1 << PB0);

    // backward
    PORTB |=  (1 << PB1);
    PORTB &= ~(1 << PB4);
    delay_ms_custom(200);
    PORTB &= ~(1 << PB1);
    PORTB |=  (1 << PB4);
}

ISR(INT0_vect) {
    triggered = 1;
}

ISR(WDT_vect) {
    if (!(PINB & (1 << PB5))) {
        wdt_counter++;
    }
}

int main(void) {
    cli(); // deshabilita interrupciones globales
    
    setup_spray();
    setup_manual_sw();
    setup_onoff_sw();
    
    sei(); // habilita  interrupciones globales

    while (1) {
        sleep_mode();  // Entrar en modo de bajo consumo

        if (triggered) {
            to_spray();
            triggered = 0;
        }

        if (!(PINB & (1 << PB5))) {
            if (wdt_counter >= AUTOMATIC_ACTIVATION_TIME) {
                to_spray();
                wdt_counter = 0;
            }
        } else {
            wdt_counter = 0;
        }
    }
}
