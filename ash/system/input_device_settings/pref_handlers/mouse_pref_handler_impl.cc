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

// Whether or not settings taken during the transition period should be
// persisted to the prefs. Values should only ever be true if the original
// setting was a user-configured value.
struct ForceMouseSettingPersistence {
  bool swap_right = false;
  bool sensitivity = false;
  bool reverse_scrolling = false;
  bool acceleration_enabled = false;
  bool scroll_acceleration = false;
  bool scroll_sensitivity = false;
};

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
mojom::MouseSettingsPtr GetMouseSettingsFromPrefs(
    PrefService* prefs,
    ForceMouseSettingPersistence& force_persistence) {
  mojom::MouseSettingsPtr settings = mojom::MouseSettings::New();

  const auto* swap_right_preference =
      prefs->GetUserPrefValue(prefs::kPrimaryMouseButtonRight);
  settings->swap_right = swap_right_preference
                             ? swap_right_preference->GetBool()
                             : kDefaultSwapRight;
  force_persistence.swap_right = swap_right_preference != nullptr;

  const auto* sensitivity_preference =
      prefs->GetUserPrefValue(prefs::kMouseSensitivity);
  settings->sensitivity = sensitivity_preference
                              ? sensitivity_preference->GetInt()
                              : kDefaultSensitivity;
  force_persistence.sensitivity = sensitivity_preference != nullptr;

  const auto* reverse_scrolling_preference =
      prefs->GetUserPrefValue(prefs::kMouseReverseScroll);
  settings->reverse_scrolling = reverse_scrolling_preference
                                    ? reverse_scrolling_preference->GetBool()
                                    : kDefaultReverseScrolling;
  force_persistence.reverse_scrolling = reverse_scrolling_preference != nullptr;

  const auto* acceleration_enabled_preference =
      prefs->GetUserPrefValue(prefs::kMouseAcceleration);
  settings->acceleration_enabled =
      acceleration_enabled_preference
          ? acceleration_enabled_preference->GetBool()
          : kDefaultAccelerationEnabled;
  force_persistence.acceleration_enabled =
      acceleration_enabled_preference != nullptr;

  const auto* scroll_acceleration_preference =
      prefs->GetUserPrefValue(prefs::kMouseScrollAcceleration);
  settings->scroll_acceleration =
      scroll_acceleration_preference ? scroll_acceleration_preference->GetBool()
                                     : kDefaultSensitivity;
  force_persistence.scroll_acceleration =
      scroll_acceleration_preference != nullptr;

  const auto* scroll_sensitivity_preference =
      prefs->GetUserPrefValue(prefs::kMouseScrollSensitivity);
  settings->scroll_sensitivity = scroll_sensitivity_preference
                                     ? scroll_sensitivity_preference->GetInt()
                                     : kDefaultScrollAcceleration;
  force_persistence.scroll_sensitivity =
      scroll_sensitivity_preference != nullptr;

  return settings;
}

