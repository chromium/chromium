// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_AUTO_SLEEP_DEVICE_WEEKLY_SCHEDULED_SUSPEND_POLICY_HANDLER_H_
#define CHROME_BROWSER_ASH_APP_MODE_AUTO_SLEEP_DEVICE_WEEKLY_SCHEDULED_SUSPEND_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefRegistrySimple;

namespace policy {

class PolicyMap;

// Handles the `DeviceWeeklyScheduledSuspend` policy. Enforces the policy schema
// and validates that the schedule has valid entries without overlaps. Valid
// schedules are mapped to the `kDeviceWeeklyScheduledSuspend` pref.
class DeviceWeeklyScheduledSuspendPolicyHandler
    : public SchemaValidatingPolicyHandler {
 public:
  explicit DeviceWeeklyScheduledSuspendPolicyHandler(
      const policy::Schema& chrome_schema);
  DeviceWeeklyScheduledSuspendPolicyHandler(
      const DeviceWeeklyScheduledSuspendPolicyHandler&) = delete;
  DeviceWeeklyScheduledSuspendPolicyHandler& operator=(
      const DeviceWeeklyScheduledSuspendPolicyHandler&) = delete;
  ~DeviceWeeklyScheduledSuspendPolicyHandler() override;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // SchemaValidatingPolicyHandler:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_APP_MODE_AUTO_SLEEP_DEVICE_WEEKLY_SCHEDULED_SUSPEND_POLICY_HANDLER_H_
