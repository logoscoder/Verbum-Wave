
/**
 * Verbum Wave é uma aplicação utilizada para geração de sinais
 * através de PWM (Pulse-width modulation / Modulação por largura de pulso).
 * Gerando as ondas: senoidal, quadrada, dente de serra e triangular; bem
 * como permitindo a personalização de algumas ondas e o envio de algumas
 * ondas personalizadas pré-definidas.
 *
 * Criado por Jessé Silva aka logoscoder - 08/2020 - 100% Jesus <3
 * 
 * Este código foi feito baseado em outros, são eles:
 *    https://forum.arduino.cc/index.php?topic=171387.0
 */
 
/**
 * Cabeçalhos...
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>

/**
 * Configurações.
 */
 
// Pinos das pontas de prova.
#define VERBUM_PIN_1 10
#define VERBUM_PIN_2 11

/**
 * Macros e especificações.
 */
#define initialization setup
#define command_control loop
#define serial Serial

#define check_command(COMMAND) check_command_buffer(command, COMMAND)

#define INTERRUPT_PERIOD 512
#define FINT (F_CPU / INTERRUPT_PERIOD) // 16kHz?
#define FS (FINT)

/**
 * Controle da seleção dos tipos de onda.
 */
int wave_type = 1;

/**
 * Controle dos tipos de ondas principais.
 */
 
// Tabela pré-calculada da onda senoidal.
const char PROGMEM sinetable [256] = 
{
  128,131,134,137,140,143,146,149,152,156,159,162,165,168,171,174,
  176,179,182,185,188,191,193,196,199,201,204,206,209,211,213,216,
  218,220,222,224,226,228,230,232,234,236,237,239,240,242,243,245,
  246,247,248,249,250,251,252,252,253,254,254,255,255,255,255,255,
  255,255,255,255,255,255,254,254,253,252,252,251,250,249,248,247,
  246,245,243,242,240,239,237,236,234,232,230,228,226,224,222,220,
  218,216,213,211,209,206,204,201,199,196,193,191,188,185,182,179,
  176,174,171,168,165,162,159,156,152,149,146,143,140,137,134,131,
  128,124,121,118,115,112,109,106,103,99, 96, 93, 90, 87, 84, 81,
  79, 76, 73, 70, 67, 64, 62, 59, 56, 54, 51, 49, 46, 44, 42, 39,
  37, 35, 33, 31, 29, 27, 25, 23, 21, 19, 18, 16, 15, 13, 12, 10,
  9,  8,  7,  6,  5,  4,  3,  3,  2,  1,  1,  0,  0,  0,  0,  0, 
  0,  0,  0,  0,  0,  0,  1,  1,  2,  3,  3,  4,  5,  6,  7,  8, 
  9,  10, 12, 13, 15, 16, 18, 19, 21, 23, 25, 27, 29, 31, 33, 35,
  37, 39, 42, 44, 46, 49, 51, 54, 56, 59, 62, 64, 67, 70, 73, 76,
  79, 81, 84, 87, 90, 93, 96, 99, 103,106,109,112,115,118,121,124
};

unsigned char wavetable [256];
unsigned int frequency_coef = 100;
bool sound_pwm = false;
bool sound_on = false;
bool square_wave_type_toggle = false;

/**
 * Protótipos.
 */
void initialization (void);
void command_control (void);
void command_control_check (void);
void setup_pwm_sound (void);
void start_sound (void);
void stop_sound (void);
void set_frequency (unsigned int freq);
void load_voice (int voice);
void sine_wave (void);
void sawtooth_wave (void);
void triangle_wave (void);
void square_wave (void);
void process_frequency (char * command);
void process_wave_type (char * command);
char * read_string_data (void);
int check_command_buffer (char * buffer, char * command);

/**
 * Inicializações...
 */
