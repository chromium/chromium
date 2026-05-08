// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_enabling.h"

#include <string>
#include <utility>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_pref_names_internal.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_features.mojom-features.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/variations/service/test_variations_service.h"
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
  std::string GetPermanentCountryCode() override {
    return permanent_country_code_;
  }
  std::string GetSessionCountryCode() override { return session_country_code_; }
  std::string GetLocale() override { return locale_; }
  void SetPermanentCountryCode(const std::string& country_code) {
    permanent_country_code_ = country_code;
  }
  void SetSessionCountryCode(const std::string& country_code) {
    session_country_code_ = country_code;
  }
  void SetBothCountryCodes(const std::string& country_code) {
    session_country_code_ = country_code;
    permanent_country_code_ = country_code;
  }
  void SetLocale(const std::string& locale) { locale_ = locale; }

 private:
  std::string permanent_country_code_ = "us";
  std::string session_country_code_ = "us";
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
  EXPECT_EQ(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria(), true);
}

TEST_F(GlicEnablingTest, GlicFeatureNotEnabledTest) {
  // Turn feature flag off
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures({}, {features::kGlic});
  EXPECT_EQ(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria(), false);
}

TEST_F(GlicEnablingTest, CountryFilteringNotEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kGlicCountryFiltering);
  delegate_.SetBothCountryCodes("zz");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());
  histogram_tester_->ExpectUniqueSample(
      "Glic.CountryFilteringResult",
      GlicFilteringResult::kAllowedFilteringDisabled, 1);
}

TEST_F(GlicEnablingTest,
       CountryFilteringEnabledWithDefaultParams_PermanentCountryCode) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(features::kGlicCountryFiltering,
                                              {});
  delegate_.SetSessionCountryCode("");
  delegate_.SetPermanentCountryCode("us");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());
  delegate_.SetPermanentCountryCode("US");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());
  delegate_.SetPermanentCountryCode("zz");
  EXPECT_FALSE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());

  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult",
      GlicFilteringResult::kAllowedInInclusionList, 2);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult",
      GlicFilteringResult::kBlockedNotInInclusionList, 1);
  histogram_tester_->ExpectTotalCount("Glic.CountryFilteringResult", 3);
}

TEST_F(GlicEnablingTest,
       CountryFilteringEnabledWithDefaultParams_SessionCountryCode) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(features::kGlicCountryFiltering,
                                              {});
  delegate_.SetPermanentCountryCode("");
  delegate_.SetSessionCountryCode("us");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());
  delegate_.SetSessionCountryCode("US");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());
  delegate_.SetSessionCountryCode("zz");
  EXPECT_FALSE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());

  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult",
      GlicFilteringResult::kAllowedInInclusionList, 2);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult",
      GlicFilteringResult::kBlockedNotInInclusionList, 1);
  histogram_tester_->ExpectTotalCount("Glic.CountryFilteringResult", 3);
}

TEST_F(GlicEnablingTest,
       CountryFilteringEnabledWithLists_PermanentCountryCode) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kGlicCountryFiltering,
      {{"disabled_countries", "zz"}, {"enabled_countries", "us,uk,zz"}});

  delegate_.SetSessionCountryCode("");
  delegate_.SetPermanentCountryCode("us");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());
  delegate_.SetPermanentCountryCode("UK");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());
  delegate_.SetPermanentCountryCode("zz");
  EXPECT_FALSE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());
  delegate_.SetPermanentCountryCode("qq");
  EXPECT_FALSE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());

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

TEST_F(GlicEnablingTest, CountryFilteringEnabledWithLists_SessionCountryCode) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kGlicCountryFiltering,
      {{"disabled_countries", "zz"}, {"enabled_countries", "us,uk,zz"}});

  delegate_.SetPermanentCountryCode("");
  delegate_.SetSessionCountryCode("us");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());
  delegate_.SetSessionCountryCode("UK");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());
  delegate_.SetSessionCountryCode("zz");
  EXPECT_FALSE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());
  delegate_.SetSessionCountryCode("qq");
  EXPECT_FALSE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());

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

