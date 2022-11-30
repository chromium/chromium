// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_BROWSER_SIGNIN_POLICY_HANDLER_H_
#define CHROME_BROWSER_POLICY_BROWSER_SIGNIN_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {
// Values for the BrowserSignin policy.
// VALUES MUST COINCIDE WITH THE BrowserSignin POLICY DEFINITION.
enum class BrowserSigninMode {
  kDisabled = 0,
  kEnabled = 1,
  kForced = 2,
};

// ConfigurationPolicyHandler for the BrowserSignin policy. This handles all
// non-iOS platforms. The iOS equivalent handler is at
// ios/chrome/browser/policy/browser_signin_policy_handler.h
class BrowserSigninPolicyHandler : public IntRangePolicyHandler {
 public:
  explicit BrowserSigninPolicyHandler(Schema chrome_schema);
  BrowserSigninPolicyHandler(const BrowserSigninPolicyHandler&) = delete;
  BrowserSigninPolicyHandler& operator=(const BrowserSigninPolicyHandler&) =
      delete;
  ~BrowserSigninPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_BROWSER_SIGNIN_POLICY_HANDLER_H_
