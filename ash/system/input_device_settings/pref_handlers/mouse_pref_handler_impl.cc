// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/pref_handlers/mouse_pref_handler_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_defaults.h"
#include "ash/system/input_device_settings/input_device_settings_logging.h"
#include "ash/system/input_device_settings/input_device_settings_metadata.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/system/input_device_settings/input_device_settings_utils.h"
#include "ash/system/input_device_settings/input_device_tracker.h"
#include "ash/system/input_device_settings/settings_updated_metrics_info.h"
#include "base/check.h"
#include "base/json/values_util.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"

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

bool GetDefaultSwapRightValue(const mojom::MousePolicies& mouse_policies) {
  if (mouse_policies.swap_right_policy &&
      mouse_policies.swap_right_policy->policy_status ==
          mojom::PolicyStatus::kRecommended) {
    return mouse_policies.swap_right_policy->value;
  }

  return kDefaultSwapRight;
}

// Append all missing button remappings to the current list with default
// remapping action.
void UpdateButtonRemappingsWithCompleteList(
    mojom::MouseButtonConfig mouse_button_config,
    std::vector<mojom::ButtonRemappingPtr>& current_button_remappings) {
  auto default_remappings =
      GetButtonRemappingListForConfig(mouse_button_config);
  for (auto& remapping : default_remappings) {
    const auto iter = base::ranges::find(
        current_button_remappings, *remapping->button,
        [](const mojom::ButtonRemappingPtr& current_remapping) {
          return *current_remapping->button;
        });
    if (iter == current_button_remappings.end()) {
      current_button_remappings.push_back(std::move(remapping));
    }
  }
}

// GetMouseSettingsFromPrefs returns a mouse settings based on user prefs
// to be used as settings for new mouses.
mojom::MouseSettingsPtr GetMouseSettingsFromPrefs(
    PrefService* prefs,
    const mojom::MousePolicies& mouse_policies,
    ForceMouseSettingPersistence& force_persistence) {
  mojom::MouseSettingsPtr settings = mojom::MouseSettings::New();

  const auto* swap_right_preference =
      prefs->GetUserPrefValue(prefs::kPrimaryMouseButtonRight);
  settings->swap_right = swap_right_preference
                             ? swap_right_preference->GetBool()
                             : GetDefaultSwapRightValue(mouse_policies);
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
                                     : kDefaultScrollAccelerationEnabled;
  force_persistence.scroll_acceleration =
      scroll_acceleration_preference != nullptr;

  const auto* scroll_sensitivity_preference =
      prefs->GetUserPrefValue(prefs::kMouseScrollSensitivity);
  settings->scroll_sensitivity = scroll_sensitivity_preference
                                     ? scroll_sensitivity_preference->GetInt()
                                     : kDefaultScrollSensitivity;
  force_persistence.scroll_sensitivity =
      scroll_sensitivity_preference != nullptr;

  return settings;
}

mojom::MouseSettingsPtr RetrieveMouseSettings(
    const mojom::MousePolicies& mouse_policies,
    const base::Value::Dict& settings_dict) {
  mojom::MouseSettingsPtr settings = mojom::MouseSettings::New();
  settings->swap_right =
      settings_dict.FindBool(prefs::kMouseSettingSwapRight)
          .value_or(GetDefaultSwapRightValue(mouse_policies));
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
          .value_or(kDefaultScrollSensitivity);
  settings->scroll_acceleration =
      settings_dict.FindBool(prefs::kMouseSettingScrollAcceleration)
          .value_or(kDefaultScrollAccelerationEnabled);
  return settings;
}

mojom::MouseSettingsPtr GetDefaultMouseSettings(
    PrefService* pref_service,
    const mojom::MousePolicies& mouse_policies) {
  if (pref_service) {
    return RetrieveMouseSettings(
        mouse_policies, pref_service->GetDict(prefs::kMouseDefaultSettings));
  }

  return RetrieveMouseSettings(mouse_policies, /*settings_dict=*/{});
}

