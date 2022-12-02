// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_PREF_MANAGER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_PREF_MANAGER_H_

#include "ash/ash_export.h"

namespace ash {

// Manages the prefs which correspond to system settings for input devices.
class ASH_EXPORT InputDevicePrefManager {
 public:
  InputDevicePrefManager();
  InputDevicePrefManager(const InputDevicePrefManager&) = delete;
  InputDevicePrefManager& operator=(const InputDevicePrefManager&) = delete;
  ~InputDevicePrefManager();
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_PREF_MANAGER_H_
