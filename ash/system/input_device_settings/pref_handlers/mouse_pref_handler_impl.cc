// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/pref_handlers/mouse_pref_handler_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "ash/public/mojom/input_device_settings.mojom-shared.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_defaults.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/system/input_device_settings/input_device_settings_utils.h"
#include "ash/system/input_device_settings/input_device_tracker.h"
#include "base/check.h"
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

mojom::MouseSettingsPtr GetDefaultMouseSettings(
    const mojom::MousePolicies& mouse_policies) {
  mojom::MouseSettingsPtr settings = mojom::MouseSettings::New();
  settings->swap_right = GetDefaultSwapRightValue(mouse_policies);
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
    const mojom::MousePolicies& mouse_policies,
    const mojom::Mouse& mouse,
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
          .value_or(kDefaultSensitivity);
  settings->scroll_acceleration =
      settings_dict.FindBool(prefs::kMouseSettingScrollAcceleration)
          .value_or(kDefaultScrollAcceleration);
  return settings;
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
                           kDefaultSensitivity,
                           force_persistence.scroll_sensitivity,
                           existing_settings_dict)) {
    settings_dict.Set(prefs::kMouseSettingScrollSensitivity,
                      mouse.settings->scroll_sensitivity);
  }

  if (ShouldPersistSetting(
          prefs::kMouseSettingScrollAcceleration,
          mouse.settings->scroll_acceleration, kDefaultScrollAcceleration,
          force_persistence.scroll_acceleration, existing_settings_dict)) {
    settings_dict.Set(prefs::kMouseSettingScrollAcceleration,
                      mouse.settings->scroll_acceleration);
  }

  return settings_dict;
}

void UpdateButtonRemappingDictPref(PrefService* pref_service,
                                   const mojom::Mouse& mouse) {
  const mojom::MouseSettings& settings = *mouse.settings;
  base::Value::List button_remappings =
      ConvertButtonRemappingArrayToList(settings.button_remappings);
  base::Value::Dict button_remappings_dict =
      pref_service->GetDict(prefs::kMouseButtonRemappingsDictPref).Clone();
  button_remappings_dict.Set(mouse.device_key, std::move(button_remappings));
  pref_service->SetDict(std::string(prefs::kMouseButtonRemappingsDictPref),
                        std::move(button_remappings_dict));
}

void UpdateMouseSettingsImpl(
    PrefService* pref_service,
    const mojom::MousePolicies& mouse_policies,
    const mojom::Mouse& mouse,
    const ForceMouseSettingPersistence& force_persistence) {
  DCHECK(mouse.settings);
  base::Value::Dict devices_dict =
      pref_service->GetDict(prefs::kMouseDeviceSettingsDictPref).Clone();
  base::Value::Dict* existing_settings_dict =
      devices_dict.FindDict(mouse.device_key);

  base::Value::Dict settings_dict = ConvertSettingsToDict(
      mouse, mouse_policies, force_persistence, existing_settings_dict);
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
    UpdateButtonRemappingDictPref(pref_service, mouse);
  }
}

mojom::MouseSettingsPtr GetMouseSettingsFromOldLocalStatePrefs(
    PrefService* local_state,
    const AccountId& account_id,
    const mojom::MousePolicies& mouse_policies,
    const mojom::Mouse& mouse) {
  mojom::MouseSettingsPtr settings = GetDefaultMouseSettings(mouse_policies);
  settings->swap_right =
      user_manager::KnownUser(local_state)
          .FindBoolPath(account_id, prefs::kOwnerPrimaryMouseButtonRight)
          .value_or(kDefaultSwapRight);

  return settings;
}

}  // namespace

MousePrefHandlerImpl::MousePrefHandlerImpl() = default;
MousePrefHandlerImpl::~MousePrefHandlerImpl() = default;

void MousePrefHandlerImpl::InitializeMouseSettings(
    PrefService* pref_service,
    const mojom::MousePolicies& mouse_policies,
    mojom::Mouse* mouse) {
  if (!pref_service) {
    mouse->settings = GetDefaultMouseSettings(mouse_policies);
    return;
  }

  const auto& devices_dict =
      pref_service->GetDict(prefs::kMouseDeviceSettingsDictPref);
  const auto* settings_dict = devices_dict.FindDict(mouse->device_key);
  ForceMouseSettingPersistence force_persistence;

  if (settings_dict) {
    mouse->settings =
        RetrieveMouseSettings(mouse_policies, *mouse, *settings_dict);
  } else if (Shell::Get()->input_device_tracker()->WasDevicePreviouslyConnected(
                 InputDeviceTracker::InputDeviceCategory::kMouse,
                 mouse->device_key)) {
    mouse->settings = GetMouseSettingsFromPrefs(pref_service, mouse_policies,
                                                force_persistence);
  } else {
    mouse->settings = GetDefaultMouseSettings(mouse_policies);
  }
  if (features::IsPeripheralCustomizationEnabled()) {
    const auto& button_remappings_dict =
        pref_service->GetDict(prefs::kMouseButtonRemappingsDictPref);
    const auto* button_remappings_list =
        button_remappings_dict.FindList(mouse->device_key);
    if (button_remappings_list) {
      mouse->settings->button_remappings =
          ConvertListToButtonRemappingArray(*button_remappings_list);
    }
  }
  DCHECK(mouse->settings);

  UpdateMouseSettingsImpl(pref_service, mouse_policies, *mouse,
                          force_persistence);

  if (mouse_policies.swap_right_policy &&
      mouse_policies.swap_right_policy->policy_status ==
          mojom::PolicyStatus::kManaged) {
    mouse->settings->swap_right = mouse_policies.swap_right_policy->value;
  }
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
  CHECK(local_state);
  // If the flag is disabled, clear all the settings dictionaries.
  if (!features::IsInputDeviceSettingsSplitEnabled()) {
    user_manager::KnownUser known_user(local_state);
    known_user.SetPath(account_id, prefs::kMouseLoginScreenInternalSettingsPref,
                       absl::nullopt);
    known_user.SetPath(account_id, prefs::kMouseLoginScreenExternalSettingsPref,
                       absl::nullopt);
    return;
  }

  const auto* settings_dict = GetLoginScreenSettingsDict(
      local_state, account_id,
      mouse->is_external ? prefs::kMouseLoginScreenExternalSettingsPref
                         : prefs::kMouseLoginScreenInternalSettingsPref);
  if (settings_dict) {
    mouse->settings =
        RetrieveMouseSettings(mouse_policies, *mouse, *settings_dict);
  } else {
    mouse->settings = GetMouseSettingsFromOldLocalStatePrefs(
        local_state, account_id, mouse_policies, *mouse);
  }

  if (mouse_policies.swap_right_policy &&
      mouse_policies.swap_right_policy->policy_status ==
          mojom::PolicyStatus::kManaged) {
    mouse->settings->swap_right = mouse_policies.swap_right_policy->value;
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
          absl::make_optional<base::Value>(ConvertSettingsToDict(
              mouse, mouse_policies, /*force_persistence=*/{}, settings_dict)));
}

void MousePrefHandlerImpl::InitializeWithDefaultMouseSettings(
    const mojom::MousePolicies& mouse_policies,
    mojom::Mouse* mouse) {
  mouse->settings = GetDefaultMouseSettings(mouse_policies);
}

}  // namespace ash
