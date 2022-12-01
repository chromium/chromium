// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_CONTROLLER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_CONTROLLER_H_

#include "ash/ash_export.h"

namespace ash {

// Used to denote the category of a given input device.
enum class InputDeviceCategory {
  kMouse,
  kTouchpad,
  kPointingStick,
  kKeyboard,
};

// Controller to manage input device settings.
class ASH_EXPORT InputDeviceSettingsController {
 public:
  InputDeviceSettingsController();
  InputDeviceSettingsController(const InputDeviceSettingsController&) = delete;
  InputDeviceSettingsController& operator=(
      const InputDeviceSettingsController&) = delete;
  ~InputDeviceSettingsController();
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_CONTROLLER_H_
