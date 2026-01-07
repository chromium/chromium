// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_CHROME_INCOGNITO_MODE_POLICY_HANDLER_H_
#define CHROME_BROWSER_PROFILES_CHROME_INCOGNITO_MODE_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace policy {

class PolicyErrorMap;
class PolicyMap;

// ConfigurationPolicyHandler for the incognito mode policies.
class ChromeIncognitoModePolicyHandler : public ConfigurationPolicyHandler {
 public:
  ChromeIncognitoModePolicyHandler();

  ChromeIncognitoModePolicyHandler(const ChromeIncognitoModePolicyHandler&) =
      delete;
  ChromeIncognitoModePolicyHandler& operator=(
      const ChromeIncognitoModePolicyHandler&) = delete;

  ~ChromeIncognitoModePolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_PROFILES_CHROME_INCOGNITO_MODE_POLICY_HANDLER_H_
