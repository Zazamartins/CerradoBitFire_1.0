#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "inc/ssd1306.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "ds18b20/temp.h"

// Definições dos pinos
const uint I2C_SDA = 14;
const uint I2C_SCL = 15;
const uint LED_VERDE = 11;
const uint LED_VERMELHO = 13;
const uint BUZZER_PIN = 21; // Pino do buzzer

// Parâmetros de temperatura (ajustáveis)
const float TEMP_MIN = 40.0f;  // Começa a transição
const float TEMP_MAX = 70.0f;  // Vermelho total
const uint16_t PERIODO_PWM = 2000;

// Protótipos de função
void configurar_leds();
void atualizar_leds(float temperatura);
void pwm_init_buzzer(uint pin);
void play_tone(uint pin, uint frequency, uint duration_ms);
void play_intense_tone(uint pin);

int main() {
    stdio_init_all();
    
    // Configuração dos LEDs
    configurar_leds();
    
    // Configuração do buzzer
    pwm_init_buzzer(BUZZER_PIN);

    // Resto da inicialização (display e sensor)
    gpio_init(16);
    if(presence(16) == 1) {
        printf("Erro de comunicacao\n");
    }

    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init();

    struct render_area frame_area = {
        start_column : 0,
        end_column : ssd1306_width - 1,
        start_page : 0,
        end_page : ssd1306_n_pages - 1
    };
    calculate_render_area_buffer_length(&frame_area);
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, &frame_area);

    for(;;) {
        float temperatura;
        do {
            temperatura = getTemperature(16);
        } while(temperatura < -999);

        // Atualiza LEDs baseado na temperatura
        atualizar_leds(temperatura);

        // Se a temperatura ultrapassar 70°C, aciona o buzzer com sequência de notas intensas
        if (temperatura > 70.0f) {
            play_intense_tone(BUZZER_PIN);  // Chama a sequência de tons intensos
        }

        // Resto do código do display
        printf("%.2f\r Celsius\n", temperatura);
        char temp_str[10];
        snprintf(temp_str, sizeof(temp_str), "%.2f", temperatura);
        memset(ssd, 0, ssd1306_buffer_length);

        int text_width1 = strlen("TEMPERATURA") * 6;
        int text_width2 = strlen("CELSIUS") * 6;
        int text_width3 = strlen(temp_str) * 6;

        int start_x1 = (ssd1306_width - text_width1) / 2;
        int start_x2 = (ssd1306_width - text_width2) / 2;
        int start_x3 = (ssd1306_width - text_width3) / 2;

        ssd1306_draw_string(ssd, start_x1, 0, "TEMPERATURA");
        ssd1306_draw_string(ssd, start_x2, 20, "CELSIUS");
        ssd1306_draw_string(ssd, start_x3, 40, temp_str);
        render_on_display(ssd, &frame_area);

        sleep_ms(500);
    }
    return 0;
}

void configurar_leds() {
    // Configuração do LED verde
    gpio_set_function(LED_VERDE, GPIO_FUNC_PWM);
    uint slice_verde = pwm_gpio_to_slice_num(LED_VERDE);
    pwm_set_wrap(slice_verde, PERIODO_PWM);
    pwm_set_enabled(slice_verde, true);

    // Configuração do LED vermelho
    gpio_set_function(LED_VERMELHO, GPIO_FUNC_PWM);
    uint slice_vermelho = pwm_gpio_to_slice_num(LED_VERMELHO);
    pwm_set_wrap(slice_vermelho, PERIODO_PWM);
    pwm_set_enabled(slice_vermelho, true);
}

void atualizar_leds(float temperatura) {
    // Calcula a intensidade dos LEDs
    float proporcao = (temperatura - TEMP_MIN) / (TEMP_MAX - TEMP_MIN);
    proporcao = proporcao < 0 ? 0 : (proporcao > 1 ? 1 : proporcao); // Limita entre 0 e 1

    uint16_t verde = PERIODO_PWM * (1 - proporcao);
    uint16_t vermelho = PERIODO_PWM * proporcao;

    // Aplica os valores nos LEDs
    pwm_set_gpio_level(LED_VERDE, verde);
    pwm_set_gpio_level(LED_VERMELHO, vermelho);
}

// Inicializa o PWM no pino do buzzer
void pwm_init_buzzer(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pin);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 4.0f); // Ajusta divisor de clock
    pwm_init(slice_num, &config, true);
    pwm_set_gpio_level(pin, 0); // Desliga o PWM inicialmente
}

// Toca uma nota com a frequência e duração especificadas
void play_tone(uint pin, uint frequency, uint duration_ms) {
    uint slice_num = pwm_gpio_to_slice_num(pin);
    uint32_t clock_freq = clock_get_hz(clk_sys);  // Frequência do sistema
    uint32_t top = clock_freq / frequency - 1;

    pwm_set_wrap(slice_num, top);
    pwm_set_gpio_level(pin, top * 0.9); // Aumenta o Duty Cycle para 90%

    sleep_ms(duration_ms);

    pwm_set_gpio_level(pin, 0); // Desliga o som após a duração
    sleep_ms(50); // Pausa entre notas
}

// Função para tocar uma sequência de notas mais intensas
void play_intense_tone(uint pin) {
    uint frequencies[] = {2000, 2500, 3000}; // Frequências mais altas para maior intensidade
    uint durations[] = {200, 150, 100}; // Duração das notas, mais curtas para um efeito mais intenso

    for (int i = 0; i < 3; i++) {
        play_tone(pin, frequencies[i], durations[i]);
    }
}
