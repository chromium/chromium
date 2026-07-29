// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_enabling.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/mock_log.h"
#include "base/test/scoped_command_line.h"
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
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/startup_visibility.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/subscription_eligibility/subscription_eligibility_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/variations/service/test_variations_service.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/variations_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/test/glic_user_session_test_helper.h"
#include "chromeos/constants/chromeos_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
#include "base/android/android_info.h"
#else
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/local_device_info_provider_impl.h"
#endif

using base::test::FeatureRef;

namespace glic {
namespace {

void FlushMessageLoop() {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

class TestDelegate : public GlicEnablingDelegate {
 public:
  TestDelegate() = default;
  TestDelegate(const TestDelegate& other)
      : permanent_country_code_(other.permanent_country_code_),
        session_country_code_(other.session_country_code_),
        locale_(other.locale_) {}

  std::unique_ptr<TestDelegate> Clone() const {
    return std::make_unique<TestDelegate>(*this);
  }

  std::string GetPermanentCountryCode() const override {
    return permanent_country_code_;
  }
  std::string GetSessionCountryCode() const override {
    return session_country_code_;
  }
  std::string GetLocale() const override { return locale_; }

  // Grants test access to protected base class method.
  using GlicEnablingDelegate::GetCountryEnablement;

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
#if BUILDFLAG(IS_ANDROID)
    if (base::android::android_info::sdk_int() <
        base::android::android_info::SDK_VERSION_S) {
      GTEST_SKIP() << "Glic requires Android S+";
    }
#endif
    // Note: We're not creating GlobalFeatures in this unit test because
    // GlicBackgroundModeManager fails to be constructed without additional
    // setup.
    testing::Test::SetUp();
    // Force basic password store to avoid talking to Linux desktop keyring
    // services, which can run asynchronously and cause flakiness.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII("password-store",
                                                              "basic");
    scoped_feature_list_.InitWithFeatures(
        {
            features::kGlic,
#if BUILDFLAG(IS_CHROMEOS)
            chromeos::features::kFeatureManagementGlic,
#endif  // BUILDFLAG(IS_CHROMEOS)
        },
#if BUILDFLAG(IS_ANDROID)
        {}
#else
        // Disable smart restart metrics to prevent background D-Bus queries to
        // screensaver status, which causes asynchronous test flakiness.
        {features::kSmartRestartMetrics}
#endif
    );
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    scoped_feature_list_.Reset();
    FlushMessageLoop();
    testing::Test::TearDown();
  }

 protected:
  GlicGlobalEnabling CreateGlobalEnabling() {
    return GlicGlobalEnabling(delegate_.Clone());
  }

  TestDelegate delegate_;
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// Test
TEST_F(GlicEnablingTest, GlicFeatureEnabledTest) {
  EXPECT_TRUE(CreateGlobalEnabling().IsEnabledByGlobalCriteria());
}

TEST_F(GlicEnablingTest, GlicFeatureNotEnabledTest) {
  // Turn feature flag off
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures({}, {features::kGlic});
  EXPECT_FALSE(CreateGlobalEnabling().IsEnabledByGlobalCriteria());
}

TEST_F(GlicEnablingTest, IneligibleProfileDoesNotLogIsConsentedMetrics) {
  GlicEnabling::ProfileEnablement enablement;
  enablement.is_regular_profile = false;
  enablement.fre_is_consented = true;

  EXPECT_FALSE(enablement.IsEnabled());

  enablement.RecordStartupMetrics();
  enablement.RecordSteadyStateMetrics();

  histogram_tester_->ExpectTotalCount(
      "Glic.ProfileEnablement.IsConsented.Startup", 0);
  histogram_tester_->ExpectTotalCount(
      "Glic.ProfileEnablement.IsConsented.SteadyState", 0);
}

TEST_F(GlicEnablingTest, CountryFilteringNotEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kGlicCountryFiltering);
  EXPECT_TRUE(TestDelegate::GetCountryEnablement("zz", "zz"));
  histogram_tester_->ExpectUniqueSample(
      "Glic.CountryFilteringResult2",
      GlicFilteringResult::kAllowedFilteringDisabled, 1);
}

TEST_F(GlicEnablingTest,
       CountryFilteringEnabledWithDefaultParams_PermanentCountryCode) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(features::kGlicCountryFiltering,
                                              {});
  EXPECT_TRUE(TestDelegate::GetCountryEnablement("us", ""));
  EXPECT_TRUE(TestDelegate::GetCountryEnablement("US", ""));
  EXPECT_FALSE(TestDelegate::GetCountryEnablement("zz", ""));

  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult2",
      GlicFilteringResult::kAllowedInInclusionList, 2);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult2",
      GlicFilteringResult::kBlockedNotInInclusionList, 1);
  histogram_tester_->ExpectTotalCount("Glic.CountryFilteringResult2", 3);
}

TEST_F(GlicEnablingTest,
       CountryFilteringEnabledWithDefaultParams_SessionCountryCode) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(features::kGlicCountryFiltering,
                                              {});
  EXPECT_TRUE(TestDelegate::GetCountryEnablement("", "us"));
  EXPECT_TRUE(TestDelegate::GetCountryEnablement("", "US"));
  EXPECT_FALSE(TestDelegate::GetCountryEnablement("", "zz"));

  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult2",
      GlicFilteringResult::kAllowedInInclusionList, 2);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult2",
      GlicFilteringResult::kBlockedNotInInclusionList, 1);
  histogram_tester_->ExpectTotalCount("Glic.CountryFilteringResult2", 3);
}

TEST_F(GlicEnablingTest,
       CountryFilteringEnabledWithLists_PermanentCountryCode) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kGlicCountryFiltering,
      {{"disabled_countries", "zz"}, {"enabled_countries", "us,uk,zz"}});

  EXPECT_TRUE(TestDelegate::GetCountryEnablement("us", ""));
  EXPECT_TRUE(TestDelegate::GetCountryEnablement("UK", ""));
  EXPECT_FALSE(TestDelegate::GetCountryEnablement("zz", ""));
  EXPECT_FALSE(TestDelegate::GetCountryEnablement("qq", ""));

  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult2",
      GlicFilteringResult::kAllowedInInclusionList, 2);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult2",
      GlicFilteringResult::kBlockedInExclusionList, 1);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult2",
      GlicFilteringResult::kBlockedNotInInclusionList, 1);
  histogram_tester_->ExpectTotalCount("Glic.CountryFilteringResult2", 4);
}

TEST_F(GlicEnablingTest, CountryFilteringEnabledWithLists_SessionCountryCode) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kGlicCountryFiltering,
      {{"disabled_countries", "zz"}, {"enabled_countries", "us,uk,zz"}});

  EXPECT_TRUE(TestDelegate::GetCountryEnablement("", "us"));
  EXPECT_TRUE(TestDelegate::GetCountryEnablement("", "UK"));
  EXPECT_FALSE(TestDelegate::GetCountryEnablement("", "zz"));
  EXPECT_FALSE(TestDelegate::GetCountryEnablement("", "qq"));

  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult2",
      GlicFilteringResult::kAllowedInInclusionList, 2);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult2",
      GlicFilteringResult::kBlockedInExclusionList, 1);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult2",
      GlicFilteringResult::kBlockedNotInInclusionList, 1);
  histogram_tester_->ExpectTotalCount("Glic.CountryFilteringResult2", 4);
}

TEST_F(GlicEnablingTest,
       CountryFilteringEnabledWithLists_DifferentCountryCodes) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kGlicCountryFiltering,
      {{"disabled_countries", "zz"}, {"enabled_countries", "us,uk,zz"}});

  EXPECT_FALSE(TestDelegate::GetCountryEnablement("zz", "us"));
  EXPECT_FALSE(TestDelegate::GetCountryEnablement("us", "zz"));
  EXPECT_TRUE(TestDelegate::GetCountryEnablement("qq", "us"));
  EXPECT_TRUE(TestDelegate::GetCountryEnablement("us", "qq"));
  EXPECT_FALSE(TestDelegate::GetCountryEnablement("qq", "qq"));

  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult2",
      GlicFilteringResult::kBlockedInExclusionList, 2);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult2",
      GlicFilteringResult::kAllowedInInclusionList, 2);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult2",
      GlicFilteringResult::kBlockedNotInInclusionList, 1);
  histogram_tester_->ExpectTotalCount("Glic.CountryFilteringResult2", 5);
}