TEST_F(GlicEnablingTest,
       CountryFilteringEnabledWithLists_DifferentCountryCodes) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kGlicCountryFiltering,
      {{"disabled_countries", "zz"}, {"enabled_countries", "us,uk,zz"}});

  delegate_.SetPermanentCountryCode("zz");
  delegate_.SetSessionCountryCode("us");
  EXPECT_FALSE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());

  delegate_.SetPermanentCountryCode("us");
  delegate_.SetSessionCountryCode("zz");
  EXPECT_FALSE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());

  delegate_.SetPermanentCountryCode("qq");
  delegate_.SetSessionCountryCode("us");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());

  delegate_.SetPermanentCountryCode("us");
  delegate_.SetSessionCountryCode("qq");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());

  delegate_.SetBothCountryCodes("qq");
  EXPECT_FALSE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());

  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult",
      GlicFilteringResult::kBlockedInExclusionList, 2);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult",
      GlicFilteringResult::kAllowedInInclusionList, 2);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult",
      GlicFilteringResult::kBlockedNotInInclusionList, 1);
  histogram_tester_->ExpectTotalCount("Glic.CountryFilteringResult", 5);
}

TEST_F(GlicEnablingTest,
       CountryFilteringEnabledWithLists_SessionCountryIgnored) {
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {{features::kGlicCountryFiltering,
        {{"disabled_countries", "zz"}, {"enabled_countries", "us,uk,zz"}}}},
      {features::kGlicUseSessionCountryForFiltering});

  delegate_.SetPermanentCountryCode("zz");
  delegate_.SetSessionCountryCode("us");
  EXPECT_FALSE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());

  delegate_.SetPermanentCountryCode("us");
  delegate_.SetSessionCountryCode("zz");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());

  delegate_.SetPermanentCountryCode("qq");
  delegate_.SetSessionCountryCode("us");
  EXPECT_FALSE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());

  delegate_.SetPermanentCountryCode("us");
  delegate_.SetSessionCountryCode("qq");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());

  delegate_.SetBothCountryCodes("qq");
  EXPECT_FALSE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());

  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult",
      GlicFilteringResult::kBlockedInExclusionList, 1);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult",
      GlicFilteringResult::kAllowedInInclusionList, 2);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult",
      GlicFilteringResult::kBlockedNotInInclusionList, 2);
  histogram_tester_->ExpectTotalCount("Glic.CountryFilteringResult", 5);
}

TEST_F(GlicEnablingTest, CountryFilteringEnabledWithStar) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kGlicCountryFiltering,
      {{"disabled_countries", "zz"}, {"enabled_countries", "*"}});

  delegate_.SetBothCountryCodes("us");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());
  delegate_.SetBothCountryCodes("ru");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());
  delegate_.SetBothCountryCodes("zz");
  EXPECT_FALSE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());

  delegate_.SetPermanentCountryCode("zz");
  delegate_.SetSessionCountryCode("us");
  EXPECT_FALSE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());

  delegate_.SetPermanentCountryCode("us");
  delegate_.SetSessionCountryCode("zz");
  EXPECT_FALSE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());

  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult",
      GlicFilteringResult::kAllowedWildcardInclusion, 2);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult",
      GlicFilteringResult::kBlockedInExclusionList, 3);
  histogram_tester_->ExpectTotalCount("Glic.CountryFilteringResult", 5);
}

TEST_F(GlicEnablingTest, LocaleFilteringNotEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kGlicLocaleFiltering);
  delegate_.SetLocale("foobar");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());
  histogram_tester_->ExpectUniqueSample(
      "Glic.LocaleFilteringResult",
      GlicFilteringResult::kAllowedFilteringDisabled, 1);
}

