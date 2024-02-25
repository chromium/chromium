// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/geolocation_access_level.h"
#include "chrome/browser/ash/policy/handlers/configuration_policy_handler_ash.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

// TODO(b/324421097): Migrate these tests to its `pref_mapping/*.json` file.
class PrivacyHubPolicyTest
    : public PolicyTest,
      public testing::WithParamInterface<std::optional<int>> {};

IN_PROC_BROWSER_TEST_F(PrivacyHubPolicyTest, CheckDefault) {
  const PrefService* const prefs = browser()->profile()->GetPrefs();
  EXPECT_FALSE(
      prefs->IsManagedPreference(ash::prefs::kUserGeolocationAccessLevel));
  EXPECT_EQ(static_cast<int>(ash::GeolocationAccessLevel::kAllowed),
            prefs->GetInteger(ash::prefs::kUserGeolocationAccessLevel));
  EXPECT_EQ(ash::GeolocationAccessLevel::kAllowed,
            ash::SimpleGeolocationProvider::GetInstance()
                ->GetGeolocationAccessLevel());
}

IN_PROC_BROWSER_TEST_P(PrivacyHubPolicyTest, CheckPolicyToPrefMapping) {
  base::Value test_policy_value =
      GetParam() ? base::Value(*GetParam()) : base::Value();

  PolicyMap policies;
  policies.Set(key::kGoogleLocationServicesEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               test_policy_value.Clone(), nullptr);
  UpdateProviderPolicy(policies);

  const PrefService* const prefs = browser()->profile()->GetPrefs();

  if (test_policy_value.is_none()) {
    EXPECT_FALSE(
        prefs->IsManagedPreference(ash::prefs::kUserGeolocationAccessLevel));
    EXPECT_EQ(static_cast<int>(ash::GeolocationAccessLevel::kAllowed),
              prefs->GetInteger(ash::prefs::kUserGeolocationAccessLevel));
  } else {
    EXPECT_TRUE(
        prefs->IsManagedPreference(ash::prefs::kUserGeolocationAccessLevel));
    EXPECT_EQ(test_policy_value.GetInt(),
              prefs->GetInteger(ash::prefs::kUserGeolocationAccessLevel));
  }
}

INSTANTIATE_TEST_SUITE_P(
    AllPolicyValues,
    PrivacyHubPolicyTest,
    testing::Values(
        static_cast<int>(ash::GeolocationAccessLevel::kDisallowed),
        static_cast<int>(ash::GeolocationAccessLevel::kAllowed),
        static_cast<int>(ash::GeolocationAccessLevel::kOnlyAllowedForSystem),
        std::optional<int>()));

}  // namespace policy
