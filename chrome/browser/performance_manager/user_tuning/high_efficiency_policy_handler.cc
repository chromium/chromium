// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/user_tuning/high_efficiency_policy_handler.h"

#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

using performance_manager::user_tuning::prefs::kHighEfficiencyModeState;
using HighEfficiencyModeState =
    performance_manager::user_tuning::prefs::HighEfficiencyModeState;

namespace performance_manager {

HighEfficiencyPolicyHandler::HighEfficiencyPolicyHandler()
    : policy::TypeCheckingPolicyHandler(policy::key::kHighEfficiencyModeEnabled,
                                        base::Value::Type::BOOLEAN) {}

HighEfficiencyPolicyHandler::~HighEfficiencyPolicyHandler() = default;

void HighEfficiencyPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::BOOLEAN);
  if (!value) {
    return;
  }

  HighEfficiencyModeState state = value->GetBool()
                                      ? HighEfficiencyModeState::kEnabledOnTimer
                                      : HighEfficiencyModeState::kDisabled;

  prefs->SetValue(kHighEfficiencyModeState,
                  base::Value(static_cast<int>(state)));
}

}  // namespace performance_manager