TEST_F(GlicEnablingTest, LocaleFilteringEnabledWithDefaults) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kGlicLocaleFiltering);

  delegate_.SetLocale("en-us");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());
  delegate_.SetLocale("en-uk");
  EXPECT_FALSE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());
  delegate_.SetLocale("");
  EXPECT_FALSE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());

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
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());
  delegate_.SetLocale("en-US");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());
  delegate_.SetLocale("EN_us");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());
  delegate_.SetLocale("en-ru");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());
  delegate_.SetLocale("en-zz");
  EXPECT_FALSE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());
  delegate_.SetLocale("en-ot");
  EXPECT_FALSE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());

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
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());
  delegate_.SetLocale("en-ru");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());
  delegate_.SetLocale("en-zz");
  EXPECT_FALSE(GlicGlobalEnabling(delegate_).IsEnabledByGlobalCriteria());

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
};

TEST_F(GlicEnablingTrustFirstOnboardingTest, NotConsented_ReturnsReady) {
  glic::GlicKeyedService::Get(profile())->enabling().SetCompletedFre(
      prefs::FreStatus::kIncomplete);

  EXPECT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kReady);
}

TEST_F(GlicEnablingTrustFirstOnboardingTest, Consented_ReturnsReady) {
  glic::GlicKeyedService::Get(profile())->enabling().SetCompletedFre(
      prefs::FreStatus::kCompleted);

  EXPECT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kReady);
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(GlicEnablingTrustFirstOnboardingTest, NotSignedIn_ReturnsIneligible) {
  glic::GlicKeyedService::Get(profile())->enabling().SetCompletedFre(
      prefs::FreStatus::kIncomplete);

  // Simulate "Not signed in" by removing the primary account.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  signin::ClearPrimaryAccount(identity_manager);

  EXPECT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kIneligible);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(GlicEnablingTrustFirstOnboardingTest,
       IsEnabledAndConsentForProfile_NotConsented_ReturnsFalse) {
  glic::GlicKeyedService::Get(profile())->enabling().SetCompletedFre(
      prefs::FreStatus::kIncomplete);

  EXPECT_FALSE(GlicEnabling::IsEnabledAndConsentForProfile(profile()));
}

TEST_F(GlicEnablingTrustFirstOnboardingTest,
       IsEnabledAndConsentForProfile_Consented_ReturnsTrue) {
  glic::GlicKeyedService::Get(profile())->enabling().SetCompletedFre(
      prefs::FreStatus::kCompleted);

  EXPECT_TRUE(GlicEnabling::IsEnabledAndConsentForProfile(profile()));
}

struct GatedFeatureParams {
  std::string name;
  bool is_feature_enabled = false;
  bool is_onboarding_param_enabled = false;
  bool has_user_consented = false;

  bool expected_result = false;
};

