// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_pref_names.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace policy {

class PowerSoundsPolicyTest : public PolicyTest {};

IN_PROC_BROWSER_TEST_F(PowerSoundsPolicyTest, ChargingSoundsEnabled) {
  // Verify the pref behavior before policy.
  PrefService* prefs = g_browser_process->local_state();
  EXPECT_FALSE(prefs->IsManagedPreference(ash::prefs::kChargingSoundsEnabled));
  EXPECT_FALSE(prefs->GetBoolean(ash::prefs::kChargingSoundsEnabled));
  prefs->SetBoolean(ash::prefs::kChargingSoundsEnabled, true);
  EXPECT_TRUE(prefs->GetBoolean(ash::prefs::kChargingSoundsEnabled));

  // Verify the pref can be force disabled.
  PolicyMap policies;
  policies.Set(key::kDeviceChargingSoundsEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD, base::Value(false),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(prefs->IsManagedPreference(ash::prefs::kChargingSoundsEnabled));
  EXPECT_FALSE(prefs->GetBoolean(ash::prefs::kChargingSoundsEnabled));
  prefs->SetBoolean(ash::prefs::kChargingSoundsEnabled, true);
  EXPECT_FALSE(prefs->GetBoolean(ash::prefs::kChargingSoundsEnabled));

  // Verify the pref can be force enabled.
  policies.Set(key::kDeviceChargingSoundsEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD, base::Value(true),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(prefs->IsManagedPreference(ash::prefs::kChargingSoundsEnabled));
  EXPECT_TRUE(prefs->GetBoolean(ash::prefs::kChargingSoundsEnabled));
  prefs->SetBoolean(ash::prefs::kChargingSoundsEnabled, false);
  EXPECT_TRUE(prefs->GetBoolean(ash::prefs::kChargingSoundsEnabled));
}

IN_PROC_BROWSER_TEST_F(PowerSoundsPolicyTest, LowBatterySoundEnabled) {
  // Verify the pref behavior before policy.
  PrefService* prefs = g_browser_process->local_state();
  EXPECT_FALSE(prefs->IsManagedPreference(ash::prefs::kLowBatterySoundEnabled));
  EXPECT_FALSE(prefs->GetBoolean(ash::prefs::kLowBatterySoundEnabled));
  prefs->SetBoolean(ash::prefs::kLowBatterySoundEnabled, true);
  EXPECT_TRUE(prefs->GetBoolean(ash::prefs::kLowBatterySoundEnabled));

  // Verify the pref can be force disabled.
  PolicyMap policies;
  policies.Set(key::kDeviceLowBatterySoundEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD, base::Value(false),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(prefs->IsManagedPreference(ash::prefs::kLowBatterySoundEnabled));
  EXPECT_FALSE(prefs->GetBoolean(ash::prefs::kLowBatterySoundEnabled));
  prefs->SetBoolean(ash::prefs::kLowBatterySoundEnabled, true);
  EXPECT_FALSE(prefs->GetBoolean(ash::prefs::kLowBatterySoundEnabled));

  // Verify the pref can be force enabled.
  policies.Set(key::kDeviceLowBatterySoundEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD, base::Value(true),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(prefs->IsManagedPreference(ash::prefs::kLowBatterySoundEnabled));
  EXPECT_TRUE(prefs->GetBoolean(ash::prefs::kLowBatterySoundEnabled));
  prefs->SetBoolean(ash::prefs::kLowBatterySoundEnabled, false);
  EXPECT_TRUE(prefs->GetBoolean(ash::prefs::kLowBatterySoundEnabled));
}

}  // namespace policy
