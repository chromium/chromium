// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/pref_handlers/mouse_pref_handler_impl.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_defaults.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/system/input_device_settings/input_device_tracker.h"
#include "base/check.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace {

mojom::MouseSettingsPtr GetDefaultMouseSettings() {
  mojom::MouseSettingsPtr settings = mojom::MouseSettings::New();
  settings->swap_right = kDefaultSwapRight;
  settings->sensitivity = kDefaultSensitivity;
  settings->reverse_scrolling = kDefaultReverseScrolling;
  settings->acceleration_enabled = kDefaultAccelerationEnabled;
  settings->scroll_sensitivity = kDefaultSensitivity;
  settings->scroll_acceleration = kDefaultScrollAcceleration;
  return settings;
}

// GetMouseSettingsFromPrefs returns a mouse settings based on user prefs
// to be used as settings for new mouses.
mojom::MouseSettingsPtr GetMouseSettingsFromPrefs(PrefService* prefs) {
  mojom::MouseSettingsPtr settings = mojom::MouseSettings::New();
  settings->swap_right = prefs->GetBoolean(prefs::kPrimaryMouseButtonRight);
  settings->sensitivity = prefs->GetInteger(prefs::kMouseSensitivity);
  settings->reverse_scrolling = prefs->GetBoolean(prefs::kMouseReverseScroll);
  settings->acceleration_enabled = prefs->GetBoolean(prefs::kMouseAcceleration);
  settings->scroll_sensitivity =
      prefs->GetInteger(prefs::kMouseScrollSensitivity);
  settings->scroll_acceleration =
      prefs->GetBoolean(prefs::kMouseScrollAcceleration);
  return settings;
}

}  // namespace

MousePrefHandlerImpl::MousePrefHandlerImpl() = default;
MousePrefHandlerImpl::~MousePrefHandlerImpl() = default;

void MousePrefHandlerImpl::InitializeMouseSettings(PrefService* pref_service,
                                                   mojom::Mouse* mouse) {
  const auto& devices_dict =
      pref_service->GetDict(prefs::kMouseDeviceSettingsDictPref);
  const auto* settings_dict = devices_dict.FindDict(mouse->device_key);
  if (!settings_dict) {
    mouse->settings = GetNewMouseSettings(pref_service, *mouse);
  } else {
    mouse->settings =
        RetreiveMouseSettings(pref_service, *mouse, *settings_dict);
  }
  DCHECK(mouse->settings);

  UpdateMouseSettings(pref_service, *mouse);
}

void MousePrefHandlerImpl::UpdateMouseSettings(PrefService* pref_service,
                                               const mojom::Mouse& mouse) {
  DCHECK(mouse.settings);
  const mojom::MouseSettings& settings = *mouse.settings;
  // Populate `settings_dict` with all settings in `settings`.
  base::Value::Dict settings_dict;
  settings_dict.Set(prefs::kMouseSettingSwapRight, settings.swap_right);
  settings_dict.Set(prefs::kMouseSettingSensitivity, settings.sensitivity);
  settings_dict.Set(prefs::kMouseSettingReverseScrolling,
                    settings.reverse_scrolling);
  settings_dict.Set(prefs::kMouseSettingAccelerationEnabled,
                    settings.acceleration_enabled);
  settings_dict.Set(prefs::kMouseSettingScrollSensitivity,
                    settings.scroll_sensitivity);
  settings_dict.Set(prefs::kMouseSettingScrollAcceleration,
                    settings.scroll_acceleration);

  // Retrieve old settings and merge with the new ones.
  base::Value::Dict devices_dict =
      pref_service->GetDict(prefs::kMouseDeviceSettingsDictPref).Clone();

  // If an old settings dict already exists for the device, merge the updated
  // settings into the old settings. Otherwise, insert the dict at
  // `mouse.device_key`.
  base::Value::Dict* old_settings_dict =
      devices_dict.FindDict(mouse.device_key);
  if (old_settings_dict) {
    old_settings_dict->Merge(std::move(settings_dict));
  } else {
    devices_dict.Set(mouse.device_key, std::move(settings_dict));
  }

  pref_service->SetDict(std::string(prefs::kMouseDeviceSettingsDictPref),
                        std::move(devices_dict));
}

mojom::MouseSettingsPtr MousePrefHandlerImpl::GetNewMouseSettings(
    PrefService* prefs,
    const mojom::Mouse& mouse) {
  // TODO(michaelcheco): Remove once transitioned to per-device settings.
  if (Shell::Get()->input_device_tracker()->WasDevicePreviouslyConnected(
          InputDeviceTracker::InputDeviceCategory::kMouse, mouse.device_key)) {
    return GetMouseSettingsFromPrefs(prefs);
  }

  return GetDefaultMouseSettings();
}

mojom::MouseSettingsPtr MousePrefHandlerImpl::RetreiveMouseSettings(
    PrefService* pref_service,
    const mojom::Mouse& mouse,
    const base::Value::Dict& settings_dict) {
  mojom::MouseSettingsPtr settings = mojom::MouseSettings::New();
  settings->swap_right = settings_dict.FindBool(prefs::kMouseSettingSwapRight)
                             .value_or(kDefaultSwapRight);
  settings->sensitivity = settings_dict.FindInt(prefs::kMouseSettingSensitivity)
                              .value_or(kDefaultSensitivity);
  settings->reverse_scrolling =
      settings_dict.FindBool(prefs::kMouseSettingReverseScrolling)
          .value_or(kDefaultReverseScrolling);
  settings->acceleration_enabled =
      settings_dict.FindBool(prefs::kMouseSettingAccelerationEnabled)
          .value_or(kDefaultAccelerationEnabled);
  settings->scroll_sensitivity =
      settings_dict.FindInt(prefs::kMouseSettingScrollSensitivity)
          .value_or(kDefaultSensitivity);
  settings->scroll_acceleration =
      settings_dict.FindBool(prefs::kMouseSettingScrollAcceleration)
          .value_or(kDefaultScrollAcceleration);
  return settings;
}

}  // namespace ash
