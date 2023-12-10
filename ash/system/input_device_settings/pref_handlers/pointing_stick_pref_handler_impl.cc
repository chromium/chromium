// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/pref_handlers/pointing_stick_pref_handler_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_defaults.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/system/input_device_settings/input_device_settings_utils.h"
#include "ash/system/input_device_settings/input_device_tracker.h"
#include "ash/system/input_device_settings/settings_updated_metrics_info.h"
#include "base/check.h"
#include "base/json/values_util.h"
#include "base/values.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"

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

base::Value::Dict ConvertSettingsToDict(
    const mojom::PointingStick& pointing_stick,
    const ForcePointingStickSettingPersistence& force_persistence,
    const base::Value::Dict* existing_settings_dict) {
  // Populate `settings_dict` with all settings in `settings`.
  base::Value::Dict settings_dict;

  if (ShouldPersistSetting(prefs::kPointingStickSettingSwapRight,
                           pointing_stick.settings->swap_right,
                           kDefaultSwapRight, force_persistence.swap_right,
                           existing_settings_dict)) {
    settings_dict.Set(prefs::kPointingStickSettingSwapRight,
                      pointing_stick.settings->swap_right);
  }

  if (ShouldPersistSetting(
          prefs::kPointingStickSettingSensitivity,
          static_cast<int>(pointing_stick.settings->sensitivity),
          kDefaultSensitivity, force_persistence.sensitivity,
          existing_settings_dict)) {
    settings_dict.Set(prefs::kPointingStickSettingSensitivity,
                      pointing_stick.settings->sensitivity);
  }

  if (ShouldPersistSetting(prefs::kPointingStickSettingAcceleration,
                           pointing_stick.settings->acceleration_enabled,
                           kDefaultAccelerationEnabled,
                           force_persistence.acceleration_enabled,
                           existing_settings_dict)) {
    settings_dict.Set(prefs::kPointingStickSettingAcceleration,
                      pointing_stick.settings->acceleration_enabled);
  }

  return settings_dict;
}

void UpdateInternalPointingStickImpl(
    PrefService* pref_service,
    const mojom::PointingStick& pointing_stick,
    const ForcePointingStickSettingPersistence& force_persistence) {
  CHECK(pointing_stick.settings);
  CHECK(!pointing_stick.is_external);

  base::Value::Dict existing_settings_dict =
      pref_service->GetDict(prefs::kPointingStickInternalSettings).Clone();
  base::Value::Dict settings_dict = ConvertSettingsToDict(
      pointing_stick, force_persistence, &existing_settings_dict);

  // Merge the new settings into the old settings so that all settings are
  // transferred over.
  existing_settings_dict.Merge(std::move(settings_dict));
  pref_service->SetDict(prefs::kPointingStickInternalSettings,
                        std::move(existing_settings_dict));
}

