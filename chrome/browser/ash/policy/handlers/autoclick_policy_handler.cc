// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/autoclick_policy_handler.h"

#include "ash/constants/ash_pref_names.h"
#include "base/values.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace policy {

AutoclickPolicyHandler::AutoclickPolicyHandler()
    : policy::TypeCheckingPolicyHandler(policy::key::kAutoclickEnabled,
                                        base::Value::Type::BOOLEAN) {}

AutoclickPolicyHandler::~AutoclickPolicyHandler() = default;

void AutoclickPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value = policies.GetValue(policy_name());
  if (!value)
    return;
  ash::AccessibilityManager::Get()->EnableAutoclickWithoutConfirmationDialog(
      value->GetBool());
  // Although above call changes the pref as well, we need to also update the
  // passed-in PrefValueMap to mark the pref as forced by policy and prevent
  // users from changing it themselves.
  prefs->SetBoolean(ash::prefs::kAccessibilityAutoclickEnabled,
                    value->GetBool());
}

}  // namespace policy
