// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/battery_saver_policy_handler.h"

#include "base/numerics/safe_conversions.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_pref_names.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace policy {

using performance_manager::user_tuning::prefs::BatterySaverModeState;

BatterySaverPolicyHandler::BatterySaverPolicyHandler()
    : TypeCheckingPolicyHandler(key::kBatterySaverModeAvailability,
                                base::Value::Type::INTEGER) {}

void BatterySaverPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                    PrefValueMap* prefs) {
  const base::Value* value = policies.GetValue(
      key::kBatterySaverModeAvailability, base::Value::Type::INTEGER);
  if (!value) {
    return;
  }
  switch (value->GetInt()) {
    case base::strict_cast<int>(BatterySaverModeState::kDisabled):
#if BUILDFLAG(IS_CHROMEOS_ASH)
      prefs->SetBoolean(ash::prefs::kPowerBatterySaver, false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

      prefs->SetInteger(
          performance_manager::user_tuning::prefs::kBatterySaverModeState,
          base::strict_cast<int>(BatterySaverModeState::kDisabled));
      break;

    case base::strict_cast<int>(BatterySaverModeState::kEnabledBelowThreshold):
    case base::strict_cast<int>(BatterySaverModeState::kEnabledOnBattery):
      // kEnabledOnBattery as a policy value is deprecated and treated as
      // kEnabledBelowThreshold to unify policy with ChromeOS Battery Saver
      // which does not support it.
      prefs->SetInteger(
          performance_manager::user_tuning::prefs::kBatterySaverModeState,
          base::strict_cast<int>(
              BatterySaverModeState::kEnabledBelowThreshold));

      // kEnabledBelowThreshold is the default behavior of ChromeOS Battery
      // Saver, so we don't need apply any policy to
      // ash::prefs::kPowerBatterySaver.
      break;
  }
}

}  // namespace policy
