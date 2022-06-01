/*
 * nrf_hal_stm32.c
 *
 *  Created on: May 31, 2022
 *      Author: Dell
 */


#include "nrf_hal_stm32.h"
#include <string.h>

#ifndef SS_Pin
#define GPIO_PIN_6
#endif

#define _BIT(n) (1U<<(n))
#define _SET_BIT(v, n) ((v)|_BIT(n))
#define _CLR_BIT(v, n) ((v)&~_BIT(n))

#define false 0
#define true 1

#define cs_enable() HAL_GPIO_WritePin(GPIOB, SS_Pin, GPIO_PIN_RESET);
#define cs_disable() HAL_GPIO_WritePin(GPIOB, SS_Pin, GPIO_PIN_SET);

SPI_HandleTypeDef* _hspi_;
uint32_t _Timeout_;

void hal_nrf_init(SPI_HandleTypeDef* hspi, uint32_t Timeout)
{
	_hspi_ = hspi;
	_Timeout_ = Timeout;
}

uint8_t hal_nrf_read_reg(uint8_t reg)
{
    uint8_t tx_buf[2];
    uint8_t rx_buf[2];
    tx_buf[0] = reg;
    tx_buf[1] = 0xFF;
    cs_enable();
    HAL_SPI_TransmitReceive(_hspi_, tx_buf, rx_buf, 2, _Timeout_);
    cs_disable();
    return rx_buf[1];
}

uint16_t hal_nrf_read_multibyte_reg(uint8_t reg, uint8_t *pbuf, uint8_t len)
{
    uint8_t length;
    uint8_t buf[NRF_MAX_PL+1];

    switch(reg)
    {
    case HAL_NRF_PIPE0:
    case HAL_NRF_PIPE1:
    case HAL_NRF_TX:
        length = (!len ? hal_nrf_get_address_width() : len);
        buf[0] = RX_ADDR_P0+reg;
        break;

    case HAL_NRF_RX_PLOAD:
        reg = hal_nrf_get_rx_data_source();

        if (reg < 7) {
            length = (!len ? hal_nrf_read_rx_payload_width() : len);
            buf[0] = R_RX_PAYLOAD;
        } else
            length = 0;
        break;

    default:
        length = 0;
        break;
    }

    if (length > 0) {
        memset(&buf[1], 0, length);
        uint8_t rx_buf[6];
        cs_enable();
        HAL_SPI_TransmitReceive(_hspi_, buf, rx_buf, length+1, _Timeout_);
        cs_disable();
        memcpy(pbuf, &rx_buf[1], length);
    }
    return (((uint16_t)reg << 8) | length);
}
uint8_t hal_nrf_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t tx_buf[2];
    uint8_t rx_buf[2];
    tx_buf[0] = W_REGISTER+reg;
    tx_buf[1] = value;
    cs_enable();
    HAL_SPI_TransmitReceive(_hspi_, tx_buf, rx_buf, 2, _Timeout_);
    cs_disable();
    return rx_buf[0];
}

void hal_nrf_write_multibyte_reg(uint8_t reg, const uint8_t *pbuf, uint8_t length)
{
    uint8_t buf[NRF_MAX_PL+1];

    buf[0] = reg;
    memcpy(&buf[1], pbuf, length);
    cs_enable();
    HAL_SPI_Transmit(_hspi_, buf, length+1, _Timeout_);
    cs_disable();
}

uint8_t hal_nrf_nop(void)
{
    uint8_t status;
    cs_enable();
    uint8_t data = NOP;
    HAL_SPI_TransmitReceive(_hspi_, &data, &status, 1, _Timeout_);
    cs_disable();
    return status;
}

uint8_t hal_nrf_get_rx_data_source(void)
{
    /* read STATUS.RX_P_NO content */
    return ((hal_nrf_nop() & (uint8_t)(_BIT(3)|_BIT(2)|_BIT(1))) >> 1);
}

void hal_nrf_reuse_tx(void)
{
    cs_enable();
    uint8_t data = REUSE_TX_PL;
    HAL_SPI_Transmit(_hspi_, &data, 1, _Timeout_);
    cs_disable();
}