// Base class for testing features that follow the "gated" enablement pattern
// (Multi-instance -> Feature Flag -> Consent OR Onboarding Gate).
class GlicEnablingGatedFeatureTest
    : public GlicEnablingProfileReadyStateTestBase,
      public testing::WithParamInterface<GatedFeatureParams> {
 public:
  void SetUpFeature(const base::Feature& feature,
                    const base::FeatureParam<bool>& onboarding_param) {
    GlicEnablingProfileReadyStateTestBase::SetUp();

    std::vector<base::test::FeatureRefAndParams> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    // Always enable multi-instance features as a prerequisite.
    enabled_features.push_back({features::kGlicMultiInstance, {}});
    enabled_features.push_back({mojom::features::kGlicMultiTab, {}});
    enabled_features.push_back({features::kGlicMultitabUnderlines, {}});

    if (GetParam().is_feature_enabled) {
      enabled_features.push_back(
          {feature,
           {{onboarding_param.name,
             GetParam().is_onboarding_param_enabled ? "true" : "false"}}});
    } else {
      disabled_features.push_back(feature);
    }

    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
  }

 protected:
  void SetConsent(bool has_consent) {
    const auto fre_status = has_consent ? prefs::FreStatus::kCompleted
                                        : prefs::FreStatus::kIncomplete;
    glic::GlicKeyedService::Get(profile())->enabling().SetCompletedFre(
        fre_status);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// --- PDF Auto-Open Tests ---

class GlicEnablingAutoOpenForPdfTest : public GlicEnablingGatedFeatureTest {
 public:
  void SetUp() override {
    SetUpFeature(features::kAutoOpenGlicForPdf,
                 features::kAutoOpenGlicForPdfWithOnboarding);
  }
};

TEST_P(GlicEnablingAutoOpenForPdfTest, ExpectedBehavior) {
  SetConsent(GetParam().has_user_consented);
  EXPECT_EQ(GetParam().expected_result,
            GlicEnabling::IsAutoOpenForPdfEnabled(profile()))
      << "Failed for case: " << GetParam().name;
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GlicEnablingAutoOpenForPdfTest,
    testing::Values(
        GatedFeatureParams{.name = "Default (All Off)",
                           .expected_result = false},
        GatedFeatureParams{.name = "TrustFirstOnly (Not Enough)",
                           .expected_result = false},
        GatedFeatureParams{.name = "Consented (Pure)",
                           .is_feature_enabled = true,
                           .has_user_consented = true,
                           .expected_result = true},
        GatedFeatureParams{.name = "ConsentedWithTrustFirst",
                           .is_feature_enabled = true,
                           .has_user_consented = true,
                           .expected_result = true},
        GatedFeatureParams{.name = "FeatureEnabledWithTrustFirst (No Gate)",
                           .is_feature_enabled = true,
                           .expected_result = false},
        GatedFeatureParams{.name = "OnboardingGatedWithTrustFirst (Success)",
                           .is_feature_enabled = true,
                           .is_onboarding_param_enabled = true,
                           .expected_result = true}));

struct ContextMenuFeatureParams {
  std::string name;
  bool is_feature_enabled = false;
  bool has_user_consented = false;

  bool expected_result = false;
};

class GlicEnablingContextMenuTest
    : public GlicEnablingProfileReadyStateTestBase,
      public testing::WithParamInterface<ContextMenuFeatureParams> {
 public:
  void SetUp() override {
    GlicEnablingProfileReadyStateTestBase::SetUp();

    std::vector<base::test::FeatureRefAndParams> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    // Always enable multi-instance features as a prerequisite.
    enabled_features.push_back({features::kGlicMultiInstance, {}});
    enabled_features.push_back({mojom::features::kGlicMultiTab, {}});
    enabled_features.push_back({features::kGlicMultitabUnderlines, {}});

    if (GetParam().is_feature_enabled) {
      enabled_features.push_back({features::kGlicContextMenu, {}});
    } else {
      disabled_features.push_back(features::kGlicContextMenu);
    }

    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
  }

 protected:
  void SetConsent(bool has_consent) {
    const auto fre_status = has_consent ? prefs::FreStatus::kCompleted
                                        : prefs::FreStatus::kIncomplete;
    glic::GlicKeyedService::Get(profile())->enabling().SetCompletedFre(
        fre_status);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(GlicEnablingContextMenuTest, ExpectedBehavior) {
  SetConsent(GetParam().has_user_consented);
  base::HistogramTester histogram_tester;
  bool expected = GetParam().expected_result;
  EXPECT_EQ(expected, GlicEnabling::IsContextualMenuItemEnabled(profile()))
      << "Failed for case: " << GetParam().name;
  histogram_tester.ExpectUniqueSample("Glic.WebContentContextMenu.Enabled",
                                      expected, 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GlicEnablingContextMenuTest,
    testing::Values(
        ContextMenuFeatureParams{.name = "TrustFirstOnly (Not Enough)",
                                 .expected_result = false},
        ContextMenuFeatureParams{.name = "ConsentedWithTrustFirst",
                                 .is_feature_enabled = true,
                                 .has_user_consented = true,
                                 .expected_result = true},
        ContextMenuFeatureParams{.name = "FeatureEnabledWithTrustFirst",
                                 .is_feature_enabled = true,
                                 .expected_result = true}));

TEST_F(GlicEnablingProfileEligibilityTest,
       UserEnabledActuationOnWebChangedCallback) {
  bool callback_called = false;
  auto subscription =
      glic::GlicKeyedService::Get(profile())
          ->enabling()
          .RegisterOnUserEnabledActuationOnWebChanged(
              base::BindLambdaForTesting([&]() { callback_called = true; }));

  glic::GlicKeyedService::Get(profile())
      ->enabling()
      .SetUserEnabledActuationOnWeb(true);
  EXPECT_TRUE(callback_called);

  callback_called = false;
  glic::GlicKeyedService::Get(profile())
      ->enabling()
      .SetUserEnabledActuationOnWeb(false);
  EXPECT_TRUE(callback_called);
}

TEST_F(GlicEnablingProfileEligibilityTest,
       ExperimentalTriggeringEnabledChangedCallback) {
  bool callback_called = false;
  auto subscription =
      glic::GlicKeyedService::Get(profile())
          ->enabling()
          .RegisterOnExperimentalTriggeringEnabledChanged(
              base::BindLambdaForTesting([&]() { callback_called = true; }));

  glic::GlicKeyedService::Get(profile())
      ->enabling()
      .SetExperimentalTriggeringEnabled(true);
  EXPECT_TRUE(callback_called);

  callback_called = false;
  glic::GlicKeyedService::Get(profile())
      ->enabling()
      .SetExperimentalTriggeringEnabled(false);
  EXPECT_TRUE(callback_called);
}

TEST_F(GlicEnablingProfileEligibilityTest, ConsentChangedCallback) {
  bool callback_called = false;
  auto subscription = glic::GlicKeyedService::Get(profile())
                          ->enabling()
                          .RegisterOnConsentChanged(base::BindLambdaForTesting(
                              [&]() { callback_called = true; }));

  glic::GlicKeyedService::Get(profile())->enabling().SetCompletedFre(
      prefs::FreStatus::kCompleted);
  EXPECT_TRUE(callback_called);

  callback_called = false;
  glic::GlicKeyedService::Get(profile())->enabling().SetCompletedFre(
      prefs::FreStatus::kIncomplete);
  EXPECT_TRUE(callback_called);
}

TEST_F(GlicEnablingProfileReadyStateTestBase,
       GetExperimentalTriggeringState_NonManaged_Ready) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kGlicExperimentalTriggering);

  auto& enabling = glic::GlicKeyedService::Get(profile())->enabling();
  enabling.SetCompletedFre(prefs::FreStatus::kCompleted);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicUserEnabledActuationOnWeb,
                                    true);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicExperimentalTriggeringEnabled,
                                    true);

  EXPECT_EQ(enabling.GetExperimentalTriggeringState(),
            syncer::DeviceInfo::GlicExperimentalTriggeringState::kReady);
}

TEST_F(GlicEnablingProfileReadyStateTestBase,
       GetExperimentalTriggeringState_Managed_DefaultOff) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kGlicExperimentalTriggering);

  policy::ScopedManagementServiceOverrideForTesting
      scoped_management_service_override(
          policy::ManagementServiceFactory::GetForProfile(profile()),
          policy::EnterpriseManagementAuthority::CLOUD);

  auto& enabling = glic::GlicKeyedService::Get(profile())->enabling();
  enabling.SetCompletedFre(prefs::FreStatus::kCompleted);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicUserEnabledActuationOnWeb,
                                    true);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicExperimentalTriggeringEnabled,
                                    true);

  EXPECT_EQ(enabling.GetExperimentalTriggeringState(),
            syncer::DeviceInfo::GlicExperimentalTriggeringState::kUnavailable);
}

