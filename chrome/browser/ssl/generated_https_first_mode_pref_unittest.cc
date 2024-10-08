// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/generated_https_first_mode_pref.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/settings_private/generated_pref_test_base.h"
#include "chrome/browser/extensions/api/settings_private/generated_prefs_factory.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ssl/https_first_mode_settings_tracker.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace settings_api = extensions::api::settings_private;
namespace settings_private = extensions::settings_private;

constexpr char kEmail[] = "test@example.com";

// The test parameter controls whether the user is signed in.
class GeneratedHttpsFirstModePrefTest : public testing::Test {
 protected:
  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        safe_browsing::AdvancedProtectionStatusManagerFactory::GetInstance(),
        safe_browsing::AdvancedProtectionStatusManagerFactory::
            GetDefaultFactoryForTesting());
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
  }

  void SignIn(bool is_under_advanced_protection) {
    AccountInfo account_info =
        identity_test_env()->MakeAccountAvailable(kEmail);
    account_info.is_under_advanced_protection = is_under_advanced_protection;
    identity_test_env()->SetPrimaryAccount(account_info.email,
                                           signin::ConsentLevel::kSync);
    identity_test_env()->UpdateAccountInfoForAccount(account_info);
  }

  TestingProfile* profile() { return profile_.get(); }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return profile_->GetTestingPrefService();
  }

  content::BrowserTaskEnvironment task_environment_;

 private:
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
};

// Check that enabling/disabling Advanced Protection modifies the generated
// pref. The user is initially signed in (this affects how AP manager notifies
// its observers).
TEST_F(GeneratedHttpsFirstModePrefTest,
       AdvancedProtectionStatusChange_InitiallySignedIn) {
  GeneratedHttpsFirstModePref pref(profile());

  // Check that when source information changes, the pref observer is fired.
  settings_private::TestGeneratedPrefObserver test_observer;
  pref.AddObserver(&test_observer);

  // Sign in, otherwise AP manager won't notify observers of the AP status.
  SignIn(/*is_under_advanced_protection=*/false);

  safe_browsing::AdvancedProtectionStatusManager* aps_manager =
      safe_browsing::AdvancedProtectionStatusManagerFactory::GetForProfile(
          profile());
  EXPECT_EQ(
      static_cast<HttpsFirstModeSetting>(pref.GetPrefObject().value->GetInt()),
      HttpsFirstModeSetting::kDisabled);
  EXPECT_FALSE(*pref.GetPrefObject().user_control_disabled);
  EXPECT_EQ(test_observer.GetUpdatedPrefName(), kGeneratedHttpsFirstModePref);
  test_observer.Reset();

  // Enable Advanced Protection. This should disable changing the pref.
  aps_manager->SetAdvancedProtectionStatusForTesting(true);
  EXPECT_EQ(
      static_cast<HttpsFirstModeSetting>(pref.GetPrefObject().value->GetInt()),
      HttpsFirstModeSetting::kEnabledFull);
  EXPECT_TRUE(*pref.GetPrefObject().user_control_disabled);
  EXPECT_EQ(test_observer.GetUpdatedPrefName(), kGeneratedHttpsFirstModePref);

  aps_manager->UnsubscribeFromSigninEvents();
}

// Similar to AdvancedProtectionStatusChange_InitiallySignedIn but the user is
// initially not signed in.
TEST_F(GeneratedHttpsFirstModePrefTest,
       AdvancedProtectionStatusChange_InitiallyNotSignedIn) {
  GeneratedHttpsFirstModePref pref(profile());

  // Check that when source information changes, the pref observer is fired.
  settings_private::TestGeneratedPrefObserver test_observer;
  pref.AddObserver(&test_observer);

  safe_browsing::AdvancedProtectionStatusManager* aps_manager =
      safe_browsing::AdvancedProtectionStatusManagerFactory::GetForProfile(
          profile());
  EXPECT_EQ(
      static_cast<HttpsFirstModeSetting>(pref.GetPrefObject().value->GetInt()),
      HttpsFirstModeSetting::kDisabled);
  EXPECT_FALSE(*pref.GetPrefObject().user_control_disabled);
  // If the user isn't signed in, AP manager doesn't update the AP status on
  // startup, so the pref doesn't get a notification.
  EXPECT_TRUE(test_observer.GetUpdatedPrefName().empty());
  test_observer.Reset();

  // Enabled Advanced Protection. This should disable changing the pref.
  SignIn(/*is_under_advanced_protection=*/true);
  // aps_manager->SetAdvancedProtectionStatusForTesting(true);
  EXPECT_EQ(
      static_cast<HttpsFirstModeSetting>(pref.GetPrefObject().value->GetInt()),
      HttpsFirstModeSetting::kEnabledFull);
  EXPECT_TRUE(*pref.GetPrefObject().user_control_disabled);
  EXPECT_EQ(test_observer.GetUpdatedPrefName(), kGeneratedHttpsFirstModePref);

  aps_manager->UnsubscribeFromSigninEvents();
}

