// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_EXTENSION_REQUEST_EXTENSION_REQUEST_POLICY_HANDLER_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_EXTENSION_REQUEST_EXTENSION_REQUEST_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace enterprise_reporting {

// Sets prefs for policy CloudExtensionRequestEnabled policy.
//
// The policy value is set to pref iff CloudReportingEnabled is set to True.
// Otherwise, it will use the default value.
class ExtensionRequestPolicyHandler : public policy::TypeCheckingPolicyHandler {
 public:
  ExtensionRequestPolicyHandler();
  ~ExtensionRequestPolicyHandler() override;

  // policy::TypeCheckingPolicyHandler
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_EXTENSION_REQUEST_EXTENSION_REQUEST_POLICY_HANDLER_H_
