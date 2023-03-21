// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_METRICS_MANAGER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_METRICS_MANAGER_H_

#include "ash/ash_export.h"

namespace ash {

class ASH_EXPORT InputDeviceSettingsMetricsManager {
 public:
  InputDeviceSettingsMetricsManager();
  InputDeviceSettingsMetricsManager(const InputDeviceSettingsMetricsManager&) =
      delete;
  InputDeviceSettingsMetricsManager& operator=(
      const InputDeviceSettingsMetricsManager&) = delete;
  ~InputDeviceSettingsMetricsManager();
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_METRICS_MANAGER_H_
