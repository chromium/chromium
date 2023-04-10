// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/pref_handlers/mouse_pref_handler_impl.h"

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
  const mojom::MouseSettings& settings = *mouse.settings;

  // Populate `settings_dict` with all settings in `settings`.
  base::Value::Dict settings_dict;

  if (ShouldPersistSetting(
          mouse_policies.swap_right_policy, prefs::kMouseSettingSwapRight,
          settings.swap_right, GetDefaultSwapRightValue(mouse_policies),
          force_persistence.swap_right, existing_settings_dict)) {
    settings_dict.Set(prefs::kMouseSettingSwapRight, settings.swap_right);
  }

  if (ShouldPersistSetting(prefs::kMouseSettingSensitivity,
                           static_cast<int>(settings.sensitivity),
                           kDefaultSensitivity, force_persistence.sensitivity,
                           existing_settings_dict)) {
    settings_dict.Set(prefs::kMouseSettingSensitivity, settings.sensitivity);
  }

  if (ShouldPersistSetting(prefs::kMouseSettingReverseScrolling,
                           settings.reverse_scrolling, kDefaultReverseScrolling,
                           force_persistence.reverse_scrolling,
                           existing_settings_dict)) {
    settings_dict.Set(prefs::kMouseSettingReverseScrolling,
                      settings.reverse_scrolling);
  }

  if (ShouldPersistSetting(
          prefs::kMouseSettingAccelerationEnabled,
          settings.acceleration_enabled, kDefaultAccelerationEnabled,
          force_persistence.acceleration_enabled, existing_settings_dict)) {
    settings_dict.Set(prefs::kMouseSettingAccelerationEnabled,
                      settings.acceleration_enabled);
  }

  if (ShouldPersistSetting(
          prefs::kMouseSettingScrollSensitivity,
          static_cast<int>(settings.scroll_sensitivity), kDefaultSensitivity,
          force_persistence.scroll_sensitivity, existing_settings_dict)) {
    settings_dict.Set(prefs::kMouseSettingScrollSensitivity,
                      settings.scroll_sensitivity);
  }

  if (ShouldPersistSetting(
          prefs::kMouseSettingScrollAcceleration, settings.scroll_acceleration,
          kDefaultScrollAcceleration, force_persistence.scroll_acceleration,
          existing_settings_dict)) {
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

}  // namespace ash
