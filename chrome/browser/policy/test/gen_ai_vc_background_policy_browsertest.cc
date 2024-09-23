// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "chrome/browser/ash/policy/handlers/configuration_policy_handler_ash.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

using GenAIVcBackgroundPolicyTest = PolicyTest;

IN_PROC_BROWSER_TEST_F(GenAIVcBackgroundPolicyTest,
                       EnableFeatureIfGenAIVcBackgroundPolicyUnset) {
  Profile* profile = browser()->profile();

  profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  UpdateProviderPolicy(PolicyMap());

  EXPECT_TRUE(
      ash::personalization_app::IsManagedSeaPenVcBackgroundEnabled(profile));
  EXPECT_FALSE(
      ash::personalization_app::IsManagedSeaPenVcBackgroundFeedbackEnabled(
          profile));
}
IN_PROC_BROWSER_TEST_F(GenAIVcBackgroundPolicyTest,
                       EnableFeatureIfGenAIVcBackgroundPolicyEnabled) {
  Profile* profile = browser()->profile();
  PolicyMap policies;

  profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  policies.Set(key::kGenAIVcBackgroundSettings, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(0), nullptr);
  UpdateProviderPolicy(policies);

  EXPECT_TRUE(profile->GetPrefs()->IsManagedPreference(
      ash::prefs::kGenAIVcBackgroundSettings));
  EXPECT_EQ(
      profile->GetPrefs()->GetInteger(ash::prefs::kGenAIVcBackgroundSettings),
      static_cast<int>(
          ash::personalization_app::ManagedSeaPenSettings::kAllowed));
  EXPECT_TRUE(
      ash::personalization_app::IsManagedSeaPenVcBackgroundEnabled(profile));
  EXPECT_TRUE(
      ash::personalization_app::IsManagedSeaPenVcBackgroundFeedbackEnabled(
          profile));
}

IN_PROC_BROWSER_TEST_F(
    GenAIVcBackgroundPolicyTest,
    EnableFeatureIfGenAIVcBackgroundPolicyEnabledWithoutLogging) {
  Profile* profile = browser()->profile();
  PolicyMap policies;

  profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  policies.Set(key::kGenAIVcBackgroundSettings, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(1), nullptr);
  UpdateProviderPolicy(policies);

  EXPECT_TRUE(profile->GetPrefs()->IsManagedPreference(
      ash::prefs::kGenAIVcBackgroundSettings));
  EXPECT_EQ(
      profile->GetPrefs()->GetInteger(ash::prefs::kGenAIVcBackgroundSettings),
      static_cast<int>(ash::personalization_app::ManagedSeaPenSettings::
                           kAllowedWithoutLogging));
  EXPECT_TRUE(
      ash::personalization_app::IsManagedSeaPenVcBackgroundEnabled(profile));
  EXPECT_FALSE(
      ash::personalization_app::IsManagedSeaPenVcBackgroundFeedbackEnabled(
          profile));
}

IN_PROC_BROWSER_TEST_F(GenAIVcBackgroundPolicyTest,
                       DisableFeatureIfGenAIVcBackgroundPolicyDisabled) {
  Profile* profile = browser()->profile();
  PolicyMap policies;

  profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  policies.Set(key::kGenAIVcBackgroundSettings, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(2), nullptr);
  UpdateProviderPolicy(policies);

  EXPECT_TRUE(profile->GetPrefs()->IsManagedPreference(
      ash::prefs::kGenAIVcBackgroundSettings));
  EXPECT_EQ(
      profile->GetPrefs()->GetInteger(ash::prefs::kGenAIVcBackgroundSettings),
      static_cast<int>(
          ash::personalization_app::ManagedSeaPenSettings::kDisabled));
  EXPECT_FALSE(
      ash::personalization_app::IsManagedSeaPenVcBackgroundEnabled(profile));
  EXPECT_FALSE(
      ash::personalization_app::IsManagedSeaPenVcBackgroundFeedbackEnabled(
          profile));
}

}  // namespace policy