void hal_nrf_flush_rx(void)
{
    cs_enable();
    uint8_t data = FLUSH_RX;
    HAL_SPI_Transmit(_hspi_, &data, 1, _Timeout_);
    cs_disable();
}

void hal_nrf_flush_tx(void)
{
    cs_enable();
    uint8_t data = FLUSH_TX;
    HAL_SPI_Transmit(_hspi_, &data, 1, _Timeout_);;
    cs_disable();
}

void hal_nrf_set_operation_mode(hal_nrf_operation_mode_t op_mode)
{
    uint8_t config = hal_nrf_read_reg(CONFIG);

    if(op_mode == HAL_NRF_PRX) {
        config = (uint8_t)_SET_BIT(config, PRIM_RX);
    } else {
        config = (uint8_t)_CLR_BIT(config, PRIM_RX);
    }
    hal_nrf_write_reg(CONFIG, config);
}

hal_nrf_operation_mode_t hal_nrf_get_operation_mode(void)
{
    uint8_t config = hal_nrf_read_reg(CONFIG);
    return ((config & (uint8_t)_BIT(PRIM_RX)) ? HAL_NRF_PRX : HAL_NRF_PTX);
}

void hal_nrf_activate_features(void)
{
	hal_nrf_write_reg(ACTIVATE, 0x73);
}

void hal_nrf_enable_dynamic_payload(uint8_t enable)
{
    uint8_t feature = hal_nrf_read_reg(FEATURE);

    if (enable) {
        feature = (uint8_t)_SET_BIT(feature, EN_DPL);
    } else {
        feature = (uint8_t)_CLR_BIT(feature, EN_DPL);
    }
    hal_nrf_write_reg(FEATURE, feature);
}

uint8_t hal_nrf_is_dynamic_payload_enabled(void)
{
    return ((hal_nrf_read_reg(FEATURE) & (uint8_t)_BIT(EN_DPL)) != 0);
}

void hal_nrf_enable_ack_payload(uint8_t enable)
{
    uint8_t feature = hal_nrf_read_reg(FEATURE);

    if (enable) {
        feature = (uint8_t)_SET_BIT(feature, EN_ACK_PAY);
    } else {
        feature = (uint8_t)_CLR_BIT(feature, EN_ACK_PAY);
    }
    hal_nrf_write_reg(FEATURE, feature);
}

uint8_t hal_nrf_is_ack_payload_enabled(void)
{
    return ((hal_nrf_read_reg(FEATURE) & (uint8_t)_BIT(EN_ACK_PAY)) != 0);
}

void hal_nrf_enable_dynamic_ack(uint8_t enable)
{
    uint8_t feature = hal_nrf_read_reg(FEATURE);

    if (enable) {
        feature = (uint8_t)_SET_BIT(feature, EN_DYN_ACK);
    } else {
        feature = (uint8_t)_CLR_BIT(feature, EN_DYN_ACK);
    }
    hal_nrf_write_reg(FEATURE, feature);
}

uint8_t hal_nrf_is_dynamic_ack_enabled(void)
{
    return ((hal_nrf_read_reg(FEATURE) & (uint8_t)_BIT(EN_DYN_ACK)) != 0);
}

void hal_nrf_setup_dynamic_payload(uint8_t setup)
{
    uint8_t dynpd = setup & (uint8_t)(~(_BIT(6)|_BIT(7)));
    hal_nrf_write_reg(DYNPD, dynpd);
}

void hal_nrf_write_ack_payload(
    uint8_t pipe, const uint8_t *tx_pload, uint8_t length)
{
    hal_nrf_write_multibyte_reg(W_ACK_PAYLOAD | pipe, tx_pload, length);
}

void hal_nrf_set_rf_channel(uint8_t channel)
{
    uint8_t rf_ch = (uint8_t)(channel & 0x7f);
    hal_nrf_write_reg(RF_CH, rf_ch);
}

uint8_t hal_nrf_get_rf_channel(void)
{
    return (hal_nrf_read_reg(RF_CH) & 0x7f);
}

void hal_nrf_set_output_power(hal_nrf_output_power_t power)
{
    uint8_t rf_setup = hal_nrf_read_reg(RF_SETUP);

    rf_setup &= (uint8_t)(~(uint8_t)(_BIT(RF_PWR0)|_BIT(RF_PWR1)));
    rf_setup |= (uint8_t)(((int)power & 0x03)<<RF_PWR0);
    hal_nrf_write_reg(RF_SETUP, rf_setup);
}

