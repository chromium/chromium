// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_EXTENSIBLE_ENTERPRISE_SSO_POLICY_HANDLER_H_
#define CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_EXTENSIBLE_ENTERPRISE_SSO_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {
class PolicyMap;
}  // namespace policy

namespace enterprise_auth {

extern const char kAllIdentityProviders[];
extern const char kMicrosoftIdentityProvider[];

// Policy handler for EnterpriseAuthenticationAppLink policy
class ExtensibleEnterpriseSSOPolicyHandler
    : public policy::SchemaValidatingPolicyHandler {
 public:
  explicit ExtensibleEnterpriseSSOPolicyHandler(
      const policy::Schema& chrome_schema);

  ExtensibleEnterpriseSSOPolicyHandler(
      const ExtensibleEnterpriseSSOPolicyHandler&) = delete;
  ExtensibleEnterpriseSSOPolicyHandler& operator=(
      const ExtensibleEnterpriseSSOPolicyHandler&) = delete;
  ~ExtensibleEnterpriseSSOPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace enterprise_auth

#endif  // CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_EXTENSIBLE_ENTERPRISE_SSO_POLICY_HANDLER_H_
