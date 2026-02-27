// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_enabling.h"

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_features.mojom-features.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/variations/service/variations_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/test/glic_user_session_test_helper.h"
#include "chromeos/constants/chromeos_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

using base::test::FeatureRef;

namespace glic {
namespace {

class TestDelegate : public GlicGlobalEnabling::Delegate {
 public:
  std::string GetCountryCode() override { return country_code_; }
  std::string GetLocale() override { return locale_; }
  void SetCountryCode(const std::string& country_code) {
    country_code_ = country_code;
  }
  void SetLocale(const std::string& locale) { locale_ = locale; }

 private:
  std::string country_code_ = "us";
  std::string locale_ = "en-us";
};

class GlicEnablingTest : public testing::Test {
 public:
  void SetUp() override {
    // Note: We're not creating GlobalFeatures in this unit test because
    // GlicBackgroundModeManager fails to be constructed without additional
    // setup.
    testing::Test::SetUp();
    scoped_feature_list_.InitWithFeatures(
        {
            features::kGlic,
#if BUILDFLAG(IS_CHROMEOS)
            chromeos::features::kFeatureManagementGlic,
#endif  // BUILDFLAG(IS_CHROMEOS)
        },
        {});
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    scoped_feature_list_.Reset();
    testing::Test::TearDown();
  }

 protected:
  TestDelegate delegate_;
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// Test
TEST_F(GlicEnablingTest, GlicFeatureEnabledTest) {
  EXPECT_EQ(GlicGlobalEnabling(delegate_).IsEnabledByFlags(), true);
}

TEST_F(GlicEnablingTest, GlicFeatureNotEnabledTest) {
  // Turn feature flag off
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures({}, {features::kGlic});
  EXPECT_EQ(GlicGlobalEnabling(delegate_).IsEnabledByFlags(), false);
}

TEST_F(GlicEnablingTest, CountryFilteringNotEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kGlicCountryFiltering);
  delegate_.SetCountryCode("zz");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByFlags());
  histogram_tester_->ExpectUniqueSample(
      "Glic.CountryFilteringResult",
      GlicFilteringResult::kAllowedFilteringDisabled, 1);
}

TEST_F(GlicEnablingTest, CountryFilteringEnabledWithDefaultParams) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(features::kGlicCountryFiltering,
                                              {});
  delegate_.SetCountryCode("us");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByFlags());
  delegate_.SetCountryCode("US");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByFlags());
  delegate_.SetCountryCode("zz");
  EXPECT_FALSE(GlicGlobalEnabling(delegate_).IsEnabledByFlags());

  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult",
      GlicFilteringResult::kAllowedInInclusionList, 2);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult",
      GlicFilteringResult::kBlockedNotInInclusionList, 1);
  histogram_tester_->ExpectTotalCount("Glic.CountryFilteringResult", 3);
}

TEST_F(GlicEnablingTest, CountryFilteringEnabledWithLists) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kGlicCountryFiltering,
      {{"disabled_countries", "zz"}, {"enabled_countries", "us,uk,zz"}});

  delegate_.SetCountryCode("us");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByFlags());
  delegate_.SetCountryCode("UK");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByFlags());
  delegate_.SetCountryCode("zz");
  EXPECT_FALSE(GlicGlobalEnabling(delegate_).IsEnabledByFlags());
  delegate_.SetCountryCode("qq");
  EXPECT_FALSE(GlicGlobalEnabling(delegate_).IsEnabledByFlags());

  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult",
      GlicFilteringResult::kAllowedInInclusionList, 2);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult",
      GlicFilteringResult::kBlockedInExclusionList, 1);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult",
      GlicFilteringResult::kBlockedNotInInclusionList, 1);
  histogram_tester_->ExpectTotalCount("Glic.CountryFilteringResult", 4);
}

TEST_F(GlicEnablingTest, CountryFilteringEnabledWithStar) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kGlicCountryFiltering,
      {{"disabled_countries", "zz"}, {"enabled_countries", "*"}});

  delegate_.SetCountryCode("us");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByFlags());
  delegate_.SetCountryCode("ru");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByFlags());
  delegate_.SetCountryCode("zz");
  EXPECT_FALSE(GlicGlobalEnabling(delegate_).IsEnabledByFlags());

  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult",
      GlicFilteringResult::kAllowedWildcardInclusion, 2);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult",
      GlicFilteringResult::kBlockedInExclusionList, 1);
  histogram_tester_->ExpectTotalCount("Glic.CountryFilteringResult", 3);
}