hal_nrf_output_power_t hal_nrf_get_output_power(void)
{
    return (hal_nrf_output_power_t)
        ((hal_nrf_read_reg(RF_SETUP)>>RF_PWR0) & 0x03);
}

void hal_nrf_set_datarate(hal_nrf_datarate_t datarate)
{
    uint8_t rf_setup = hal_nrf_read_reg(RF_SETUP);

    switch (datarate)
    {
    case HAL_NRF_250KBPS:
        rf_setup = (uint8_t)_SET_BIT(rf_setup, RF_DR_LOW);
        rf_setup = (uint8_t)_CLR_BIT(rf_setup, RF_DR_HIGH);
        break;
    case HAL_NRF_1MBPS:
        rf_setup = (uint8_t)_CLR_BIT(rf_setup, RF_DR_LOW);
        rf_setup = (uint8_t)_CLR_BIT(rf_setup, RF_DR_HIGH);
        break;
    case HAL_NRF_2MBPS:
    default:
        rf_setup = (uint8_t)_CLR_BIT(rf_setup, RF_DR_LOW);
        rf_setup = (uint8_t)_SET_BIT(rf_setup, RF_DR_HIGH);
        break;
    }
    hal_nrf_write_reg(RF_SETUP, rf_setup);
}

hal_nrf_datarate_t hal_nrf_get_datarate(void)
{
    uint8_t rf_setup = hal_nrf_read_reg(RF_SETUP);
    return (hal_nrf_datarate_t)
        (((rf_setup & (uint8_t)_BIT(RF_DR_LOW))<<1) |
         (rf_setup & (uint8_t)_BIT(RF_DR_HIGH)));
}

void hal_nrf_set_crc_mode(hal_nrf_crc_mode_t crc_mode)
{
    uint8_t config = hal_nrf_read_reg(CONFIG);

    switch (crc_mode)
    {
    case HAL_NRF_CRC_OFF:
        config = (uint8_t)_CLR_BIT(config, EN_CRC);
        break;
    case HAL_NRF_CRC_8BIT:
        config = (uint8_t)_SET_BIT(config, EN_CRC);
        config = (uint8_t)_CLR_BIT(config, CRCO);
        break;
    case HAL_NRF_CRC_16BIT:
        config = (uint8_t)_SET_BIT(config, EN_CRC);
        config = (uint8_t)_SET_BIT(config, CRCO);
        break;
    default:
        break;
    }
    hal_nrf_write_reg(CONFIG, config);
}

hal_nrf_crc_mode_t hal_nrf_get_crc_mode(void)
{
    uint8_t config = hal_nrf_read_reg(CONFIG);

    if ((config & (uint8_t)_BIT(EN_CRC))) {
        return ((config & (uint8_t)_BIT(CRCO)) ?
            HAL_NRF_CRC_16BIT : HAL_NRF_CRC_8BIT);
    } else
        return HAL_NRF_CRC_OFF;
}

void hal_nrf_set_auto_retr(uint8_t retr, uint16_t delay)
{
    uint8_t setup_retr =
        (uint8_t)((((delay>>8) & 0x0f) << 4) | (retr & 0x0f));
    hal_nrf_write_reg(SETUP_RETR, setup_retr);
}

uint8_t hal_nrf_get_auto_retr_ctr(void)
{
    return (hal_nrf_read_reg(SETUP_RETR) & 0x0f);
}

uint16_t hal_nrf_get_auto_retr_delay(void)
{
    return (uint16_t)((((hal_nrf_read_reg(SETUP_RETR)>>4) & 0x0f)+1)*250);
}

void hal_nrf_set_rx_payload_width(uint8_t pipe_num, uint8_t pload_width)
{
    hal_nrf_write_reg(RX_PW_P0+pipe_num, pload_width);
}

uint8_t hal_nrf_get_rx_payload_width(uint8_t pipe_num)
{
    return hal_nrf_read_reg(RX_PW_P0+pipe_num);
}

