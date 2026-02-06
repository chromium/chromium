// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_enabling.h"

#include "base/command_line.h"
#include "base/metrics/metrics_hashes.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_metrics_provider.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/subscription_eligibility/subscription_eligibility_prefs.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics/metrics_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/variations/synthetic_trial_registry.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/system_profile.pb.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

using base::test::FeatureRef;

namespace glic {
namespace {

class GlicEnablingTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    InitializeFeatureList();
    InProcessBrowserTest::SetUp();
  }

 protected:
  virtual void InitializeFeatureList() {
    scoped_feature_list_.InitWithFeatures(
        {
#if BUILDFLAG(IS_CHROMEOS)
            chromeos::features::kFeatureManagementGlic,
#endif  // BUILDFLAG(IS_CHROMEOS)
        },
        {});
  }

  Profile* profile() { return browser()->profile(); }
  ProfileManager* profile_manager() {
    return g_browser_process->profile_manager();
  }
  ProfileAttributesStorage& attributes_storage() {
    return profile_manager()->GetProfileAttributesStorage();
  }

  GlicTestEnvironment glic_test_env_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicEnablingTest, EnabledForProfileTest) {
  ASSERT_FALSE(GlicEnabling::IsEnabledForProfile(nullptr));

  ASSERT_TRUE(GlicEnabling::IsEnabledForProfile(profile()));
}

IN_PROC_BROWSER_TEST_F(GlicEnablingTest, AttributeEntryUpdatesOnChange) {
  SetGlicCapability(profile(), false);
  glic_test_env_.GetService(profile())->SetFRECompletion(
      prefs::FreStatus::kIncomplete);
  ASSERT_FALSE(GlicEnabling::IsEnabledForProfile(profile()));

  ProfileAttributesEntry* entry =
      attributes_storage().GetProfileAttributesWithPath(profile()->GetPath());
  ASSERT_TRUE(entry);
  EXPECT_FALSE(entry->IsGlicEligible());

  // Setting the model execution capability updates the glic AttributeEntry.
  SetGlicCapability(profile(), true);

  ASSERT_TRUE(GlicEnabling::IsEnabledForProfile(profile()));
  ASSERT_FALSE(GlicEnabling::IsEnabledAndConsentForProfile(profile()));
  EXPECT_TRUE(entry->IsGlicEligible());
}

class GlicEnablingWithSeparateAccountCapabilityTest : public GlicEnablingTest {
 public:
  void InitializeFeatureList() override {
    // InitWithFeaturesAndParameters is used instead of InitWithFeatures
    // because it has the side-effect of creating a FieldTrial, which is
    // required for the synthetic field trial to be populated.
    // allow empty feature parameters.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kGlic, {}},
            {features::kGlicRollout, {}},
            {switches::kGlicEligibilitySeparateAccountCapability, {}},
#if BUILDFLAG(IS_CHROMEOS)
            {chromeos::features::kFeatureManagementGlic, {}},
#endif  // BUILDFLAG(IS_CHROMEOS)
        },
        {});
  }
  ~GlicEnablingWithSeparateAccountCapabilityTest() override = default;

  void SetLegacyAccountCapability(bool capability_value) {
    auto* const identity_manager =
        IdentityManagerFactory::GetForProfile(profile());
    AccountInfo primary_account =
        identity_manager->FindExtendedAccountInfoByAccountId(
            identity_manager->GetPrimaryAccountId(
                signin::ConsentLevel::kSignin));
    AccountCapabilitiesTestMutator mutator(&primary_account.capabilities);
    mutator.set_can_use_model_execution_features(capability_value);
    signin::UpdateAccountInfoForAccount(identity_manager, primary_account);
  }
};

IN_PROC_BROWSER_TEST_F(GlicEnablingWithSeparateAccountCapabilityTest,
                       EnabledForProfileTest) {
  ASSERT_FALSE(GlicEnabling::IsEnabledForProfile(nullptr));

  ASSERT_TRUE(GlicEnabling::IsEnabledForProfile(profile()));
}