TEST_F(GlicEnablingTest, LocaleFilteringNotEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kGlicLocaleFiltering);
  delegate_.SetLocale("foobar");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByFlags());
  histogram_tester_->ExpectUniqueSample(
      "Glic.LocaleFilteringResult",
      GlicFilteringResult::kAllowedFilteringDisabled, 1);
}

TEST_F(GlicEnablingTest, LocaleFilteringEnabledWithDefaults) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kGlicLocaleFiltering);

  delegate_.SetLocale("en-us");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByFlags());
  delegate_.SetLocale("en-uk");
  EXPECT_FALSE(GlicGlobalEnabling(delegate_).IsEnabledByFlags());
  delegate_.SetLocale("");
  EXPECT_FALSE(GlicGlobalEnabling(delegate_).IsEnabledByFlags());

  histogram_tester_->ExpectBucketCount(
      "Glic.LocaleFilteringResult",
      GlicFilteringResult::kAllowedInInclusionList, 1);
  histogram_tester_->ExpectBucketCount(
      "Glic.LocaleFilteringResult",
      GlicFilteringResult::kBlockedNotInInclusionList, 2);
  histogram_tester_->ExpectTotalCount("Glic.LocaleFilteringResult", 3);
}

TEST_F(GlicEnablingTest, LocaleFilteringEnabledWithLists) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kGlicLocaleFiltering,
      {{"disabled_locales", "en-zz"},
       {"enabled_locales", "en-us,en-ru,en-zz"}});

  delegate_.SetLocale("en-us");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByFlags());
  delegate_.SetLocale("en-US");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByFlags());
  delegate_.SetLocale("EN_us");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByFlags());
  delegate_.SetLocale("en-ru");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByFlags());
  delegate_.SetLocale("en-zz");
  EXPECT_FALSE(GlicGlobalEnabling(delegate_).IsEnabledByFlags());
  delegate_.SetLocale("en-ot");
  EXPECT_FALSE(GlicGlobalEnabling(delegate_).IsEnabledByFlags());

  histogram_tester_->ExpectBucketCount(
      "Glic.LocaleFilteringResult",
      GlicFilteringResult::kAllowedInInclusionList, 4);
  histogram_tester_->ExpectBucketCount(
      "Glic.LocaleFilteringResult",
      GlicFilteringResult::kBlockedInExclusionList, 1);
  histogram_tester_->ExpectBucketCount(
      "Glic.LocaleFilteringResult",
      GlicFilteringResult::kBlockedNotInInclusionList, 1);
  histogram_tester_->ExpectTotalCount("Glic.LocaleFilteringResult", 6);
}

TEST_F(GlicEnablingTest, LocaleFilteringEnabledStar) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kGlicLocaleFiltering,
      {{"disabled_locales", "en-zz"}, {"enabled_locales", "*"}});

  delegate_.SetLocale("en-us");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByFlags());
  delegate_.SetLocale("en-ru");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByFlags());
  delegate_.SetLocale("en-zz");
  EXPECT_FALSE(GlicGlobalEnabling(delegate_).IsEnabledByFlags());

  histogram_tester_->ExpectBucketCount(
      "Glic.LocaleFilteringResult",
      GlicFilteringResult::kAllowedWildcardInclusion, 2);
  histogram_tester_->ExpectBucketCount(
      "Glic.LocaleFilteringResult",
      GlicFilteringResult::kBlockedInExclusionList, 1);
  histogram_tester_->ExpectTotalCount("Glic.LocaleFilteringResult", 3);
}

// Test for `glic::GlicEnabling::IsProfileEligible`.
class GlicEnablingProfileEligibilityTest : public testing::Test {
 public:
  GlicEnablingProfileEligibilityTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            features::kGlic,
#if BUILDFLAG(IS_CHROMEOS)
            chromeos::features::kFeatureManagementGlic,
#endif  // BUILDFLAG(IS_CHROMEOS)
        },
        /*disabled_features=*/{
            features::kGlicCountryFiltering,
            features::kGlicLocaleFiltering,
        });
  }
  ~GlicEnablingProfileEligibilityTest() override = default;

  void SetUp() override {
    raw_ptr<TestingProfileManager> testing_profile_manager =
        TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
            /*profile_manager=*/true);

#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PreProfileSetUp(
        testing_profile_manager->profile_manager());
#endif  // BUILDFLAG(IS_CHROMEOS)

    profile_ = testing_profile_manager->CreateTestingProfile(
        TestingProfile::kDefaultProfileUserName);
  }

  void TearDown() override {
    profile_ = nullptr;

    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();

#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PostProfileTearDown();
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

 protected:
  Profile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
#if BUILDFLAG(IS_CHROMEOS)
  ash::GlicUserSessionTestHelper glic_user_session_test_helper_;
#endif  // BUILDFLAG(IS_CHROMEOS)
  raw_ptr<TestingProfile> profile_ = nullptr;
};

