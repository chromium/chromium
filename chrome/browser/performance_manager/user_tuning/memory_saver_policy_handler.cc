// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/user_tuning/memory_saver_policy_handler.h"

#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

using performance_manager::user_tuning::prefs::kMemorySaverModeState;
using MemorySaverModeState =
    performance_manager::user_tuning::prefs::MemorySaverModeState;

namespace performance_manager {

MemorySaverPolicyHandler::MemorySaverPolicyHandler()
    : policy::TypeCheckingPolicyHandler(policy::key::kHighEfficiencyModeEnabled,
                                        base::Value::Type::BOOLEAN) {}

MemorySaverPolicyHandler::~MemorySaverPolicyHandler() = default;

void MemorySaverPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::BOOLEAN);
  if (!value) {
    return;
  }

  MemorySaverModeState state = value->GetBool()
                                   ? MemorySaverModeState::kEnabled
                                   : MemorySaverModeState::kDisabled;

  prefs->SetValue(kMemorySaverModeState, base::Value(static_cast<int>(state)));
}

}  // namespace performance_manager