void hal_nrf_open_pipe(hal_nrf_address_t pipe_num, uint8_t auto_ack)
{
    uint8_t en_rxaddr, en_aa;

    en_rxaddr = hal_nrf_read_reg(EN_RXADDR);
    en_aa = hal_nrf_read_reg(EN_AA);

    switch(pipe_num)
    {
    case HAL_NRF_PIPE0:
    case HAL_NRF_PIPE1:
    case HAL_NRF_PIPE2:
    case HAL_NRF_PIPE3:
    case HAL_NRF_PIPE4:
    case HAL_NRF_PIPE5:
        en_rxaddr = (uint8_t)_SET_BIT(en_rxaddr, (int)pipe_num);

        if(auto_ack) {
            en_aa = (uint8_t)_SET_BIT(en_aa, (int)pipe_num);
        }
        else {
            en_aa = (uint8_t)_CLR_BIT(en_aa, (int)pipe_num);
        }
        break;

    case HAL_NRF_ALL:
        en_rxaddr = (uint8_t)(~(_BIT(6)|_BIT(7)));

        if(auto_ack) {
            en_aa = (uint8_t)(~(_BIT(6)|_BIT(7)));
        }
        else {
            en_aa = 0;
        }
        break;

    case HAL_NRF_TX:
    default:
        goto finish;
    }

    hal_nrf_write_reg(EN_RXADDR, en_rxaddr);
    hal_nrf_write_reg(EN_AA, en_aa);
finish:
    return;
}

void hal_nrf_close_pipe(hal_nrf_address_t pipe_num)
{
    uint8_t en_rxaddr, en_aa;

    en_rxaddr = hal_nrf_read_reg(EN_RXADDR);
    en_aa = hal_nrf_read_reg(EN_AA);

    switch(pipe_num)
    {
    case HAL_NRF_PIPE0:
    case HAL_NRF_PIPE1:
    case HAL_NRF_PIPE2:
    case HAL_NRF_PIPE3:
    case HAL_NRF_PIPE4:
    case HAL_NRF_PIPE5:
        en_rxaddr = (uint8_t)_CLR_BIT(en_rxaddr, (int)pipe_num);
        en_aa = (uint8_t)_CLR_BIT(en_aa, (int)pipe_num);
        break;

    case HAL_NRF_ALL:
        en_rxaddr = 0;
        en_aa = 0;
        break;

    case HAL_NRF_TX:
    default:
        goto finish;
    }

    hal_nrf_write_reg(EN_RXADDR, en_rxaddr);
    hal_nrf_write_reg(EN_AA, en_aa);
finish:
    return;
}

uint8_t hal_nrf_get_pipe_status(uint8_t pipe_num)
{
    uint8_t en_rxaddr, en_aa;
    uint8_t en_rx_r=0, en_aa_r=0;

    en_rxaddr = hal_nrf_read_reg(EN_RXADDR);
    en_aa = hal_nrf_read_reg(EN_AA);

    if (pipe_num<=5) {
        en_rx_r = (en_rxaddr & (uint8_t)_BIT((int)pipe_num)) !=0;
        en_aa_r = (en_aa & (uint8_t)_BIT((int)pipe_num)) !=0;
    }
    return (uint8_t)(en_aa_r << 1) + en_rx_r;
}

void hal_nrf_set_address_width(hal_nrf_address_width_t address_width)
{
    uint8_t setup_aw = (uint8_t)(((int)address_width-2) & 0x03);
    hal_nrf_write_reg(SETUP_AW, setup_aw);
}

uint8_t hal_nrf_get_address_width(void)
{
    return (uint8_t)(hal_nrf_read_reg(SETUP_AW)+2);
}

void hal_nrf_set_address(const hal_nrf_address_t address, const uint8_t *addr)
{
    switch(address)
    {
    case HAL_NRF_TX:
    case HAL_NRF_PIPE0:
    case HAL_NRF_PIPE1:
        hal_nrf_write_multibyte_reg(W_REGISTER+RX_ADDR_P0+
            (uint8_t)address, addr, hal_nrf_get_address_width());
      break;
    case HAL_NRF_PIPE2:
    case HAL_NRF_PIPE3:
    case HAL_NRF_PIPE4:
    case HAL_NRF_PIPE5:
        hal_nrf_write_reg(RX_ADDR_P0 + (uint8_t)address, *addr);
        break;

    case HAL_NRF_ALL:
    default:
        break;
    }
}

