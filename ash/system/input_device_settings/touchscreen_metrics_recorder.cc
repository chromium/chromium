// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/touchscreen_metrics_recorder.h"

#include "base/metrics/histogram_functions.h"
#include "ui/events/devices/device_data_manager.h"

namespace ash {

namespace {

TouchscreenConfiguration DetermineTouchscreenConfiguration(int internal_count,
                                                           int external_count) {
  if (internal_count == 1 && external_count == 0) {
    return TouchscreenConfiguration::InternalOneExternalNone;
  }
  if (internal_count == 0 && external_count == 1) {
    return TouchscreenConfiguration::InternalNoneExternalOne;
  }
  if (internal_count == 1 && external_count == 1) {
    return TouchscreenConfiguration::InternalOneExternalOne;
  }
  if (external_count >= 2) {
    return TouchscreenConfiguration::InternalAnyExternalTwoPlus;
  }
  return TouchscreenConfiguration::Other;
}

void RecordTouchscreenConfiguration(
    const std::vector<ui::TouchscreenDevice>& devices) {
  if (devices.empty()) {
    return;
  }

  int internal_count = 0;
  int external_count = 0;
  for (const auto& device : devices) {
    if (device.type == ui::INPUT_DEVICE_INTERNAL) {
      internal_count++;
    } else if (device.type == ui::INPUT_DEVICE_USB ||
               device.type == ui::INPUT_DEVICE_BLUETOOTH) {
      external_count++;
    }
  }

  if (external_count > 0) {
    base::UmaHistogramCounts100(
        "ChromeOS.Inputs.Touchscreen.Connected.External.Count", external_count);
  }

  base::UmaHistogramEnumeration(
      "ChromeOS.Inputs.Touchscreen.Connected.Configuration",
      DetermineTouchscreenConfiguration(internal_count, external_count));
}

}  // namespace

TouchscreenMetricsRecorder::TouchscreenMetricsRecorder() {
  ui::DeviceDataManager* device_data_manager =
      ui::DeviceDataManager::GetInstance();
  DCHECK(device_data_manager);
  device_data_manager->AddObserver(this);
}

TouchscreenMetricsRecorder::~TouchscreenMetricsRecorder() {
  ui::DeviceDataManager* device_data_manager =
      ui::DeviceDataManager::GetInstance();
  DCHECK(device_data_manager);
  device_data_manager->RemoveObserver(this);
}

void TouchscreenMetricsRecorder::OnInputDeviceConfigurationChanged(
    uint8_t input_device_types) {
  if (input_device_types & ui::InputDeviceEventObserver::kTouchscreen) {
    RecordTouchscreenConfiguration(
        ui::DeviceDataManager::GetInstance()->GetTouchscreenDevices());
  }
}

void TouchscreenMetricsRecorder::OnDeviceListsComplete() {
  RecordTouchscreenConfiguration(
      ui::DeviceDataManager::GetInstance()->GetTouchscreenDevices());
}

}  // namespace ash
