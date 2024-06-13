// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/extension_developer_mode_policy_handler.h"

#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace {
// Values for the ExtensionDeveloperModeSettings policy. These must be in sync
// with the policy definition ExtensionDeveloperModeSettings.yaml.
enum ExtensionDeveloperModeSettings {
  kAllow = 0,
  kDisallow = 1,
};
}  // namespace

namespace policy {
ExtensionDeveloperModePolicyHandler::ExtensionDeveloperModePolicyHandler()
    : IntRangePolicyHandlerBase(key::kExtensionDeveloperModeSettings,
                                ExtensionDeveloperModeSettings::kAllow,
                                ExtensionDeveloperModeSettings::kDisallow,
                                /*clamp=*/false) {}

ExtensionDeveloperModePolicyHandler::~ExtensionDeveloperModePolicyHandler() =
    default;

bool ExtensionDeveloperModePolicyHandler::IsValidPolicySet(
    const PolicyMap& policies) {
  // Ensure that the value is set and valid.
  return CheckPolicySettings(policies, /*errors=*/nullptr) &&
         policies.GetValue(policy_name(), base::Value::Type::INTEGER);
}

void ExtensionDeveloperModePolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::INTEGER);
  if (value && value->GetInt() == ExtensionDeveloperModeSettings::kDisallow) {
    prefs->SetBoolean(prefs::kExtensionsUIDeveloperMode, false);
  }
}
}  // namespace policy
