#include "vl53l1x.h"

namespace esphome {
namespace vl53l1x {

void VL53L1X::dump_config() {
  ESP_LOGCONFIG(TAG, "dump config:");
  LOG_I2C_DEVICE(this)
}

void VL53L1X::setup() {
  auto speed = Wire.getClock();
  if (speed < 400000) {
    ESP_LOGW(TAG, "Slow clock speed, speed: %d", speed);
  }

  // TODO use xshut_pin, if given, to change address
  auto status = this->sensor.Begin(this->address_);
  if (status != VL53L1_ERROR_NONE) {
    // If the sensor could not be initialized print out the error code. -7 is timeout
    ESP_LOGE(TAG, "Could not initialize the sensor, error code: %d", status);
    this->mark_failed();
    return;
  }

  if (this->offset.has_value()) {
    ESP_LOGI(TAG, "Setting sensor offset calibration to %d", this->offset.value());
    status = this->sensor.SetOffsetInMm(this->offset.value());
    if (status != VL53L1_ERROR_NONE) {
      ESP_LOGE(TAG, "Could not set sensor offset calibration, error code: %d", status);
      this->mark_failed();
      return;
    }
  }

  if (this->xtalk.has_value()) {
    ESP_LOGI(TAG, "Setting sensor xtalk calibration to %d", this->xtalk.value());
    status = this->sensor.SetXTalk(this->xtalk.value());
    if (status != VL53L1_ERROR_NONE) {
      ESP_LOGE(TAG, "Could not set sensor offset calibration, error code: %d", status);
      this->mark_failed();
      return;
    }
  }

  ESP_LOGI(TAG, "Setup complete");
}

void VL53L1X::set_ranging_mode(const RangingMode *mode) {
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Cannot set ranging mode while component is failed");
    return;
  }

  auto status = this->sensor.SetDistanceMode(mode->mode);
  if (status != VL53L1_ERROR_NONE) {
    ESP_LOGE(TAG, "Could not set distance mode.  mode: %d", mode->mode);
  }

  status = this->sensor.SetTimingBudgetInMs(mode->timing_budget);
  if (status != VL53L1_ERROR_NONE) {
    ESP_LOGE(TAG, "Could not set timing budget.  timing_budget: %d ms", mode->timing_budget);
  }

  status = this->sensor.SetInterMeasurementInMs(mode->delay_between_measurements);
  if (status != VL53L1_ERROR_NONE) {
    ESP_LOGE(TAG, "Could not set measurement delay.  %d ms", mode->delay_between_measurements);
  }

  ESP_LOGI(TAG, "Set ranging mode. timing_budget: %d, delay: %d, distance_mode: %d", mode->timing_budget,
           mode->delay_between_measurements, mode->mode);
}

optional<uint16_t> VL53L1X::read_distance(ROI *roi, VL53L1_Error &status) {
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Cannot read distance while component is failed");
    return {};
  }

  ESP_LOGV(TAG, "Beginning distance read");

  status = this->sensor.SetROI(roi->width, roi->height);
  status += this->sensor.SetROICenter(roi->center);
  if (status != VL53L1_ERROR_NONE) {
    ESP_LOGE(TAG, "Could not set ROI, error code: %d", status);
    return {};
  }

  status = this->sensor.StartRanging();

  // Wait for the measurement to be ready
  // TODO use interrupt_pin, if given, to await data ready instead of polling
  uint8_t dataReady = false;
  while (!dataReady) {
    status += this->sensor.CheckForDataReady(&dataReady);
    if (status != VL53L1_ERROR_NONE) {
      ESP_LOGE(TAG, "Failed to check if data is ready, error code: %d", status);
      return {};
    }
    delay(1);
  }

  // Get the results
  uint16_t distance;
  status += this->sensor.GetDistanceInMm(&distance);
  if (status != VL53L1_ERROR_NONE) {
    ESP_LOGE(TAG, "Could not get distance, error code: %d", status);
    return {};
  }

  // After reading the results reset the interrupt to be able to take another measurement
  status = this->sensor.ClearInterrupt();
  status += this->sensor.StopRanging();
  if (status != VL53L1_ERROR_NONE) {
    ESP_LOGE(TAG, "Could not stop ranging, error code: %d", status);
    return {};
  }

  ESP_LOGV(TAG, "Finished distance read");
  return {distance};
}

}  // namespace vl53l1x
}  // namespace esphome
