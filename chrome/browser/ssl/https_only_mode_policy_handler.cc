// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/https_only_mode_policy_handler.h"

#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace policy {

HttpsOnlyModePolicyHandler::HttpsOnlyModePolicyHandler()
    : TypeCheckingPolicyHandler(key::kHttpsOnlyMode,
                                base::Value::Type::STRING) {}

HttpsOnlyModePolicyHandler::~HttpsOnlyModePolicyHandler() = default;

void HttpsOnlyModePolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                     PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(key::kHttpsOnlyMode, base::Value::Type::STRING);
  if (value && value->GetString() == "disallowed") {
    // Only apply the policy to the pref if it is set to "disallowed".
    prefs->SetBoolean(prefs::kHttpsOnlyModeEnabled, false);
  }
}

}  // namespace policy
