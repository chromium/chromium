// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_IDLE_IDLE_TIMEOUT_POLICY_HANDLER_H_
#define CHROME_BROWSER_ENTERPRISE_IDLE_IDLE_TIMEOUT_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace policy {
class PolicyErrorMap;
class PolicyMap;
}  // namespace policy

namespace enterprise_idle {

// Handles IdleTimeout policy.
class IdleTimeoutPolicyHandler : public policy::IntRangePolicyHandler {
 public:
  IdleTimeoutPolicyHandler();

  IdleTimeoutPolicyHandler(const IdleTimeoutPolicyHandler&) = delete;
  IdleTimeoutPolicyHandler& operator=(const IdleTimeoutPolicyHandler&) = delete;

  ~IdleTimeoutPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
};

// Handles IdleTimeoutActions policy.
class IdleTimeoutActionsPolicyHandler
    : public policy::SchemaValidatingPolicyHandler {
 public:
  explicit IdleTimeoutActionsPolicyHandler(policy::Schema schema);

  IdleTimeoutActionsPolicyHandler(const IdleTimeoutActionsPolicyHandler&) =
      delete;
  IdleTimeoutActionsPolicyHandler& operator=(
      const IdleTimeoutActionsPolicyHandler&) = delete;

  ~IdleTimeoutActionsPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
};

}  // namespace enterprise_idle

#endif  // CHROME_BROWSER_ENTERPRISE_IDLE_IDLE_TIMEOUT_POLICY_HANDLER_H_
