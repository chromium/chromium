// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_OVERRIDES_POLICY_HANDLER_H_
#define CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_OVERRIDES_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace policy {
class PolicyErrorMap;
class PolicyMap;
class Schema;
}  // namespace policy

namespace first_party_sets {

// A schema policy handler for First-Party Sets which validates that all sets
// provided by the policy adhere to the required First-Party Sets invariants.
class FirstPartySetsOverridesPolicyHandler
    : public policy::SchemaValidatingPolicyHandler {
 public:
  explicit FirstPartySetsOverridesPolicyHandler(const char* policy_name,
                                                const policy::Schema& schema);
  FirstPartySetsOverridesPolicyHandler(
      const FirstPartySetsOverridesPolicyHandler&) = delete;
  FirstPartySetsOverridesPolicyHandler& operator=(
      const FirstPartySetsOverridesPolicyHandler&) = delete;
  ~FirstPartySetsOverridesPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace first_party_sets

#endif  // CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_OVERRIDES_POLICY_HANDLER_H_