IN_PROC_BROWSER_TEST_F(GlicEnablingWithSeparateAccountCapabilityTest,
                       UnaffectedUserNotAddedToSyntheticFieldTrial) {
  ASSERT_NE(base::FeatureList::GetFieldTrial(
                switches::kGlicEligibilitySeparateAccountCapability),
            nullptr);
  auto initial_num_trials = g_browser_process->metrics_service()
                                ->GetSyntheticTrialRegistry()
                                ->GetCurrentSyntheticFieldTrialsForTest()
                                .size();

  // Set the legacy account capability to true, so that their eligibility is not
  // affected by the experiment.
  SetLegacyAccountCapability(true);

  // Check user eligibility.
  ASSERT_TRUE(GlicEnabling::IsEnabledForProfile(profile()));

  // Check that no new synthetic field trials were added.
  auto synthetic_trials = g_browser_process->metrics_service()
                              ->GetSyntheticTrialRegistry()
                              ->GetCurrentSyntheticFieldTrialsForTest();
  ASSERT_EQ(synthetic_trials.size(), initial_num_trials);
}

IN_PROC_BROWSER_TEST_F(GlicEnablingWithSeparateAccountCapabilityTest,
                       AffectedUserAddedToSyntheticFieldTrial) {
  ASSERT_NE(base::FeatureList::GetFieldTrial(
                switches::kGlicEligibilitySeparateAccountCapability),
            nullptr);
  auto initial_num_trials = g_browser_process->metrics_service()
                                ->GetSyntheticTrialRegistry()
                                ->GetCurrentSyntheticFieldTrialsForTest()
                                .size();

  // Set the legacy account capability to false, so that their eligibility is
  // affected by the experiment.
  SetLegacyAccountCapability(false);

  // Check user eligibility. This method will have the side-effect of adding
  // them to the synthetic field trial.
  ASSERT_TRUE(GlicEnabling::IsEnabledForProfile(profile()));

  // Check the user is in the synthetic field trial.
  auto synthetic_trials = g_browser_process->metrics_service()
                              ->GetSyntheticTrialRegistry()
                              ->GetCurrentSyntheticFieldTrialsForTest();
  ASSERT_EQ(synthetic_trials.size(), initial_num_trials + 1);
  EXPECT_EQ(synthetic_trials[synthetic_trials.size() - 1].name,
            base::HashFieldTrialName(
                kGlicEligibilitySeparateAccountCapabilitySyntheticTrialName));
}

IN_PROC_BROWSER_TEST_F(GlicEnablingWithSeparateAccountCapabilityTest,
                       SeparateAccountCapabilityUnknownTest) {
  // Sign in with a different account (this resets all account capabilities to
  // unknown).
  auto* const identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  auto account_info = signin::MakePrimaryAccountAvailable(
      identity_manager, "glic-test-2@example.com",
      signin::ConsentLevel::kSignin);
  account_info = AccountInfo::Builder(account_info)
                     .SetFullName("Glic 2 Testing")
                     .SetGivenName("Glic 2")
                     .Build();
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);
  ASSERT_FALSE(GlicEnabling::IsEnabledForProfile(profile()));

  // If the "can_use_gemini_in_chrome" capability is unknown, we should fall
  // back to the legacy "can_use_model_execution_features" capability.
  //
  // This is important during rollout, where existing users may not yet hav
  // fetched the new capability and it would be undesirable to disable GLIC for
  // them.
  SetLegacyAccountCapability(true);
  EXPECT_TRUE(GlicEnabling::IsEnabledForProfile(profile()));
}

