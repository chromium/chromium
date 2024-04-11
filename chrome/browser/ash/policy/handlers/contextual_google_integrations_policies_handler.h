// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_HANDLERS_CONTEXTUAL_GOOGLE_INTEGRATIONS_POLICIES_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_HANDLERS_CONTEXTUAL_GOOGLE_INTEGRATIONS_POLICIES_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace policy {

class PolicyErrorMap;
class PolicyMap;
class Schema;

// Handles `ContextualGoogleIntegrationsEnabled` and
// `ContextualGoogleIntegrationsConfiguration` policies.
class ContextualGoogleIntegrationsPoliciesHandler
    : public SchemaValidatingPolicyHandler {
 public:
  explicit ContextualGoogleIntegrationsPoliciesHandler(const Schema& schema);
  ContextualGoogleIntegrationsPoliciesHandler(
      const ContextualGoogleIntegrationsPoliciesHandler&) = delete;
  ContextualGoogleIntegrationsPoliciesHandler& operator=(
      const ContextualGoogleIntegrationsPoliciesHandler&) = delete;
  ~ContextualGoogleIntegrationsPoliciesHandler() override;

  // SchemaValidatingPolicyHandler:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;

 protected:
  // SchemaValidatingPolicyHandler:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_HANDLERS_CONTEXTUAL_GOOGLE_INTEGRATIONS_POLICIES_HANDLER_H_
