// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/extension_request/extension_request_policy_handler.h"

#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace enterprise_reporting {

ExtensionRequestPolicyHandler::ExtensionRequestPolicyHandler()
    : policy::TypeCheckingPolicyHandler(
          policy::key::kCloudExtensionRequestEnabled,
          base::Value::Type::BOOLEAN) {}

ExtensionRequestPolicyHandler::~ExtensionRequestPolicyHandler() = default;

bool ExtensionRequestPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  if (!policies.IsPolicySet(policy_name()))
    return true;
  if (!TypeCheckingPolicyHandler::CheckPolicySettings(policies, errors))
    return false;
  const base::Value* cloud_reporting_policy_value = policies.GetValue(
      policy::key::kCloudReportingEnabled, base::Value::Type::BOOLEAN);
  if (!cloud_reporting_policy_value ||
      !cloud_reporting_policy_value->GetBool()) {
    errors->AddError(policy_name(), IDS_POLICY_DEPENDENCY_ERROR,
                     policy::key::kCloudReportingEnabled, "Enabled");
    return false;
  }

  const policy::PolicyMap::Entry* extension_request_policy =
      policies.Get(policy::key::kCloudExtensionRequestEnabled);
  if (extension_request_policy->source != policy::POLICY_SOURCE_CLOUD) {
    errors->AddError(policy_name(), IDS_POLICY_CLOUD_SOURCE_ONLY_ERROR);
    return false;
  }

#if !BUILDFLAG(IS_CHROMEOS)
  // Disable extension workflow when it's set by user cloud policy but machine
  // is not managed or managed by a different domain.
  if (extension_request_policy->scope == policy::POLICY_SCOPE_USER &&
      !policies.IsUserAffiliated()) {
    errors->AddError(policy_name(), IDS_POLICY_USER_IS_NOT_AFFILIATED_ERROR);
    return false;
  }
#endif

  return true;
}

void ExtensionRequestPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* extension_request_policy_value =
      policies.GetValue(policy_name(), base::Value::Type::BOOLEAN);

  if (extension_request_policy_value) {
    prefs->SetValue(prefs::kCloudExtensionRequestEnabled,
                    extension_request_policy_value->Clone());
  }
}

}  // namespace enterprise_reporting