uint8_t hal_nrf_get_address(uint8_t address, uint8_t *addr)
{
    switch (address)
    {
    case HAL_NRF_PIPE0:
    case HAL_NRF_PIPE1:
    case HAL_NRF_TX:
        return (uint8_t)hal_nrf_read_multibyte_reg(address, addr, 0);
    default:
        *addr = hal_nrf_read_reg(RX_ADDR_P0 + address);
        return 1;
    }
}

void hal_nrf_config_rx_pipe(hal_nrf_address_t pipe_num,
    const uint8_t *addr, uint8_t auto_ack, uint8_t pload_width)
{
    hal_nrf_open_pipe(pipe_num, auto_ack);

    if (addr) {
        hal_nrf_set_address(pipe_num, addr);
    }
    hal_nrf_set_rx_payload_width((uint8_t)pipe_num, pload_width);
}

void hal_nrf_config_tx(const uint8_t *addr,
    hal_nrf_output_power_t power, uint8_t retr, uint16_t delay)
{
    hal_nrf_set_output_power(power);

    hal_nrf_set_auto_retr(retr, delay);

    if (addr) {
        hal_nrf_set_address(HAL_NRF_TX, addr);
        hal_nrf_set_address(HAL_NRF_PIPE0, addr);
    }

    /* open RX pipe 0 for receiving ack */
    hal_nrf_open_pipe(HAL_NRF_PIPE0, true);
}

void hal_nrf_set_irq_mode(hal_nrf_irq_source_t int_source, uint8_t irq_state)
{
    uint8_t config = hal_nrf_read_reg(CONFIG);

    switch (int_source)
    {
    case HAL_NRF_MAX_RT:
        if (irq_state) {
            config = (uint8_t)_CLR_BIT(config, MASK_MAX_RT);
        } else {
            config = (uint8_t)_SET_BIT(config, MASK_MAX_RT);
        }
        break;
    case HAL_NRF_TX_DS:
        if (irq_state) {
            config = (uint8_t)_CLR_BIT(config, MASK_TX_DS);
        } else {
            config = (uint8_t)_SET_BIT(config, MASK_TX_DS);
        }
        break;
    case HAL_NRF_RX_DR:
        if (irq_state) {
            config = (uint8_t)_CLR_BIT(config, MASK_RX_DR);
        } else {
            config = (uint8_t)_SET_BIT(config, MASK_RX_DR);
        }
        break;
    }
    hal_nrf_write_reg(CONFIG, config);
}

uint8_t hal_nrf_get_irq_mode(hal_nrf_irq_source_t int_source)
{
    uint8_t retval = false;
    uint8_t config = hal_nrf_read_reg(CONFIG);

    switch (int_source)
    {
    case HAL_NRF_MAX_RT:
        retval = !(config & (uint8_t)_BIT(MASK_MAX_RT));
        break;
    case HAL_NRF_TX_DS:
        retval = !(config & (uint8_t)_BIT(MASK_TX_DS));
        break;
    case HAL_NRF_RX_DR:
        retval = !(config & (uint8_t)_BIT(MASK_RX_DR));
        break;
    }
    return retval;
}

uint8_t hal_nrf_get_irq_flags(void)
{
    return hal_nrf_get_status() & (uint8_t)(_BIT(MAX_RT)|_BIT(TX_DS)|_BIT(RX_DR));
}

uint8_t hal_nrf_get_clear_irq_flags(void)
{
    return hal_nrf_write_reg(STATUS,
        (uint8_t)(_BIT(MAX_RT)|_BIT(TX_DS)|_BIT(RX_DR))) &
        (uint8_t)(_BIT(MAX_RT)|_BIT(TX_DS)|_BIT(RX_DR));
}

