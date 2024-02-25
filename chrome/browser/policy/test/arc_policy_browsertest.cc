// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/session/arc_session_runner.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "ash/constants/ash_features.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/policy/handlers/configuration_policy_handler_ash.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace policy {

class ArcPolicyTest : public PolicyTest {
 public:
  ArcPolicyTest() {
    feature_list_.InitAndDisableFeature(ash::features::kCrosPrivacyHub);
  }
  ArcPolicyTest(const ArcPolicyTest&) = delete;
  ArcPolicyTest& operator=(const ArcPolicyTest&) = delete;
  ~ArcPolicyTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();
    arc::ArcSessionManager::SetUiEnabledForTesting(false);
    arc::ArcSessionManager::Get()->SetArcSessionRunnerForTesting(
        std::make_unique<arc::ArcSessionRunner>(
            base::BindRepeating(arc::FakeArcSession::Create)));

    browser()->profile()->GetPrefs()->SetBoolean(arc::prefs::kArcSignedIn,
                                                 true);
    browser()->profile()->GetPrefs()->SetBoolean(arc::prefs::kArcTermsAccepted,
                                                 true);
  }

  void TearDownOnMainThread() override {
    arc::ArcSessionManager::Get()->Shutdown();
    PolicyTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PolicyTest::SetUpCommandLine(command_line);
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetArcEnabledByPolicy(bool enabled) {
    PolicyMap policies;
    policies.Set(key::kArcEnabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(enabled), nullptr);
    UpdateProviderPolicy(policies);
    if (browser()) {
      const PrefService* const prefs = browser()->profile()->GetPrefs();
      EXPECT_EQ(prefs->GetBoolean(arc::prefs::kArcEnabled), enabled);
    }
  }

  base::test::ScopedFeatureList feature_list_;
};

// Test ArcEnabled policy.
IN_PROC_BROWSER_TEST_F(ArcPolicyTest, ArcEnabled) {
  const PrefService* const pref = browser()->profile()->GetPrefs();
  const auto* const arc_session_manager = arc::ArcSessionManager::Get();

  // ARC is switched off by default.
  EXPECT_FALSE(pref->GetBoolean(arc::prefs::kArcEnabled));
  EXPECT_FALSE(arc_session_manager->enable_requested());

  // Enable ARC.
  SetArcEnabledByPolicy(true);
  EXPECT_TRUE(arc_session_manager->enable_requested());

  // Disable ARC.
  SetArcEnabledByPolicy(false);
  EXPECT_FALSE(arc_session_manager->enable_requested());
}