base::Value::Dict ConvertSettingsToDict(
    const mojom::Mouse& mouse,
    const mojom::MousePolicies& mouse_policies,
    const ForceMouseSettingPersistence& force_persistence,
    const base::Value::Dict* existing_settings_dict) {
  // Populate `settings_dict` with all settings in `settings`.
  base::Value::Dict settings_dict;

  if (ShouldPersistSetting(
          mouse_policies.swap_right_policy, prefs::kMouseSettingSwapRight,
          mouse.settings->swap_right, GetDefaultSwapRightValue(mouse_policies),
          force_persistence.swap_right, existing_settings_dict)) {
    settings_dict.Set(prefs::kMouseSettingSwapRight,
                      mouse.settings->swap_right);
  }

  if (ShouldPersistSetting(prefs::kMouseSettingSensitivity,
                           static_cast<int>(mouse.settings->sensitivity),
                           kDefaultSensitivity, force_persistence.sensitivity,
                           existing_settings_dict)) {
    settings_dict.Set(prefs::kMouseSettingSensitivity,
                      mouse.settings->sensitivity);
  }

  if (ShouldPersistSetting(
          prefs::kMouseSettingReverseScrolling,
          mouse.settings->reverse_scrolling, kDefaultReverseScrolling,
          force_persistence.reverse_scrolling, existing_settings_dict)) {
    settings_dict.Set(prefs::kMouseSettingReverseScrolling,
                      mouse.settings->reverse_scrolling);
  }

  if (ShouldPersistSetting(
          prefs::kMouseSettingAccelerationEnabled,
          mouse.settings->acceleration_enabled, kDefaultAccelerationEnabled,
          force_persistence.acceleration_enabled, existing_settings_dict)) {
    settings_dict.Set(prefs::kMouseSettingAccelerationEnabled,
                      mouse.settings->acceleration_enabled);
  }

  if (ShouldPersistSetting(prefs::kMouseSettingScrollSensitivity,
                           static_cast<int>(mouse.settings->scroll_sensitivity),
                           kDefaultScrollSensitivity,
                           force_persistence.scroll_sensitivity,
                           existing_settings_dict)) {
    settings_dict.Set(prefs::kMouseSettingScrollSensitivity,
                      mouse.settings->scroll_sensitivity);
  }

  if (ShouldPersistSetting(prefs::kMouseSettingScrollAcceleration,
                           mouse.settings->scroll_acceleration,
                           kDefaultScrollAccelerationEnabled,
                           force_persistence.scroll_acceleration,
                           existing_settings_dict)) {
    settings_dict.Set(prefs::kMouseSettingScrollAcceleration,
                      mouse.settings->scroll_acceleration);
  }

  return settings_dict;
}

void UpdateButtonRemappingDictPref(PrefService* pref_service,
                                   const mojom::Mouse& mouse,
                                   const base::Time time_stamp) {
  const mojom::MouseSettings& settings = *mouse.settings;
  base::Value::List button_remappings = ConvertButtonRemappingArrayToList(
      settings.button_remappings, mouse.customization_restriction);
  base::Value::Dict button_remappings_dict =
      pref_service->GetDict(prefs::kMouseButtonRemappingsDictPref).Clone();
  button_remappings_dict.Set(mouse.device_key, std::move(button_remappings));
  const auto time_stamp_path =
      base::StrCat({prefs::kLastUpdatedKey, ".", mouse.device_key});
  button_remappings_dict.SetByDottedPath(time_stamp_path,
                                         base::TimeToValue(time_stamp));
  pref_service->SetDict(std::string(prefs::kMouseButtonRemappingsDictPref),
                        std::move(button_remappings_dict));
}

void UpdateMouseSettingsImpl(
    PrefService* pref_service,
    const mojom::MousePolicies& mouse_policies,
    const mojom::Mouse& mouse,
    const ForceMouseSettingPersistence& force_persistence) {
  DCHECK(mouse.settings);
  const base::Time time_stamp = base::Time::Now();
  base::Value::Dict devices_dict =
      pref_service->GetDict(prefs::kMouseDeviceSettingsDictPref).Clone();
  base::Value::Dict* existing_settings_dict =
      devices_dict.FindDict(mouse.device_key);

  base::Value::Dict settings_dict = ConvertSettingsToDict(
      mouse, mouse_policies, force_persistence, existing_settings_dict);

  settings_dict.Set(prefs::kLastUpdatedKey, base::TimeToValue(time_stamp));
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

  if (features::IsPeripheralCustomizationEnabled()) {
    UpdateButtonRemappingDictPref(pref_service, mouse, time_stamp);
  }
}

mojom::MouseSettingsPtr GetMouseSettingsFromOldLocalStatePrefs(
    PrefService* local_state,
    const AccountId& account_id,
    const mojom::MousePolicies& mouse_policies,
    const mojom::Mouse& mouse) {
  mojom::MouseSettingsPtr settings =
      GetDefaultMouseSettings(/*pref_service=*/nullptr, mouse_policies);
  settings->swap_right =
      user_manager::KnownUser(local_state)
          .FindBoolPath(account_id, prefs::kOwnerPrimaryMouseButtonRight)
          .value_or(kDefaultSwapRight);

  return settings;
}