uint8_t hal_nrf_clear_irq_flags_get_status(void)
{
    /* When RFIRQ is cleared (when calling write_reg), pipe information
      is unreliable (read again with read_reg) */
    uint8_t retval = hal_nrf_write_reg(STATUS,
        (uint8_t)(_BIT(MAX_RT)|_BIT(TX_DS)|_BIT(RX_DR))) &
        (uint8_t)(_BIT(MAX_RT)|_BIT(TX_DS)|_BIT(RX_DR));

    retval |= hal_nrf_read_reg(STATUS) &
        (uint8_t)~(_BIT(MAX_RT)|_BIT(TX_DS)|_BIT(RX_DR));

    return retval;
}

void hal_nrf_clear_irq_flag(hal_nrf_irq_source_t int_source)
{
    hal_nrf_write_reg(STATUS, (uint8_t)_BIT(int_source));
}

void hal_nrf_set_power_mode(hal_nrf_pwr_mode_t pwr_mode)
{
    uint8_t config = hal_nrf_read_reg(CONFIG);

    if(pwr_mode == HAL_NRF_PWR_UP) {
        config = (uint8_t)_SET_BIT(config, PWR_UP);
    } else {
        config = (uint8_t)_CLR_BIT(config, PWR_UP);
    }
    hal_nrf_write_reg(CONFIG, config);
}

hal_nrf_pwr_mode_t hal_nrf_get_power_mode(void)
{
    uint8_t config = hal_nrf_read_reg(CONFIG);
    return ((config & (uint8_t)_BIT(PWR_UP)) ? HAL_NRF_PWR_UP : HAL_NRF_PWR_DOWN);
}

uint8_t hal_nrf_get_tx_fifo_status(void)
{
    return ((hal_nrf_read_reg(FIFO_STATUS) & (uint8_t)(_BIT(TX_FIFO_FULL)|_BIT(TX_EMPTY))) >> 4);
}

uint8_t hal_nrf_tx_fifo_empty(void)
{
    return (uint8_t)((hal_nrf_read_reg(FIFO_STATUS) >> TX_EMPTY) & 0x01);
}

uint8_t hal_nrf_tx_fifo_full(void)
{
    return (uint8_t)((hal_nrf_read_reg(FIFO_STATUS) >> TX_FIFO_FULL) & 0x01);
}

uint8_t hal_nrf_get_rx_fifo_status(void)
{
    return (hal_nrf_read_reg(FIFO_STATUS) &
        (uint8_t)(_BIT(RX_FULL)|_BIT(RX_EMPTY)));
}

uint8_t hal_nrf_rx_fifo_empty(void)
{
    return (uint8_t)((hal_nrf_read_reg(FIFO_STATUS) >> RX_EMPTY) & 0x01);
}

uint8_t hal_nrf_rx_fifo_full(void)
{
    return (uint8_t)((hal_nrf_read_reg(FIFO_STATUS) >> RX_FULL) & 0x01);
}

uint8_t hal_nrf_get_fifo_status(void)
{
    return hal_nrf_read_reg(FIFO_STATUS);
}

uint8_t hal_nrf_get_auto_retr_status(void)
{
    return hal_nrf_read_reg(OBSERVE_TX);
}

uint8_t hal_nrf_get_transmit_attempts(void)
{
    return (hal_nrf_read_reg(OBSERVE_TX) &
        (uint8_t)(_BIT(3)|_BIT(2)|_BIT(1)|_BIT(0)));
}

uint8_t hal_nrf_get_packet_lost_ctr(void)
{
    return ((hal_nrf_read_reg(OBSERVE_TX) &
        (uint8_t)(_BIT(7)|_BIT(6)|_BIT(5)|_BIT(4))) >> 4);
}

uint8_t hal_nrf_get_carrier_detect(void)
{
    return (uint8_t)(hal_nrf_read_reg(CD) & 0x01);
}

uint8_t hal_nrf_read_rx_payload_width(void)
{
    return hal_nrf_read_reg(R_RX_PL_WID);
}

uint16_t hal_nrf_read_rx_payload(uint8_t *rx_pload)
{
    return hal_nrf_read_multibyte_reg((uint8_t)HAL_NRF_RX_PLOAD, rx_pload, 0);
}

void hal_nrf_write_tx_payload(const uint8_t *tx_pload, uint8_t length)
{
    hal_nrf_write_multibyte_reg(W_TX_PAYLOAD, tx_pload, length);
}