TEST_F(GlicEnablingTest,
       CountryFilteringEnabledWithLists_SessionCountryIgnored) {
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {{features::kGlicCountryFiltering,
        {{"disabled_countries", "zz"}, {"enabled_countries", "us,uk,zz"}}}},
      {features::kGlicUseSessionCountryForFiltering});

  EXPECT_FALSE(TestDelegate::GetCountryEnablement("zz", "us"));
  EXPECT_TRUE(TestDelegate::GetCountryEnablement("us", "zz"));
  EXPECT_FALSE(TestDelegate::GetCountryEnablement("qq", "us"));
  EXPECT_TRUE(TestDelegate::GetCountryEnablement("us", "qq"));
  EXPECT_FALSE(TestDelegate::GetCountryEnablement("qq", "qq"));

  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult2",
      GlicFilteringResult::kBlockedInExclusionList, 1);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult2",
      GlicFilteringResult::kAllowedInInclusionList, 2);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult2",
      GlicFilteringResult::kBlockedNotInInclusionList, 2);
  histogram_tester_->ExpectTotalCount("Glic.CountryFilteringResult2", 5);
}

TEST_F(GlicEnablingTest, CountryFilteringEnabledWithStar) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kGlicCountryFiltering,
      {{"disabled_countries", "zz"}, {"enabled_countries", "*"}});

  EXPECT_TRUE(TestDelegate::GetCountryEnablement("us", "us"));
  EXPECT_TRUE(TestDelegate::GetCountryEnablement("ru", "ru"));
  EXPECT_FALSE(TestDelegate::GetCountryEnablement("zz", "zz"));
  EXPECT_FALSE(TestDelegate::GetCountryEnablement("zz", "us"));
  EXPECT_FALSE(TestDelegate::GetCountryEnablement("us", "zz"));

  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult2",
      GlicFilteringResult::kAllowedWildcardInclusion, 2);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult2",
      GlicFilteringResult::kBlockedInExclusionList, 3);
  histogram_tester_->ExpectTotalCount("Glic.CountryFilteringResult2", 5);
}

TEST_F(GlicEnablingTest, LocaleFilteringNotEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kGlicLocaleFiltering);
  delegate_.SetLocale("foobar");
  EXPECT_TRUE(CreateGlobalEnabling().IsEnabledByGlobalCriteria());
  histogram_tester_->ExpectUniqueSample(
      "Glic.LocaleFilteringResult",
      GlicFilteringResult::kAllowedFilteringDisabled, 1);
}

TEST_F(GlicEnablingTest, LocaleFilteringEnabledWithDefaults) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kGlicLocaleFiltering);

  delegate_.SetLocale("en-us");
  EXPECT_TRUE(CreateGlobalEnabling().IsEnabledByGlobalCriteria());
  delegate_.SetLocale("en-uk");
  EXPECT_FALSE(CreateGlobalEnabling().IsEnabledByGlobalCriteria());
  delegate_.SetLocale("");
  EXPECT_FALSE(CreateGlobalEnabling().IsEnabledByGlobalCriteria());

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
  EXPECT_TRUE(CreateGlobalEnabling().IsEnabledByGlobalCriteria());
  delegate_.SetLocale("en-US");
  EXPECT_TRUE(CreateGlobalEnabling().IsEnabledByGlobalCriteria());
  delegate_.SetLocale("EN_us");
  EXPECT_TRUE(CreateGlobalEnabling().IsEnabledByGlobalCriteria());
  delegate_.SetLocale("en-ru");
  EXPECT_TRUE(CreateGlobalEnabling().IsEnabledByGlobalCriteria());
  delegate_.SetLocale("en-zz");
  EXPECT_FALSE(CreateGlobalEnabling().IsEnabledByGlobalCriteria());
  delegate_.SetLocale("en-ot");
  EXPECT_FALSE(CreateGlobalEnabling().IsEnabledByGlobalCriteria());

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
  EXPECT_TRUE(CreateGlobalEnabling().IsEnabledByGlobalCriteria());
  delegate_.SetLocale("en-ru");
  EXPECT_TRUE(CreateGlobalEnabling().IsEnabledByGlobalCriteria());
  delegate_.SetLocale("en-zz");
  EXPECT_FALSE(CreateGlobalEnabling().IsEnabledByGlobalCriteria());

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
    // Force basic password store to avoid talking to Linux desktop keyring
    // services, which can run asynchronously and cause flakiness.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII("password-store",
                                                              "basic");
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
#if !BUILDFLAG(IS_ANDROID)
            // Disable smart restart metrics to prevent background D-Bus queries
            // to screensaver status, which causes asynchronous test flakiness.
            features::kSmartRestartMetrics,
#endif
        });
  }
  ~GlicEnablingProfileEligibilityTest() override = default;

  void SetUp() override {
#if BUILDFLAG(IS_ANDROID)
    if (base::android::android_info::sdk_int() <
        base::android::android_info::SDK_VERSION_S) {
      GTEST_SKIP() << "Glic requires Android S+";
    }
#endif
    raw_ptr<TestingProfileManager> testing_profile_manager =
        TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
            /*profile_manager=*/true);

#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PreProfileSetUp(
        testing_profile_manager->profile_manager());
#endif  // BUILDFLAG(IS_CHROMEOS)

    profile_ = testing_profile_manager->CreateTestingProfile(
        TestingProfile::kDefaultProfileUserName,
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories());

    // Set a default avatar icon index to avoid Skia text rendering in tests,
    // which otherwise crashes Android `unit_tests` that lack font files.
    testing_profile_manager->profile_manager()
        ->GetProfileAttributesStorage()
        .GetProfileAttributesWithPath(profile_->GetPath())
        ->SetAvatarIconIndex(1);

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);
  }

  void TearDown() override {
    if (IsSkipped()) {
      return;
    }
    identity_test_env_adaptor_.reset();
    profile_ = nullptr;
    FlushMessageLoop();

    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();

#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PostProfileTearDown();
#endif  // BUILDFLAG(IS_CHROMEOS)
    FlushMessageLoop();
  }

 protected:
  Profile* profile() { return profile_.get(); }
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;

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

TEST_F(GlicEnablingProfileEligibilityTest, WasPreviouslyNotAllowedTest) {
  // 1. Initially, when signed out and never previously evaluated, it defaults
  // to false (not previously not allowed).
  EXPECT_FALSE(GlicEnabling::WasPreviouslyNotAllowed(profile()));

  // 2. Sign in a capable account.
  auto* identity_test_env = identity_test_env_adaptor_->identity_test_env();
  AccountInfo account_info = identity_test_env->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_can_use_model_execution_features(true);
  signin::UpdateAccountInfoForAccount(identity_test_env->identity_manager(),
                                      account_info);

  // 3. Now they should be eligible, and not previously not allowed.
  EXPECT_FALSE(GlicEnabling::WasPreviouslyNotAllowed(profile()));

  // 4. Become ineligible while signed in.
  mutator.set_can_use_model_execution_features(false);
  signin::UpdateAccountInfoForAccount(identity_test_env->identity_manager(),
                                      account_info);

  // 5. They are now ineligible but still signed in, so previously not allowed
  // should be true.
  EXPECT_TRUE(GlicEnabling::WasPreviouslyNotAllowed(profile()));

  // 6. Make them eligible again.
  mutator.set_can_use_model_execution_features(true);
  signin::UpdateAccountInfoForAccount(identity_test_env->identity_manager(),
                                      account_info);
  EXPECT_FALSE(GlicEnabling::WasPreviouslyNotAllowed(profile()));

  // 7. Sign out.
#if !BUILDFLAG(IS_CHROMEOS)
  signin::ClearPrimaryAccount(identity_test_env->identity_manager());

  // 8. Even after signing out, WasPreviouslyNotAllowed should remain false.
  EXPECT_FALSE(GlicEnabling::WasPreviouslyNotAllowed(profile()));
#endif
}

TEST_F(GlicEnablingProfileEligibilityTest,
       IsEnabledForFirstRunProfile_Eligible) {
  auto* identity_test_env = identity_test_env_adaptor_->identity_test_env();
  AccountInfo account_info = identity_test_env->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_can_use_model_execution_features(true);
  signin::UpdateAccountInfoForAccount(identity_test_env->identity_manager(),
                                      account_info);

  EXPECT_TRUE(GlicEnabling::IsEnabledForFirstRunProfile(profile(), "us", "us",
                                                        account_info));
}

