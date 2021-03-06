//
// Created by yura on 29.03.19.
//

#include <Arduino.h>
#include "mhz19b.h"

#define SENSOR_RETRY_COUNT (30)

Mhz19b::Mhz19b(Stream &stream):stream(stream) {
    COTOMETR_LOG_ADD_DEBUG("Mhz19b inited!");
}

int Mhz19b::get_co2_uart() {
    COTOMETR_LOG_CLEAR();

    set_buffer(COMMAND_READ_CO2);

    int res = send_request();
    if (res != 0)
        return res;

    res = recv_response();
    if(res != 0)
        return res;

#ifdef COTOMETR_DEBUG_LOG
    String responce;
    // print out the response in hexa
    for (int i = 0; i < 9; i++) {
        responce += String((int)buffer[i], HEX) + "   ";
    }
    COTOMETR_LOG_ADD_DEBUG(responce);
#endif

    // ppm
    int ppm_uart = 256 * (int)buffer[2] + (int)buffer[3];
    COTOMETR_LOG_ADD_DEBUG("PPM UART: " + String(ppm_uart));

    // temp
    int temp = (unsigned char)buffer[4] - 40;
    COTOMETR_LOG_ADD_DEBUG("Temperature? " + String(temp));

    return ppm_uart;
}

int Mhz19b::set_zero_point_calibration() {
    COTOMETR_LOG_CLEAR();

    set_buffer(COMMAND_CALIBRATE_ZERO);

    int res = send_request();

    return res;
}
//TODO: add check FF
int Mhz19b::set_span_point_calibration(int span_level) {
    COTOMETR_LOG_CLEAR();

    if(span_level < 1000 || span_level > 5000)
    {
        COTOMETR_LOG_ADD_DEBUG("Invalid span level: " + span_level);
    }

    //E.g.: SPAN is 2000ppm，HIGH = 2000 / 256；LOW = 2000 % 256
    uint8_t high = highByte(span_level);
    uint8_t low = lowByte(span_level);

    set_buffer(COMMAND_CALIBRATE_SPAN, high, low);

    int res = send_request();

    return res;
}

int Mhz19b::set_auto_calibrate(bool is_auto_calibrated) {
    COTOMETR_LOG_CLEAR();
    COTOMETR_LOG_ADD_DEBUG("turn to " + (is_auto_calibrated ? String("ON") : String("OFF")));

    uint8_t byte3 = is_auto_calibrated ? (uint8_t)0xA0 : (uint8_t)0;

    set_buffer(COMMAND_CALIBRATE_AUTO_ON_OFF, byte3);

    int res = send_request();
    delay(1000);

    return res;
}

int Mhz19b::set_range(int range) {
    COTOMETR_LOG_CLEAR();

    if (range != 2000 && range != 5000) {
        COTOMETR_LOG_ADD_DEBUG("invalid range = " + String(range));
        return -1;
    }

    uint8_t high = highByte(range);
    uint8_t low = lowByte(range);

    set_buffer(COMMAND_RANGE_SETTING, high, low);

    return send_request();
}

unsigned char Mhz19b::get_crc(unsigned char *buff) {

    unsigned char checksum = 0;
    for (uint8_t i = 1; i < 8; i++) {
        checksum += buff[i];
    }

    checksum = (uint8_t)0xff - checksum;
    checksum += 1;
    return checksum;
}

int Mhz19b::send_request() {

    size_t write_size = stream.write(buffer, sizeof(buffer));//request PPM CO2
    stream.flush();

    if (write_size != sizeof(buffer))
    {
        clear_serial_cache();

        int write_error = stream.getWriteError();
        COTOMETR_LOG_ADD_DEBUG("Could not send the whole request. Only " + String(write_size) +
                         " has been sent, write error = " + String(write_error));
        return -1;
    }

    return 0;
}

int Mhz19b::recv_response()
{
    size_t response_size;
    uint8_t recv_command = buffer[2];

    if(!is_available())
    {
        clear_serial_cache();
        COTOMETR_LOG_ADD_DEBUG("The sensor doesn't response to receive data");
        return -1;
    }

    if ((response_size = (size_t)stream.available()) != sizeof(buffer)){
        clear_serial_cache();
        COTOMETR_LOG_ADD_DEBUG("Available less memory than needed" +
                          String(response_size));

        return -1;
    }

    response_size = stream.readBytes((char*)buffer, sizeof(buffer));
    if (response_size != sizeof(buffer)) {
        COTOMETR_LOG_ADD_DEBUG("Could not receive the responce. read size = "
                         + String(response_size) + ", error");
        return -1;
    }

    unsigned char calculated_crc = get_crc(buffer);
    unsigned char received_crc = buffer[8];

    if (calculated_crc != received_crc || buffer[0] != 0xff || buffer[1] != recv_command )
    {
        COTOMETR_LOG_ADD_DEBUG("ERROR recv, CRC=" + String(received_crc) +" Should be "
                         + String(calculated_crc) + ", [0]=" + String((int)buffer[0], HEX) + ", [1]="
                         + String((int)buffer[1], HEX) );
        return -1;
    }

    return 0;
}

void Mhz19b::set_buffer(uint8_t byte2, uint8_t byte3, uint8_t byte4) {
    uint8_t cmd[9] = {0xFF,0x01,0x86,0x00,0x00,0x00,0x00,0x00,0x79};

    buffer[0] = 0xFF;
    buffer[1] = 0x01;
    buffer[2] = byte2;
    buffer[3] = byte3;
    buffer[4] = byte4;
    buffer[5] = 0;
    buffer[6] = 0;
    buffer[7] = 0;
    buffer[8] = get_crc(buffer);
}

bool Mhz19b::is_available() {
    int i = 0;
    while (stream.available() == 0 && i < SENSOR_RETRY_COUNT) {
        delay(50);
        i++;
    }

    return i != SENSOR_RETRY_COUNT;
}

void Mhz19b::clear_serial_cache() {
    stream.flush();

    is_available();

    while (stream.read() > 0);
}
