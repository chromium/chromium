// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_POLICY_HANDLER_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace policy {
class PolicyErrorMap;
class PolicyMap;
}  // namespace policy

// ConfigurationPolicyHandler for the privacy sandbox policies.
class PrivacySandboxPolicyHandler : public policy::ConfigurationPolicyHandler {
 public:
  PrivacySandboxPolicyHandler();

  PrivacySandboxPolicyHandler(const PrivacySandboxPolicyHandler&) = delete;
  PrivacySandboxPolicyHandler& operator=(const PrivacySandboxPolicyHandler&) =
      delete;

  ~PrivacySandboxPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_POLICY_HANDLER_H_