IN_PROC_BROWSER_TEST_F(
    GlicEnablingWithSeparateAccountCapabilityTest,
    AffectedUserAddedToSyntheticFieldTrial_EligibilityChanges) {
  ASSERT_NE(base::FeatureList::GetFieldTrial(
                switches::kGlicEligibilitySeparateAccountCapability),
            nullptr);
  auto initial_num_trials = g_browser_process->metrics_service()
                                ->GetSyntheticTrialRegistry()
                                ->GetCurrentSyntheticFieldTrialsForTest()
                                .size();

  // Set the legacy account capability to false, so that their eligibility is
  // affected by the experiment. This adds the user to a synthetic field trial.
  SetLegacyAccountCapability(false);
  ASSERT_TRUE(GlicEnabling::IsEnabledForProfile(profile()));

  // Check the user is in the synthetic field trial.
  auto synthetic_trials = g_browser_process->metrics_service()
                              ->GetSyntheticTrialRegistry()
                              ->GetCurrentSyntheticFieldTrialsForTest();
  ASSERT_EQ(synthetic_trials.size(), initial_num_trials + 1);
  const auto& trial = synthetic_trials[synthetic_trials.size() - 1];
  EXPECT_EQ(trial.name,
            base::HashFieldTrialName(
                kGlicEligibilitySeparateAccountCapabilitySyntheticTrialName));
  auto synthetic_trial_group = trial.group;

  // Set the legacy account capability to true, making the user's eligibility no
  // longer affected by the experiment.
  SetLegacyAccountCapability(true);
  ASSERT_TRUE(GlicEnabling::IsEnabledForProfile(profile()));

  // Check that the user's synthetic field trial group is unchanged - the
  // previous trial membership is preserved until the next session, and this is
  // not treated as a "MultiProfileDetected" conflict.
  synthetic_trials = g_browser_process->metrics_service()
                         ->GetSyntheticTrialRegistry()
                         ->GetCurrentSyntheticFieldTrialsForTest();
  ASSERT_EQ(synthetic_trials.size(), initial_num_trials + 1);
  const auto& updated_trial = synthetic_trials[synthetic_trials.size() - 1];
  EXPECT_EQ(updated_trial.name,
            base::HashFieldTrialName(
                kGlicEligibilitySeparateAccountCapabilitySyntheticTrialName));
  EXPECT_EQ(updated_trial.group, synthetic_trial_group);
}

class GlicEnablingTieredRolloutTest : public GlicEnablingTest {
 public:
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeatures(
        {
            features::kGlic,
            features::kGlicTieredRollout,
#if BUILDFLAG(IS_CHROMEOS)
            chromeos::features::kFeatureManagementGlic,
#endif  // BUILDFLAG(IS_CHROMEOS)
        },
        {features::kGlicRollout});
  }
  ~GlicEnablingTieredRolloutTest() override = default;

  void SetTieredRolloutEligibilityForProfile(bool is_eligible) {
    profile()->GetPrefs()->SetBoolean(prefs::kGlicRolloutEligibility,
                                      is_eligible);
  }

  // Explicitly calls ProvideCurrentSessionData() for all metrics providers.
  void ProvideCurrentSessionData() {
    // The purpose of the below call is to avoid a DCHECK failure in an
    // unrelated metrics provider, in
    // |FieldTrialsProvider::ProvideCurrentSessionData()|.
    metrics::SystemProfileProto system_profile_proto;
    g_browser_process->metrics_service()
        ->GetDelegatingProviderForTesting()
        ->ProvideSystemProfileMetricsWithLogCreationTime(base::TimeTicks::Now(),
                                                         &system_profile_proto);
    metrics::ChromeUserMetricsExtension uma_proto;
    g_browser_process->metrics_service()
        ->GetDelegatingProviderForTesting()
        ->ProvideCurrentSessionData(&uma_proto);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicEnablingTieredRolloutTest, EnabledForProfileTest) {
  // Should not be enabled as profile not eligible for tiered rollout.
  EXPECT_FALSE(GlicEnabling::IsEnabledForProfile(profile()));

  // Should be enabled as now eligible for tiered rollout.
  SetTieredRolloutEligibilityForProfile(/*is_eligible=*/true);
  EXPECT_TRUE(GlicEnabling::IsEnabledForProfile(profile()));

  // Simulate user no longer eligible.
  SetTieredRolloutEligibilityForProfile(/*is_eligible=*/false);
  EXPECT_FALSE(GlicEnabling::IsEnabledForProfile(profile()));
}

IN_PROC_BROWSER_TEST_F(GlicEnablingTieredRolloutTest,
                       InTieredRolloutGroupOtherCriteriaNotPassing) {
  glic_test_env_.GetService(profile())->SetModelExecutionCapability(false);
  // Should be enabled as profile.
  SetTieredRolloutEligibilityForProfile(/*is_eligible=*/true);
  EXPECT_FALSE(GlicEnabling::IsEnabledForProfile(profile()));

  // No profiles eligible so this trial should not be emitted.
  base::HistogramTester histogram_tester;
  ProvideCurrentSessionData();

  histogram_tester.ExpectTotalCount("Glic.TieredRolloutEnablementStatus", 0);
}