bool HasDefaultSettings(PrefService* pref_service) {
  const auto* pref = pref_service->FindPreference(prefs::kMouseDefaultSettings);
  return pref && pref->HasUserSetting();
}

void InitializeSettingsUpdateMetricInfo(
    PrefService* pref_service,
    const mojom::Mouse& mouse,
    SettingsUpdatedMetricsInfo::Category category) {
  CHECK(pref_service);

  const auto& settings_metric_info =
      pref_service->GetDict(prefs::kMouseUpdateSettingsMetricInfo);
  const auto* device_metric_info = settings_metric_info.Find(mouse.device_key);
  if (device_metric_info) {
    return;
  }

  auto updated_metric_info = settings_metric_info.Clone();

  const SettingsUpdatedMetricsInfo metrics_info(category, base::Time::Now());
  updated_metric_info.Set(mouse.device_key, metrics_info.ToDict());

  pref_service->SetDict(prefs::kMouseUpdateSettingsMetricInfo,
                        std::move(updated_metric_info));
}

void InitializeMouseSettingsImpl(PrefService* pref_service,
                                 const mojom::MousePolicies& mouse_policies,
                                 mojom::Mouse* mouse,
                                 bool force_initialize_to_default_settings) {
  if (!pref_service) {
    mouse->settings = GetDefaultMouseSettings(pref_service, mouse_policies);
    return;
  }

  const auto& devices_dict =
      pref_service->GetDict(prefs::kMouseDeviceSettingsDictPref);
  const auto* settings_dict = devices_dict.FindDict(mouse->device_key);
  if (force_initialize_to_default_settings) {
    settings_dict = nullptr;
  }

  ForceMouseSettingPersistence force_persistence;
  SettingsUpdatedMetricsInfo::Category category;
  if (settings_dict) {
    category = SettingsUpdatedMetricsInfo::Category::kSynced;
    mouse->settings = RetrieveMouseSettings(mouse_policies, *settings_dict);
  } else if (Shell::Get()->input_device_tracker()->WasDevicePreviouslyConnected(
                 InputDeviceTracker::InputDeviceCategory::kMouse,
                 mouse->device_key)) {
    category = SettingsUpdatedMetricsInfo::Category::kDefault;
    mouse->settings = GetMouseSettingsFromPrefs(pref_service, mouse_policies,
                                                force_persistence);
  } else {
    mouse->settings = GetDefaultMouseSettings(pref_service, mouse_policies);
    category = HasDefaultSettings(pref_service)
                   ? SettingsUpdatedMetricsInfo::Category::kDefault
                   : SettingsUpdatedMetricsInfo::Category::kFirstEver;
  }
  if (category == SettingsUpdatedMetricsInfo::Category::kSynced) {
    PR_LOG(INFO, Feature::IDS)
        << GetMouseSettingsLog("Synced with latest settings", *mouse);
  } else if (category == SettingsUpdatedMetricsInfo::Category::kDefault) {
    PR_LOG(INFO, Feature::IDS)
        << GetMouseSettingsLog("Using default settings", *mouse);
  } else {
    PR_LOG(INFO, Feature::IDS)
        << GetMouseSettingsLog("First time connected", *mouse);
  }
  if (features::IsPeripheralCustomizationEnabled()) {
    const auto& button_remappings_dict =
        pref_service->GetDict(prefs::kMouseButtonRemappingsDictPref);
    const auto* button_remappings_list =
        button_remappings_dict.FindList(mouse->device_key);
    if (button_remappings_list) {
      auto button_remappings = ConvertListToButtonRemappingArray(
          *button_remappings_list, mouse->customization_restriction);
      UpdateButtonRemappingsWithCompleteList(mouse->mouse_button_config,
                                             button_remappings);
      mouse->settings->button_remappings = std::move(button_remappings);
    } else {
      mouse->settings->button_remappings =
          GetButtonRemappingListForConfig(mouse->mouse_button_config);
    }
  }
  DCHECK(mouse->settings);
  InitializeSettingsUpdateMetricInfo(pref_service, *mouse, category);

  UpdateMouseSettingsImpl(pref_service, mouse_policies, *mouse,
                          force_persistence);

  if (mouse_policies.swap_right_policy &&
      mouse_policies.swap_right_policy->policy_status ==
          mojom::PolicyStatus::kManaged) {
    mouse->settings->swap_right = mouse_policies.swap_right_policy->value;
  }
}

}  // namespace

MousePrefHandlerImpl::MousePrefHandlerImpl() = default;
MousePrefHandlerImpl::~MousePrefHandlerImpl() = default;

