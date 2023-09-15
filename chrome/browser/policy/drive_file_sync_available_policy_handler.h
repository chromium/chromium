// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DRIVE_FILE_SYNC_AVAILABLE_POLICY_HANDLER_H_
#define CHROME_BROWSER_POLICY_DRIVE_FILE_SYNC_AVAILABLE_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {

class PolicyErrorMap;
class PolicyMap;

// Handles the DriveFileSyncAvailable policy.
class DriveFileSyncAvailablePolicyHandler : public TypeCheckingPolicyHandler {
 public:
  DriveFileSyncAvailablePolicyHandler();
  DriveFileSyncAvailablePolicyHandler(
      const DriveFileSyncAvailablePolicyHandler&) = delete;
  DriveFileSyncAvailablePolicyHandler& operator=(
      const DriveFileSyncAvailablePolicyHandler&) = delete;
  ~DriveFileSyncAvailablePolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DRIVE_FILE_SYNC_AVAILABLE_POLICY_HANDLER_H_
