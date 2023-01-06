// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_PREF_NAMES_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_PREF_NAMES_H_

namespace ash::prefs {
constexpr char kMouseObservedDevicesPref[] = "settings.mouse.observed_devices";
constexpr char kTouchpadObservedDevicesPref[] =
    "settings.touchpad.observed_devices";
constexpr char kPointingStickObservedDevicesPref[] =
    "settings.pointing_stick.observed_devices";
constexpr char kKeyboardObservedDevicesPref[] =
    "settings.keyboard.observed_devices";
}  // namespace ash::prefs

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_PREF_NAMES_H_