TEST_F(GlicEnablingProfileEligibilityTest, Eligible) {
  EXPECT_TRUE(GlicEnabling::IsProfileEligible(profile()));
}

class GlicEnablingProfileReadyStateTestBase
    : public GlicEnablingProfileEligibilityTest {
 public:
  GlicEnablingProfileReadyStateTestBase() = default;

  void SetUp() override {
    GlicEnablingProfileEligibilityTest::SetUp();
    // Ensure the profile is Enabled by default.
    // Disable rollout check and user status check complexities for these tests.
    // We already have kGlic enabled from the base class.
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlicRollout},
        /*disabled_features=*/{features::kGlicUserStatusCheck});

    // Make sure we have a primary account so we don't fail the "capable" check.
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile());
    AccountInfo account_info = signin::MakePrimaryAccountAvailable(
        identity_manager, "test@example.com", signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_use_model_execution_features(true);
    signin::UpdateAccountInfoForAccount(identity_manager, account_info);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class GlicEnablingTrustFirstOnboardingTest
    : public GlicEnablingProfileReadyStateTestBase {
 public:
  void SetUp() override {
    GlicEnablingProfileReadyStateTestBase::SetUp();
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {features::kGlicTrustFirstOnboarding, features::kGlicMultiInstance,
         mojom::features::kGlicMultiTab, features::kGlicMultitabUnderlines},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class GlicEnablingStandardFreTest
    : public GlicEnablingProfileReadyStateTestBase {
 public:
  void SetUp() override {
    GlicEnablingProfileReadyStateTestBase::SetUp();
    scoped_feature_list_.InitAndDisableFeature(
        features::kGlicTrustFirstOnboarding);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(GlicEnablingTrustFirstOnboardingTest, NotConsented_ReturnsReady) {
  profile()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      static_cast<int>(prefs::FreStatus::kIncomplete));

  EXPECT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kReady);
}

TEST_F(GlicEnablingTrustFirstOnboardingTest, Consented_ReturnsFalse) {
  profile()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre, static_cast<int>(prefs::FreStatus::kCompleted));

  EXPECT_FALSE(
      GlicEnabling::IsTrustFirstOnboardingEnabledForProfile(profile()));
}

TEST_F(GlicEnablingTrustFirstOnboardingTest, NotConsented_ReturnsTrue) {
  profile()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      static_cast<int>(prefs::FreStatus::kIncomplete));

  EXPECT_TRUE(GlicEnabling::IsTrustFirstOnboardingEnabledForProfile(profile()));
}

TEST_F(GlicEnablingStandardFreTest, NotConsented_ReturnsIneligible) {
  profile()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      static_cast<int>(prefs::FreStatus::kIncomplete));

  EXPECT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kIneligible);
}

class GlicEnablingAnyFreModeTest : public GlicEnablingProfileReadyStateTestBase,
                                   public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    GlicEnablingProfileReadyStateTestBase::SetUp();
    if (GetParam()) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/
          {features::kGlicTrustFirstOnboarding, features::kGlicMultiInstance,
           mojom::features::kGlicMultiTab, features::kGlicMultitabUnderlines},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kGlicTrustFirstOnboarding);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(GlicEnablingAnyFreModeTest, Consented_ReturnsReady) {
  profile()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre, static_cast<int>(prefs::FreStatus::kCompleted));

  EXPECT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kReady);
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_P(GlicEnablingAnyFreModeTest, NotSignedIn_ReturnsIneligible) {
  profile()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      static_cast<int>(prefs::FreStatus::kIncomplete));

  // Simulate "Not signed in" by removing the primary account.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  signin::ClearPrimaryAccount(identity_manager);

  EXPECT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kIneligible);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_P(GlicEnablingAnyFreModeTest,
       IsEnabledAndConsentForProfile_NotConsented_ReturnsFalse) {
  profile()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      static_cast<int>(prefs::FreStatus::kIncomplete));

  EXPECT_FALSE(GlicEnabling::IsEnabledAndConsentForProfile(profile()));
}

TEST_P(GlicEnablingAnyFreModeTest,
       IsEnabledAndConsentForProfile_Consented_ReturnsTrue) {
  profile()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre, static_cast<int>(prefs::FreStatus::kCompleted));

  EXPECT_TRUE(GlicEnabling::IsEnabledAndConsentForProfile(profile()));
}

INSTANTIATE_TEST_SUITE_P(All, GlicEnablingAnyFreModeTest, testing::Bool());

}  // namespace
}  // namespace glic