mojom::MouseSettingsPtr RetrieveMouseSettings(
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

bool ExistingSettingsHasValue(base::StringPiece setting_key,
                              const base::Value::Dict* existing_settings_dict) {
  if (!existing_settings_dict) {
    return false;
  }

  return existing_settings_dict->Find(setting_key) != nullptr;
}

void UpdateMouseSettingsImpl(
    PrefService* pref_service,
    const mojom::Mouse& mouse,
    const ForceMouseSettingPersistence& force_persistence) {
  DCHECK(mouse.settings);
  base::Value::Dict devices_dict =
      pref_service->GetDict(prefs::kMouseDeviceSettingsDictPref).Clone();
  base::Value::Dict* existing_settings_dict =
      devices_dict.FindDict(mouse.device_key);
  const mojom::MouseSettings& settings = *mouse.settings;

  // Populate `settings_dict` with all settings in `settings`.
  base::Value::Dict settings_dict;

  // Settings should only be persisted if one or more of the following is true:
  // - Setting was previously persisted to storage
  // - `force_persistence` requires the setting to be persisted, this means this
  //   device is being transitioned from the old global settings to per-device
  //   settings and the user specified the specific value for this setting.
  // - Setting is different than the default, which means the user manually
  //   changed the value.

  if (ExistingSettingsHasValue(prefs::kMouseSettingSwapRight,
                               existing_settings_dict) ||
      force_persistence.swap_right ||
      settings.swap_right != kDefaultSwapRight) {
    settings_dict.Set(prefs::kMouseSettingSwapRight, settings.swap_right);
  }

  if (ExistingSettingsHasValue(prefs::kMouseSettingSensitivity,
                               existing_settings_dict) ||
      force_persistence.sensitivity ||
      settings.sensitivity != kDefaultSensitivity) {
    settings_dict.Set(prefs::kMouseSettingSensitivity, settings.sensitivity);
  }

  if (ExistingSettingsHasValue(prefs::kMouseSettingReverseScrolling,
                               existing_settings_dict) ||
      force_persistence.reverse_scrolling ||
      settings.reverse_scrolling != kDefaultReverseScrolling) {
    settings_dict.Set(prefs::kMouseSettingReverseScrolling,
                      settings.reverse_scrolling);
  }

  if (ExistingSettingsHasValue(prefs::kMouseSettingAccelerationEnabled,
                               existing_settings_dict) ||
      force_persistence.acceleration_enabled ||
      settings.acceleration_enabled != kDefaultAccelerationEnabled) {
    settings_dict.Set(prefs::kMouseSettingAccelerationEnabled,
                      settings.acceleration_enabled);
  }

  if (ExistingSettingsHasValue(prefs::kMouseSettingScrollSensitivity,
                               existing_settings_dict) ||
      force_persistence.scroll_sensitivity ||
      settings.scroll_sensitivity != kDefaultSensitivity) {
    settings_dict.Set(prefs::kMouseSettingScrollSensitivity,
                      settings.scroll_sensitivity);
  }

  if (ExistingSettingsHasValue(prefs::kMouseSettingScrollAcceleration,
                               existing_settings_dict) ||
      force_persistence.scroll_acceleration ||
      settings.scroll_acceleration != kDefaultScrollAcceleration) {
    settings_dict.Set(prefs::kMouseSettingScrollAcceleration,
                      settings.scroll_acceleration);
  }

  // If an old settings dict already exists for the device, merge the updated
  // settings into the old settings. Otherwise, insert the dict at
  // `mouse.device_key`.
  if (existing_settings_dict) {
    existing_settings_dict->Merge(std::move(settings_dict));
  } else {
    devices_dict.Set(mouse.device_key, std::move(settings_dict));
  }

  pref_service->SetDict(std::string(prefs::kMouseDeviceSettingsDictPref),
                        std::move(devices_dict));
}

}  // namespace

MousePrefHandlerImpl::MousePrefHandlerImpl() = default;
MousePrefHandlerImpl::~MousePrefHandlerImpl() = default;

void MousePrefHandlerImpl::InitializeMouseSettings(PrefService* pref_service,
                                                   mojom::Mouse* mouse) {
  if (!pref_service) {
    mouse->settings = GetDefaultMouseSettings();
    return;
  }

  const auto& devices_dict =
      pref_service->GetDict(prefs::kMouseDeviceSettingsDictPref);
  const auto* settings_dict = devices_dict.FindDict(mouse->device_key);
  ForceMouseSettingPersistence force_persistence;

  if (settings_dict) {
    mouse->settings =
        RetrieveMouseSettings(pref_service, *mouse, *settings_dict);
  } else if (Shell::Get()->input_device_tracker()->WasDevicePreviouslyConnected(
                 InputDeviceTracker::InputDeviceCategory::kMouse,
                 mouse->device_key)) {
    mouse->settings =
        GetMouseSettingsFromPrefs(pref_service, force_persistence);
  } else {
    mouse->settings = GetDefaultMouseSettings();
  }
  DCHECK(mouse->settings);

  UpdateMouseSettingsImpl(pref_service, *mouse, force_persistence);
}

void MousePrefHandlerImpl::UpdateMouseSettings(PrefService* pref_service,
                                               const mojom::Mouse& mouse) {
  UpdateMouseSettingsImpl(pref_service, mouse, /*force_persistence=*/{});
}

}  // namespace ash