TEST_F(GlicEnablingProfileReadyStateTestBase,
       GetExperimentalTriggeringState_Managed_PolicyEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kGlicExperimentalTriggering);

  policy::ScopedManagementServiceOverrideForTesting
      scoped_management_service_override(
          policy::ManagementServiceFactory::GetForProfile(profile()),
          policy::EnterpriseManagementAuthority::CLOUD);

  profile()->GetPrefs()->SetInteger(
      prefs::kGlicExperimentalTriggeringPolicySettings,
      std::to_underlying(
          glic::prefs::GlicExperimentalTriggeringPolicyState::kEnabled));

  auto& enabling = glic::GlicKeyedService::Get(profile())->enabling();
  enabling.SetCompletedFre(prefs::FreStatus::kCompleted);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicUserEnabledActuationOnWeb,
                                    true);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicExperimentalTriggeringEnabled,
                                    true);

  EXPECT_EQ(enabling.GetExperimentalTriggeringState(),
            syncer::DeviceInfo::GlicExperimentalTriggeringState::kReady);
}

TEST_F(GlicEnablingProfileReadyStateTestBase,
       GetExperimentalTriggeringState_WorkspaceAccount_DefaultOff) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      {features::kGlicExperimentalTriggering, features::kGlicUserStatusCheck},
      {});

  // Make account managed (Workspace)
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  AccountInfo account_info =
      identity_manager->FindExtendedAccountInfoByAccountId(
          identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin));
  account_info =
      AccountInfo::Builder(account_info).SetHostedDomain("example.com").Build();
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);

  auto& enabling = glic::GlicKeyedService::Get(profile())->enabling();
  enabling.SetCompletedFre(prefs::FreStatus::kCompleted);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicUserEnabledActuationOnWeb,
                                    true);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicExperimentalTriggeringEnabled,
                                    true);

  EXPECT_EQ(enabling.GetExperimentalTriggeringState(),
            syncer::DeviceInfo::GlicExperimentalTriggeringState::kUnavailable);
}

