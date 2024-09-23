// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_EXTENSION_DEVELOPER_MODE_POLICY_HANDLER_H_
#define CHROME_BROWSER_POLICY_EXTENSION_DEVELOPER_MODE_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {

// Handles the ExtensionDeveloperModePolicySettings and controls the managed
// value of kExtensionsUIDeveloperMode.
class ExtensionDeveloperModePolicyHandler : public IntRangePolicyHandlerBase {
 public:
  ExtensionDeveloperModePolicyHandler();

  ExtensionDeveloperModePolicyHandler(
      const ExtensionDeveloperModePolicyHandler&) = delete;
  ExtensionDeveloperModePolicyHandler& operator=(
      const ExtensionDeveloperModePolicyHandler&) = delete;
  ~ExtensionDeveloperModePolicyHandler() override;

  // Returns true if the policy is set to a valid value.
  bool IsValidPolicySet(const PolicyMap& policies);

 protected:
  // IntRangePolicyHandlerBase methods:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_EXTENSION_DEVELOPER_MODE_POLICY_HANDLER_H_
