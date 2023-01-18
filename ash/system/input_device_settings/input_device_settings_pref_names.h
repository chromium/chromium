// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_PREF_NAMES_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_PREF_NAMES_H_

namespace ash::prefs {
// Prefs which contain lists of observed devices for a few milestones before
// per-device settings are enabled.
constexpr char kKeyboardObservedDevicesPref[] =
    "settings.keyboard.observed_devices";
constexpr char kMouseObservedDevicesPref[] = "settings.mouse.observed_devices";
constexpr char kPointingStickObservedDevicesPref[] =
    "settings.pointing_stick.observed_devices";
constexpr char kTouchpadObservedDevicesPref[] =
    "settings.touchpad.observed_devices";

// Prefs which contain dictionaries of settings for each connected device.
constexpr char kKeyboardDeviceSettingsDictPref[] = "settings.keyboard.devices";
constexpr char kMouseDeviceSettingsDictPref[] = "settings.mouse.devices";
constexpr char kPointingStickDeviceSettingsDictPref[] =
    "settings.pointing_stick.devices";
constexpr char kTouchpadDeviceSettingsDictPref[] = "settings.touchpad.devices";

// Keyboard settings dictionary keys.
constexpr char kKeyboardSettingAutoRepeatDelay[] = "auto_repeat_delay";
constexpr char kKeyboardSettingAutoRepeatEnabled[] = "auto_repeat_enabled";
constexpr char kKeyboardSettingAutoRepeatInterval[] = "auto_repeat_interval";
constexpr char kKeyboardSettingModifierRemappings[] = "modifier_remappings";
constexpr char kKeyboardSettingSuppressMetaFKeyRewrites[] =
    "suppress_meta_fkey_rewrites";
constexpr char kKeyboardSettingTopRowAreFKeys[] = "top_row_are_fkeys";
}  // namespace ash::prefs

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_PREF_NAMES_H_