class GlicEnablingSimultaneousRolloutTest
    : public GlicEnablingTieredRolloutTest {
 public:
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeatures(
        {
            features::kGlic,
            features::kGlicTieredRollout,
            features::kGlicRollout,
#if BUILDFLAG(IS_CHROMEOS)
            chromeos::features::kFeatureManagementGlic,
#endif  // BUILDFLAG(IS_CHROMEOS)
        },
        {});
  }
  ~GlicEnablingSimultaneousRolloutTest() override = default;
};

IN_PROC_BROWSER_TEST_F(GlicEnablingSimultaneousRolloutTest,
                       EnabledForProfileTest) {
  // Eligible for tiered rollout. Profile enabled for GLIC.
  SetTieredRolloutEligibilityForProfile(/*is_eligible=*/true);
  ASSERT_TRUE(GlicEnabling::IsEnabledForProfile(profile()));

  {
    base::HistogramTester histogram_tester;
    ProvideCurrentSessionData();
    histogram_tester.ExpectUniqueSample(
        "Glic.TieredRolloutEnablementStatus",
        GlicTieredRolloutEnablementStatus::kAllProfilesEnabled, 1);
  }

  // ChromeOS does not support multiple profiles.
#if !BUILDFLAG(IS_CHROMEOS)
  // Add another profile and have it signed in. The default value for
  // tiered rollout is false but this profile is enabled via the general
  // GlicRollout flag and canUseModelExecutionFeatures check.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath new_path = profile_manager->GenerateNextProfileDirectoryPath();
  Profile* second_profile =
      &profiles::testing::CreateProfileSync(profile_manager, new_path);
  ASSERT_TRUE(GlicEnabling::IsEnabledForProfile(second_profile));

  {
    base::HistogramTester histogram_tester;
    ProvideCurrentSessionData();
    histogram_tester.ExpectUniqueSample(
        "Glic.TieredRolloutEnablementStatus",
        GlicTieredRolloutEnablementStatus::kSomeProfilesEnabled, 1);
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)

  // Primary profile no longer eligible for tiered rollout. Should not have
  // effect on overall enablement, but will have an effect on the histogram
  // emitted.
  SetTieredRolloutEligibilityForProfile(/*is_eligible=*/false);
  ASSERT_TRUE(GlicEnabling::IsEnabledForProfile(profile()));

  {
    base::HistogramTester histogram_tester;
    ProvideCurrentSessionData();
    histogram_tester.ExpectUniqueSample(
        "Glic.TieredRolloutEnablementStatus",
        GlicTieredRolloutEnablementStatus::kNoProfilesEnabled, 1);
  }
}

// Test fixtures for testing-related flags for multi-instance enablement by
// tier.
class GlicMultiInstanceEnablingTestingFlagsBrowserTest
    : public GlicEnablingTest {
 public:
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeatures(
        {
            features::kGlic,
            features::kGlicEnableMultiInstanceBasedOnTier,
#if BUILDFLAG(IS_CHROMEOS)
            chromeos::features::kFeatureManagementGlic,
#endif  // BUILDFLAG(IS_CHROMEOS)
        },
        {features::kGlicMultiInstance});
  }
  ~GlicMultiInstanceEnablingTestingFlagsBrowserTest() override = default;

 protected:
  // Helper to set the AI subscription tier for the profile.
  void SetAiSubscriptionTier(int tier) {
    profile()->GetPrefs()->SetInteger(
        subscription_eligibility::prefs::kAiSubscriptionTier, tier);
  }

  // Helper to set the kGlicMultiInstanceEnabledBySubscriptionTier pref.
  void SetGlicMultiInstanceEnabledBySubscriptionTierPref(bool value) {
    g_browser_process->local_state()->SetBoolean(
        glic::prefs::kGlicMultiInstanceEnabledBySubscriptionTier, value);
  }

  // Helper to get the kGlicMultiInstanceEnabledBySubscriptionTier pref.
  bool GetGlicMultiInstanceEnabledBySubscriptionTierPref() {
    return g_browser_process->local_state()->GetBoolean(
        glic::prefs::kGlicMultiInstanceEnabledBySubscriptionTier);
  }
};

class GlicMultiInstanceEnablingNoFlagsBrowserTest
    : public GlicMultiInstanceEnablingTestingFlagsBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  bool IsG1() const { return std::get<0>(GetParam()); }
  bool WasPreviouslyEligible() const { return std::get<1>(GetParam()); }
};