void initialization (void)
{
  // Comunicação serial.
  serial.begin(9600);
  while (!serial);
  delay(100);
  
  pinMode(VERBUM_PIN_1, OUTPUT);
  pinMode(VERBUM_PIN_2, OUTPUT);

  serial.println("Verbum Wave - started!");
  load_voice(0); // Ativa por padrão a onda senoidal.
}

/**
 * Controle da comunicação com a aplicação no notebook.
 * E da execução de outros tipos de onda fora da estrutura de
 * tipos principais, como por exemplo, a onda quadrada 1.
 */
void command_control (void)
{
  command_control_check();
}

void command_control_check (void)
{
  // Entrada de comandos pelo serial.
  if (serial.available() > 0)
  {
    char * command = read_string_data();
    if (command)
    {
      if (strstr(command, "c:"))
      {
        // Envia resposta para aplicação do notebook.
        serial.write("1");
        
        // +++ Processa comando...
        
        // Seleciona os tipos de ondas.
        if (check_command("c:o"))
          process_wave_type(command);
        
        /**
         * Controle dos tipos principais.
         */
        // Reproduz onda senoidal.
        else if (check_command("c:w1"))
          load_voice(0);
        
        // Reproduz onda triangular.
        else if (check_command("c:w2"))
          load_voice(2);
        
        // Reproduz onda dente de serra.
        else if (check_command("c:w3"))
          load_voice(1);
        
        // Reproduz onda quadrada.
        else if (check_command("c:w4"))
          load_voice(3);
        
        // Ajusta frequência.
        else if (check_command("c:f"))
          process_frequency(command);
      }

      free(command);
    }
  }
}

// Formata dados dos parametros.
#define process_data_param                          \
  if (!command)                                     \
    return;                                         \
                                                    \
  char ch_data [10];                                \
  int size = strlen(command);                       \
                                                    \
  memset(ch_data, 0x00, 10);                        \
                                                    \
  for (int a=0,b=0; a<size; a++)                    \
    if (command[a] >= '0'  && command[a] <= '9')    \
      ch_data[b++] = command[a]

// Formata dados da frequência recebidos.
void process_frequency (char * command)
{
  process_data_param;
  set_frequency(atoi(ch_data));
}

// Formata dados do tipo de onda selecionada.
void process_wave_type (char * command)
{
  process_data_param;
  wave_type = atoi(ch_data);
}

/**
 * Functions to handle converting PCM to PWM and outputting sound
 */

// This is called at sampling freq to output 8-bit samples to PWM.
ISR(TIMER1_COMPA_vect)
{
  static unsigned int phase0;
  static unsigned int sig0;
  static unsigned int tempphase;

  // Tipos de onda principais.
  if (wave_type == 1)
  {
    if (sound_pwm)
    {
      tempphase = phase0 + frequency_coef;
      sig0 = wavetable[phase0>>8];
      phase0 = tempphase;
      OCR2A = sig0; // output the sample
    }
    else 
    {
      // Onda quadrada.
      if (square_wave_type_toggle)
      { 
        digitalWrite(VERBUM_PIN_1, HIGH);  
        square_wave_type_toggle = false;
      }
      else
      {
        digitalWrite(VERBUM_PIN_1, LOW);  
        square_wave_type_toggle = true;
      }
    }
  }
}