// Test ArcBackupRestoreServiceEnabled policy.
IN_PROC_BROWSER_TEST_F(ArcPolicyTest, ArcBackupRestoreServiceEnabled) {
  PrefService* const pref = browser()->profile()->GetPrefs();

  // Enable ARC backup and restore in user prefs.
  pref->SetBoolean(arc::prefs::kArcBackupRestoreEnabled, true);

  // ARC backup and restore is disabled by policy by default.
  UpdateProviderPolicy(PolicyMap());
  EXPECT_FALSE(pref->GetBoolean(arc::prefs::kArcBackupRestoreEnabled));
  EXPECT_TRUE(pref->IsManagedPreference(arc::prefs::kArcBackupRestoreEnabled));

  // Set ARC backup and restore to user control via policy.
  PolicyMap policies;
  policies.Set(
      key::kArcBackupRestoreServiceEnabled, POLICY_LEVEL_MANDATORY,
      POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
      base::Value(static_cast<int>(ArcServicePolicyValue::kUnderUserControl)),
      nullptr);
  UpdateProviderPolicy(policies);

  // User choice should be honored now.
  EXPECT_TRUE(pref->GetBoolean(arc::prefs::kArcBackupRestoreEnabled));
  EXPECT_FALSE(pref->IsManagedPreference(arc::prefs::kArcBackupRestoreEnabled));
  pref->SetBoolean(arc::prefs::kArcBackupRestoreEnabled, false);
  EXPECT_FALSE(pref->GetBoolean(arc::prefs::kArcBackupRestoreEnabled));
  pref->SetBoolean(arc::prefs::kArcBackupRestoreEnabled, true);
  EXPECT_TRUE(pref->GetBoolean(arc::prefs::kArcBackupRestoreEnabled));

  // Set ARC backup and restore to disabled via policy.
  policies.Set(key::kArcBackupRestoreServiceEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(static_cast<int>(ArcServicePolicyValue::kDisabled)),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(pref->GetBoolean(arc::prefs::kArcBackupRestoreEnabled));
  EXPECT_TRUE(pref->IsManagedPreference(arc::prefs::kArcBackupRestoreEnabled));

  // Disable ARC backup and restore in user prefs.
  pref->SetBoolean(arc::prefs::kArcBackupRestoreEnabled, true);

  // Set ARC backup and restore to enabled via policy.
  policies.Set(key::kArcBackupRestoreServiceEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(static_cast<int>(ArcServicePolicyValue::kEnabled)),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(pref->GetBoolean(arc::prefs::kArcBackupRestoreEnabled));
  EXPECT_TRUE(pref->IsManagedPreference(arc::prefs::kArcBackupRestoreEnabled));
}

// Test ArcGoogleLocationServicesEnabled policy and its interplay with the
// DefaultGeolocationSetting policy.
IN_PROC_BROWSER_TEST_F(ArcPolicyTest, ArcGoogleLocationServicesEnabled) {
  ASSERT_FALSE(ash::features::IsCrosPrivacyHubLocationEnabled());
  PrefService* const pref = browser()->profile()->GetPrefs();

  // Values of the ArcGoogleLocationServicesEnabled policy to be tested.
  auto test_policy_values =
      base::Value::List()
          .Append(base::Value())
          .Append(static_cast<int>(ArcServicePolicyValue::kDisabled))
          .Append(static_cast<int>(ArcServicePolicyValue::kUnderUserControl))
          .Append(static_cast<int>(ArcServicePolicyValue::kEnabled));

  // Values of the DefaultGeolocationSetting policy to be tested.
  auto test_default_geo_policy_values = base::Value::List()
                                            .Append(base::Value())  // unset
                                            .Append(1)   // 'AllowGeolocation'
                                            .Append(2)   // 'BlockGeolocation'
                                            .Append(3);  // 'AskGeolocation'

  // Switch on the pref in user prefs.
  pref->SetBoolean(arc::prefs::kArcLocationServiceEnabled, true);

  // The pref is overridden to disabled by policy by default.
  UpdateProviderPolicy(PolicyMap());
  EXPECT_FALSE(pref->GetBoolean(arc::prefs::kArcLocationServiceEnabled));
  EXPECT_TRUE(
      pref->IsManagedPreference(arc::prefs::kArcLocationServiceEnabled));

  for (const auto& test_policy_value : test_policy_values) {
    for (const auto& test_default_geo_policy_value :
         test_default_geo_policy_values) {
      PolicyMap policies;
      if (test_policy_value.is_int()) {
        policies.Set(key::kArcGoogleLocationServicesEnabled,
                     POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                     POLICY_SOURCE_CLOUD, test_policy_value.Clone(), nullptr);
      }
      if (test_default_geo_policy_value.is_int()) {
        policies.Set(key::kDefaultGeolocationSetting, POLICY_LEVEL_MANDATORY,
                     POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                     test_default_geo_policy_value.Clone(), nullptr);
      }
      UpdateProviderPolicy(policies);

      const bool should_be_disabled_by_policy =
          test_policy_value.is_none() ||
          (test_policy_value.GetInt() ==
           static_cast<int>(ArcServicePolicyValue::kDisabled));
      const bool should_be_enabled_by_policy =
          test_policy_value.is_int() &&
          test_policy_value.GetInt() ==
              static_cast<int>(ArcServicePolicyValue::kEnabled);
      const bool should_be_disabled_by_default_geo_policy =
          test_default_geo_policy_value.is_int() &&
          test_default_geo_policy_value.GetInt() == 2;
      const bool expected_pref_value =
          !(should_be_disabled_by_policy ||
            should_be_disabled_by_default_geo_policy);
      EXPECT_EQ(expected_pref_value,
                pref->GetBoolean(arc::prefs::kArcLocationServiceEnabled))
          << "ArcGoogleLocationServicesEnabled policy is set to "
          << test_policy_value << "DefaultGeolocationSetting policy is set to "
          << test_default_geo_policy_value;

      const bool expected_pref_managed =
          should_be_disabled_by_policy || should_be_enabled_by_policy ||
          should_be_disabled_by_default_geo_policy;
      EXPECT_EQ(
          expected_pref_managed,
          pref->IsManagedPreference(arc::prefs::kArcLocationServiceEnabled))
          << "ArcGoogleLocationServicesEnabled policy is set to "
          << test_policy_value << "DefaultGeolocationSetting policy is set to "
          << test_default_geo_policy_value;
    }
  }
}

class ArcLocationPolicyWhenPhEnabledTest
    : public ArcPolicyTest,
      public testing::WithParamInterface<ArcServicePolicyValue> {
 public:
  ArcLocationPolicyWhenPhEnabledTest() {
    feature_list_.Reset();
    feature_list_.InitAndEnableFeature(ash::features::kCrosPrivacyHub);
  }

  void SetArcLocationPolicy(ArcServicePolicyValue value) {
    PolicyMap policies;
    policies.Set(key::kArcGoogleLocationServicesEnabled, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::Value(static_cast<int>(value)), nullptr);
    UpdateProviderPolicy(policies);
  }
};

IN_PROC_BROWSER_TEST_P(ArcLocationPolicyWhenPhEnabledTest,
                       SetArcLocationPolicy) {
  ASSERT_TRUE(ash::features::IsCrosPrivacyHubLocationEnabled());
  SetArcLocationPolicy(GetParam());

  // Check that `ArcGoogleLocationServicesEnabled` policy, as it's being
  // deprecated, no longer affects the underlying pref.
  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_TRUE(prefs->FindPreference(arc::prefs::kArcLocationServiceEnabled)
                  ->IsDefaultValue());
  EXPECT_FALSE(
      prefs->IsManagedPreference(arc::prefs::kArcLocationServiceEnabled));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ArcLocationPolicyWhenPhEnabledTest,
    testing::Values(ArcServicePolicyValue::kDisabled,
                    ArcServicePolicyValue::kUnderUserControl,
                    ArcServicePolicyValue::kEnabled));

}  // namespace policy