TEST_F(GlicEnablingProfileEligibilityTest,
       IsEnabledForFirstRunProfile_IneligibleAccount) {
  auto* identity_test_env = identity_test_env_adaptor_->identity_test_env();
  AccountInfo account_info = identity_test_env->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_can_use_model_execution_features(false);
  signin::UpdateAccountInfoForAccount(identity_test_env->identity_manager(),
                                      account_info);

  EXPECT_FALSE(GlicEnabling::IsEnabledForFirstRunProfile(profile(), "us", "us",
                                                         account_info));
}

TEST_F(GlicEnablingProfileEligibilityTest,
       IsEnabledForFirstRunProfile_BlockedCountry) {
  base::test::ScopedFeatureList country_features;
  country_features.InitAndEnableFeatureWithParameters(
      features::kGlicCountryFiltering,
      {{"disabled_countries", "zz"}, {"enabled_countries", "us,uk"}});

  auto* identity_test_env = identity_test_env_adaptor_->identity_test_env();
  AccountInfo account_info = identity_test_env->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_can_use_model_execution_features(true);
  signin::UpdateAccountInfoForAccount(identity_test_env->identity_manager(),
                                      account_info);

  EXPECT_FALSE(GlicEnabling::IsEnabledForFirstRunProfile(profile(), "zz", "zz",
                                                         account_info));
  EXPECT_TRUE(GlicEnabling::IsEnabledForFirstRunProfile(profile(), "us", "us",
                                                        account_info));
}

class GlicEnablingProfileReadyStateTestBase
    : public GlicEnablingProfileEligibilityTest {
 public:
  explicit GlicEnablingProfileReadyStateTestBase(
      const std::vector<base::test::FeatureRef>& extra_enabled_features = {},
      const std::vector<base::test::FeatureRef>& extra_disabled_features = {}) {
    // Ensure the profile is Enabled by default.
    // Disable rollout check and user status check complexities for these tests.
    // We already have kGlic enabled from the base class.
    std::vector<base::test::FeatureRef> enabled_features = {
        features::kGlicRollout};
    enabled_features.insert(enabled_features.end(),
                            extra_enabled_features.begin(),
                            extra_enabled_features.end());

    std::vector<base::test::FeatureRef> disabled_features = {
        features::kGlicUserStatusCheck};
    disabled_features.insert(disabled_features.end(),
                             extra_disabled_features.begin(),
                             extra_disabled_features.end());

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  void SetUp() override {
    GlicEnablingProfileEligibilityTest::SetUp();
    if (IsSkipped()) {
      return;
    }

    // Make sure we have a primary account so we don't fail the "capable" check.
    auto* identity_test_env = identity_test_env_adaptor_->identity_test_env();
    AccountInfo account_info = identity_test_env->MakePrimaryAccountAvailable(
        "test@example.com", signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info);
    mutator.set_can_use_model_execution_features(true);
    signin::UpdateAccountInfoForAccount(identity_test_env->identity_manager(),
                                        account_info);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class GlicEnablingTrustFirstOnboardingTest
    : public GlicEnablingProfileReadyStateTestBase {};

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

class GlicEnablingAnchorEntryPointTestBase : public testing::Test {
 public:
  GlicEnablingAnchorEntryPointTestBase() {
    // Force basic password store to avoid talking to Linux desktop keyring
    // services, which can run asynchronously and cause flakiness.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII("password-store",
                                                              "basic");
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            features::kGlicRollout,
#if BUILDFLAG(IS_CHROMEOS)
            chromeos::features::kFeatureManagementGlic,
#endif  // BUILDFLAG(IS_CHROMEOS)
        },
        /*disabled_features=*/{
            features::kGlic,  // Explicitly disable kGlic to fail global
                              // criteria
            features::kGlicUserStatusCheck,  // Disable user status check to
                                             // isolate from remote dogfood
                                             // status fetcher dependencies.
            // Disable filtering features to isolate tests from host machine
            // locale/country settings. Country filtering tested in
            // GlicEnablingAnchorEntryPointCountryTest.
            features::kGlicCountryFiltering,
            features::kGlicLocaleFiltering,
#if !BUILDFLAG(IS_ANDROID)
            // Disable smart restart metrics to prevent background D-Bus queries
            // to screensaver status, which causes asynchronous test flakiness.
            features::kSmartRestartMetrics,
#endif
        });
  }

  void SetUp() override {
#if BUILDFLAG(IS_ANDROID)
    if (base::android::android_info::sdk_int() <
        base::android::android_info::SDK_VERSION_S) {
      GTEST_SKIP() << "Glic requires Android S+";
    }
#endif
    raw_ptr<TestingProfileManager> testing_profile_manager =
        TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
            /*profile_manager=*/true);

#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PreProfileSetUp(
        testing_profile_manager->profile_manager());
#endif  // BUILDFLAG(IS_CHROMEOS)

    profile_ = testing_profile_manager->CreateTestingProfile(
        TestingProfile::kDefaultProfileUserName,
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories());

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);

    // Make sure we have a primary account so we don't fail the "capable" check.
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile());
    AccountInfo account_info = signin::MakePrimaryAccountAvailable(
        identity_manager, "test@example.com", signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info);
    mutator.set_can_use_model_execution_features(true);
    signin::UpdateAccountInfoForAccount(identity_manager, account_info);
  }

  void TearDown() override {
    if (IsSkipped()) {
      return;
    }
    GlicEnabling::SetSystemRequirementMetForTesting(std::nullopt);
    identity_test_env_adaptor_.reset();
    profile_ = nullptr;
    FlushMessageLoop();
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PostProfileTearDown();
#endif  // BUILDFLAG(IS_CHROMEOS)
    FlushMessageLoop();
  }

  Profile* profile() { return profile_.get(); }

 protected:
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
#if BUILDFLAG(IS_CHROMEOS)
  ash::GlicUserSessionTestHelper glic_user_session_test_helper_;
#endif  // BUILDFLAG(IS_CHROMEOS)
  raw_ptr<TestingProfile> profile_ = nullptr;
};

TEST_F(GlicEnablingAnchorEntryPointTestBase,
       FeatureFlagDisablesButtonWhenAnchored) {
  profile()->GetPrefs()->SetInteger(
      glic::prefs::kGlicCompletedFre,
      std::to_underlying(glic::prefs::FreStatus::kCompleted));

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      features::kGlicAnchorEntryPointForOnboardedUsers);

  base::HistogramTester histogram_tester;

  // Profile should NOT be eligible because the main kGlic flag is disabled,
  // which acts as a global killswitch.
  EXPECT_FALSE(GlicEnabling::IsProfileEligible(profile()));

  GlicEnabling::ProfileEnablement enablement =
      GlicEnabling::EnablementForProfile(profile());

  EXPECT_FALSE(enablement.IsProfileEligible());

  // The button should be hidden in the UI because the main feature flag
  // kGlic is disabled.
  EXPECT_FALSE(enablement.ShouldShowGlicButton());

  enablement.RecordStartupMetrics();
  enablement.RecordSteadyStateMetrics();

  // Since ShouldShowGlicButton() is false, no anchored reasons should be
  // logged.
  EXPECT_THAT(histogram_tester.GetAllSamplesForPrefix(
                  "Glic.ProfileEnablement.AnchoredDespiteEligibilityReason"),
              testing::IsEmpty());
}

TEST_F(GlicEnablingAnchorEntryPointTestBase, FeatureFlagDisablesAnchoring) {
  profile()->GetPrefs()->SetInteger(
      glic::prefs::kGlicCompletedFre,
      std::to_underlying(glic::prefs::FreStatus::kCompleted));

  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(
      features::kGlicAnchorEntryPointForOnboardedUsers);

  base::HistogramTester histogram_tester;

  // Profile should NOT be eligible because the anchor entry point feature is
  // disabled and kGlic (global criteria) is failing.
  EXPECT_FALSE(GlicEnabling::IsProfileEligible(profile()));

  GlicEnabling::ProfileEnablement enablement =
      GlicEnabling::EnablementForProfile(profile());
  enablement.RecordStartupMetrics();

  EXPECT_THAT(histogram_tester.GetAllSamplesForPrefix(
                  "Glic.ProfileEnablement.AnchoredDespiteEligibilityReason"),
              testing::IsEmpty());
}

