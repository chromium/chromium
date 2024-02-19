// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVTOOLS_GEN_AI_POLICY_HANDLER_H_
#define CHROME_BROWSER_POLICY_DEVTOOLS_GEN_AI_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace policy {

class PolicyMap;

class DevtoolsGenAiPolicyHandler : public IntRangePolicyHandlerBase {
 public:
  DevtoolsGenAiPolicyHandler();
  DevtoolsGenAiPolicyHandler(const DevtoolsGenAiPolicyHandler&) = delete;
  DevtoolsGenAiPolicyHandler& operator=(const DevtoolsGenAiPolicyHandler&) =
      delete;
  ~DevtoolsGenAiPolicyHandler() override;

  // IntRangePolicyHandlerBase:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVTOOLS_GEN_AI_POLICY_HANDLER_H_
