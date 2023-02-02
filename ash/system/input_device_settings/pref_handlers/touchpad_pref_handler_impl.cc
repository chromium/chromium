// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/pref_handlers/touchpad_pref_handler_impl.h"

#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/system/input_device_settings/input_device_settings_defaults.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "base/check.h"
#include "components/prefs/pref_service.h"

namespace ash {

TouchpadPrefHandlerImpl::TouchpadPrefHandlerImpl() = default;
TouchpadPrefHandlerImpl::~TouchpadPrefHandlerImpl() = default;

void TouchpadPrefHandlerImpl::InitializeTouchpadSettings(
    PrefService* pref_service,
    mojom::Touchpad* touchpad) {
  const auto& devices_dict =
      pref_service->GetDict(prefs::kTouchpadDeviceSettingsDictPref);
  const auto* settings_dict = devices_dict.FindDict(touchpad->device_key);
  if (!settings_dict) {
    touchpad->settings = GetNewTouchpadSettings(*touchpad);
  } else {
    touchpad->settings =
        RetreiveTouchpadSettings(pref_service, *touchpad, *settings_dict);
  }
  DCHECK(touchpad->settings);

  UpdateTouchpadSettings(pref_service, *touchpad);
}

void TouchpadPrefHandlerImpl::UpdateTouchpadSettings(
    PrefService* pref_service,
    const mojom::Touchpad& touchpad) {
  DCHECK(touchpad.settings);
  const mojom::TouchpadSettings& settings = *touchpad.settings;
  // Populate `settings_dict` with all settings in `settings`.
  base::Value::Dict settings_dict;
  settings_dict.Set(prefs::kTouchpadSettingSensitivity, settings.sensitivity);
  settings_dict.Set(prefs::kTouchpadSettingReverseScrolling,
                    settings.reverse_scrolling);
  settings_dict.Set(prefs::kTouchpadSettingAccelerationEnabled,
                    settings.acceleration_enabled);
  settings_dict.Set(prefs::kTouchpadSettingScrollSensitivity,
                    settings.scroll_sensitivity);
  settings_dict.Set(prefs::kTouchpadSettingScrollAcceleration,
                    settings.scroll_acceleration);
  settings_dict.Set(prefs::kTouchpadSettingTapToClickEnabled,
                    settings.tap_to_click_enabled);
  settings_dict.Set(prefs::kTouchpadSettingThreeFingerClickEnabled,
                    settings.three_finger_click_enabled);
  settings_dict.Set(prefs::kTouchpadSettingTapDraggingEnabled,
                    settings.tap_dragging_enabled);
  settings_dict.Set(prefs::kTouchpadSettingHapticSensitivity,
                    settings.haptic_sensitivity);
  settings_dict.Set(prefs::kTouchpadSettingHapticEnabled,
                    settings.haptic_enabled);

  // Retrieve old settings and merge with the new ones.
  base::Value::Dict devices_dict =
      pref_service->GetDict(prefs::kTouchpadDeviceSettingsDictPref).Clone();

  // If an old settings dict already exists for the device, merge the updated
  // settings into the old settings. Otherwise, insert the dict at
  // `touchpad.device_key`.
  base::Value::Dict* old_settings_dict =
      devices_dict.FindDict(touchpad.device_key);
  if (old_settings_dict) {
    old_settings_dict->Merge(std::move(settings_dict));
  } else {
    devices_dict.Set(touchpad.device_key, std::move(settings_dict));
  }

  pref_service->SetDict(std::string(prefs::kTouchpadDeviceSettingsDictPref),
                        std::move(devices_dict));
}

mojom::TouchpadSettingsPtr TouchpadPrefHandlerImpl::GetNewTouchpadSettings(
    const mojom::Touchpad& touchpad) {
  // TODO(michaelcheco): Implement pulling from old device settings if the
  // device was observed in the transition period.
  mojom::TouchpadSettingsPtr settings = mojom::TouchpadSettings::New();
  settings->sensitivity = kDefaultSensitivity;
  settings->reverse_scrolling = kDefaultReverseScrolling;
  settings->acceleration_enabled = kDefaultAccelerationEnabled;
  settings->tap_to_click_enabled = kDefaultTapToClickEnabled;
  settings->three_finger_click_enabled = kDefaultThreeFingerClickEnabled;
  settings->tap_dragging_enabled = kDefaultTapDraggingEnabled;
  settings->scroll_sensitivity = kDefaultSensitivity;
  settings->scroll_acceleration = kDefaultScrollAcceleration;
  settings->haptic_sensitivity = kDefaultHapticSensitivity;
  settings->haptic_enabled = kDefaultHapticFeedbackEnabled;
  return settings;
}

mojom::TouchpadSettingsPtr TouchpadPrefHandlerImpl::RetreiveTouchpadSettings(
    PrefService* pref_service,
    const mojom::Touchpad& touchpad,
    const base::Value::Dict& settings_dict) {
  mojom::TouchpadSettingsPtr settings = mojom::TouchpadSettings::New();
  settings->sensitivity =
      settings_dict.FindInt(prefs::kTouchpadSettingSensitivity)
          .value_or(kDefaultSensitivity);
  settings->reverse_scrolling =
      settings_dict.FindBool(prefs::kTouchpadSettingReverseScrolling)
          .value_or(kDefaultReverseScrolling);
  settings->acceleration_enabled =
      settings_dict.FindBool(prefs::kTouchpadSettingAccelerationEnabled)
          .value_or(kDefaultAccelerationEnabled);
  settings->tap_to_click_enabled =
      settings_dict.FindBool(prefs::kTouchpadSettingTapToClickEnabled)
          .value_or(kDefaultTapToClickEnabled);
  settings->three_finger_click_enabled =
      settings_dict.FindBool(prefs::kTouchpadSettingThreeFingerClickEnabled)
          .value_or(kDefaultThreeFingerClickEnabled);
  settings->tap_dragging_enabled =
      settings_dict.FindBool(prefs::kTouchpadSettingTapDraggingEnabled)
          .value_or(kDefaultTapDraggingEnabled);
  settings->scroll_sensitivity =
      settings_dict.FindInt(prefs::kTouchpadSettingScrollSensitivity)
          .value_or(kDefaultSensitivity);
  settings->scroll_acceleration =
      settings_dict.FindBool(prefs::kTouchpadSettingScrollAcceleration)
          .value_or(kDefaultScrollAcceleration);
  settings->haptic_sensitivity =
      settings_dict.FindInt(prefs::kTouchpadSettingHapticSensitivity)
          .value_or(kDefaultSensitivity);
  settings->haptic_enabled =
      settings_dict.FindBool(prefs::kTouchpadSettingHapticEnabled)
          .value_or(kDefaultHapticFeedbackEnabled);
  return settings;
}

}  // namespace ash
