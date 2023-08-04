// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_INSTALLER_POLICY_HANDLER_H_
#define CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_INSTALLER_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace bruschetta {

// BruschettaInstallerPolicyHandler is responsible for mapping the
// BruschettaInstallerConfiguration enterprise policy into chrome preferences.
//
// For concrete examples, see
// //components/policy/test/data/policy_test_cases.json
class BruschettaInstallerPolicyHandler
    : public policy::SimpleSchemaValidatingPolicyHandler {
 public:
  explicit BruschettaInstallerPolicyHandler(policy::Schema schema);
  ~BruschettaInstallerPolicyHandler() override;

  // ConfigurationPolicyHandler:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace bruschetta

#endif  // CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_INSTALLER_POLICY_HANDLER_H_
