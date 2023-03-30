// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_policy_handler.h"

#include "ash/constants/ash_pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace ash {

InputDeviceSettingsPolicyHandler::InputDeviceSettingsPolicyHandler() = default;
InputDeviceSettingsPolicyHandler::~InputDeviceSettingsPolicyHandler() = default;

void InputDeviceSettingsPolicyHandler::Initialize(PrefService* pref_service) {
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
}

void InputDeviceSettingsPolicyHandler::OnKeyboardPoliciesChanged(
    const std::string& pref_name) {
  // TODO(dpad): Implement retrieval of policy status.
}

void InputDeviceSettingsPolicyHandler::OnMousePoliciesChanged(
    const std::string& pref_name) {
  // TODO(dpad): Implement retrieval of policy status.
}

}  // namespace ash
