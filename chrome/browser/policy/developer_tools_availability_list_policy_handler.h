// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVELOPER_TOOLS_AVAILABILITY_LIST_POLICY_HANDLER_H_
#define CHROME_BROWSER_POLICY_DEVELOPER_TOOLS_AVAILABILITY_LIST_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {

// Handles the DeveloperToolsAvailabilityAllowlist and
// DeveloperToolsAvailabilityBlocklist policies. This is a parent class for the
// two handlers.
class DeveloperToolsAvailabilityListPolicyHandler
    : public TypeCheckingPolicyHandler {
 public:
  DeveloperToolsAvailabilityListPolicyHandler(const char* policy_name,
                                              const char* pref_path);
  DeveloperToolsAvailabilityListPolicyHandler(
      const DeveloperToolsAvailabilityListPolicyHandler&) = delete;
  DeveloperToolsAvailabilityListPolicyHandler& operator=(
      const DeveloperToolsAvailabilityListPolicyHandler&) = delete;
  ~DeveloperToolsAvailabilityListPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 protected:
  bool ValidatePolicy(const std::string& url_pattern);

 private:
  const char* pref_path_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVELOPER_TOOLS_AVAILABILITY_LIST_POLICY_HANDLER_H_
