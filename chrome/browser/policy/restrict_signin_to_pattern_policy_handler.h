// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_RESTRICT_SIGNIN_TO_PATTERN_POLICY_HANDLER_H_
#define CHROME_BROWSER_POLICY_RESTRICT_SIGNIN_TO_PATTERN_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {

// Policy handler for the `RestrictSigninToPattern` policy.
// Subclasses `SimplePolicyHandler` to map the policy to
// `prefs::kGoogleServicesUsernamePattern`, while emitting a non-blocking
// warning on chrome://policy if the pattern cannot match standard Google
// account email addresses or is an invalid regular expression.
class RestrictSigninToPatternPolicyHandler : public SimplePolicyHandler {
 public:
  RestrictSigninToPatternPolicyHandler();
  RestrictSigninToPatternPolicyHandler(
      const RestrictSigninToPatternPolicyHandler&) = delete;
  RestrictSigninToPatternPolicyHandler& operator=(
      const RestrictSigninToPatternPolicyHandler&) = delete;
  ~RestrictSigninToPatternPolicyHandler() override;

  // ConfigurationPolicyHandler:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_RESTRICT_SIGNIN_TO_PATTERN_POLICY_HANDLER_H_