void UpdatePointingStickSettingsImpl(
    PrefService* pref_service,
    const mojom::PointingStick& pointing_stick,
    const ForcePointingStickSettingPersistence& force_persistence) {
  DCHECK(pointing_stick.settings);

  if (!pointing_stick.is_external) {
    UpdateInternalPointingStickImpl(pref_service, pointing_stick,
                                    force_persistence);
    return;
  }

  base::Value::Dict devices_dict =
      pref_service->GetDict(prefs::kPointingStickDeviceSettingsDictPref)
          .Clone();
  base::Value::Dict* existing_settings_dict =
      devices_dict.FindDict(pointing_stick.device_key);

  base::Value::Dict settings_dict = ConvertSettingsToDict(
      pointing_stick, force_persistence, existing_settings_dict);
  const base::Time time_stamp = base::Time::Now();
  settings_dict.Set(prefs::kLastUpdatedKey, base::TimeToValue(time_stamp));

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

mojom::PointingStickSettingsPtr GetPointingStickSettingsFromOldLocalStatePrefs(
    PrefService* local_state,
    const AccountId& account_id,
    const mojom::PointingStick& pointing_stick) {
  mojom::PointingStickSettingsPtr settings = GetDefaultPointingStickSettings();
  settings->swap_right =
      user_manager::KnownUser(local_state)
          .FindBoolPath(account_id,
                        prefs::kOwnerPrimaryPointingStickButtonRight)
          .value_or(kDefaultSwapRight);

  return settings;
}

void InitializeSettingsUpdateMetricInfo(
    PrefService* pref_service,
    const mojom::PointingStick& pointing_stick,
    SettingsUpdatedMetricsInfo::Category category) {
  CHECK(pref_service);

  const auto& settings_metric_info =
      pref_service->GetDict(prefs::kPointingStickUpdateSettingsMetricInfo);
  const auto* device_metric_info =
      settings_metric_info.Find(pointing_stick.device_key);
  if (device_metric_info) {
    return;
  }

  auto updated_metric_info = settings_metric_info.Clone();

  const SettingsUpdatedMetricsInfo metrics_info(category, base::Time::Now());
  updated_metric_info.Set(pointing_stick.device_key, metrics_info.ToDict());

  pref_service->SetDict(prefs::kPointingStickUpdateSettingsMetricInfo,
                        std::move(updated_metric_info));
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

  const base::Value::Dict* settings_dict = nullptr;
  if (!pointing_stick->is_external) {
    settings_dict =
        &pref_service->GetDict(prefs::kPointingStickInternalSettings);
    if (settings_dict->empty()) {
      settings_dict = nullptr;
    }
  } else {
    const auto& devices_dict =
        pref_service->GetDict(prefs::kPointingStickDeviceSettingsDictPref);
    settings_dict = devices_dict.FindDict(pointing_stick->device_key);
  }

  ForcePointingStickSettingPersistence force_persistence;
  SettingsUpdatedMetricsInfo::Category category;
  if (settings_dict) {
    category = SettingsUpdatedMetricsInfo::Category::kSynced;
    pointing_stick->settings =
        RetrievePointingStickSettings(*pointing_stick, *settings_dict);
  } else if (Shell::Get()->input_device_tracker()->WasDevicePreviouslyConnected(
                 InputDeviceTracker::InputDeviceCategory::kPointingStick,
                 pointing_stick->device_key)) {
    category = SettingsUpdatedMetricsInfo::Category::kDefault;
    pointing_stick->settings =
        GetPointingStickSettingsFromPrefs(pref_service, force_persistence);
  } else {
    // Pointing Stick currently does not do syncing defaults like other devices
    // as only internal pointing sticks are supported in the first place.
    category = SettingsUpdatedMetricsInfo::Category::kFirstEver;
    pointing_stick->settings = GetDefaultPointingStickSettings();
  }
  DCHECK(pointing_stick->settings);
  InitializeSettingsUpdateMetricInfo(pref_service, *pointing_stick, category);

  UpdatePointingStickSettingsImpl(pref_service, *pointing_stick,
                                  force_persistence);
}

void PointingStickPrefHandlerImpl::UpdatePointingStickSettings(
    PrefService* pref_service,
    const mojom::PointingStick& pointing_stick) {
  UpdatePointingStickSettingsImpl(pref_service, pointing_stick,
                                  /*force_persistence=*/{});
}

void PointingStickPrefHandlerImpl::InitializeLoginScreenPointingStickSettings(
    PrefService* local_state,
    const AccountId& account_id,
    mojom::PointingStick* pointing_stick) {
  // Verify if the flag is enabled.
  if (!features::IsInputDeviceSettingsSplitEnabled()) {
    return;
  }
  CHECK(local_state);

  const auto* settings_dict = GetLoginScreenSettingsDict(
      local_state, account_id,
      pointing_stick->is_external
          ? prefs::kPointingStickLoginScreenExternalSettingsPref
          : prefs::kPointingStickLoginScreenInternalSettingsPref);
  if (settings_dict) {
    pointing_stick->settings =
        RetrievePointingStickSettings(*pointing_stick, *settings_dict);
  } else {
    pointing_stick->settings = GetPointingStickSettingsFromOldLocalStatePrefs(
        local_state, account_id, *pointing_stick);
  }
}

void PointingStickPrefHandlerImpl::UpdateLoginScreenPointingStickSettings(
    PrefService* local_state,
    const AccountId& account_id,
    const mojom::PointingStick& pointing_stick) {
  CHECK(local_state);
  const auto* pref_name =
      pointing_stick.is_external
          ? prefs::kPointingStickLoginScreenExternalSettingsPref
          : prefs::kPointingStickLoginScreenInternalSettingsPref;
  auto* settings_dict =
      GetLoginScreenSettingsDict(local_state, account_id, pref_name);

  user_manager::KnownUser(local_state)
      .SetPath(account_id, pref_name,
               std::make_optional<base::Value>(ConvertSettingsToDict(
                   pointing_stick, /*force_persistence=*/{}, settings_dict)));
}

void PointingStickPrefHandlerImpl::InitializeWithDefaultPointingStickSettings(
    mojom::PointingStick* pointing_stick) {
  pointing_stick->settings = GetDefaultPointingStickSettings();
}

}  // namespace ash
