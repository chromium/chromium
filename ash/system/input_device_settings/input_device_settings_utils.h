// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_UTILS_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_UTILS_H_

#include "ash/ash_export.h"
#include "ui/events/devices/input_device.h"

namespace ash {

// Checks if a given value is within the bounds set by `ui::mojom::ModifierKey`.
bool IsValidModifier(int val);

// Builds `device_key` for use in storing device settings in prefs.
ASH_EXPORT std::string BuildDeviceKey(const ui::InputDevice& device);

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_UTILS_H_