void setup_pwm_sound (void)
{
  // Set up Timer 2 to do pulse width modulation on the speaker pin.
  // Use internal clock (datasheet p.160)
  ASSR &= ~(_BV(EXCLK) | _BV(AS2));
  // Set fast PWM mode  (p.157)
  TCCR2A |= _BV(WGM21) | _BV(WGM20);
  TCCR2B &= ~_BV(WGM22);
  // Do non-inverting PWM on pin OC2A (p.155)
  // On the Arduino this is pin 11.
  TCCR2A = (TCCR2A | _BV(COM2A1)) & ~_BV(COM2A0);
  TCCR2A &= ~(_BV(COM2B1) | _BV(COM2B0));
  // No prescaler (p.158)
  TCCR2B = (TCCR2B & ~(_BV(CS12) | _BV(CS11))) | _BV(CS10);
  // Set initial pulse width to the first sample.
  OCR2A = 0;
  // Set up Timer 1 to send a sample every interrupt.
  cli();
  // Set CTC mode (Clear Timer on Compare Match) (p.133)
  // Have to set OCR1A *after*, otherwise it gets reset to 0!
  TCCR1B = (TCCR1B & ~_BV(WGM13)) | _BV(WGM12);
  TCCR1A = TCCR1A & ~(_BV(WGM11) | _BV(WGM10));
  // No prescaler (p.134)
  TCCR1B = (TCCR1B & ~(_BV(CS12) | _BV(CS11))) | _BV(CS10);
  // Set the compare register (OCR1A).
  // OCR1A is a 16-bit register, so we have to do this with
  // interrupts disabled to be safe.
  OCR1A = INTERRUPT_PERIOD;
  // Enable interrupt when TCNT1 == OCR1A (p.136)
  TIMSK1 |= _BV(OCIE1A);
  
  sei();
  sound_pwm = true;
}

void start_sound (void)
{
  // Enable interrupt when TCNT1 == OCR1A (p.136) 
  cli();
  TIMSK1 |= _BV(OCIE1A);
  sei();
  sound_on = true;
}

void stop_sound (void)
{
  cli(); 
  // Disable playback per-sample interrupt.
  TIMSK1 &= ~_BV(OCIE1A);
  sei();
  sound_on = false;
}

void set_frequency (unsigned int freq)
{
  if (sound_pwm) {
    unsigned long templong = freq;
    frequency_coef = templong * 65536 / FS;
  }
  else 
  {
    // Multiply by 2, because its only toggled once per cycle.
    unsigned long periode = F_CPU/(2*freq);
    cli();
    OCR1A = periode;
  }
}

void load_voice (int voice)
{
  if (sound_on) // if sound is on
    stop_sound(); // turn sound off
  
  switch (voice)
  {
    case 0: sine_wave(); break;
    case 1: sawtooth_wave(); break;
    case 2: triangle_wave(); break;
    case 3: square_wave(); break;
  }
  
  if (!sound_pwm)
    setup_pwm_sound();
  
  start_sound(); // start sound again
}

void sine_wave (void)
{
  for (int i = 0; i < 256; ++i)
    wavetable[i] = pgm_read_byte_near(sinetable + i);
}

void sawtooth_wave (void)
{
  for (int i = 0; i < 256; ++i)
    wavetable[i] = i; // sawtooth
}


void triangle_wave (void)
{
  int value = 255;

  for (int i = 0; i < 128; ++i)
    wavetable[i] = i * 2;
  
  for (int i = 128; i < 256; ++i)
  {
    wavetable[i] = value;
    value -= 2;
  }
}

void square_wave (void)
{
  for (int i = 0; i < 128; ++i)
    wavetable[i] = 255;

  for (int i = 128; i < 256; ++i)
    wavetable[i] = 0;
}

/**
 * Funções auxiliares.
 */
 
/**
 * Verifica se há comando em buffer.
 */
int check_command_buffer (char * buffer, char * command)
{
  if (!buffer || !command)
    return 0;
  
  int size = strlen(command);
  if (size <= 0)
    return 0;
  
  return strstr(buffer, command);
}

/**
 * Faz leitura de string da entrada serial.
 */
char * read_string_data (void)
{
  char * response = NULL, ch;
  int size = 0;
  
  while (serial.available() > 0)
  {
    ch = serial.read();
    
    if (ch != '\n' && ch != '\r' && ch != 't')
    {
      if (! (response = (char *) realloc(response, size + 2)) )
      {
        serial.println("Memory error.");
        exit(1);
      }
      
      response[size  ] = ch;
      response[size+1] = 0x00;
      size++;
    }
    
    delay(10); // Delay necessário para entrada serial o próximo caractere.
  }
  
  return response;
}