TEST_F(GlicEnablingAnchorEntryPointTestBase,
       AnchoredButtonForIneligibleAccount) {
  profile()->GetPrefs()->SetInteger(
      glic::prefs::kGlicCompletedFre,
      static_cast<int>(glic::prefs::FreStatus::kCompleted));

  // Change the primary account capability to false (ineligible).
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  AccountInfo account_info =
      identity_manager->FindExtendedAccountInfoByAccountId(
          identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin));
  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_can_use_model_execution_features(false);
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);

  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      {features::kGlicAnchorEntryPointForOnboardedUsers, features::kGlic}, {});

  base::HistogramTester histogram_tester;

  // Profile should be eligible because the anchor entry point feature is active
  // and user is onboarded, even though the account is ineligible.
  EXPECT_TRUE(GlicEnabling::IsProfileEligible(profile()));

  GlicEnabling::ProfileEnablement enablement =
      GlicEnabling::EnablementForProfile(profile());
  EXPECT_TRUE(enablement.ShouldShowGlicButton());
  EXPECT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kIneligibleAccount);
  enablement.RecordStartupMetrics();
  enablement.RecordSteadyStateMetrics();

  // It should log the account ineligible reason to the new consolidated metric.
  using Reason =
      GlicEnabling::ProfileEnablement::AnchoredDespiteEligibilityReason;
  EXPECT_THAT(
      histogram_tester.GetAllSamplesForPrefix(
          "Glic.ProfileEnablement.AnchoredDespiteEligibilityReason"),
      testing::UnorderedElementsAre(
          testing::Pair(
              "Glic.ProfileEnablement.AnchoredDespiteEligibilityReason.Startup",
              base::BucketsAre(
                  base::Bucket(Reason::kPrimaryAccountNotCapable, 1))),
          testing::Pair(
              "Glic.ProfileEnablement.AnchoredDespiteEligibilityReason."
              "SteadyState",
              base::BucketsAre(
                  base::Bucket(Reason::kPrimaryAccountNotCapable, 1)))));
}

TEST_F(GlicEnablingAnchorEntryPointTestBase,
       GlobalDisablementPropagatedToReadyState) {
  profile()->GetPrefs()->SetInteger(
      glic::prefs::kGlicCompletedFre,
      std::to_underlying(glic::prefs::FreStatus::kCompleted));

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      features::kGlicAnchorEntryPointForOnboardedUsers);

  // When kGlic is disabled (acting as a killswitch), it overrides the anchor
  // entry point feature flag. Therefore, the anchor override is inactive and
  // GetProfileReadyState() returns kIneligible rather than kIneligibleAccount.
  // The entry point button itself is also hidden.
  EXPECT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kIneligible);
  EXPECT_FALSE(
      GlicEnabling::EnablementForProfile(profile()).ShouldShowGlicButton());
}

TEST_F(GlicEnablingAnchorEntryPointTestBase,
       PrimaryAccountNotCapable_ReturnsIneligibleAccountWhenAnchored) {
  profile()->GetPrefs()->SetInteger(
      glic::prefs::kGlicCompletedFre,
      std::to_underlying(glic::prefs::FreStatus::kCompleted));

  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      {features::kGlicAnchorEntryPointForOnboardedUsers, features::kGlic}, {});

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  AccountInfo account_info =
      identity_manager->FindExtendedAccountInfoByAccountId(
          identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin));
  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_can_use_model_execution_features(false);
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);

  // When anchored, capability failures map to kIneligibleAccount.
  EXPECT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kIneligibleAccount);
}

TEST_F(GlicEnablingAnchorEntryPointTestBase,
       SystemRequirementFail_BypassesAnchorState) {
  profile()->GetPrefs()->SetInteger(
      glic::prefs::kGlicCompletedFre,
      std::to_underlying(glic::prefs::FreStatus::kCompleted));

  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      {features::kGlicAnchorEntryPointForOnboardedUsers, features::kGlic}, {});

  // Force system requirements to fail
  GlicEnabling::SetSystemRequirementMetForTesting(false);

  // The fallback anchor state should be rejected, resulting in a hard block.
  EXPECT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kIneligible);

  // UI Entrypoints should be completely hidden.
  EXPECT_FALSE(
      GlicEnabling::EnablementForProfile(profile()).ShouldShowGlicButton());
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(GlicEnablingTrustFirstOnboardingTest,
       NotSignedIn_ReturnsSignInRequired) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kGlicShowForSignedOut);

  glic::GlicKeyedService::Get(profile())->enabling().SetCompletedFre(
      prefs::FreStatus::kIncomplete);

  // Simulate "Not signed in" by removing the primary account.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  signin::ClearPrimaryAccount(identity_manager);

  EXPECT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kSignInRequired);
  EXPECT_TRUE(GlicEnabling::IsEnabledForProfile(profile()));
}

TEST_F(GlicEnablingTrustFirstOnboardingTest,
       NotSignedIn_FeatureDisabled_ReturnsIneligible) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kGlicShowForSignedOut);

  glic::GlicKeyedService::Get(profile())->enabling().SetCompletedFre(
      prefs::FreStatus::kIncomplete);

  // Simulate "Not signed in" by removing the primary account.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  signin::ClearPrimaryAccount(identity_manager);

  EXPECT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kIneligible);
  EXPECT_FALSE(GlicEnabling::IsEnabledForProfile(profile()));
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

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(GlicEnablingTrustFirstOnboardingTest, ResetFreOnSignOut) {
  auto& enabling = glic::GlicKeyedService::Get(profile())->enabling();
  enabling.SetCompletedFre(prefs::FreStatus::kCompleted);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  signin::ClearPrimaryAccount(identity_manager);

#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(enabling.GetCompletedFre(), prefs::FreStatus::kNotStarted);
#else
  EXPECT_EQ(enabling.GetCompletedFre(), prefs::FreStatus::kCompleted);
#endif
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

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
    if (IsSkipped()) {
      return;
    }

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
    if (IsSkipped()) {
      return;
    }

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
  EXPECT_EQ(expected, GlicEnabling::IsContextualMenuItemEnabled(profile(), u""))
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

TEST_F(GlicEnablingProfileEligibilityTest,
       IsExperimentalTriggeringEnabledDefault) {
  auto& enabling = glic::GlicKeyedService::Get(profile())->enabling();

  // By default, the preference should be at its default value.
  EXPECT_TRUE(enabling.IsExperimentalTriggeringEnabledDefault());

  // Set it to true explicitly.
  enabling.SetExperimentalTriggeringEnabled(true);
  EXPECT_FALSE(enabling.IsExperimentalTriggeringEnabledDefault());

  // Set it to false explicitly
  enabling.SetExperimentalTriggeringEnabled(false);
  EXPECT_FALSE(enabling.IsExperimentalTriggeringEnabledDefault());
}

TEST_F(GlicEnablingProfileEligibilityTest,
       IsExperimentalTriggeringUserControlled) {
  auto& enabling = glic::GlicKeyedService::Get(profile())->enabling();

  // By default, the preference is user-controlled.
  EXPECT_TRUE(enabling.IsExperimentalTriggeringUserControlled());

  // Make the preference managed (enforced by policy)
  static_cast<TestingProfile*>(profile())
      ->GetTestingPrefService()
      ->SetManagedPref(prefs::kGlicExperimentalTriggeringEnabled,
                       std::make_unique<base::Value>(false));
  EXPECT_FALSE(enabling.IsExperimentalTriggeringUserControlled());
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(GlicEnablingProfileEligibilityTest,
       ExperimentalTriggering_ProfileTeardownNoCrash) {
  base::test::ScopedFeatureList feature_list(
      features::kGlicExperimentalTriggering);

  auto* identity_test_env = identity_test_env_adaptor_->identity_test_env();
  identity_test_env->MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);
  SetGlicCapability(profile(), true);

  auto* glic_service = GlicKeyedServiceFactory::GetGlicKeyedService(profile());
  ASSERT_TRUE(glic_service);
  glic_service->enabling().SetCompletedFre(prefs::FreStatus::kCompleted);
  glic_service->enabling().SetUserEnabledActuationOnWeb(true);
  glic_service->enabling().SetExperimentalTriggeringEnabled(true);

  auto* device_info_sync_service =
      DeviceInfoSyncServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(device_info_sync_service);
  auto* provider = static_cast<syncer::LocalDeviceInfoProviderImpl*>(
      device_info_sync_service->GetLocalDeviceInfoProvider());
  ASSERT_TRUE(provider);
  provider->Initialize("cache_guid", "client_name", "manufacturer", "model",
                       "full_hardware_class",
                       /*android_os_build_fingerprint_prefix=*/std::nullopt,
                       /*device_info_restored_from_store=*/nullptr);

  // 1. Simulate GlicKeyedService Phase 1 Shutdown.
  glic_service->Shutdown();

  // 2. Trigger GlicEnabling state change post-shutdown.
  glic_service->enabling().SetExperimentalTriggeringEnabled(false);
}
#endif

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
      prefs::kGlicSparkPolicySettings,
      std::to_underlying(glic::prefs::GlicSparkPolicyState::kEnabled));

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
      prefs::kGlicSparkPolicySettings,
      std::to_underlying(glic::prefs::GlicSparkPolicyState::kEnabled));

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
      prefs::kGlicSparkPolicySettings,
      std::to_underlying(glic::prefs::GlicSparkPolicyState::kDisabled));

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
      prefs::kGlicSparkPolicySettings,
      std::to_underlying(glic::prefs::GlicSparkPolicyState::kEnabled));

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
      prefs::kGlicSparkPolicySettings,
      std::to_underlying(glic::prefs::GlicSparkPolicyState::kEnabled));

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
      prefs::kGlicSparkPolicySettings,
      std::to_underlying(glic::prefs::GlicSparkPolicyState::kEnabled));

  // Bypass should make it ready.
  EXPECT_EQ(enabling.GetExperimentalTriggeringState(),
            syncer::DeviceInfo::GlicExperimentalTriggeringState::kReady);

  // Verify helper functions return true.
  EXPECT_TRUE(enabling.HasConsented());
  EXPECT_TRUE(enabling.GetUserEnabledActuationOnWeb());
  EXPECT_TRUE(enabling.GetExperimentalTriggeringEnabled());
}

