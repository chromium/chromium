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

}  // namespace

InputDeviceSettingsPolicyHandler::InputDeviceSettingsPolicyHandler(
    EnterprisePolicyCallback keyboard_policy_callback,
    EnterprisePolicyCallback mouse_policy_callback)
    : keyboard_policy_callback_(std::move(keyboard_policy_callback)),
      mouse_policy_callback_(std::move(mouse_policy_callback)) {}
InputDeviceSettingsPolicyHandler::~InputDeviceSettingsPolicyHandler() = default;

void InputDeviceSettingsPolicyHandler::Initialize(PrefService* pref_service) {
  CHECK(pref_service);

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

  RefreshKeyboardPolicies(/*notify=*/false);
  RefreshMousePolicies(/*notify=*/false);
}

void InputDeviceSettingsPolicyHandler::RefreshKeyboardPolicies(bool notify) {
  keyboard_policies_.top_row_are_fkeys_policy = GetBooleanPreferencePolicy(
      pref_change_registrar_.prefs(), prefs::kSendFunctionKeys);

  if (notify) {
    keyboard_policy_callback_.Run();
  }
}

void InputDeviceSettingsPolicyHandler::RefreshMousePolicies(bool notify) {
  mouse_policies_.swap_right_policy = GetBooleanPreferencePolicy(
      pref_change_registrar_.prefs(), prefs::kPrimaryMouseButtonRight);

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
