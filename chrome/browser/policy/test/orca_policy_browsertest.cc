// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "chrome/browser/ash/input_method/editor_mediator.h"
#include "chrome/browser/ash/policy/handlers/configuration_policy_handler_ash.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class OrcaPolicyTest : public PolicyTest {
 public:
  OrcaPolicyTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kOrca,
                              chromeos::features::kFeatureManagementOrca,
                              ash::features::kOrcaControlledByPolicy},
        // TODO: b:329215512: Remove the OrcaUseAccountCapabilities from the
        // disable list.
        /*disabled_features=*/{chromeos::features::kOrcaDogfood,
                               ash::features::kOrcaUseAccountCapabilities});
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(OrcaPolicyTest,
                       DisableOrcaOnManagedProfilesIfOrcaPolicyUnset) {
  Profile* profile = browser()->profile();
  ash::input_method::EditorMediator editor_mediator(profile, "au");

  profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);

  EXPECT_FALSE(profile->GetPrefs()->IsManagedPreference(
      ash::prefs::kManagedOrcaEnabled));
  EXPECT_FALSE(editor_mediator.IsAllowedForUse());
}

IN_PROC_BROWSER_TEST_F(OrcaPolicyTest,
                       EnableOrcaOnManagedProfilesIfOrcaPolicyEnabled) {
  Profile* profile = browser()->profile();
  ash::input_method::EditorMediator editor_mediator(profile, "au");
  PolicyMap policies;

  profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  policies.Set(key::kOrcaEnabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  UpdateProviderPolicy(policies);

  EXPECT_TRUE(profile->GetPrefs()->IsManagedPreference(
      ash::prefs::kManagedOrcaEnabled));
  EXPECT_TRUE(profile->GetPrefs()->GetBoolean(ash::prefs::kManagedOrcaEnabled));
  EXPECT_TRUE(editor_mediator.IsAllowedForUse());
}

IN_PROC_BROWSER_TEST_F(OrcaPolicyTest,
                       DisableOrcaOnManagedProfilesIfOrcaPolicyDisabled) {
  Profile* profile = browser()->profile();
  ash::input_method::EditorMediator editor_mediator(profile, "au");
  PolicyMap policies;

  profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  policies.Set(key::kOrcaEnabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  UpdateProviderPolicy(policies);

  EXPECT_TRUE(profile->GetPrefs()->IsManagedPreference(
      ash::prefs::kManagedOrcaEnabled));
  EXPECT_FALSE(
      profile->GetPrefs()->GetBoolean(ash::prefs::kManagedOrcaEnabled));
  EXPECT_FALSE(editor_mediator.IsAllowedForUse());
}

}  // namespace policy
