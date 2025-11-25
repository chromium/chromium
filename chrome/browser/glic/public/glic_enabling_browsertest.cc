// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_enabling.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_metrics_provider.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics/metrics_service.h"
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

  void TearDown() override {
    scoped_feature_list_.Reset();
    InProcessBrowserTest::TearDown();
  }

 protected:
  virtual void InitializeFeatureList() {
    scoped_feature_list_.InitWithFeatures(
        {
            features::kGlic,
            features::kTabstripComboButton,
            features::kGlicRollout,
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

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicEnablingTest, EnabledForProfileTest) {
  ASSERT_FALSE(GlicEnabling::IsEnabledForProfile(nullptr));

  ASSERT_FALSE(GlicEnabling::IsEnabledForProfile(profile()));
  ForceSigninAndModelExecutionCapability(profile());
  ASSERT_TRUE(GlicEnabling::IsEnabledForProfile(profile()));
}

IN_PROC_BROWSER_TEST_F(GlicEnablingTest, AttributeEntryUpdatesOnChange) {
  SigninWithPrimaryAccount(profile());
  ASSERT_FALSE(GlicEnabling::IsEnabledForProfile(profile()));

  ProfileAttributesEntry* entry =
      attributes_storage().GetProfileAttributesWithPath(profile()->GetPath());
  ASSERT_TRUE(entry);
  EXPECT_FALSE(entry->IsGlicEligible());

  // Setting the model execution capability updates the glic AttributeEntry.
  SetModelExecutionCapability(profile(), true);

  ASSERT_TRUE(GlicEnabling::IsEnabledForProfile(profile()));
  ASSERT_FALSE(GlicEnabling::IsEnabledAndConsentForProfile(profile()));
  EXPECT_TRUE(entry->IsGlicEligible());
}

class GlicEnablingTieredRolloutTest : public GlicEnablingTest {
 public:
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeatures(
        {
            features::kGlic,
            features::kTabstripComboButton,
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
};

IN_PROC_BROWSER_TEST_F(GlicEnablingTieredRolloutTest, EnabledForProfileTest) {
  ForceSigninAndModelExecutionCapability(profile());

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
            features::kTabstripComboButton,
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
  ForceSigninAndModelExecutionCapability(profile());

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
  ForceSigninAndModelExecutionCapability(second_profile);
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

}  // namespace
}  // namespace glic
