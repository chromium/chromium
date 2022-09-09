// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_NTP_CUSTOM_BACKGROUND_ENABLED_POLICY_HANDLER_H_
#define CHROME_BROWSER_SEARCH_NTP_CUSTOM_BACKGROUND_ENABLED_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace policy {
class PolicyMap;
}  // namespace policy

// Handles the |policy::key::kNTPCustomBackgroundEnabled| policy. If set to
// false, clears the |prefs::kNtpCustomBackgroundDict| dictionary pref.
class NtpCustomBackgroundEnabledPolicyHandler
    : public policy::TypeCheckingPolicyHandler {
 public:
  NtpCustomBackgroundEnabledPolicyHandler();

  NtpCustomBackgroundEnabledPolicyHandler(
      const NtpCustomBackgroundEnabledPolicyHandler&) = delete;
  NtpCustomBackgroundEnabledPolicyHandler& operator=(
      const NtpCustomBackgroundEnabledPolicyHandler&) = delete;

  ~NtpCustomBackgroundEnabledPolicyHandler() override;

 protected:
  // ConfigurationPolicyHandler:
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

#endif  // CHROME_BROWSER_SEARCH_NTP_CUSTOM_BACKGROUND_ENABLED_POLICY_HANDLER_H_
