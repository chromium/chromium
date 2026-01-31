// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_CHROME_INCOGNITO_MODE_POLICY_HANDLER_H_
#define CHROME_BROWSER_PROFILES_CHROME_INCOGNITO_MODE_POLICY_HANDLER_H_

#include "components/policy/core/browser/incognito/incognito_mode_policy_handler.h"

class PrefValueMap;

namespace policy {

class PolicyErrorMap;
class PolicyMap;

// ConfigurationPolicyHandler for the chrome-specific logic of Incognito mode
// policies and handling the deprecated IncognitoEnabled policy.
class ChromeIncognitoModePolicyHandler : public IncognitoModePolicyHandler {
 public:
  ChromeIncognitoModePolicyHandler();
  ChromeIncognitoModePolicyHandler(const ChromeIncognitoModePolicyHandler&) =
      delete;
  ChromeIncognitoModePolicyHandler& operator=(
      const ChromeIncognitoModePolicyHandler&) = delete;
  ~ChromeIncognitoModePolicyHandler() override;

  // Extends the superclass's CheckPolicySettings to also validate the
  // deprecated kIncognitoEnabled policy when kIncognitoModeAvailability is
  // unset.
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;

  // Interprets the IncognitoModeAvailability and IncognitoEnabled policies and
  // translates them to the kIncognitoModeAvailability preference value.
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_PROFILES_CHROME_INCOGNITO_MODE_POLICY_HANDLER_H_
