// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_metrics_provider.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_pref_names_internal.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/service/glic_onboarding_tracker.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/scoped_metrics_service_for_synthetic_trials.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/synthetic_trial_registry.h"
#include "components/variations/variations_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif

namespace glic {

class GlicMetricsProviderTest : public testing::Test {
 public:
  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled_features = {
        features::kGlic, features::kGlicRollout,
        features::kGlicShowForSignedOut};
#if BUILDFLAG(IS_CHROMEOS)
    enabled_features.push_back(chromeos::features::kFeatureManagementGlic);
#endif
    scoped_feature_list_.InitWithFeatures(
        enabled_features,
        {features::kGlicCountryFiltering, features::kGlicLocaleFiltering});

    testing_profile_manager_ =
        TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
            /*profile_manager=*/true);
    metrics_service_ = std::make_unique<ScopedMetricsServiceForSyntheticTrials>(
        TestingBrowserProcess::GetGlobal());
    profile1_ = profile_manager()->CreateTestingProfile("profile1");
    profile2_ = profile_manager()->CreateTestingProfile("profile2");
  }

  void TearDown() override {
    profile1_ = nullptr;
    profile2_ = nullptr;
    metrics_service_.reset();
    testing_profile_manager_ = nullptr;
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
  }

 protected:
  TestingProfileManager* profile_manager() { return testing_profile_manager_; }
  Profile* profile1() { return profile1_; }
  Profile* profile2() { return profile2_; }

  std::vector<variations::ActiveGroupId> GetSyntheticFieldTrials() {
    return metrics_service_->Get()
        ->GetSyntheticTrialRegistry()
        ->GetCurrentSyntheticFieldTrialsForTest();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ScopedMetricsServiceForSyntheticTrials> metrics_service_;
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<TestingProfileManager> testing_profile_manager_;
  raw_ptr<Profile> profile1_;
  raw_ptr<Profile> profile2_;
};

TEST_F(GlicMetricsProviderTest, ProvideCurrentSessionData) {
  GlicMetricsProvider provider;
  base::HistogramTester histograms;

  provider.ProvideCurrentSessionData(nullptr);

  // Should have recorded metrics for both profiles.
  histograms.ExpectTotalCount("Glic.ProfileEnablement.IsEnabled.SteadyState",
                              2);
}

TEST_F(GlicMetricsProviderTest, ProvideCurrentSessionData_ZoomLevel) {
  // Set FRE completion for profile2 only.
  profile2()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre, static_cast<int>(prefs::FreStatus::kCompleted));
  // Set zoom level for profile2.
  profile2()->GetPrefs()->SetInteger(prefs::kGlicZoomLevel, 125);

  GlicMetricsProvider provider;
  base::HistogramTester histograms;

  provider.ProvideCurrentSessionData(nullptr);

  // Should have recorded a single sample, as only profile2 completed FRE.
  histograms.ExpectUniqueSample("Glic.ZoomLevel.SteadyState", 125, 1);

  // Set FRE completion for profile1.
  profile1()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre, static_cast<int>(prefs::FreStatus::kCompleted));

  provider.ProvideCurrentSessionData(nullptr);
  histograms.ExpectTotalCount("Glic.ZoomLevel.SteadyState", 3);
}

TEST_F(GlicMetricsProviderTest, ProvideCurrentSessionData_OnboardingStatus) {
  GlicMetricsProvider provider;
  base::HistogramTester histograms;

  provider.ProvideCurrentSessionData(nullptr);

  histograms.ExpectUniqueSample("Glic.Onboarding.Profiles.Status",
                                OnboardingStatus::kNoInteraction, 1);
  histograms.ExpectUniqueSample("Glic.Onboarding.Profiles.Invoked",
                                GlicProfilesAllSomeNone::kNone, 1);
  histograms.ExpectUniqueSample("Glic.Onboarding.Profiles.OptIn",
                                GlicProfilesAllSomeNone::kNone, 1);
  histograms.ExpectUniqueSample("Glic.Onboarding.Profiles.UserSubmit",
                                GlicProfilesAllSomeNone::kNone, 1);
  EXPECT_TRUE(variations::ContainsTrialAndGroupName(
      GetSyntheticFieldTrials(), "GlicOnboardingStatus", "NoInteraction"));

  // Set profile1 to kNotOptedInButInvoked (Invoked=Some, OptIn=None,
  // UserSubmit=None)
  profile1()->GetPrefs()->SetInteger(
      prefs::kGlicOnboardingStatus,
      static_cast<int>(OnboardingStatus::kNotOptedInButInvoked));

  provider.ProvideCurrentSessionData(nullptr);
  histograms.ExpectBucketCount("Glic.Onboarding.Profiles.Status",
                               OnboardingStatus::kNotOptedInButInvoked, 1);
  histograms.ExpectBucketCount("Glic.Onboarding.Profiles.Invoked",
                               GlicProfilesAllSomeNone::kSome, 1);
  EXPECT_TRUE(variations::ContainsTrialAndGroupName(GetSyntheticFieldTrials(),
                                                    "GlicOnboardingStatus",
                                                    "NotOptedInButInvoked"));

  // Set profile2 to kPromptAndOptIn (Invoked=All, OptIn=Some, UserSubmit=Some)
  profile2()->GetPrefs()->SetInteger(
      prefs::kGlicOnboardingStatus,
      static_cast<int>(OnboardingStatus::kPromptAndOptIn));

  provider.ProvideCurrentSessionData(nullptr);
  histograms.ExpectBucketCount("Glic.Onboarding.Profiles.Status",
                               OnboardingStatus::kPromptAndOptIn, 1);
  histograms.ExpectBucketCount("Glic.Onboarding.Profiles.Invoked",
                               GlicProfilesAllSomeNone::kAll, 1);
  histograms.ExpectBucketCount("Glic.Onboarding.Profiles.OptIn",
                               GlicProfilesAllSomeNone::kSome, 1);
  histograms.ExpectBucketCount("Glic.Onboarding.Profiles.UserSubmit",
                               GlicProfilesAllSomeNone::kSome, 1);
  EXPECT_TRUE(variations::ContainsTrialAndGroupName(
      GetSyntheticFieldTrials(), "GlicOnboardingStatus", "PromptAndOptIn"));
}

}  // namespace glic
