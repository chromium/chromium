// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/devtools_gen_ai_policy_handler.h"

#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace policy {

DevtoolsGenAiPolicyHandler::DevtoolsGenAiPolicyHandler()
    : IntRangePolicyHandlerBase(key::kDevToolsGenAiSettings, 0, 2, false) {}

DevtoolsGenAiPolicyHandler::~DevtoolsGenAiPolicyHandler() = default;

void DevtoolsGenAiPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                     PrefValueMap* prefs) {
  const base::Value* const value =
      policies.GetValue(policy_name(), base::Value::Type::INTEGER);
  int value_in_range;
  if (value && EnsureInRange(value, &value_in_range, nullptr)) {
    // Map unimplemented value 1 (enable, but don't use data) to 2 (disable)
    if (value_in_range == 1) {
      value_in_range = 2;
    }
    prefs->SetInteger(prefs::kDevToolsGenAiSettings, value_in_range);
  }
}

}  // namespace policy