void MousePrefHandlerImpl::InitializeMouseSettings(
    PrefService* pref_service,
    const mojom::MousePolicies& mouse_policies,
    mojom::Mouse* mouse) {
  InitializeMouseSettingsImpl(pref_service, mouse_policies, mouse,
                              /*force_initialize_to_default_settings=*/false);
}

void MousePrefHandlerImpl::UpdateMouseSettings(
    PrefService* pref_service,
    const mojom::MousePolicies& mouse_policies,
    const mojom::Mouse& mouse) {
  UpdateMouseSettingsImpl(pref_service, mouse_policies, mouse,
                          /*force_persistence=*/{});
}

void MousePrefHandlerImpl::InitializeLoginScreenMouseSettings(
    PrefService* local_state,
    const AccountId& account_id,
    const mojom::MousePolicies& mouse_policies,
    mojom::Mouse* mouse) {
  // Verify if the flag is enabled.
  if (!features::IsInputDeviceSettingsSplitEnabled()) {
    return;
  }
  CHECK(local_state);

  const auto* settings_dict = GetLoginScreenSettingsDict(
      local_state, account_id,
      mouse->is_external ? prefs::kMouseLoginScreenExternalSettingsPref
                         : prefs::kMouseLoginScreenInternalSettingsPref);
  if (settings_dict) {
    mouse->settings = RetrieveMouseSettings(mouse_policies, *settings_dict);
  } else {
    mouse->settings = GetMouseSettingsFromOldLocalStatePrefs(
        local_state, account_id, mouse_policies, *mouse);
  }

  if (mouse_policies.swap_right_policy &&
      mouse_policies.swap_right_policy->policy_status ==
          mojom::PolicyStatus::kManaged) {
    mouse->settings->swap_right = mouse_policies.swap_right_policy->value;
  }

  if (features::IsPeripheralCustomizationEnabled()) {
    const auto* button_remappings_list = GetLoginScreenButtonRemappingList(
        local_state, account_id,
        prefs::kMouseLoginScreenButtonRemappingListPref);
    if (button_remappings_list) {
      mouse->settings->button_remappings = ConvertListToButtonRemappingArray(
          *button_remappings_list, mouse->customization_restriction);
    } else {
      mouse->settings->button_remappings =
          GetButtonRemappingListForConfig(mouse->mouse_button_config);
    }
  }
}

void MousePrefHandlerImpl::UpdateLoginScreenMouseSettings(
    PrefService* local_state,
    const AccountId& account_id,
    const mojom::MousePolicies& mouse_policies,
    const mojom::Mouse& mouse) {
  CHECK(local_state);
  const auto* pref_name = mouse.is_external
                              ? prefs::kMouseLoginScreenExternalSettingsPref
                              : prefs::kMouseLoginScreenInternalSettingsPref;
  auto* settings_dict =
      GetLoginScreenSettingsDict(local_state, account_id, pref_name);

  user_manager::KnownUser(local_state)
      .SetPath(
          account_id, pref_name,
          std::make_optional<base::Value>(ConvertSettingsToDict(
              mouse, mouse_policies, /*force_persistence=*/{}, settings_dict)));

  if (features::IsPeripheralCustomizationEnabled()) {
    const auto* button_remapping_list_pref =
        prefs::kMouseLoginScreenButtonRemappingListPref;
    user_manager::KnownUser(local_state)
        .SetPath(
            account_id, button_remapping_list_pref,
            std::make_optional<base::Value>(ConvertButtonRemappingArrayToList(
                mouse.settings->button_remappings,
                mouse.customization_restriction,
                /*redact_button_names=*/true)));
  }
}

void MousePrefHandlerImpl::InitializeWithDefaultMouseSettings(
    const mojom::MousePolicies& mouse_policies,
    mojom::Mouse* mouse) {
  mouse->settings =
      GetDefaultMouseSettings(/*pref_service=*/nullptr, mouse_policies);
}

void MousePrefHandlerImpl::UpdateDefaultMouseSettings(
    PrefService* pref_service,
    const mojom::MousePolicies& mouse_policies,
    const mojom::Mouse& mouse) {
  // All settings should be persisted fully when storing defaults.
  auto settings_dict =
      ConvertSettingsToDict(mouse, mouse_policies, /*force_persistence=*/{true},
                            /*existing_settings_dict=*/nullptr);
  pref_service->SetDict(prefs::kMouseDefaultSettings, std::move(settings_dict));
}

void MousePrefHandlerImpl::ForceInitializeWithDefaultSettings(
    PrefService* pref_service,
    const mojom::MousePolicies& mouse_policies,
    mojom::Mouse* mouse) {
  InitializeMouseSettingsImpl(pref_service, mouse_policies, mouse,
                              /*force_initialize_to_default_settings=*/true);
}

}  // namespace ash
