// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_POWER_BATTERY_CHARGING_OPTIMIZATION_POLICY_HANDLER_H_
#define CHROME_BROWSER_POLICY_POWER_BATTERY_CHARGING_OPTIMIZATION_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/common/policy_map.h"
#include "components/prefs/pref_value_map.h"

class PrefValueMap;

namespace policy {
class PolicyMap;

// This policy handler handles the DevicePowerBatteryChargingOptimization
// policy which is an enum ranging from 1 to 3.
class PowerBatteryChargingOptimizationPolicyHandler
    : public TypeCheckingPolicyHandler {
 public:
  PowerBatteryChargingOptimizationPolicyHandler();
  ~PowerBatteryChargingOptimizationPolicyHandler() override;

  // ConfigurationPolicyHandler:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  // The minimum and maximum allowed values for the policy.
  static constexpr int kMinPolicyValue = 1;
  static constexpr int kMaxPolicyValue = 3;

  // Enum for different charging optimization modes.
  enum ChargingOptimizationMode {
    kStandard = 1,
    kAdaptive = 2,
    kLimited = 3,
  };
};

}  // namespace policy
#endif  // CHROME_BROWSER_POLICY_POWER_BATTERY_CHARGING_OPTIMIZATION_POLICY_HANDLER_H_