void hal_nrf_write_tx_payload_noack(const uint8_t *tx_pload, uint8_t length)
{
    hal_nrf_write_multibyte_reg(W_TX_PAYLOAD_NOACK, tx_pload, length);
}

uint8_t hal_nrf_get_reuse_tx_status(void)
{
    return (uint8_t)((hal_nrf_read_reg(FIFO_STATUS) &
        (uint8_t)_BIT(TX_REUSE)) >> TX_REUSE);
}

void hal_nrf_set_pll_mode(uint8_t pll_lock)
{
    uint8_t rf_setup = hal_nrf_read_reg(RF_SETUP);

    if (pll_lock) {
        rf_setup = (uint8_t)_SET_BIT(rf_setup, PLL_LOCK);
    } else {
        rf_setup = (uint8_t)_CLR_BIT(rf_setup, PLL_LOCK);
    }
    hal_nrf_write_reg(RF_SETUP, rf_setup);
}

uint8_t hal_nrf_get_pll_mode(void)
{
    return ((hal_nrf_read_reg(RF_SETUP) & (uint8_t)_BIT(PLL_LOCK)) != 0);
}

void hal_nrf_enable_continious_wave(uint8_t enable)
{
    uint8_t rf_setup = hal_nrf_read_reg(RF_SETUP);

    if (enable) {
        rf_setup = (uint8_t)_SET_BIT(rf_setup, CONT_WAVE);
    } else {
        rf_setup = (uint8_t)_CLR_BIT(rf_setup, CONT_WAVE);
    }
    hal_nrf_write_reg(RF_SETUP, rf_setup);
}

uint8_t hal_nrf_is_continious_wave_enabled(void)
{
    return ((hal_nrf_read_reg(RF_SETUP) & (uint8_t)_BIT(CONT_WAVE)) != 0);
}

void hal_nrf_save_ctx(hal_nrf_ctx_t *p_ctx)
{
    memset(p_ctx, 0, sizeof(*p_ctx));

    p_ctx->config     = hal_nrf_read_reg(CONFIG);
    p_ctx->en_aa      = hal_nrf_read_reg(EN_AA);
    p_ctx->en_rxaddr  = hal_nrf_read_reg(EN_RXADDR);
    p_ctx->setup_aw   = hal_nrf_read_reg(SETUP_AW);
    p_ctx->setup_retr = hal_nrf_read_reg(SETUP_RETR);
    p_ctx->rf_ch      = hal_nrf_read_reg(RF_CH);
    p_ctx->rf_setup   = hal_nrf_read_reg(RF_SETUP);

    hal_nrf_read_multibyte_reg(
        HAL_NRF_PIPE0, p_ctx->rx_addr_p0, sizeof(p_ctx->rx_addr_p0));

    hal_nrf_read_multibyte_reg(
        HAL_NRF_PIPE1, p_ctx->rx_addr_p1, sizeof(p_ctx->rx_addr_p1));

    p_ctx->rx_addr_p2 = hal_nrf_read_reg(RX_ADDR_P2);
    p_ctx->rx_addr_p3 = hal_nrf_read_reg(RX_ADDR_P3);
    p_ctx->rx_addr_p4 = hal_nrf_read_reg(RX_ADDR_P4);
    p_ctx->rx_addr_p5 = hal_nrf_read_reg(RX_ADDR_P5);

    hal_nrf_read_multibyte_reg(
        HAL_NRF_TX, p_ctx->tx_addr, sizeof(p_ctx->tx_addr));

    p_ctx->rx_pw_p0   = hal_nrf_read_reg(RX_PW_P0);
    p_ctx->rx_pw_p1   = hal_nrf_read_reg(RX_PW_P1);
    p_ctx->rx_pw_p2   = hal_nrf_read_reg(RX_PW_P2);
    p_ctx->rx_pw_p3   = hal_nrf_read_reg(RX_PW_P3);
    p_ctx->rx_pw_p4   = hal_nrf_read_reg(RX_PW_P4);
    p_ctx->rx_pw_p5   = hal_nrf_read_reg(RX_PW_P5);
    p_ctx->dynpd      = hal_nrf_read_reg(DYNPD);
    p_ctx->feature    = hal_nrf_read_reg(FEATURE);

    return;
}