class GlicEnablingCombinedObserverTest
    : public GlicEnablingProfileReadyStateTestBase {
 public:
  GlicEnablingCombinedObserverTest()
      : GlicEnablingProfileReadyStateTestBase(
            {features::kGlicExperimentalTriggering},
            {}) {}
  ~GlicEnablingCombinedObserverTest() override = default;
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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)

constexpr char kPrefProjectId[] = "pref-project";
constexpr char kPrefAppId[] = "pref-engine";
constexpr char kPrefLocation[] = "pref-location";

constexpr char kCmdProjectId[] = "cmd-project";
constexpr char kCmdAppId[] = "cmd-engine";
constexpr char kCmdLocation[] = "cmd-location";

glic::mojom::GeminiEnterpriseSettings GetPrefSettings() {
  return glic::mojom::GeminiEnterpriseSettings(kPrefProjectId, kPrefAppId,
                                               kPrefLocation);
}

glic::mojom::GeminiEnterpriseSettings GetCmdSettings() {
  return glic::mojom::GeminiEnterpriseSettings(kCmdProjectId, kCmdAppId,
                                               kCmdLocation);
}

std::string ToJsonString(
    const glic::mojom::GeminiEnterpriseSettings& settings) {
  return base::StringPrintf(
      R"({"project_id": "%s", "app_id": "%s", "location": "%s"})",
      settings.project_id.c_str(), settings.app_id.c_str(),
      settings.location.c_str());
}

base::DictValue ToDictValue(
    const glic::mojom::GeminiEnterpriseSettings& settings) {
  base::DictValue dict;
  dict.Set("project_id", settings.project_id);
  dict.Set("app_id", settings.app_id);
  dict.Set("location", settings.location);
  return dict;
}

struct GeminiEnterpriseSettingsParams {
  bool feature_enabled = false;
  bool is_enterprise = true;
  std::optional<glic::mojom::GeminiEnterpriseSettings> pref_settings;
  std::optional<glic::mojom::GeminiEnterpriseSettings> cmd_settings;
  std::optional<glic::mojom::GeminiEnterpriseSettings> expected_settings;
};

class GlicEnablingGeminiEnterpriseSettingsTest
    : public GlicEnablingProfileEligibilityTest,
      public testing::WithParamInterface<GeminiEnterpriseSettingsParams> {
 public:
  GlicEnablingGeminiEnterpriseSettingsTest() {
    const auto& params = GetParam();
    if (params.feature_enabled) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kGlicGeminiEnterpriseSettingsEnabled);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kGlicGeminiEnterpriseSettingsEnabled);
    }
  }

  void SetUp() override {
    GlicEnablingProfileEligibilityTest::SetUp();
    if (IsSkipped()) {
      return;
    }

    const auto& params = GetParam();

    auto* identity_test_env = identity_test_env_adaptor_->identity_test_env();
    // Glic requires the model execution capability to be enabled for the
    // profile to be eligible.
    AccountInfo account_info = identity_test_env->MakePrimaryAccountAvailable(
        params.is_enterprise ? "user@enterprise.com" : "user@gmail.com",
        signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info);
    mutator.set_can_use_model_execution_features(true);
    if (params.is_enterprise) {
      account_info = AccountInfo::Builder(account_info)
                         .SetHostedDomain("enterprise.com")
                         .Build();
    }
    identity_test_env->UpdateAccountInfoForAccount(account_info);

    if (params.pref_settings.has_value()) {
      profile()->GetPrefs()->SetDict(glic::prefs::kGlicGeminiEnterpriseSettings,
                                     ToDictValue(params.pref_settings.value()));
    }

    if (params.cmd_settings.has_value()) {
      scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
          switches::kGlicGeminiEnterpriseSettingsOverride,
          ToJsonString(params.cmd_settings.value()));
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedCommandLine scoped_command_line_;
};

TEST_P(GlicEnablingGeminiEnterpriseSettingsTest, ExpectedBehavior) {
  std::optional<glic::mojom::GeminiEnterpriseSettings> settings =
      GlicEnabling::GetGeminiEnterpriseSettings(profile());

  const auto& expected = GetParam().expected_settings;

  if (expected.has_value()) {
    EXPECT_THAT(
        settings,
        testing::Optional(testing::AllOf(
            testing::Field("project_id",
                           &glic::mojom::GeminiEnterpriseSettings::project_id,
                           expected->project_id),
            testing::Field("app_id",
                           &glic::mojom::GeminiEnterpriseSettings::app_id,
                           expected->app_id),
            testing::Field("location",
                           &glic::mojom::GeminiEnterpriseSettings::location,
                           expected->location))));
  } else {
    EXPECT_EQ(settings, std::nullopt);
  }

  auto enablement = GlicEnabling::EnablementForProfile(profile());
  if (expected.has_value()) {
    ASSERT_TRUE(enablement.gemini_enterprise_settings.has_value());
    EXPECT_EQ(enablement.gemini_enterprise_settings->project_id,
              expected->project_id);
    EXPECT_EQ(enablement.gemini_enterprise_settings->app_id, expected->app_id);
    EXPECT_EQ(enablement.gemini_enterprise_settings->location,
              expected->location);
  } else {
    EXPECT_FALSE(enablement.gemini_enterprise_settings.has_value());
  }
  EXPECT_EQ(enablement.EligibleForGeminiEnterpriseSettings(),
            expected.has_value());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GlicEnablingGeminiEnterpriseSettingsTest,
    testing::Values(
        GeminiEnterpriseSettingsParams{.feature_enabled = true,
                                       .expected_settings = std::nullopt},
        GeminiEnterpriseSettingsParams{.feature_enabled = true,
                                       .pref_settings = GetPrefSettings(),
                                       .expected_settings = GetPrefSettings()},
        GeminiEnterpriseSettingsParams{.feature_enabled = false,
                                       .pref_settings = GetPrefSettings(),
                                       .expected_settings = std::nullopt},
        GeminiEnterpriseSettingsParams{.feature_enabled = true,
                                       .pref_settings = GetPrefSettings(),
                                       .cmd_settings = GetCmdSettings(),
                                       .expected_settings = GetCmdSettings()},
        GeminiEnterpriseSettingsParams{.feature_enabled = false,
                                       .cmd_settings = GetCmdSettings(),
                                       .expected_settings = std::nullopt},
        GeminiEnterpriseSettingsParams{.feature_enabled = true,
                                       .cmd_settings = GetCmdSettings(),
                                       .expected_settings = GetCmdSettings()},
        GeminiEnterpriseSettingsParams{.feature_enabled = true,
                                       .is_enterprise = false,
                                       .pref_settings = GetPrefSettings(),
                                       .expected_settings = std::nullopt}));

class GlicEnablingGeminiEnterpriseSettingsErrorTest
    : public GlicEnablingProfileEligibilityTest {
 public:
  GlicEnablingGeminiEnterpriseSettingsErrorTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kGlicGeminiEnterpriseSettingsEnabled);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedCommandLine scoped_command_line_;
};

