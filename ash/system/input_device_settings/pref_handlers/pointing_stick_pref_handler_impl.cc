// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/pref_handlers/pointing_stick_pref_handler_impl.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_defaults.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/system/input_device_settings/input_device_settings_utils.h"
#include "ash/system/input_device_settings/input_device_tracker.h"
#include "base/check.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace {

// Whether or not settings taken during the transition period should be
// persisted to the prefs. Values should only ever be true if the original
// setting was a user-configured value.
struct ForcePointingStickSettingPersistence {
  bool swap_right = false;
  bool sensitivity = false;
  bool acceleration_enabled = false;
};

mojom::PointingStickSettingsPtr GetDefaultPointingStickSettings() {
  mojom::PointingStickSettingsPtr settings =
      mojom::PointingStickSettings::New();
  settings->sensitivity = kDefaultSensitivity;
  settings->swap_right = kDefaultSwapRight;
  settings->acceleration_enabled = kDefaultAccelerationEnabled;
  return settings;
}

// GetPointingStickSettingsFromPrefs returns pointing stick settings based on
// user prefs to be used as settings for new pointing sticks.
mojom::PointingStickSettingsPtr GetPointingStickSettingsFromPrefs(
    PrefService* prefs,
    ForcePointingStickSettingPersistence& force_persistence) {
  mojom::PointingStickSettingsPtr settings =
      mojom::PointingStickSettings::New();

  const auto* swap_right_preference =
      prefs->GetUserPrefValue(prefs::kPrimaryPointingStickButtonRight);
  settings->swap_right = swap_right_preference
                             ? swap_right_preference->GetBool()
                             : kDefaultSwapRight;
  force_persistence.swap_right = swap_right_preference != nullptr;

  const auto* sensitivity_preference =
      prefs->GetUserPrefValue(prefs::kPointingStickSensitivity);
  settings->sensitivity = sensitivity_preference
                              ? sensitivity_preference->GetInt()
                              : kDefaultSensitivity;
  force_persistence.sensitivity = sensitivity_preference != nullptr;

  const auto* acceleration_enabled_preference =
      prefs->GetUserPrefValue(prefs::kPointingStickAcceleration);
  settings->acceleration_enabled =
      acceleration_enabled_preference
          ? acceleration_enabled_preference->GetBool()
          : kDefaultAccelerationEnabled;
  force_persistence.acceleration_enabled =
      acceleration_enabled_preference != nullptr;

  return settings;
}

mojom::PointingStickSettingsPtr RetrievePointingStickSettings(
    const mojom::PointingStick& pointing_stick,
    const base::Value::Dict& settings_dict) {
  mojom::PointingStickSettingsPtr settings =
      mojom::PointingStickSettings::New();
  settings->sensitivity =
      settings_dict.FindInt(prefs::kPointingStickSettingSensitivity)
          .value_or(kDefaultSensitivity);
  settings->swap_right =
      settings_dict.FindBool(prefs::kPointingStickSettingSwapRight)
          .value_or(kDefaultSwapRight);
  settings->acceleration_enabled =
      settings_dict.FindBool(prefs::kPointingStickSettingAcceleration)
          .value_or(kDefaultAccelerationEnabled);
  return settings;
}

void UpdatePointingStickSettingsImpl(
    PrefService* pref_service,
    const mojom::PointingStick& pointing_stick,
    const ForcePointingStickSettingPersistence& force_persistence) {
  DCHECK(pointing_stick.settings);
  base::Value::Dict devices_dict =
      pref_service->GetDict(prefs::kPointingStickDeviceSettingsDictPref)
          .Clone();
  base::Value::Dict* existing_settings_dict =
      devices_dict.FindDict(pointing_stick.device_key);
  const mojom::PointingStickSettings& settings = *pointing_stick.settings;

  // Populate `settings_dict` with all settings in `settings`.
  base::Value::Dict settings_dict;

  if (ShouldPersistSetting(prefs::kPointingStickSettingSwapRight,
                           settings.swap_right, kDefaultSwapRight,
                           force_persistence.swap_right,
                           existing_settings_dict)) {
    settings_dict.Set(prefs::kPointingStickSettingSwapRight,
                      settings.swap_right);
  }

  if (ShouldPersistSetting(prefs::kPointingStickSettingSensitivity,
                           static_cast<int>(settings.sensitivity),
                           kDefaultSensitivity, force_persistence.sensitivity,
                           existing_settings_dict)) {
    settings_dict.Set(prefs::kPointingStickSettingSensitivity,
                      settings.sensitivity);
  }

  if (ShouldPersistSetting(
          prefs::kPointingStickSettingAcceleration,
          settings.acceleration_enabled, kDefaultAccelerationEnabled,
          force_persistence.acceleration_enabled, existing_settings_dict)) {
    settings_dict.Set(prefs::kPointingStickSettingAcceleration,
                      settings.acceleration_enabled);
  }

  // If an old settings dict already exists for the device, merge the updated
  // settings into the old settings. Otherwise, insert the dict at
  // `pointing_stick.device_key`.
  if (existing_settings_dict) {
    existing_settings_dict->Merge(std::move(settings_dict));
  } else {
    devices_dict.Set(pointing_stick.device_key, std::move(settings_dict));
  }

  pref_service->SetDict(
      std::string(prefs::kPointingStickDeviceSettingsDictPref),
      std::move(devices_dict));
}

}  // namespace

PointingStickPrefHandlerImpl::PointingStickPrefHandlerImpl() = default;
PointingStickPrefHandlerImpl::~PointingStickPrefHandlerImpl() = default;

void PointingStickPrefHandlerImpl::InitializePointingStickSettings(
    PrefService* pref_service,
    mojom::PointingStick* pointing_stick) {
  if (!pref_service) {
    pointing_stick->settings = GetDefaultPointingStickSettings();
    return;
  }

  const auto& devices_dict =
      pref_service->GetDict(prefs::kPointingStickDeviceSettingsDictPref);
  const auto* settings_dict = devices_dict.FindDict(pointing_stick->device_key);
  ForcePointingStickSettingPersistence force_persistence;

  if (settings_dict) {
    pointing_stick->settings =
        RetrievePointingStickSettings(*pointing_stick, *settings_dict);
  } else if (Shell::Get()->input_device_tracker()->WasDevicePreviouslyConnected(
                 InputDeviceTracker::InputDeviceCategory::kPointingStick,
                 pointing_stick->device_key)) {
    pointing_stick->settings =
        GetPointingStickSettingsFromPrefs(pref_service, force_persistence);
  } else {
    pointing_stick->settings = GetDefaultPointingStickSettings();
  }
  DCHECK(pointing_stick->settings);

  UpdatePointingStickSettingsImpl(pref_service, *pointing_stick,
                                  force_persistence);
}

void PointingStickPrefHandlerImpl::UpdatePointingStickSettings(
    PrefService* pref_service,
    const mojom::PointingStick& pointing_stick) {
  UpdatePointingStickSettingsImpl(pref_service, pointing_stick,
                                  /*force_persistence=*/{});
}

}  // namespace ash
