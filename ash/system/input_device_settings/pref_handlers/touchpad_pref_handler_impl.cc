// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/pref_handlers/touchpad_pref_handler_impl.h"

#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "base/check.h"
#include "base/notreached.h"
#include "components/prefs/pref_service.h"

namespace ash {

TouchpadPrefHandlerImpl::TouchpadPrefHandlerImpl() = default;
TouchpadPrefHandlerImpl::~TouchpadPrefHandlerImpl() = default;

// TODO(michaelcheco): Implement touchpad settings initialization.
void TouchpadPrefHandlerImpl::InitializeTouchpadSettings(
    PrefService* pref_service,
    mojom::Touchpad* touchpad) {
  NOTIMPLEMENTED();
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

}  // namespace ash