TEST_F(GlicEnablingGeminiEnterpriseSettingsErrorTest, InvalidJsonLogsError) {
  scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kGlicGeminiEnterpriseSettingsOverride, "invalid json");

  base::test::MockLog mock_log;
  EXPECT_CALL(
      mock_log,
      Log(logging::LOGGING_ERROR, testing::_, testing::_, testing::_,
          testing::HasSubstr("Gemini Enterprise settings override is not a "
                             "valid JSON dictionary.")))
      .Times(1);
  mock_log.StartCapturingLogs();

  EXPECT_EQ(GlicEnabling::GetGeminiEnterpriseSettings(profile()), std::nullopt);

  mock_log.StopCapturingLogs();
}

TEST_F(GlicEnablingGeminiEnterpriseSettingsErrorTest, MissingFieldsLogsError) {
  scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kGlicGeminiEnterpriseSettingsOverride,
      "{\"project_id\": \"p\"}");

  base::test::MockLog mock_log;
  EXPECT_CALL(
      mock_log,
      Log(logging::LOGGING_ERROR, testing::_, testing::_, testing::_,
          testing::HasSubstr("Gemini Enterprise settings override is missing "
                             "required fields.")))
      .Times(1);
  mock_log.StartCapturingLogs();

  EXPECT_EQ(GlicEnabling::GetGeminiEnterpriseSettings(profile()), std::nullopt);

  mock_log.StopCapturingLogs();
}

#endif

class GlicEnablingWebActuationToggleTest
    : public GlicEnablingProfileEligibilityTest {
 public:
  GlicEnablingWebActuationToggleTest() {
    feature_list_.InitAndEnableFeature(features::kGlicActor);
  }

 protected:
  base::test::ScopedCommandLine scoped_command_line_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(GlicEnablingWebActuationToggleTest, AlwaysShowSwitch) {
  scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
      switches::kGlicAlwaysShowWebActuationToggle);

  auto* glic_service = GlicKeyedService::Get(profile());
  EXPECT_TRUE(glic_service->enabling().ShouldShowWebActuationToggle());
}

TEST_F(GlicEnablingWebActuationToggleTest, FeatureDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kGlicWebActuationSetting);

  auto* glic_service = GlicKeyedService::Get(profile());
  EXPECT_FALSE(glic_service->enabling().ShouldShowWebActuationToggle());
}

TEST_F(GlicEnablingWebActuationToggleTest, CapabilityIneligible) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kGlicWarming);
  auto* identity_test_env = identity_test_env_adaptor_->identity_test_env();
  AccountInfo account_info = identity_test_env->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_can_use_model_execution_features(false);
  signin::UpdateAccountInfoForAccount(identity_test_env->identity_manager(),
                                      account_info);

  auto* glic_service = GlicKeyedService::Get(profile());
  EXPECT_FALSE(glic_service->enabling().ShouldShowWebActuationToggle());
}

TEST_F(GlicEnablingWebActuationToggleTest, ManagedProfile_CannotActOnWeb) {
  auto* identity_test_env = identity_test_env_adaptor_->identity_test_env();
  AccountInfo account_info = identity_test_env->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_can_use_model_execution_features(true);
  signin::UpdateAccountInfoForAccount(identity_test_env->identity_manager(),
                                      account_info);

  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);

  profile()->GetPrefs()->SetInteger(
      glic::prefs::kGlicActuationOnWeb,
      std::to_underlying(
          glic::prefs::GlicActuationOnWebPolicyState::kDisabled));
  profile()->GetPrefs()->SetInteger(
      subscription_eligibility::prefs::kAiSubscriptionTier, 1);

  auto* glic_service = GlicKeyedService::Get(profile());
  EXPECT_FALSE(glic_service->enabling().ShouldShowWebActuationToggle());
}

TEST_F(GlicEnablingWebActuationToggleTest, ManagedProfile_CanActOnWeb) {
  auto* identity_test_env = identity_test_env_adaptor_->identity_test_env();
  AccountInfo account_info = identity_test_env->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_can_use_model_execution_features(true);
  signin::UpdateAccountInfoForAccount(identity_test_env->identity_manager(),
                                      account_info);

  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);

  profile()->GetPrefs()->SetInteger(
      glic::prefs::kGlicActuationOnWeb,
      std::to_underlying(glic::prefs::GlicActuationOnWebPolicyState::kEnabled));
  profile()->GetPrefs()->SetInteger(
      subscription_eligibility::prefs::kAiSubscriptionTier, 1);

  auto* glic_service = GlicKeyedService::Get(profile());
  EXPECT_TRUE(glic_service->enabling().ShouldShowWebActuationToggle());
}

class GlicEnablingAnchorEntryPointCountryTest
    : public GlicEnablingAnchorEntryPointTestBase {
 public:
  GlicEnablingAnchorEntryPointCountryTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {
            {features::kGlicCountryFiltering, {{"disabled_countries", "zz"}}},
            {features::kGlicAnchorEntryPointForOnboardedUsers, {}},
            {features::kGlic, {}},
        },
        /*disabled_features=*/{});
  }

  void SetUp() override {
    variations::TestVariationsService::RegisterPrefs(local_state_.registry());
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        &local_state_, &enabled_state_provider_, std::wstring(),
        base::FilePath(), metrics::StartupVisibility::kUnknown);
    variations_service_ = std::make_unique<variations::TestVariationsService>(
        &local_state_, metrics_state_manager_.get());

    variations_service_->OverrideStoredPermanentCountry("zz");
    TestingBrowserProcess::GetGlobal()->SetVariationsService(
        variations_service_.get());

    GlicEnablingAnchorEntryPointTestBase::SetUp();
  }

  void TearDown() override {
    GlicEnablingAnchorEntryPointTestBase::TearDown();
    TestingBrowserProcess::GetGlobal()->SetVariationsService(nullptr);
    variations_service_.reset();
    metrics_state_manager_.reset();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple local_state_;
  metrics::TestEnabledStateProvider enabled_state_provider_{/*consent=*/false,
                                                            /*enabled=*/false};
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<variations::TestVariationsService> variations_service_;
};

TEST_F(GlicEnablingAnchorEntryPointCountryTest,
       AnchoredButtonForCountryDisabled) {
  profile()->GetPrefs()->SetInteger(
      glic::prefs::kGlicCompletedFre,
      static_cast<int>(glic::prefs::FreStatus::kCompleted));

  base::HistogramTester histogram_tester;

  // Profile should be eligible because the anchor entry point feature is active
  // and user is onboarded, even though the country is disabled.
  EXPECT_TRUE(GlicEnabling::IsProfileEligible(profile()));

  GlicEnabling::ProfileEnablement enablement =
      GlicEnabling::EnablementForProfile(profile());
  EXPECT_TRUE(enablement.ShouldShowGlicButton());
  EXPECT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kLocationMismatch);
  enablement.RecordStartupMetrics();
  enablement.RecordSteadyStateMetrics();

  // It should log the country disabled reason to the consolidated metric.
  using Reason =
      GlicEnabling::ProfileEnablement::AnchoredDespiteEligibilityReason;
  EXPECT_THAT(
      histogram_tester.GetAllSamplesForPrefix(
          "Glic.ProfileEnablement.AnchoredDespiteEligibilityReason"),
      testing::UnorderedElementsAre(
          testing::Pair(
              "Glic.ProfileEnablement.AnchoredDespiteEligibilityReason.Startup",
              base::BucketsAre(base::Bucket(Reason::kCountryDisabled, 1))),
          testing::Pair(
              "Glic.ProfileEnablement.AnchoredDespiteEligibilityReason."
              "SteadyState",
              base::BucketsAre(base::Bucket(Reason::kCountryDisabled, 1)))));
}

