// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/extension_request/extension_request_policy_handler.h"

#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace enterprise_reporting {

ExtensionRequestPolicyHandler::ExtensionRequestPolicyHandler()
    : policy::TypeCheckingPolicyHandler(
          policy::key::kCloudExtensionRequestEnabled,
          base::Value::Type::BOOLEAN) {}

ExtensionRequestPolicyHandler::~ExtensionRequestPolicyHandler() = default;

void ExtensionRequestPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* cloud_reporting_policy_value =
      policies.GetValue(policy::key::kCloudReportingEnabled);
  if (!cloud_reporting_policy_value ||
      !cloud_reporting_policy_value->is_bool() ||
      !cloud_reporting_policy_value->GetBool())
    return;

  const base::Value* extension_request_policy_value =
      policies.GetValue(policy_name());

  if (extension_request_policy_value) {
    prefs->SetValue(prefs::kCloudExtensionRequestEnabled,
                    extension_request_policy_value->Clone());
  }
}

}  // namespace enterprise_reporting
