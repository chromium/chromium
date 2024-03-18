// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/tracking_protection_policy_handler.h"

#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/strings/grit/components_strings.h"

TrackingProtectionPolicyHandler::TrackingProtectionPolicyHandler() = default;

TrackingProtectionPolicyHandler::~TrackingProtectionPolicyHandler() = default;

bool TrackingProtectionPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  return true;
}

void TrackingProtectionPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  // Handle IPP policy if one exists.
  const base::Value* ip_protection_enabled =
      policies.GetValue(policy::key::kPrivacySandboxIpProtectionEnabled,
                        base::Value::Type::BOOLEAN);

  if (ip_protection_enabled) {
    prefs->SetBoolean(prefs::kIpProtectionEnabled,
                      ip_protection_enabled->GetBool());
  }
}