TEST_F(GlicEnablingAnchorEntryPointCountryTest,
       AnchoredButtonForMultipleIneligibilityReasons) {
  profile()->GetPrefs()->SetInteger(
      glic::prefs::kGlicCompletedFre,
      static_cast<int>(glic::prefs::FreStatus::kCompleted));

  // 1. Trigger account capability ineligibility.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  AccountInfo account_info =
      identity_manager->FindExtendedAccountInfoByAccountId(
          identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin));
  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_can_use_model_execution_features(false);
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);

  // 2. Country filtering is also disabled (overridden to "zz" in SetUp).

  base::HistogramTester histogram_tester;

  // Profile should be eligible because the anchor entry point feature is active
  // and user is onboarded, even though the country and account are ineligible.
  EXPECT_TRUE(GlicEnabling::IsProfileEligible(profile()));

  GlicEnabling::ProfileEnablement enablement =
      GlicEnabling::EnablementForProfile(profile());
  EXPECT_TRUE(enablement.ShouldShowGlicButton());

  // GetProfileReadyState prioritizes capability over country filtering, so it
  // should return kIneligibleAccount.
  EXPECT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kIneligibleAccount);

  enablement.RecordStartupMetrics();
  enablement.RecordSteadyStateMetrics();

  // Both ineligibility reasons should be logged to the consolidated metric.
  using Reason =
      GlicEnabling::ProfileEnablement::AnchoredDespiteEligibilityReason;
  EXPECT_THAT(
      histogram_tester.GetAllSamplesForPrefix(
          "Glic.ProfileEnablement.AnchoredDespiteEligibilityReason"),
      testing::UnorderedElementsAre(
          testing::Pair(
              "Glic.ProfileEnablement.AnchoredDespiteEligibilityReason.Startup",
              base::BucketsAre(
                  base::Bucket(Reason::kCountryDisabled, 1),
                  base::Bucket(Reason::kPrimaryAccountNotCapable, 1))),
          testing::Pair(
              "Glic.ProfileEnablement.AnchoredDespiteEligibilityReason."
              "SteadyState",
              base::BucketsAre(
                  base::Bucket(Reason::kCountryDisabled, 1),
                  base::Bucket(Reason::kPrimaryAccountNotCapable, 1)))));
}

class GlicEnablingCountryCheckTest : public GlicEnablingProfileEligibilityTest {
 public:
  GlicEnablingCountryCheckTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            features::kGlic,
            features::kGlicCountryFiltering,
            features::kGlicAnchorEntryPointForOnboardedUsers,
        },
        /*disabled_features=*/{features::kGlicUserStatusCheck});
  }

 protected:
  TestDelegate* SetProfileCountryDelegate(
      const std::string& permanent_country,
      const std::string& session_country = "") {
    auto delegate = std::make_unique<TestDelegate>();
    TestDelegate* raw_delegate = delegate.get();
    delegate->SetPermanentCountryCode(permanent_country);
    delegate->SetSessionCountryCode(session_country);
    g_browser_process->GetFeatures()
        ->glic_global_enabling()
        .UpdateStateForTesting(std::move(delegate));
    return raw_delegate;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(GlicEnablingCountryCheckTest, GlobalEnablingBypassesCountryFilter) {
  TestDelegate delegate;
  delegate.SetBothCountryCodes("zz");
  // Global criteria check does not evaluate country filtering, so it returns
  // true even for disabled countries.
  EXPECT_TRUE(GlicGlobalEnabling(delegate.Clone()).IsEnabledByGlobalCriteria());
}

TEST_F(GlicEnablingCountryCheckTest, NullProfileReturnsFalse) {
  EXPECT_FALSE(GlicEnabling::IsProfileEligible(nullptr));
  EXPECT_FALSE(GlicEnabling::EnablementForProfile(nullptr).IsEnabled());
}

TEST_F(GlicEnablingCountryCheckTest,
       CountryCheckEvaluationAndEnablementCaching) {
  auto& global_enabling =
      g_browser_process->GetFeatures()->glic_global_enabling();

  // 1. Initial check with a disabled country ("zz") returns false.
  TestDelegate* delegate = SetProfileCountryDelegate("zz");
  EXPECT_FALSE(global_enabling.IsCountryEnabled());

  // 2. Update country to an enabled one ("us"). Re-evaluates and returns true.
  delegate->SetPermanentCountryCode("us");
  EXPECT_TRUE(global_enabling.IsCountryEnabled());

  // 3. Update country back to "zz". Since true is cached on global_enabling,
  // it continues to return true without re-querying the delegate.
  delegate->SetPermanentCountryCode("zz");
  EXPECT_TRUE(global_enabling.IsCountryEnabled());

  // 4. Reset cache for testing. Evaluates against delegate ("zz") and returns
  // false.
  global_enabling.UpdateStateForTesting(delegate->Clone());
  EXPECT_FALSE(global_enabling.IsCountryEnabled());
}

TEST_F(GlicEnablingCountryCheckTest,
       CountryCheckEvaluationAndCountryCheckResultCaching) {
  base::HistogramTester histogram_tester;

  auto& global_enabling =
      g_browser_process->GetFeatures()->glic_global_enabling();

  // 1. Initial check with a disabled country ("zz"). Evaluates to
  // kBlockedNotInInclusionList and records the sample once.
  TestDelegate* delegate = SetProfileCountryDelegate("zz", "zz");
  EXPECT_FALSE(global_enabling.IsCountryEnabled());
  histogram_tester.ExpectUniqueSample(
      "Glic.CountryFilteringResult2",
      GlicFilteringResult::kBlockedNotInInclusionList, 1);

  // 2. Subsequent check with no change in the delegate's country. Enablement
  // check returns false again, but the histogram should not be recorded a
  // second time.
  EXPECT_FALSE(global_enabling.IsCountryEnabled());
  histogram_tester.ExpectUniqueSample(
      "Glic.CountryFilteringResult2",
      GlicFilteringResult::kBlockedNotInInclusionList, 1);

  // 3. Update permanent country to another disabled one ("yy") and checks
  // country enablement twice. Should re-record kBlockedNotInInclusionList once.
  delegate->SetPermanentCountryCode("yy");
  EXPECT_FALSE(global_enabling.IsCountryEnabled());
  EXPECT_FALSE(global_enabling.IsCountryEnabled());
  histogram_tester.ExpectBucketCount(
      "Glic.CountryFilteringResult2",
      GlicFilteringResult::kBlockedNotInInclusionList, 2);
  histogram_tester.ExpectTotalCount("Glic.CountryFilteringResult2", 2);

  // 4. Update session country to another disabled one ("xx") and checks
  // country enablement twice. Should re-record kBlockedNotInInclusionList once.
  delegate->SetSessionCountryCode("xx");
  EXPECT_FALSE(global_enabling.IsCountryEnabled());
  EXPECT_FALSE(global_enabling.IsCountryEnabled());
  histogram_tester.ExpectBucketCount(
      "Glic.CountryFilteringResult2",
      GlicFilteringResult::kBlockedNotInInclusionList, 3);
  histogram_tester.ExpectTotalCount("Glic.CountryFilteringResult2", 3);

  // 5. Update country to an enabled one ("us"). Two country checks return true,
  // but only a single sample is recorded to another bucket of the histogram.
  delegate->SetPermanentCountryCode("us");
  EXPECT_TRUE(global_enabling.IsCountryEnabled());
  EXPECT_TRUE(global_enabling.IsCountryEnabled());
  histogram_tester.ExpectBucketCount(
      "Glic.CountryFilteringResult2",
      GlicFilteringResult::kAllowedInInclusionList, 1);
  histogram_tester.ExpectTotalCount("Glic.CountryFilteringResult2", 4);

  // 6. Reset cache and re-evaluate with "zz" again. Should re-record
  // kBlockedNotInInclusionList since cache was cleared.
  delegate->SetPermanentCountryCode("zz");
  global_enabling.UpdateStateForTesting(delegate->Clone());
  EXPECT_FALSE(global_enabling.IsCountryEnabled());
  histogram_tester.ExpectBucketCount(
      "Glic.CountryFilteringResult2",
      GlicFilteringResult::kBlockedNotInInclusionList, 4);
  histogram_tester.ExpectTotalCount("Glic.CountryFilteringResult2", 5);
}

TEST_F(GlicEnablingCountryCheckTest,
       ProfileReadyState_LocationMismatchWhenAnchored) {
  // The pref change triggers observers, so the country must be set beforehand.
  SetProfileCountryDelegate("zz");
  profile()->GetPrefs()->SetInteger(
      glic::prefs::kGlicCompletedFre,
      static_cast<int>(glic::prefs::FreStatus::kCompleted));

  auto* identity_test_env = identity_test_env_adaptor_->identity_test_env();
  AccountInfo account_info = identity_test_env->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_can_use_model_execution_features(true);
  signin::UpdateAccountInfoForAccount(identity_test_env->identity_manager(),
                                      account_info);

  GlicEnabling::ProfileEnablement enablement =
      GlicEnabling::EnablementForProfile(profile());
  EXPECT_FALSE(enablement.allowed_by_country_filter);
  EXPECT_FALSE(enablement.IsEnabled());
  EXPECT_TRUE(enablement.ShouldShowGlicButton());
  EXPECT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kLocationMismatch);
}

