// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/power_battery_charging_optimization_policy_handler.h"

#include "ash/constants/ash_pref_names.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/values.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace policy {

PowerBatteryChargingOptimizationPolicyHandler::
    PowerBatteryChargingOptimizationPolicyHandler()
    : TypeCheckingPolicyHandler(key::kDevicePowerBatteryChargingOptimization,
                                base::Value::Type::INTEGER) {}

PowerBatteryChargingOptimizationPolicyHandler::
    ~PowerBatteryChargingOptimizationPolicyHandler() = default;

bool PowerBatteryChargingOptimizationPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  // First, check if the policy has the correct type (integer).
  if (!TypeCheckingPolicyHandler::CheckPolicySettings(policies, errors)) {
    return false;
  }

  // Then, check if the integer falls within the valid range (1-3).
  const auto* charging_optimization_policy_value = policies.GetValue(
      key::kDevicePowerBatteryChargingOptimization, base::Value::Type::INTEGER);
  if (!charging_optimization_policy_value) {
    return true;  // Policy not set, so it's valid.
  }

  if (charging_optimization_policy_value->is_int()) {
    int charging_optimization_value =
        charging_optimization_policy_value->GetInt();
    if (charging_optimization_value >= kMinPolicyValue &&
        charging_optimization_value <= kMaxPolicyValue) {
      return true;
    }
  }

  return false;
}

void PowerBatteryChargingOptimizationPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* charging_optimization_policy_value = policies.GetValue(
      key::kDevicePowerBatteryChargingOptimization, base::Value::Type::INTEGER);

  if (charging_optimization_policy_value != nullptr &&
      charging_optimization_policy_value->is_int()) {
    int charging_optimization_value =
        charging_optimization_policy_value->GetInt();
    switch (charging_optimization_value) {
      case ChargingOptimizationMode::kStandard:
        prefs->SetBoolean(ash::prefs::kPowerAdaptiveChargingEnabled, false);
        prefs->SetBoolean(ash::prefs::kPowerChargeLimitEnabled, false);
        break;
      case ChargingOptimizationMode::kAdaptive:
        prefs->SetBoolean(ash::prefs::kPowerAdaptiveChargingEnabled, true);
        prefs->SetBoolean(ash::prefs::kPowerChargeLimitEnabled, false);
        break;
      case ChargingOptimizationMode::kLimited:
        prefs->SetBoolean(ash::prefs::kPowerAdaptiveChargingEnabled, false);
        prefs->SetBoolean(ash::prefs::kPowerChargeLimitEnabled, true);
        break;
      default:
        LOG(WARNING) << "Unknown value for "
                     << key::kDevicePowerBatteryChargingOptimization
                     << ". Setting kPowerAdaptiveChargingEnabled to true and "
                     << "kPowerChargeLimitEnabled to false.";
        prefs->SetBoolean(ash::prefs::kPowerAdaptiveChargingEnabled, true);
        prefs->SetBoolean(ash::prefs::kPowerChargeLimitEnabled, false);
        break;
    }
  }
}

}  // namespace policy
