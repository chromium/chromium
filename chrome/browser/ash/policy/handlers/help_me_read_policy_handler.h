// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_HANDLERS_HELP_ME_READ_POLICY_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_HANDLERS_HELP_ME_READ_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace policy {

class PolicyMap;

// Handles HelpMeReadSettings policy, interprets it into prefs that controls
// feature availability and whether feedback is allowed.
class HelpMeReadPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  HelpMeReadPolicyHandler();
  HelpMeReadPolicyHandler(const HelpMeReadPolicyHandler&) = delete;
  HelpMeReadPolicyHandler& operator=(const HelpMeReadPolicyHandler&) = delete;
  ~HelpMeReadPolicyHandler() override = default;

  // ConfigurationPolicyHandler:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_HANDLERS_HELP_ME_READ_POLICY_HANDLER_H_