// Check the generated pref respects updates to the underlying preference.
TEST_F(GeneratedHttpsFirstModePrefTest, UpdatePreference) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kHttpsFirstBalancedMode);

  GeneratedHttpsFirstModePref pref(profile());

  // Check setting the generated pref updates the underlying preference.
  EXPECT_EQ(
      pref.SetPref(std::make_unique<base::Value>(
                       static_cast<int>(HttpsFirstModeSetting::kEnabledFull))
                       .get()),
      settings_private::SetPrefResult::SUCCESS);
  EXPECT_TRUE(prefs()->GetUserPref(prefs::kHttpsOnlyModeEnabled)->GetBool());

  EXPECT_EQ(pref.SetPref(std::make_unique<base::Value>(
                             static_cast<int>(HttpsFirstModeSetting::kDisabled))
                             .get()),
            settings_private::SetPrefResult::SUCCESS);
  EXPECT_FALSE(prefs()->GetUserPref(prefs::kHttpsOnlyModeEnabled)->GetBool());

  // Check that changing the underlying preference correctly updates the
  // generated pref.
  prefs()->SetUserPref(prefs::kHttpsOnlyModeEnabled,
                       std::make_unique<base::Value>(true));
  EXPECT_EQ(
      static_cast<HttpsFirstModeSetting>(pref.GetPrefObject().value->GetInt()),
      HttpsFirstModeSetting::kEnabledFull);

  prefs()->SetUserPref(prefs::kHttpsOnlyModeEnabled,
                       std::make_unique<base::Value>(false));
  EXPECT_EQ(
      static_cast<HttpsFirstModeSetting>(pref.GetPrefObject().value->GetInt()),
      HttpsFirstModeSetting::kDisabled);

  // Confirm that a type mismatch is reported as such.
  EXPECT_EQ(pref.SetPref(std::make_unique<base::Value>(true).get()),
            extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH);

  // Confirm that an integer value outside the enum range is reported as a type
  // mismatch.
  EXPECT_EQ(pref.SetPref(std::make_unique<base::Value>(10).get()),
            extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH);

  // With Balanced Mode feature disabled, check that trying to set the
  // generated pref to kEnabledBalanced fails and the underlying pref remains
  // disabled.
  EXPECT_EQ(pref.SetPref(
                std::make_unique<base::Value>(
                    static_cast<int>(HttpsFirstModeSetting::kEnabledBalanced))
                    .get()),
            settings_private::SetPrefResult::PREF_TYPE_UNSUPPORTED);

  // With Balanced Mode feature disabled, check that setting the underlying
  // Balanced pref to `true` does not change the generated pref from kDisabled.
  prefs()->SetUserPref(prefs::kHttpsFirstBalancedMode,
                       std::make_unique<base::Value>(true));
  EXPECT_EQ(
      static_cast<HttpsFirstModeSetting>(pref.GetPrefObject().value->GetInt()),
      HttpsFirstModeSetting::kDisabled);
}

