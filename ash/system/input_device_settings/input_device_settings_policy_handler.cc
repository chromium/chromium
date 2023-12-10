// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_policy_handler.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace {

mojom::InputDeviceSettingsPolicyPtr GetBooleanPreferencePolicy(
    const PrefService* pref_service,
    const std::string& pref_name) {
  mojom::InputDeviceSettingsPolicyPtr policy;

  CHECK(pref_service);
  const auto* pref = pref_service->FindPreference(pref_name);
  if (!pref) {
    return policy;
  }

  if (pref->IsManaged()) {
    auto* value = pref->GetValue();
    CHECK(value && value->is_bool());
    policy = mojom::InputDeviceSettingsPolicy::New(
        mojom::PolicyStatus::kManaged, value->GetBool());

    // Prefs with recommended values must use `GetRecommendedValue` instead
    // of `IsRecommended` as `IsRecommended` may return false even if the pref
    // has a recommended value.
  } else if (auto* value = pref->GetRecommendedValue(); value) {
    CHECK(value->is_bool());
    policy = mojom::InputDeviceSettingsPolicy::New(
        mojom::PolicyStatus::kRecommended, value->GetBool());
  }

  return policy;
}

mojom::InputDeviceSettingsFkeyPolicyPtr GetFkeyPreferencePolicy(
    const PrefService* pref_service) {
  mojom::InputDeviceSettingsFkeyPolicyPtr policy;

  const auto* pref =
      pref_service->FindPreference(prefs::kExtendedFkeysModifier);
  if (!pref) {
    return policy;
  }

  if (pref->IsManaged()) {
    auto* value = pref->GetValue();
    CHECK(value && value->is_int());
    policy = mojom::InputDeviceSettingsFkeyPolicy::New(
        mojom::PolicyStatus::kManaged,
        static_cast<ui::mojom::ExtendedFkeysModifier>(value->GetInt()));

    // Prefs with recommended values must use `GetRecommendedValue` instead
    // of `IsRecommended` as `IsRecommended` may return false even if the pref
    // has a recommended value.
  } else if (auto* value = pref->GetRecommendedValue(); value) {
    CHECK(value->is_int());
    policy = mojom::InputDeviceSettingsFkeyPolicy::New(
        mojom::PolicyStatus::kRecommended,
        static_cast<ui::mojom::ExtendedFkeysModifier>(value->GetInt()));
  }

  return policy;
}

}  // namespace

InputDeviceSettingsPolicyHandler::InputDeviceSettingsPolicyHandler(
    EnterprisePolicyCallback keyboard_policy_callback,
    EnterprisePolicyCallback mouse_policy_callback)
    : keyboard_policy_callback_(std::move(keyboard_policy_callback)),
      mouse_policy_callback_(std::move(mouse_policy_callback)) {}
InputDeviceSettingsPolicyHandler::~InputDeviceSettingsPolicyHandler() = default;

void InputDeviceSettingsPolicyHandler::Initialize(PrefService* local_state,
                                                  PrefService* pref_service) {
  has_user_prefs_ = pref_service != nullptr;
  if (local_state) {
    pref_change_registrar_local_state_.Init(local_state);
    // Add pref observer for kOwnerPrimaryMouseButtonRight only when there is
    // no user pref_service.
    if (!pref_service) {
      pref_change_registrar_local_state_.Add(
          prefs::kOwnerPrimaryMouseButtonRight,
          base::BindRepeating(
              &InputDeviceSettingsPolicyHandler::OnMousePoliciesChanged,
              base::Unretained(this)));
    }
    pref_change_registrar_local_state_.Add(
        prefs::kDeviceSwitchFunctionKeysBehaviorEnabled,
        base::BindRepeating(
            &InputDeviceSettingsPolicyHandler::OnKeyboardPoliciesChanged,
            base::Unretained(this)));
  }
  if (pref_service) {
    pref_change_registrar_.Init(pref_service);
    pref_change_registrar_.Add(
        prefs::kPrimaryMouseButtonRight,
        base::BindRepeating(
            &InputDeviceSettingsPolicyHandler::OnMousePoliciesChanged,
            base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kSendFunctionKeys,
        base::BindRepeating(
            &InputDeviceSettingsPolicyHandler::OnKeyboardPoliciesChanged,
            base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kExtendedFkeysModifier,
        base::BindRepeating(
            &InputDeviceSettingsPolicyHandler::OnKeyboardPoliciesChanged,
            base::Unretained(this)));
  }

  RefreshKeyboardPolicies(/*notify=*/false);
  RefreshMousePolicies(/*notify=*/false);
}

void InputDeviceSettingsPolicyHandler::RefreshKeyboardPolicies(bool notify) {
  if (has_user_prefs_) {
    keyboard_policies_.top_row_are_fkeys_policy = GetBooleanPreferencePolicy(
        pref_change_registrar_.prefs(), prefs::kSendFunctionKeys);

    keyboard_policies_.extended_fkeys_policy =
        GetFkeyPreferencePolicy(pref_change_registrar_.prefs());
  }

  if (pref_change_registrar_local_state_.prefs()) {
    keyboard_policies_.enable_meta_fkey_rewrites_policy =
        GetBooleanPreferencePolicy(
            pref_change_registrar_local_state_.prefs(),
            prefs::kDeviceSwitchFunctionKeysBehaviorEnabled);
  }

  if (notify) {
    keyboard_policy_callback_.Run();
  }
}

void InputDeviceSettingsPolicyHandler::RefreshMousePolicies(bool notify) {
  if (has_user_prefs_) {
    mouse_policies_.swap_right_policy = GetBooleanPreferencePolicy(
        pref_change_registrar_.prefs(), prefs::kPrimaryMouseButtonRight);
  } else {
    mouse_policies_.swap_right_policy =
        GetBooleanPreferencePolicy(pref_change_registrar_local_state_.prefs(),
                                   prefs::kOwnerPrimaryMouseButtonRight);
  }

  if (notify) {
    mouse_policy_callback_.Run();
  }
}

void InputDeviceSettingsPolicyHandler::OnKeyboardPoliciesChanged(
    const std::string& pref_name) {
  RefreshKeyboardPolicies(/*notify=*/true);
}

void InputDeviceSettingsPolicyHandler::OnMousePoliciesChanged(
    const std::string& pref_name) {
  RefreshMousePolicies(/*notify=*/true);
}

}  // namespace ash
