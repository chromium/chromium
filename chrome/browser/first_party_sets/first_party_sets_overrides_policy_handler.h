// Copyright 2022 The Chromium Authors. All rights reserved.
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
// The policy sets are then used to override the public list of sets.
class FirstPartySetsOverridesPolicyHandler
    : public policy::SchemaValidatingPolicyHandler {
 public:
  explicit FirstPartySetsOverridesPolicyHandler(const policy::Schema& schema);
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

  // Returns the validated policy, which is stored in the 'validated_dict_'
  // member variable.
  //
  // This method must only be called after CheckPolicySettings returns true,
  // which indicates that validating the policy was successful and the member
  // variable was populated.
  base::Value::Dict GetValidatedDictForTesting();

 private:
  // Result of validating the policy sets, stored for future use in
  // ApplyPolicySettings
  absl::optional<base::Value::Dict> validated_dict_;
};

}  // namespace first_party_sets

#endif  // CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_OVERRIDES_POLICY_HANDLER_H_