// Variant of UpdatePreference, but with the Balanced Mode feature flag enabled.
// The full set of Settings are available (Full, Balanced, and Disabled) and
// they should fully control the underlying prefs and vice-versa.
TEST_F(GeneratedHttpsFirstModePrefTest, UpdatePref_HttpsFirstModeNewSettings) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kHttpsFirstBalancedMode);

  GeneratedHttpsFirstModePref pref(profile());

  // Check setting the generated pref updates the underlying preference.
  EXPECT_EQ(
      pref.SetPref(std::make_unique<base::Value>(
                       static_cast<int>(HttpsFirstModeSetting::kEnabledFull))
                       .get()),
      settings_private::SetPrefResult::SUCCESS);
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kHttpsOnlyModeEnabled));
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kHttpsFirstBalancedMode));

  EXPECT_EQ(pref.SetPref(
                std::make_unique<base::Value>(
                    static_cast<int>(HttpsFirstModeSetting::kEnabledBalanced))
                    .get()),
            settings_private::SetPrefResult::SUCCESS);
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kHttpsOnlyModeEnabled));
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kHttpsFirstBalancedMode));

  EXPECT_EQ(pref.SetPref(std::make_unique<base::Value>(
                             static_cast<int>(HttpsFirstModeSetting::kDisabled))
                             .get()),
            settings_private::SetPrefResult::SUCCESS);
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kHttpsOnlyModeEnabled));
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kHttpsFirstBalancedMode));

  // Check that changing the underlying preference correctly updates the
  // generated pref.
  prefs()->SetUserPref(prefs::kHttpsOnlyModeEnabled,
                       std::make_unique<base::Value>(true));
  prefs()->SetUserPref(prefs::kHttpsFirstBalancedMode,
                       std::make_unique<base::Value>(false));
  EXPECT_EQ(
      static_cast<HttpsFirstModeSetting>(pref.GetPrefObject().value->GetInt()),
      HttpsFirstModeSetting::kEnabledFull);

  prefs()->SetUserPref(prefs::kHttpsOnlyModeEnabled,
                       std::make_unique<base::Value>(false));
  prefs()->SetUserPref(prefs::kHttpsFirstBalancedMode,
                       std::make_unique<base::Value>(true));
  EXPECT_EQ(
      static_cast<HttpsFirstModeSetting>(pref.GetPrefObject().value->GetInt()),
      HttpsFirstModeSetting::kEnabledBalanced);

  prefs()->SetUserPref(prefs::kHttpsOnlyModeEnabled,
                       std::make_unique<base::Value>(false));
  prefs()->SetUserPref(prefs::kHttpsFirstBalancedMode,
                       std::make_unique<base::Value>(false));
  EXPECT_EQ(
      static_cast<HttpsFirstModeSetting>(pref.GetPrefObject().value->GetInt()),
      HttpsFirstModeSetting::kDisabled);

  // If (somehow) both prefs are set to True, we should treat that as being
  // fully-enabled.
  prefs()->SetUserPref(prefs::kHttpsOnlyModeEnabled,
                       std::make_unique<base::Value>(true));
  prefs()->SetUserPref(prefs::kHttpsFirstBalancedMode,
                       std::make_unique<base::Value>(true));
  EXPECT_EQ(
      static_cast<HttpsFirstModeSetting>(pref.GetPrefObject().value->GetInt()),
      HttpsFirstModeSetting::kEnabledFull);

  // Confirm that a type mismatch is reported as such.
  EXPECT_EQ(pref.SetPref(std::make_unique<base::Value>(true).get()),
            extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH);

  // Confirm that an integer value outside the enum range is reported as a type
  // mismatch.
  EXPECT_EQ(pref.SetPref(std::make_unique<base::Value>(10).get()),
            extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH);
}

// Check that the management state (e.g. enterprise controlled pref) of the
// underlying preference is applied to the generated preference.
TEST_F(GeneratedHttpsFirstModePrefTest, ManagementState) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kHttpsFirstBalancedMode);

  GeneratedHttpsFirstModePref pref(profile());
  EXPECT_EQ(pref.GetPrefObject().enforcement, settings_api::Enforcement::kNone);
  EXPECT_EQ(pref.GetPrefObject().controlled_by,
            settings_api::ControlledBy::kNone);

  // Set HTTPS-Only Mode with recommended enforcement and check the generated
  // pref.
  prefs()->SetRecommendedPref(prefs::kHttpsOnlyModeEnabled,
                              std::make_unique<base::Value>(true));
  EXPECT_EQ(pref.GetPrefObject().enforcement,
            settings_api::Enforcement::kRecommended);
  EXPECT_EQ(static_cast<HttpsFirstModeSetting>(
                pref.GetPrefObject().recommended_value->GetInt()),
            HttpsFirstModeSetting::kEnabledFull);

  // Set HTTPS-Only Mode with full enforcement and check the generated pref.
  prefs()->SetManagedPref(prefs::kHttpsOnlyModeEnabled,
                          std::make_unique<base::Value>(true));
  EXPECT_EQ(pref.GetPrefObject().enforcement,
            settings_api::Enforcement::kEnforced);
  EXPECT_EQ(pref.GetPrefObject().controlled_by,
            settings_api::ControlledBy::kDevicePolicy);

  // Check that the generated pref cannot be changed when the backing pref is
  // managed.
  EXPECT_EQ(pref.SetPref(std::make_unique<base::Value>(
                             static_cast<int>(HttpsFirstModeSetting::kDisabled))
                             .get()),
            settings_private::SetPrefResult::PREF_NOT_MODIFIABLE);
}