TEST_F(GlicEnablingProfileReadyStateTestBase,
       GetExperimentalTriggeringState_WorkspaceAccount_PolicyEnabled) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      {features::kGlicExperimentalTriggering, features::kGlicUserStatusCheck},
      {});

  // Make account managed (Workspace)
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  AccountInfo account_info =
      identity_manager->FindExtendedAccountInfoByAccountId(
          identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin));
  account_info =
      AccountInfo::Builder(account_info).SetHostedDomain("example.com").Build();
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);

  profile()->GetPrefs()->SetInteger(
      prefs::kGlicExperimentalTriggeringPolicySettings,
      std::to_underlying(
          glic::prefs::GlicExperimentalTriggeringPolicyState::kEnabled));

  auto& enabling = glic::GlicKeyedService::Get(profile())->enabling();
  enabling.SetCompletedFre(prefs::FreStatus::kCompleted);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicUserEnabledActuationOnWeb,
                                    true);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicExperimentalTriggeringEnabled,
                                    true);

  EXPECT_EQ(enabling.GetExperimentalTriggeringState(),
            syncer::DeviceInfo::GlicExperimentalTriggeringState::kReady);
}

TEST_F(GlicEnablingProfileReadyStateTestBase,
       GetExperimentalTriggeringState_DogfoodBypass) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      {features::kGlicExperimentalTriggering, features::kGlicUserStatusCheck},
      {});

  // Setup TestVariationsService
  TestingPrefServiceSimple local_state;
  variations::TestVariationsService::RegisterPrefs(local_state.registry());
  metrics::TestEnabledStateProvider enabled_state_provider(/*consent=*/false,
                                                           /*enabled=*/false);
  auto metrics_state_manager = metrics::MetricsStateManager::Create(
      &local_state, &enabled_state_provider, std::wstring(), base::FilePath(),
      metrics::StartupVisibility::kUnknown);
  auto variations_service = std::make_unique<variations::TestVariationsService>(
      &local_state, metrics_state_manager.get());

  variations_service->SetIsLikelyDogfoodClientForTesting(true);
  TestingBrowserProcess::GetGlobal()->SetVariationsService(
      variations_service.get());
  struct ScopedVariationsServiceReset {
    ~ScopedVariationsServiceReset() {
      TestingBrowserProcess::GetGlobal()->SetVariationsService(nullptr);
    }
  } reset_variations_service;

  // Mock the profile as managed so we actually enter the policy check
  policy::ScopedManagementServiceOverrideForTesting
      scoped_management_service_override(
          policy::ManagementServiceFactory::GetForProfile(profile()),
          policy::EnterpriseManagementAuthority::CLOUD);

  profile()->GetPrefs()->SetInteger(
      prefs::kGlicExperimentalTriggeringPolicySettings,
      std::to_underlying(
          glic::prefs::GlicExperimentalTriggeringPolicyState::kDisabled));

  auto& enabling = glic::GlicKeyedService::Get(profile())->enabling();
  enabling.SetCompletedFre(prefs::FreStatus::kCompleted);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicUserEnabledActuationOnWeb,
                                    true);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicExperimentalTriggeringEnabled,
                                    true);

  EXPECT_EQ(enabling.GetExperimentalTriggeringState(),
            syncer::DeviceInfo::GlicExperimentalTriggeringState::kReady);
}
TEST_F(GlicEnablingProfileEligibilityTest,
       GetExperimentalTriggeringState_AllDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kGlicExperimentalTriggering,
                             features::kGlicExperimentalTriggeringOptInBypass});

  auto& enabling = glic::GlicKeyedService::Get(profile())->enabling();
  EXPECT_EQ(enabling.GetExperimentalTriggeringState(),
            syncer::DeviceInfo::GlicExperimentalTriggeringState::kUnavailable);
}

