// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/lacros_selection_policy_handler.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#error This file shall only be used in ash.
#endif

#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

LacrosSelectionPolicyHandler::LacrosSelectionPolicyHandler()
    : TypeCheckingPolicyHandler(key::kLacrosSelection,
                                base::Value::Type::STRING) {}

bool LacrosSelectionPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  return GetValue(policies, errors).has_value();
}

void LacrosSelectionPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  auto enum_value = GetValue(policies, nullptr);
  if (enum_value.has_value())
    prefs->SetInteger(prefs::kLacrosSelection, static_cast<int>(*enum_value));
}

std::optional<ash::standalone_browser::LacrosSelectionPolicy>
LacrosSelectionPolicyHandler::GetValue(const PolicyMap& policies,
                                       PolicyErrorMap* errors) {
  const base::Value* value;
  const bool value_found = CheckAndGetValue(policies, errors, &value) && value;
  if (!value_found)
    return std::nullopt;

  auto parsed =
      ash::standalone_browser::ParseLacrosSelectionPolicy(value->GetString());
  if (!parsed.has_value() && errors) {
    errors->AddError(policy_name(), IDS_POLICY_INVALID_SELECTION_ERROR,
                     "LacrosSelection value");
  }
  return parsed;
}

}  // namespace policy