// Variant of ManagementState, but with the Balanced Mode feature flag enabled.
// The full set of Settings are available (Full, Balanced, and Disabled) and
// both underlying prefs can be controlled by enetprise policy.
TEST_F(GeneratedHttpsFirstModePrefTest,
       ManagementState_HttpsFirstModeNewSettings) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kHttpsFirstBalancedMode);

  GeneratedHttpsFirstModePref pref(profile());
  EXPECT_EQ(pref.GetPrefObject().enforcement, settings_api::Enforcement::kNone);
  EXPECT_EQ(pref.GetPrefObject().controlled_by,
            settings_api::ControlledBy::kNone);

  // Emulate a recommended "force_enabled" policy value, where the fully enabled
  // pref has a recommended value of "true".
  prefs()->SetRecommendedPref(prefs::kHttpsOnlyModeEnabled,
                              std::make_unique<base::Value>(true));
  prefs()->SetRecommendedPref(prefs::kHttpsFirstBalancedMode,
                              std::make_unique<base::Value>(false));
  EXPECT_EQ(pref.GetPrefObject().enforcement,
            settings_api::Enforcement::kRecommended);
  EXPECT_EQ(static_cast<HttpsFirstModeSetting>(
                pref.GetPrefObject().recommended_value->GetInt()),
            HttpsFirstModeSetting::kEnabledFull);

  // Emulate a recommended "force_balanced_enabled" policy value, where the
  // recommended prefs have fully enabled as false and balanced as true.
  prefs()->SetRecommendedPref(prefs::kHttpsOnlyModeEnabled,
                              std::make_unique<base::Value>(false));
  prefs()->SetRecommendedPref(prefs::kHttpsFirstBalancedMode,
                              std::make_unique<base::Value>(true));
  EXPECT_EQ(pref.GetPrefObject().enforcement,
            settings_api::Enforcement::kRecommended);
  EXPECT_EQ(static_cast<HttpsFirstModeSetting>(
                pref.GetPrefObject().recommended_value->GetInt()),
            HttpsFirstModeSetting::kEnabledBalanced);

  // Emulate an enforced "force_enabled" policy value.
  prefs()->SetManagedPref(prefs::kHttpsOnlyModeEnabled,
                          std::make_unique<base::Value>(true));
  prefs()->SetManagedPref(prefs::kHttpsFirstBalancedMode,
                          std::make_unique<base::Value>(false));
  EXPECT_EQ(pref.GetPrefObject().enforcement,
            settings_api::Enforcement::kEnforced);
  EXPECT_EQ(pref.GetPrefObject().controlled_by,
            settings_api::ControlledBy::kDevicePolicy);
  EXPECT_EQ(
      static_cast<HttpsFirstModeSetting>(pref.GetPrefObject().value->GetInt()),
      HttpsFirstModeSetting::kEnabledFull);

  // Emulate an enforced "force_balanced_enabled" policy value.
  prefs()->SetManagedPref(prefs::kHttpsOnlyModeEnabled,
                          std::make_unique<base::Value>(false));
  prefs()->SetManagedPref(prefs::kHttpsFirstBalancedMode,
                          std::make_unique<base::Value>(true));
  EXPECT_EQ(pref.GetPrefObject().enforcement,
            settings_api::Enforcement::kEnforced);
  EXPECT_EQ(pref.GetPrefObject().controlled_by,
            settings_api::ControlledBy::kDevicePolicy);
  EXPECT_EQ(
      static_cast<HttpsFirstModeSetting>(pref.GetPrefObject().value->GetInt()),
      HttpsFirstModeSetting::kEnabledBalanced);

  // Check that the generated pref cannot be changed when the backing pref is
  // managed.
  EXPECT_EQ(pref.SetPref(std::make_unique<base::Value>(
                             static_cast<int>(HttpsFirstModeSetting::kDisabled))
                             .get()),
            settings_private::SetPrefResult::PREF_NOT_MODIFIABLE);

  // Emulate an enforced "disallowed" policy value.
  prefs()->SetManagedPref(prefs::kHttpsOnlyModeEnabled,
                          std::make_unique<base::Value>(false));
  prefs()->SetManagedPref(prefs::kHttpsFirstBalancedMode,
                          std::make_unique<base::Value>(false));
  EXPECT_EQ(pref.GetPrefObject().enforcement,
            settings_api::Enforcement::kEnforced);
  EXPECT_EQ(pref.GetPrefObject().controlled_by,
            settings_api::ControlledBy::kDevicePolicy);
  EXPECT_EQ(
      static_cast<HttpsFirstModeSetting>(pref.GetPrefObject().value->GetInt()),
      HttpsFirstModeSetting::kDisabled);
}