TEST_F(GlicEnablingProfileEligibilityTest,
       GetExperimentalTriggeringState_BypassEnabled_MainDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kGlicExperimentalTriggeringOptInBypass},
      /*disabled_features=*/{features::kGlicExperimentalTriggering});

  auto& enabling = glic::GlicKeyedService::Get(profile())->enabling();
  EXPECT_EQ(enabling.GetExperimentalTriggeringState(),
            syncer::DeviceInfo::GlicExperimentalTriggeringState::kUnavailable);
}

TEST_F(GlicEnablingProfileReadyStateTestBase,
       GetExperimentalTriggeringState_MainEnabled_BypassDisabled_NeedsOptIn) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kGlic,
                            features::kGlicExperimentalTriggering},
      /*disabled_features=*/{features::kGlicExperimentalTriggeringOptInBypass});

  auto& enabling = glic::GlicKeyedService::Get(profile())->enabling();

  // Ensure we are not opted in.
  enabling.SetCompletedFre(prefs::FreStatus::kIncomplete);
  enabling.SetUserEnabledActuationOnWeb(false);
  enabling.SetExperimentalTriggeringEnabled(false);

  // Bypass the enterprise policy check which defaults to disabled.
  profile()->GetPrefs()->SetInteger(
      prefs::kGlicExperimentalTriggeringPolicySettings,
      std::to_underlying(
          glic::prefs::GlicExperimentalTriggeringPolicyState::kEnabled));

  EXPECT_EQ(enabling.GetExperimentalTriggeringState(),
            syncer::DeviceInfo::GlicExperimentalTriggeringState::kNeedsOptIn);
}

TEST_F(GlicEnablingProfileReadyStateTestBase,
       GetExperimentalTriggeringState_MainEnabled_BypassDisabled_Ready) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kGlic,
                            features::kGlicExperimentalTriggering},
      /*disabled_features=*/{features::kGlicExperimentalTriggeringOptInBypass});

  auto& enabling = glic::GlicKeyedService::Get(profile())->enabling();

  // Opt-in manually.
  enabling.SetCompletedFre(prefs::FreStatus::kCompleted);
  enabling.SetUserEnabledActuationOnWeb(true);
  enabling.SetExperimentalTriggeringEnabled(true);
  // Bypass the enterprise policy check which defaults to disabled.
  profile()->GetPrefs()->SetInteger(
      prefs::kGlicExperimentalTriggeringPolicySettings,
      std::to_underlying(
          glic::prefs::GlicExperimentalTriggeringPolicyState::kEnabled));

  EXPECT_EQ(enabling.GetExperimentalTriggeringState(),
            syncer::DeviceInfo::GlicExperimentalTriggeringState::kReady);
}

