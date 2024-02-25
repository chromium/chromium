// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_POLICY_HANDLER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_POLICY_HANDLER_H_

#include "ash/ash_export.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "base/functional/callback_forward.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace ash {

class ASH_EXPORT InputDeviceSettingsPolicyHandler {
 public:
  using EnterprisePolicyCallback = base::RepeatingClosure;

  explicit InputDeviceSettingsPolicyHandler(
      EnterprisePolicyCallback keyboard_policy_callback,
      EnterprisePolicyCallback mouse_policy_callback);
  InputDeviceSettingsPolicyHandler(const InputDeviceSettingsPolicyHandler&) =
      delete;
  InputDeviceSettingsPolicyHandler& operator=(
      const InputDeviceSettingsPolicyHandler&) = delete;
  ~InputDeviceSettingsPolicyHandler();

  void Initialize(PrefService* local_state, PrefService* pref_service);

  const mojom::KeyboardPolicies& keyboard_policies() const {
    return keyboard_policies_;
  }

  const mojom::MousePolicies& mouse_policies() const { return mouse_policies_; }

 private:
  void RefreshKeyboardPolicies(bool notify);
  void RefreshMousePolicies(bool notify);

  void OnKeyboardPoliciesChanged(const std::string& pref_name);
  void OnMousePoliciesChanged(const std::string& pref_name);

  EnterprisePolicyCallback keyboard_policy_callback_;
  EnterprisePolicyCallback mouse_policy_callback_;

  mojom::KeyboardPolicies keyboard_policies_;
  mojom::MousePolicies mouse_policies_;

  // Used to track preferences which may be controlled by enterprise
  // policies.
  PrefChangeRegistrar pref_change_registrar_;
  // PrefChangeRegistrar specific to local_state prefs.
  PrefChangeRegistrar pref_change_registrar_local_state_;

  // Keeps track whether the policy handler was initialized with user pref
  // service.
  bool has_user_prefs_ = false;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_SETTINGS_POLICY_HANDLER_H_
