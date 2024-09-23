// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_TOUCHSCREEN_METRICS_RECORDER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_TOUCHSCREEN_METRICS_RECORDER_H_

#include "ash/ash_export.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/events/devices/touchscreen_device.h"

namespace ash {

// Used for histograms and this will be in sync with
// tools/metrics/histograms/enums.xml TouchscreenConfiguration.
enum class TouchscreenConfiguration {
  // One internal, no external.
  InternalOneExternalNone = 0,
  // No internal, One external (typical for Chromebox, also common on some
  // chromebooks that don't have touch screens).
  InternalNoneExternalOne = 1,
  // One internal, One external.
  InternalOneExternalOne = 2,
  // Any number of internal, two or more external.
  InternalAnyExternalTwoPlus = 3,
  // Other possible configuration (handles INPUT_DEVICE_UNKNOWN case).
  Other = 4,
  kMaxValue = Other,
};

// Records metrics related to the configuration of input devices.
class ASH_EXPORT TouchscreenMetricsRecorder
    : public ui::InputDeviceEventObserver {
 public:
  TouchscreenMetricsRecorder();
  TouchscreenMetricsRecorder(const TouchscreenMetricsRecorder&) = delete;
  TouchscreenMetricsRecorder& operator=(const TouchscreenMetricsRecorder&) =
      delete;
  ~TouchscreenMetricsRecorder() override;

  // ui::InputDeviceEventObserver:
  void OnInputDeviceConfigurationChanged(uint8_t input_device_types) override;
  void OnDeviceListsComplete() override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_TOUCHSCREEN_METRICS_RECORDER_H_
