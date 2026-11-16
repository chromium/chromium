// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/system_title_bar_enabled_policy_handler.h"

#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace policy {

SystemTitleBarEnabledPolicyHandler::SystemTitleBarEnabledPolicyHandler()
    : TypeCheckingPolicyHandler(key::kSystemTitleBarEnabled,
                                base::Value::Type::BOOLEAN) {}

SystemTitleBarEnabledPolicyHandler::~SystemTitleBarEnabledPolicyHandler() =
    default;

void SystemTitleBarEnabledPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::BOOLEAN);
  if (value) {
    prefs->SetBoolean(prefs::kUseCustomChromeFrame, !value->GetBool());
  }
}

}  // namespace policy