// Tests the collection of settings changed metrics.
TEST_F(GeneratedHttpsFirstModePrefTest, SettingChangedMetricsLogged) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kHttpsFirstBalancedMode);

  base::HistogramTester histograms;

  GeneratedHttpsFirstModePref pref(profile());

  // Emulate changing the UI setting to fully enabled.
  pref.SetPref(std::make_unique<base::Value>(
                   static_cast<int>(HttpsFirstModeSetting::kEnabledFull))
                   .get());
  histograms.ExpectTotalCount("Security.HttpsFirstMode.SettingChanged", 1);
  histograms.ExpectBucketCount("Security.HttpsFirstMode.SettingChanged", true,
                               1);

  // Emulate changing the UI to disabled.
  pref.SetPref(std::make_unique<base::Value>(
                   static_cast<int>(HttpsFirstModeSetting::kDisabled))
                   .get());
  histograms.ExpectTotalCount("Security.HttpsFirstMode.SettingChanged", 2);
  histograms.ExpectBucketCount("Security.HttpsFirstMode.SettingChanged", false,
                               1);
}

// Tests the collection of settings changed metrics, with the
// HttpsFirstBalancedMode feature flag enabled.
TEST_F(GeneratedHttpsFirstModePrefTest,
       SettingChangedMetricsLogged_BalancedMode) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kHttpsFirstBalancedMode);

  base::HistogramTester histograms;

  GeneratedHttpsFirstModePref pref(profile());

  // Emulate changing the UI setting to fully enabled.
  pref.SetPref(std::make_unique<base::Value>(
                   static_cast<int>(HttpsFirstModeSetting::kEnabledFull))
                   .get());
  histograms.ExpectTotalCount("Security.HttpsFirstMode.SettingChanged2", 1);
  histograms.ExpectBucketCount("Security.HttpsFirstMode.SettingChanged2",
                               HttpsFirstModeSetting::kEnabledFull, 1);

  // Emulate changing the UI setting to balanced mode.
  pref.SetPref(std::make_unique<base::Value>(
                   static_cast<int>(HttpsFirstModeSetting::kEnabledBalanced))
                   .get());
  histograms.ExpectTotalCount("Security.HttpsFirstMode.SettingChanged2", 2);
  histograms.ExpectBucketCount("Security.HttpsFirstMode.SettingChanged2",
                               HttpsFirstModeSetting::kEnabledBalanced, 1);

  // Emulate changing the UI to disabled.
  pref.SetPref(std::make_unique<base::Value>(
                   static_cast<int>(HttpsFirstModeSetting::kDisabled))
                   .get());
  histograms.ExpectTotalCount("Security.HttpsFirstMode.SettingChanged2", 3);
  histograms.ExpectBucketCount("Security.HttpsFirstMode.SettingChanged2",
                               HttpsFirstModeSetting::kDisabled, 1);
}
