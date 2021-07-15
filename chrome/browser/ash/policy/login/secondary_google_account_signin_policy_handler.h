// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_LOGIN_SECONDARY_GOOGLE_ACCOUNT_SIGNIN_POLICY_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_LOGIN_SECONDARY_GOOGLE_ACCOUNT_SIGNIN_POLICY_HANDLER_H_

#include "base/macros.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/policy_export.h"

class PrefValueMap;

namespace policy {

class PolicyMap;

// Handles secondary Google account sign-in (in the content area) policy.
class POLICY_EXPORT SecondaryGoogleAccountSigninPolicyHandler
    : public TypeCheckingPolicyHandler {
 public:
  SecondaryGoogleAccountSigninPolicyHandler();
  ~SecondaryGoogleAccountSigninPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SecondaryGoogleAccountSigninPolicyHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_LOGIN_SECONDARY_GOOGLE_ACCOUNT_SIGNIN_POLICY_HANDLER_H_
