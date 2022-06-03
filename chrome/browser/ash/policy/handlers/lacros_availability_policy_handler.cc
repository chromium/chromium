// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/lacros_availability_policy_handler.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#error This file shall only be used in ash.
#endif

#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

LacrosAvailabilityPolicyHandler::LacrosAvailabilityPolicyHandler()
    : TypeCheckingPolicyHandler(key::kLacrosAvailability,
                                base::Value::Type::STRING),
      policy_value_to_enum_{
          {"user_choice",
           crosapi::browser_util::LacrosLaunchSwitch::kUserChoice},
          {"lacros_disallowed",
           crosapi::browser_util::LacrosLaunchSwitch::kLacrosDisallowed},
          {"side_by_side",
           crosapi::browser_util::LacrosLaunchSwitch::kSideBySide},
          {"lacros_primary",
           crosapi::browser_util::LacrosLaunchSwitch::kLacrosPrimary},
          {"lacros_only",
           crosapi::browser_util::LacrosLaunchSwitch::kLacrosOnly},
      } {}

LacrosAvailabilityPolicyHandler::~LacrosAvailabilityPolicyHandler() = default;

bool LacrosAvailabilityPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  return GetValue(policies, errors).has_value();
}

void LacrosAvailabilityPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  auto enum_value = GetValue(policies, nullptr);
  if (enum_value.has_value()) {
    prefs->SetInteger(prefs::kLacrosLaunchSwitch,
                      static_cast<int>(*enum_value));
  }
}

absl::optional<crosapi::browser_util::LacrosLaunchSwitch>
LacrosAvailabilityPolicyHandler::GetValue(const PolicyMap& policies,
                                          PolicyErrorMap* errors) {
  const base::Value* value;
  const bool value_found = CheckAndGetValue(policies, errors, &value) && value;
  if (!value_found) {
    return absl::nullopt;
  }

  const auto value_it = policy_value_to_enum_.find(value->GetString());
  if (value_it == policy_value_to_enum_.end()) {
    if (errors)
      errors->AddError(policy_name(), IDS_POLICY_VALUE_FORMAT_ERROR);
    return absl::nullopt;
  }

  return value_it->second;
}

}  // namespace policy