TEST_F(GlicEnablingProfileReadyStateTestBase,
       GetExperimentalTriggeringState_MainEnabled_BypassEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kGlic,
                            features::kGlicExperimentalTriggering,
                            features::kGlicExperimentalTriggeringOptInBypass},
      /*disabled_features=*/{});

  auto& enabling = glic::GlicKeyedService::Get(profile())->enabling();

  // Ensure prefs are NOT opted in.
  enabling.SetCompletedFre(prefs::FreStatus::kIncomplete);
  enabling.SetUserEnabledActuationOnWeb(false);
  enabling.SetExperimentalTriggeringEnabled(false);

  // Bypass the enterprise policy check which defaults to disabled.
  profile()->GetPrefs()->SetInteger(
      prefs::kGlicExperimentalTriggeringPolicySettings,
      std::to_underlying(
          glic::prefs::GlicExperimentalTriggeringPolicyState::kEnabled));

  // Bypass should make it ready.
  EXPECT_EQ(enabling.GetExperimentalTriggeringState(),
            syncer::DeviceInfo::GlicExperimentalTriggeringState::kReady);

  // Verify helper functions return true.
  EXPECT_TRUE(enabling.HasConsented());
  EXPECT_TRUE(enabling.GetUserEnabledActuationOnWeb());
  EXPECT_TRUE(enabling.GetExperimentalTriggeringEnabled());
}

class GlicEnablingCombinedObserverTest
    : public GlicEnablingProfileEligibilityTest {
 public:
  GlicEnablingCombinedObserverTest() {
    scoped_feature_list_combined_.InitAndEnableFeature(
        features::kGlicExperimentalTriggering);
  }
  ~GlicEnablingCombinedObserverTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_combined_;
};

TEST_F(GlicEnablingCombinedObserverTest,
       ExperimentalTriggeringStateChangedCallback) {
  bool callback_called = false;
  auto& enabling = glic::GlicKeyedService::Get(profile())->enabling();

  auto subscription = enabling.RegisterOnExperimentalTriggeringStateChanged(
      base::BindLambdaForTesting([&]() { callback_called = true; }));

  // 1. Toggle user_enabled_actuation_on_web -> should not trigger (still
  // NeedsOptIn).
  callback_called = false;
  enabling.SetUserEnabledActuationOnWeb(true);
  EXPECT_FALSE(callback_called);

  // 2. Toggle completed_fre to completed -> should not trigger (still
  // NeedsOptIn, experimental triggering is still false).
  callback_called = false;
  enabling.SetCompletedFre(prefs::FreStatus::kCompleted);
  EXPECT_FALSE(callback_called);

  // 3. Toggle experimental_triggering_enabled to true -> should trigger (Ready,
  // all three are now true/completed).
  callback_called = false;
  enabling.SetExperimentalTriggeringEnabled(true);
  EXPECT_TRUE(callback_called);

  // 4. Toggle experimental_triggering_enabled back to false -> should trigger
  // (NeedsOptIn).
  callback_called = false;
  enabling.SetExperimentalTriggeringEnabled(false);
  EXPECT_TRUE(callback_called);

  // Toggle back to ready.
  enabling.SetExperimentalTriggeringEnabled(true);

  // 5. Toggle user_enabled_actuation_on_web to false -> should trigger
  // (NeedsOptIn).
  callback_called = false;
  enabling.SetUserEnabledActuationOnWeb(false);
  EXPECT_TRUE(callback_called);

  // Toggle back to ready.
  enabling.SetUserEnabledActuationOnWeb(true);

  // 6. Toggle consent to incomplete -> should trigger (NeedsOptIn).
  callback_called = false;
  enabling.SetCompletedFre(prefs::FreStatus::kIncomplete);
  EXPECT_TRUE(callback_called);

  // 7. Toggle consent to completed -> should trigger (Ready).
  callback_called = false;
  enabling.SetCompletedFre(prefs::FreStatus::kCompleted);
  EXPECT_TRUE(callback_called);
}

}  // namespace
}  // namespace glic
