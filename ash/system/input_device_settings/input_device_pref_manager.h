// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_PREF_MANAGER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_PREF_MANAGER_H_

#include "ash/ash_export.h"
#include "ash/public/mojom/input_device_settings.mojom.h"

namespace ash {

// Manages the prefs which correspond to system settings for input devices.
class ASH_EXPORT InputDevicePrefManager {
 public:
  virtual ~InputDevicePrefManager() = default;

  // Initialize device settings in prefs and update the |settings| member of the
  // |mojom::Keyboard| object.
  virtual void InitializeKeyboardSettings(mojom::Keyboard* keyboard) = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_PREF_MANAGER_H_