IN_PROC_BROWSER_TEST_P(GlicMultiInstanceEnablingNoFlagsBrowserTest,
                       NoFlagsTest) {
  SetAiSubscriptionTier(IsG1() ? 1 : 0);
  SetGlicMultiInstanceEnabledBySubscriptionTierPref(WasPreviouslyEligible());

  bool expected_eligibility = IsG1() || WasPreviouslyEligible();
  EXPECT_EQ(
      GlicEnabling::GetAndUpdateEligibilityForGlicMultiInstanceTieredRollout(
          profile()),
      expected_eligibility);
}

INSTANTIATE_TEST_SUITE_P(All,
                         GlicMultiInstanceEnablingNoFlagsBrowserTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

class GlicMultiInstanceEnablingForceG1ForMiBrowserTest
    : public GlicMultiInstanceEnablingTestingFlagsBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    GlicMultiInstanceEnablingTestingFlagsBrowserTest::SetUpCommandLine(
        command_line);
    command_line->AppendSwitchASCII(
        switches::kGlicForceG1StatusForMultiInstance,
        GetParam() ? "true" : "false");
  }
};

IN_PROC_BROWSER_TEST_P(GlicMultiInstanceEnablingForceG1ForMiBrowserTest,
                       ForceG1ForMi) {
  bool force_g1_for_mi = GetParam();
  // Set the actual subscription tier to the opposite status of the test param,
  // to ensure that it is overridden by the command line switch in effect.
  SetAiSubscriptionTier(force_g1_for_mi ? 0 : 1);
  EXPECT_EQ(
      GlicEnabling::GetAndUpdateEligibilityForGlicMultiInstanceTieredRollout(
          profile()),
      force_g1_for_mi);
}

INSTANTIATE_TEST_SUITE_P(All,
                         GlicMultiInstanceEnablingForceG1ForMiBrowserTest,
                         testing::Bool());

class GlicMultiInstanceEnablingResetPrefBrowserTest
    : public GlicMultiInstanceEnablingTestingFlagsBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    GlicMultiInstanceEnablingTestingFlagsBrowserTest::SetUpCommandLine(
        command_line);
    command_line->AppendSwitch(switches::kGlicResetMultiInstanceEnabledByTier);
  }
};

IN_PROC_BROWSER_TEST_P(GlicMultiInstanceEnablingResetPrefBrowserTest,
                       ResetMiEnabledByTierPref) {
  bool is_eligible_by_tier = GetParam();
  SetAiSubscriptionTier(is_eligible_by_tier ? 1 : 0);
  SetGlicMultiInstanceEnabledBySubscriptionTierPref(/*value=*/true);

  EXPECT_EQ(
      GlicEnabling::GetAndUpdateEligibilityForGlicMultiInstanceTieredRollout(
          profile()),
      is_eligible_by_tier);
  // The pref should be reset to false if the user is not eligible by tier.
  EXPECT_EQ(GetGlicMultiInstanceEnabledBySubscriptionTierPref(),
            is_eligible_by_tier);
}

INSTANTIATE_TEST_SUITE_P(All,
                         GlicMultiInstanceEnablingResetPrefBrowserTest,
                         testing::Bool());

class GlicEnablingMultiInstanceFlagPrecedenceBrowserTest
    : public GlicMultiInstanceEnablingTestingFlagsBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    GlicMultiInstanceEnablingTestingFlagsBrowserTest::SetUpCommandLine(
        command_line);
    command_line->AppendSwitch(switches::kGlicResetMultiInstanceEnabledByTier);
    command_line->AppendSwitchASCII(
        switches::kGlicForceG1StatusForMultiInstance,
        GetParam() ? "true" : "false");
  }
};

IN_PROC_BROWSER_TEST_P(GlicEnablingMultiInstanceFlagPrecedenceBrowserTest,
                       FlagPrecedence) {
  bool force_g1_for_mi = GetParam();
  SetAiSubscriptionTier(force_g1_for_mi ? 0 : 1);
  SetGlicMultiInstanceEnabledBySubscriptionTierPref(/*value=*/true);

  EXPECT_EQ(
      GlicEnabling::GetAndUpdateEligibilityForGlicMultiInstanceTieredRollout(
          profile()),
      force_g1_for_mi);
}

INSTANTIATE_TEST_SUITE_P(All,
                         GlicEnablingMultiInstanceFlagPrecedenceBrowserTest,
                         testing::Bool());

}  // namespace
}  // namespace glic
