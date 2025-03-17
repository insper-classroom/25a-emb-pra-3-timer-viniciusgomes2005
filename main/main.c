#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/util/datetime.h"
#include "hardware/gpio.h"
#include "hardware/rtc.h"

const int TRIGGER_PIN = 15;
const int ECHO_PIN = 14;

volatile uint64_t pulse_start_us = 0;
volatile uint64_t pulse_end_us = 0;
volatile bool timeout_occurred = false;

void echo_pin_callback(uint gpio, uint32_t events) {
    if (events & GPIO_IRQ_EDGE_RISE) {
        pulse_start_us = to_us_since_boot(get_absolute_time());
    }
    if (events & GPIO_IRQ_EDGE_FALL) {
        pulse_end_us = to_us_since_boot(get_absolute_time());
    }
}

int64_t timeout_alarm_callback(alarm_id_t id, void *user_data) {
    timeout_occurred = true;
    return 0;
}

void send_trigger_pulse() {
    gpio_put(TRIGGER_PIN, 1);
    sleep_us(10);
    gpio_put(TRIGGER_PIN, 0);
}

void process_measurement() {
    datetime_t current_time;
    rtc_get_datetime(&current_time);

    if (timeout_occurred) {
        printf("%02d:%02d:%02d - Falha na medição\n", current_time.hour, current_time.min, current_time.sec);
    } else {
        uint64_t pulse_duration = pulse_end_us - pulse_start_us;
        float distance_cm = (pulse_duration * 0.0343f) / 2.0f;
        printf("%02d:%02d:%02d - Distância: %.1f cm\n", current_time.hour, current_time.min, current_time.sec, distance_cm);
    }
}

int main() {
    stdio_init_all();

    gpio_init(TRIGGER_PIN);
    gpio_set_dir(TRIGGER_PIN, GPIO_OUT);
    gpio_put(TRIGGER_PIN, 0);

    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(ECHO_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &echo_pin_callback);

    datetime_t init_time = {
        .year  = 2025,
        .month = 3,
        .day   = 14,
        .dotw  = 6,  
        .hour  = 11,
        .min   = 50,
        .sec   = 0
    };

    rtc_init();
    rtc_set_datetime(&init_time);

    bool measurement_enabled = false;

    printf("Digite 's' para Iniciar ou 'p' para Parar as medições:\n");

    while (true) {
        int input_char = getchar_timeout_us(100000);
        if (input_char == 's' || input_char == 'S') {
            measurement_enabled = true;
            printf("Iniciando medições...\n");
        } else if (input_char == 'p' || input_char == 'P') {
            measurement_enabled = false;
            printf("Parando medições...\n");
        }

        if (measurement_enabled) {
            pulse_start_us = 0;
            pulse_end_us = 0;
            timeout_occurred = false;
            
            alarm_id_t alarm_id = add_alarm_in_ms(30, timeout_alarm_callback, NULL, false);

            send_trigger_pulse();

            while ((pulse_end_us == 0) && (!timeout_occurred)) {
                tight_loop_contents();
            }

            if (pulse_end_us != 0) {
                cancel_alarm(alarm_id);
            }
            
            process_measurement();
            
            sleep_ms(1000);
        }
    }
    return 0;
}