TEST_F(GlicEnablingCountryCheckTest,
       ProfileReadyState_IneligibleWhenNotAnchored) {
  // The pref change triggers observers, so the country must be set beforehand.
  SetProfileCountryDelegate("zz");
  profile()->GetPrefs()->SetInteger(
      glic::prefs::kGlicCompletedFre,
      static_cast<int>(glic::prefs::FreStatus::kIncomplete));

  auto* identity_test_env = identity_test_env_adaptor_->identity_test_env();
  AccountInfo account_info = identity_test_env->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_can_use_model_execution_features(true);
  signin::UpdateAccountInfoForAccount(identity_test_env->identity_manager(),
                                      account_info);

  GlicEnabling::ProfileEnablement enablement =
      GlicEnabling::EnablementForProfile(profile());
  EXPECT_FALSE(enablement.allowed_by_country_filter);
  EXPECT_FALSE(enablement.IsEnabled());
  EXPECT_FALSE(enablement.ShouldShowGlicButton());
  EXPECT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kIneligible);
}

class GlicEnablingRecoveryMetricsTest
    : public GlicEnablingProfileReadyStateTestBase {};

TEST_F(GlicEnablingRecoveryMetricsTest, RecoveryFromSignInRequired) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kGlicShowForSignedOut);

  // 1. Initial state: Ready (since primary account is capable and signed in)
  ASSERT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kReady);

  base::HistogramTester histogram_tester;

  // 2. Transition to kSignInRequired by clearing the primary account
#if BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS we cannot clear primary account easily, so we skip
  // kSignInRequired tests on ChromeOS.
#else
  auto* identity_test_env = identity_test_env_adaptor_->identity_test_env();
  identity_test_env->ClearPrimaryAccount();

  // Verify we are in kSignInRequired
  ASSERT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kSignInRequired);

  // 3. Recover by signing back in
  AccountInfo account_info = identity_test_env->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_can_use_model_execution_features(true);
  signin::UpdateAccountInfoForAccount(identity_test_env->identity_manager(),
                                      account_info);

  // Verify we transitioned back to kReady
  ASSERT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kReady);

  // Verify metric is not logged yet in the background
  histogram_tester.ExpectTotalCount("Glic.ProfileEnablement.RecoveredFromState",
                                    0);

  // 4. Trigger user interaction to log the recovery metric
  glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile(), /*create=*/true)
      ->enabling()
      .MaybeRecordRecoveryOnInteraction();

  histogram_tester.ExpectBucketCount(
      "Glic.ProfileEnablement.RecoveredFromState",
      mojom::ProfileReadyState::kSignInRequired, 1);

  // Subsequent interactions should not log it again
  glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile(), /*create=*/true)
      ->enabling()
      .MaybeRecordRecoveryOnInteraction();
  histogram_tester.ExpectTotalCount("Glic.ProfileEnablement.RecoveredFromState",
                                    1);
#endif
}

TEST_F(GlicEnablingRecoveryMetricsTest, RecoveryFromIneligibleAccount) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kGlicAnchorEntryPointForOnboardedUsers);

  profile()->GetPrefs()->SetInteger(
      glic::prefs::kGlicCompletedFre,
      static_cast<int>(glic::prefs::FreStatus::kCompleted));

  // 1. Initial state: Ready
  ASSERT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kReady);

  base::HistogramTester histogram_tester;

  // 2. Transition to kIneligibleAccount by disabling capabilities
  auto* identity_test_env = identity_test_env_adaptor_->identity_test_env();
  AccountInfo account_info =
      identity_test_env->identity_manager()->FindExtendedAccountInfoByAccountId(
          identity_test_env->identity_manager()->GetPrimaryAccountId(
              signin::ConsentLevel::kSignin));
  {
    AccountCapabilitiesTestMutator mutator(&account_info);
    mutator.set_can_use_model_execution_features(false);
    signin::UpdateAccountInfoForAccount(identity_test_env->identity_manager(),
                                        account_info);
  }

  // Verify we are in kIneligibleAccount
  ASSERT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kIneligibleAccount);

  // 3. Recover by enabling capabilities
  {
    AccountCapabilitiesTestMutator mutator(&account_info);
    mutator.set_can_use_model_execution_features(true);
    signin::UpdateAccountInfoForAccount(identity_test_env->identity_manager(),
                                        account_info);
  }

  // Verify we transitioned back to kReady
  ASSERT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kReady);

  // Verify metric is not logged yet in the background
  histogram_tester.ExpectTotalCount("Glic.ProfileEnablement.RecoveredFromState",
                                    0);

  // 4. Trigger user interaction to log the recovery metric
  glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile(), /*create=*/true)
      ->enabling()
      .MaybeRecordRecoveryOnInteraction();

  histogram_tester.ExpectBucketCount(
      "Glic.ProfileEnablement.RecoveredFromState",
      mojom::ProfileReadyState::kIneligibleAccount, 1);

  // Subsequent interactions should not log it again
  glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile(), /*create=*/true)
      ->enabling()
      .MaybeRecordRecoveryOnInteraction();
  histogram_tester.ExpectTotalCount("Glic.ProfileEnablement.RecoveredFromState",
                                    1);
}

TEST_F(GlicEnablingRecoveryMetricsTest, RecoveryFromLocationMismatch) {
  // 1. Destroy the existing GlicKeyedService instance.
  glic::GlicKeyedServiceFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
        return glic::GlicKeyedServiceFactory::GetInstance()
            ->BuildServiceInstanceForBrowserContext(context);
      }));

  // 2. Set the "last ready state" pref to kLocationMismatch to simulate that
  // Glic was in a location mismatch state in the previous session.
  profile()->GetPrefs()->SetInteger(
      glic::prefs::kGlicLastProfileReadyState,
      static_cast<int>(mojom::ProfileReadyState::kLocationMismatch));

  base::HistogramTester histogram_tester;

  // 3. Trigger service construction by passing create=true. The constructor
  // loads the pref (kLocationMismatch) and sets up state.
  glic::GlicKeyedService* service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile(),
                                                         /*create=*/true);

  // Verify we transitioned back to kReady
  ASSERT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kReady);

  // Verify metric is not logged yet on initialization
  histogram_tester.ExpectTotalCount("Glic.ProfileEnablement.RecoveredFromState",
                                    0);

  // 4. Trigger user interaction to log the recovery metric
  service->enabling().MaybeRecordRecoveryOnInteraction();

  histogram_tester.ExpectBucketCount(
      "Glic.ProfileEnablement.RecoveredFromState",
      mojom::ProfileReadyState::kLocationMismatch, 1);

  // Subsequent interactions should not log it again
  service->enabling().MaybeRecordRecoveryOnInteraction();
  histogram_tester.ExpectTotalCount("Glic.ProfileEnablement.RecoveredFromState",
                                    1);
}

TEST_F(GlicEnablingRecoveryMetricsTest, RecoveryFromDisabledByAdmin) {
  // 1. Destroy the existing GlicKeyedService instance.
  glic::GlicKeyedServiceFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
        return glic::GlicKeyedServiceFactory::GetInstance()
            ->BuildServiceInstanceForBrowserContext(context);
      }));

  // 2. Set the "last ready state" pref to kDisabledByAdmin to simulate that
  // Glic was disabled by policy in the previous session.
  profile()->GetPrefs()->SetInteger(
      glic::prefs::kGlicLastProfileReadyState,
      static_cast<int>(mojom::ProfileReadyState::kDisabledByAdmin));

  base::HistogramTester histogram_tester;

  // 3. Trigger service construction by passing create=true. The constructor
  // loads the pref (kDisabledByAdmin) and sets up state.
  glic::GlicKeyedService* service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile(),
                                                         /*create=*/true);

  // Verify metric is not logged yet on initialization
  histogram_tester.ExpectTotalCount("Glic.ProfileEnablement.RecoveredFromState",
                                    0);

  // 4. Trigger user interaction to log the recovery metric
  service->enabling().MaybeRecordRecoveryOnInteraction();

  histogram_tester.ExpectBucketCount(
      "Glic.ProfileEnablement.RecoveredFromState",
      mojom::ProfileReadyState::kDisabledByAdmin, 1);

  // Subsequent interactions should not log it again
  service->enabling().MaybeRecordRecoveryOnInteraction();
  histogram_tester.ExpectTotalCount("Glic.ProfileEnablement.RecoveredFromState",
                                    1);
}
}  // namespace
}  // namespace glic
