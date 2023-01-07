// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_RESTORE_ON_STARTUP_POLICY_HANDLER_H_
#define CHROME_BROWSER_SESSIONS_RESTORE_ON_STARTUP_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace policy {

class PolicyErrorMap;
class PolicyMap;

// Handles RestoreOnStartup policy.
class RestoreOnStartupPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  RestoreOnStartupPolicyHandler();

  RestoreOnStartupPolicyHandler(const RestoreOnStartupPolicyHandler&) = delete;
  RestoreOnStartupPolicyHandler& operator=(
      const RestoreOnStartupPolicyHandler&) = delete;

  ~RestoreOnStartupPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_SESSIONS_RESTORE_ON_STARTUP_POLICY_HANDLER_H_
