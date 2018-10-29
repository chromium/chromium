// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_BROWSER_SIGNIN_POLICY_HANDLER_H_
#define CHROME_BROWSER_POLICY_BROWSER_SIGNIN_POLICY_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {
// Values for the BrowserSignin policy.
// VALUES MUST COINCIDE WITH THE BrowserSignin POLICY DEFINITION.
enum class BrowserSigninMode {
  kDisabled = 0,
  kEnabled = 1,
  kForced = 2,
};

// ConfigurationPolicyHandler for the RoamingProfileLocation policy.
class BrowserSigninPolicyHandler : public SchemaValidatingPolicyHandler {
 public:
  explicit BrowserSigninPolicyHandler(Schema chrome_schema);
  ~BrowserSigninPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserSigninPolicyHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_BROWSER_SIGNIN_POLICY_HANDLER_H_
