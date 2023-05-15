// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_HANDLERS_APP_LAUNCH_AUTOMATION_POLICY_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_HANDLERS_APP_LAUNCH_AUTOMATION_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {

class Schema;

// This class observes the user setting `AppLaunchAutomation` which allows
// administrators to configure automation for launching apps.
class AppLaunchAutomationPolicyHandler : public SchemaValidatingPolicyHandler {
 public:
  explicit AppLaunchAutomationPolicyHandler(const Schema& chrome_schema);

  AppLaunchAutomationPolicyHandler(const AppLaunchAutomationPolicyHandler&) =
      delete;
  AppLaunchAutomationPolicyHandler& operator=(
      const AppLaunchAutomationPolicyHandler&) = delete;

  ~AppLaunchAutomationPolicyHandler() override;

  // SchemaValidatingPolicyHandler:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_HANDLERS_APP_LAUNCH_AUTOMATION_POLICY_HANDLER_H_
