// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_enabling.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/mock_log.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/actor/glic_actor_policy_checker.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_types.h"
#include "chrome/browser/glic/gemini_enterprise/geic_fre.h"
#include "chrome/browser/glic/gemini_enterprise/geic_pref_names.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_pref_names_internal.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_features.mojom-features.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_fre_store.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_provider_latch.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/prefs/browser_prefs.h"
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
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/subscription_eligibility/subscription_eligibility_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/variations/service/test_variations_service.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/variations_switches.h"
#include "content/public/common/content_switches.h"
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
#include "chrome/test/base/scoped_metrics_service_for_synthetic_trials.h"  // nogncheck
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/local_device_info_provider_impl.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/synthetic_trial_registry.h"
#include "components/variations/variations_test_utils.h"
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

  // Grants test access to protected base class methods.
  using GlicEnablingDelegate::GetCountryEnablement;
  using GlicEnablingDelegate::GetCountryEnablementForMinorAccounts;

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
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kTestType);
#if BUILDFLAG(IS_ANDROID)
    if (base::android::android_info::sdk_int() <
        base::android::android_info::SDK_VERSION_S) {
      GTEST_SKIP() << "Glic requires Android S+";
    }
#endif
    // IsSystemRequirementMet() caches its answer in a function-local static,
    // so it is settled by whichever test in the process asks first and does
    // not see the feature state set up below. Pin it rather than depending on
    // the order tests happen to run in.
    GlicEnabling::SetSystemRequirementMetForTesting(true);

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
        {features::kSmartRestart, features::kSmartRestartMetrics}
#endif
    );
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    GlicEnabling::SetSystemRequirementMetForTesting(std::nullopt);
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
      "Glic.CountryFilteringResult2.Adult",
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
      "Glic.CountryFilteringResult2.Adult",
      GlicFilteringResult::kAllowedInInclusionList, 2);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult2.Adult",
      GlicFilteringResult::kBlockedNotInInclusionList, 1);
  histogram_tester_->ExpectTotalCount("Glic.CountryFilteringResult2.Adult", 3);
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
      "Glic.CountryFilteringResult2.Adult",
      GlicFilteringResult::kAllowedInInclusionList, 2);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult2.Adult",
      GlicFilteringResult::kBlockedNotInInclusionList, 1);
  histogram_tester_->ExpectTotalCount("Glic.CountryFilteringResult2.Adult", 3);
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
      "Glic.CountryFilteringResult2.Adult",
      GlicFilteringResult::kAllowedInInclusionList, 2);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult2.Adult",
      GlicFilteringResult::kBlockedInExclusionList, 1);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult2.Adult",
      GlicFilteringResult::kBlockedNotInInclusionList, 1);
  histogram_tester_->ExpectTotalCount("Glic.CountryFilteringResult2.Adult", 4);
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
      "Glic.CountryFilteringResult2.Adult",
      GlicFilteringResult::kAllowedInInclusionList, 2);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult2.Adult",
      GlicFilteringResult::kBlockedInExclusionList, 1);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult2.Adult",
      GlicFilteringResult::kBlockedNotInInclusionList, 1);
  histogram_tester_->ExpectTotalCount("Glic.CountryFilteringResult2.Adult", 4);
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
      "Glic.CountryFilteringResult2.Adult",
      GlicFilteringResult::kBlockedInExclusionList, 2);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult2.Adult",
      GlicFilteringResult::kAllowedInInclusionList, 2);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult2.Adult",
      GlicFilteringResult::kBlockedNotInInclusionList, 1);
  histogram_tester_->ExpectTotalCount("Glic.CountryFilteringResult2.Adult", 5);
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
      "Glic.CountryFilteringResult2.Adult",
      GlicFilteringResult::kBlockedInExclusionList, 1);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult2.Adult",
      GlicFilteringResult::kAllowedInInclusionList, 2);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult2.Adult",
      GlicFilteringResult::kBlockedNotInInclusionList, 2);
  histogram_tester_->ExpectTotalCount("Glic.CountryFilteringResult2.Adult", 5);
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
      "Glic.CountryFilteringResult2.Adult",
      GlicFilteringResult::kAllowedWildcardInclusion, 2);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult2.Adult",
      GlicFilteringResult::kBlockedInExclusionList, 3);
  histogram_tester_->ExpectTotalCount("Glic.CountryFilteringResult2.Adult", 5);
}

TEST_F(GlicEnablingTest, CountryMinorFilteringNotEnabledBlocksAll) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kGlicMinorCountryFiltering);
  EXPECT_FALSE(TestDelegate::GetCountryEnablementForMinorAccounts("us", "us"));
  EXPECT_FALSE(TestDelegate::GetCountryEnablementForMinorAccounts("zz", "zz"));
  histogram_tester_->ExpectUniqueSample(
      "Glic.CountryFilteringResult2.Minor",
      GlicFilteringResult::kBlockedNotInInclusionList, 2);
}

TEST_F(GlicEnablingTest, CountryMinorFilteringEnabledWithDefaultParams) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kGlicMinorCountryFiltering);
  EXPECT_FALSE(TestDelegate::GetCountryEnablementForMinorAccounts("zz", "zz"));
}

TEST_F(GlicEnablingTest, CountryMinorFilteringEnabledWithLists) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kGlicMinorCountryFiltering,
      {{"disabled_countries", "zz"}, {"enabled_countries", "us,uk,zz"}});

  EXPECT_TRUE(TestDelegate::GetCountryEnablementForMinorAccounts("us", ""));
  EXPECT_TRUE(TestDelegate::GetCountryEnablementForMinorAccounts("", "UK"));
  EXPECT_FALSE(TestDelegate::GetCountryEnablementForMinorAccounts("zz", "us"));
  EXPECT_FALSE(TestDelegate::GetCountryEnablementForMinorAccounts("qq", "qq"));

  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult2.Minor",
      GlicFilteringResult::kAllowedInInclusionList, 2);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult2.Minor",
      GlicFilteringResult::kBlockedInExclusionList, 1);
  histogram_tester_->ExpectBucketCount(
      "Glic.CountryFilteringResult2.Minor",
      GlicFilteringResult::kBlockedNotInInclusionList, 1);
  histogram_tester_->ExpectTotalCount("Glic.CountryFilteringResult2.Minor", 4);
}

TEST_F(GlicEnablingTest, CountryMinorFilteringEnabledWithStar) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kGlicMinorCountryFiltering,
      {{"disabled_countries", "zz"}, {"enabled_countries", "*"}});

  EXPECT_TRUE(TestDelegate::GetCountryEnablementForMinorAccounts("ru", "ru"));
  EXPECT_FALSE(TestDelegate::GetCountryEnablementForMinorAccounts("zz", "zz"));
}

TEST_F(GlicEnablingTest, CountryMinorFilteringIndependentOfAdultFiltering) {
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {{features::kGlicCountryFiltering, {{"enabled_countries", "us,ca"}}},
       {features::kGlicMinorCountryFiltering, {{"enabled_countries", "us"}}}},
      {});

  EXPECT_TRUE(TestDelegate::GetCountryEnablement("ca", "ca"));
  EXPECT_FALSE(TestDelegate::GetCountryEnablementForMinorAccounts("ca", "ca"));
  EXPECT_TRUE(TestDelegate::GetCountryEnablement("us", "us"));
  EXPECT_TRUE(TestDelegate::GetCountryEnablementForMinorAccounts("us", "us"));

  // Disabling adult country filtering does not affect minor filtering.
  base::test::ScopedFeatureList adult_filter_disabled;
  adult_filter_disabled.InitAndDisableFeature(features::kGlicCountryFiltering);
  EXPECT_TRUE(TestDelegate::GetCountryEnablement("ca", "ca"));
  EXPECT_FALSE(TestDelegate::GetCountryEnablementForMinorAccounts("ca", "ca"));
}

base::flat_set<std::string> SplitCountryList(std::string_view country_list) {
  return base::flat_set<std::string>(base::SplitString(
      country_list, ", ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY));
}

// The default minor country list must be a subset of the default country list.
TEST(GlicEnablingCountryListTest, MinorCountriesAreSubsetOfAllCountries) {
  const base::flat_set<std::string> all_countries =
      SplitCountryList(kDefaultEnabledCountries);
  const base::flat_set<std::string> minor_countries =
      SplitCountryList(kDefaultEnabledCountriesMinor);

  for (const std::string& country : minor_countries) {
    EXPECT_TRUE(all_countries.contains(country))
        << "Minor country '" << country
        << "' is missing from kDefaultEnabledCountries.";
  }
}

// Change detector for the countries where Glic is enabled by default for
// adults only (i.e. in kDefaultEnabledCountries but not in
// kDefaultEnabledCountriesMinor).
//
// If this test fails, kDefaultEnabledCountries or kDefaultEnabledCountriesMinor
// has changed. Please consider whether the other list should also be updated
// (e.g. whether a newly launched country should also be launched to minor
// users), then update `kExpectedAdultOnlyCountries` below to match.
TEST(GlicEnablingCountryListTest, AdultOnlyCountriesChangeDetector) {
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  // Minor access is enabled by default on ChromeOS, Mac, and Windows.
  constexpr std::string_view kExpectedAdultOnlyCountries =
      // Phase 2
      "id,kr,nc,pf,ph,pn,sg,th,tw,vn,wf,"
      // Phase 3a
      "ag,ar,bb,bo,br,bs,bz,cl,co,cr,dm,do,ec,gd,gt,gy,hn,ht,jm,kn,lc,mx,ni,pa,"
      "pe,py,sr,sv,tt,uy,vc,ve,"
      // Phase 3b
      "ae,am,ao,aq,az,ba,bf,bh,bi,bj,bw,cd,cf,cg,ci,cm,cv,dj,dz,eg,eh,er,et,ga,"
      "ge,gh,gm,gn,gq,gw,il,iq,jo,ke,kg,km,kw,kz,lb,lr,ls,ly,ma,md,me,mg,mk,ml,"
      "mr,mu,mw,mz,na,ne,ng,om,pr,ps,qa,rs,rw,sa,sc,sd,sl,sn,so,ss,st,sz,td,tg,"
      "tj,tm,tn,tz,ua,ug,um,uz,vi,xk,ye,za,zm,zw,"
      // Phase 4
      "ai,bm,fk,gb,gg,gi,gs,im,io,je,ky,ms,sh,tc,vg";
#else
  // Minor access is not enabled by default on other platforms.
  constexpr std::string_view kExpectedAdultOnlyCountries =
      kDefaultEnabledCountries;
#endif

  const base::flat_set<std::string> all_countries =
      SplitCountryList(kDefaultEnabledCountries);
  const base::flat_set<std::string> minor_countries =
      SplitCountryList(kDefaultEnabledCountriesMinor);

  std::vector<std::string> adult_only_countries;
  std::ranges::set_difference(all_countries, minor_countries,
                              std::back_inserter(adult_only_countries));

  EXPECT_THAT(adult_only_countries,
              testing::UnorderedElementsAreArray(
                  SplitCountryList(kExpectedAdultOnlyCountries)));
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
            features::kSmartRestart,
            features::kSmartRestartMetrics,
#endif
        });
    // By default, assume the user is in a country with minor access.
    minor_feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicMinorCountryFiltering, {{"enabled_countries", "*"}});
  }
  ~GlicEnablingProfileEligibilityTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kTestType);
#if BUILDFLAG(IS_ANDROID)
    if (base::android::android_info::sdk_int() <
        base::android::android_info::SDK_VERSION_S) {
      GTEST_SKIP() << "Glic requires Android S+";
    }
#endif
    // IsSystemRequirementMet() caches its answer in a function-local static,
    // so it is settled by whichever test in the process asks first and does
    // not see the feature state set up above. Pin it rather than depending on
    // the order tests happen to run in.
    GlicEnabling::SetSystemRequirementMetForTesting(true);

    raw_ptr<TestingProfileManager> testing_profile_manager =
        TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
            /*profile_manager=*/true);

#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PreProfileSetUp(
        testing_profile_manager->profile_manager());
#endif  // BUILDFLAG(IS_CHROMEOS)

    profile_ = testing_profile_manager->CreateTestingProfile(
        TestingProfile::kDefaultProfileUserName, CreatePrefService(),
        base::UTF8ToUTF16(TestingProfile::kDefaultProfileUserName),
        /*avatar_id=*/0,
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
    GlicEnabling::SetSystemRequirementMetForTesting(std::nullopt);
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
    task_environment_.RunUntilIdle();
  }

 protected:
  // Returns the pref service for the test profile, or null to use the default
  // testing pref service. Override to set prefs that must be in place before
  // the profile's keyed services are created.
  virtual std::unique_ptr<sync_preferences::PrefServiceSyncable>
  CreatePrefService() {
    return nullptr;
  }

  Profile* profile() { return profile_.get(); }
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedFeatureList minor_feature_list_;
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
  SetGlicCapability(mutator, true);
  signin::UpdateAccountInfoForAccount(identity_test_env->identity_manager(),
                                      account_info);

  // 3. Now they should be eligible, and not previously not allowed.
  EXPECT_FALSE(GlicEnabling::WasPreviouslyNotAllowed(profile()));

  // 4. Become ineligible while signed in.
  SetGlicCapability(mutator, false);
  signin::UpdateAccountInfoForAccount(identity_test_env->identity_manager(),
                                      account_info);

  // 5. They are now ineligible but still signed in, so previously not allowed
  // should be true.
  EXPECT_TRUE(GlicEnabling::WasPreviouslyNotAllowed(profile()));

  // 6. Make them eligible again.
  SetGlicCapability(mutator, true);
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
  SetGlicCapability(mutator, true);
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
  SetGlicCapability(mutator, false);
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
  SetGlicCapability(mutator, true);
  signin::UpdateAccountInfoForAccount(identity_test_env->identity_manager(),
                                      account_info);

  EXPECT_FALSE(GlicEnabling::IsEnabledForFirstRunProfile(profile(), "zz", "zz",
                                                         account_info));
  EXPECT_TRUE(GlicEnabling::IsEnabledForFirstRunProfile(profile(), "us", "us",
                                                        account_info));
}

TEST_F(GlicEnablingProfileEligibilityTest, IsEnabledForFirstRunProfileU18) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kGlicMinorCountryFiltering, {{"enabled_countries", "*"}});

  auto* identity_test_env = identity_test_env_adaptor_->identity_test_env();
  AccountInfo account_info = identity_test_env->MakePrimaryAccountAvailable(
      /*email=*/"test@example.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info);
  // User is eligible for Gemini in Chrome, but is U18 (cannot use adult
  // features).
  mutator.set_can_use_gemini_in_chrome(true);
  mutator.set_can_use_model_execution_features(false);
  signin::UpdateAccountInfoForAccount(identity_test_env->identity_manager(),
                                      account_info);

  // Overall profile is enabled for Gemini in Chrome.
  EXPECT_TRUE(GlicEnabling::IsEnabledForProfile(profile()));
  // But FRE should NOT be enabled for U18 users.
  EXPECT_FALSE(GlicEnabling::IsEnabledForFirstRunProfile(
      profile(), /*permanent_country=*/"us", /*session_country=*/"us",
      account_info));
}

class GlicEnablingProfileReadyStateTestBase
    : public GlicEnablingProfileEligibilityTest {
 public:
  explicit GlicEnablingProfileReadyStateTestBase(
      const std::vector<base::test::FeatureRef>& extra_enabled_features = {},
      const std::vector<base::test::FeatureRef>& extra_disabled_features = {},
      bool actor_policy_control_exemption = true) {
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

    // The test account is not eligible to act on the web, which would make
    // experimental triggering unavailable. Exempt it by default so tests can
    // exercise the other conditions.
    (actor_policy_control_exemption ? enabled_features : disabled_features)
        .push_back(features::kGlicActorPolicyControlExemption);

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kTestType);
    GlicEnablingProfileEligibilityTest::SetUp();
    if (IsSkipped()) {
      return;
    }

    // Override platform management to NONE so tests pass consistently on
    // managed developer machines.
    scoped_platform_management_override_ =
        std::make_unique<policy::ScopedManagementServiceOverrideForTesting>(
            policy::ManagementServiceFactory::GetInstance()->GetForPlatform(),
            policy::EnterpriseManagementAuthority::NONE);

    // Make sure we have a primary account so we don't fail the "capable" check.
    auto* identity_test_env = identity_test_env_adaptor_->identity_test_env();
    AccountInfo account_info = identity_test_env->MakePrimaryAccountAvailable(
        "test@example.com", signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info);
    SetGlicCapability(mutator, true);
    signin::UpdateAccountInfoForAccount(identity_test_env->identity_manager(),
                                        account_info);
  }

 private:
  std::unique_ptr<policy::ScopedManagementServiceOverrideForTesting>
      scoped_platform_management_override_;
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
            features::kSmartRestart,
            features::kSmartRestartMetrics,
#endif
        });
  }

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kTestType);
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
    SetGlicCapability(mutator, true);
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
  SetGlicCapability(mutator, false);
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

// GEiC profiles don't anchor the entry point, since the anchored error state is
// specific to consumer Gemini in Chrome (b/576834093). Without
// `features::kGeicFre`, GEiC profiles are always consented.
class GlicEnablingGeicAnchorEntryPointTest
    : public GlicEnablingAnchorEntryPointTestBase {
 public:
  GlicEnablingGeicAnchorEntryPointTest() {
    // Enables GEiC before `SetUp()` creates the profile, which latches the
    // Glic provider.
    geic_feature_list_.InitWithFeaturesAndParameters(
        {{features::kGeic,
          {{"enabled", "true"},
           {"geic-guest-url",
            "https://business.gemini.google/side-panel?configId=abc"}}}},
        {features::kGeicFre});
  }

  void SetUp() override {
    GlicEnablingAnchorEntryPointTestBase::SetUp();
    if (IsSkipped()) {
      return;
    }
    // Only GEiC profiles are consented without completing an FRE, so this also
    // checks that the profile is in GEiC mode.
    ASSERT_TRUE(GlicEnabling::HasConsentedForProfile(profile()));
  }

 private:
  base::test::ScopedFeatureList geic_feature_list_;
};

TEST_F(GlicEnablingGeicAnchorEntryPointTest,
       NotAnchoredWhenGlobalCriteriaFail) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      features::kGlicAnchorEntryPointForOnboardedUsers);

  // kGlic is disabled, so global criteria fail.
  EXPECT_FALSE(GlicEnabling::IsProfileEligible(profile()));
  GlicEnabling::ProfileEnablement enablement =
      GlicEnabling::EnablementForProfile(profile());
  EXPECT_FALSE(enablement.anchor_entrypoint_override_active);
  EXPECT_FALSE(enablement.ShouldShowGlicButton());
}

TEST_F(GlicEnablingGeicAnchorEntryPointTest, NotAnchoredForCountryDisabled) {
  auto delegate = std::make_unique<TestDelegate>();
  delegate->SetBothCountryCodes("zz");
  g_browser_process->GetFeatures()
      ->glic_global_enabling()
      .UpdateStateForTesting(std::move(delegate));

  base::test::ScopedFeatureList features;
  features.InitWithFeatures({features::kGlicAnchorEntryPointForOnboardedUsers,
                             features::kGlic, features::kGlicCountryFiltering},
                            {});

  GlicEnabling::ProfileEnablement enablement =
      GlicEnabling::EnablementForProfile(profile());
  EXPECT_FALSE(enablement.allowed_by_country_filter);
  EXPECT_FALSE(enablement.anchor_entrypoint_override_active);
  EXPECT_FALSE(enablement.ShouldShowGlicButton());
  EXPECT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kIneligible);
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
  SetGlicCapability(mutator, false);
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
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kTestType);
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
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kTestType);
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
  // The actor policy control exemption makes the experimental triggering state
  // depend only on the opt-in prefs, so that toggling them changes it.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kGlicExperimentalTriggering,
                            features::kGlicActorPolicyControlExemption},
      /*disabled_features=*/{});

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

#if !BUILDFLAG(IS_ANDROID)
TEST_F(GlicEnablingProfileEligibilityTest,
       DefaultHotkeyScopeSyntheticTrial_Unconsented) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicHotkeyLocalScope);

  ScopedMetricsServiceForSyntheticTrials metrics_service(
      TestingBrowserProcess::GetGlobal());
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();
  local_state->SetInteger(
      prefs::kGlicDefaultHotkeyScope,
      std::to_underlying(prefs::DefaultHotkeyScope::kLocal));

  // Without FRE completed, no synthetic trial is registered (even when
  // GlicKeyedService is constructed).
  glic::GlicKeyedService::Get(profile());
  EXPECT_FALSE(variations::ContainsTrialName(
      metrics_service.Get()
          ->GetSyntheticTrialRegistry()
          ->GetCurrentSyntheticFieldTrialsForTest(),
      kGlicHotkeyDefaultScopeTrialName));
}

TEST_F(GlicEnablingProfileEligibilityTest,
       DefaultHotkeyScopeSyntheticTrial_ConsentedStartup) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicHotkeyLocalScope);

  ScopedMetricsServiceForSyntheticTrials metrics_service(
      TestingBrowserProcess::GetGlobal());
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();
  local_state->SetInteger(
      prefs::kGlicDefaultHotkeyScope,
      std::to_underlying(prefs::DefaultHotkeyScope::kLocal));

  profile()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      std::to_underlying(prefs::FreStatus::kCompleted));

  // Accessing GlicKeyedService creates it, registering the trial on startup.
  glic::GlicKeyedService::Get(profile());

  EXPECT_TRUE(variations::ContainsTrialAndGroupName(
      metrics_service.Get()
          ->GetSyntheticTrialRegistry()
          ->GetCurrentSyntheticFieldTrialsForTest(),
      kGlicHotkeyDefaultScopeTrialName, kGlicHotkeyDefaultLocalScopeGroupName));
}

TEST_F(GlicEnablingProfileEligibilityTest,
       DefaultHotkeyScopeSyntheticTrial_ConsentTriggersRegistration) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicHotkeyLocalScope);

  ScopedMetricsServiceForSyntheticTrials metrics_service(
      TestingBrowserProcess::GetGlobal());
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();
  local_state->SetInteger(
      prefs::kGlicDefaultHotkeyScope,
      std::to_underlying(prefs::DefaultHotkeyScope::kLocal));

  // Completing FRE triggers UpdateConsentStatus(), which registers the trial.
  glic::GlicKeyedService::Get(profile())->enabling().SetCompletedFre(
      prefs::FreStatus::kCompleted);

  EXPECT_TRUE(variations::ContainsTrialAndGroupName(
      metrics_service.Get()
          ->GetSyntheticTrialRegistry()
          ->GetCurrentSyntheticFieldTrialsForTest(),
      kGlicHotkeyDefaultScopeTrialName, kGlicHotkeyDefaultLocalScopeGroupName));
}

TEST_F(GlicEnablingProfileEligibilityTest,
       DefaultHotkeyScopeSyntheticTrial_GlobalScope) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicHotkeyLocalScope);

  ScopedMetricsServiceForSyntheticTrials metrics_service(
      TestingBrowserProcess::GetGlobal());
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();
  local_state->SetInteger(
      prefs::kGlicDefaultHotkeyScope,
      std::to_underlying(prefs::DefaultHotkeyScope::kGlobal));

  glic::GlicKeyedService::Get(profile())->enabling().SetCompletedFre(
      prefs::FreStatus::kCompleted);

  EXPECT_TRUE(variations::ContainsTrialAndGroupName(
      metrics_service.Get()
          ->GetSyntheticTrialRegistry()
          ->GetCurrentSyntheticFieldTrialsForTest(),
      kGlicHotkeyDefaultScopeTrialName,
      kGlicHotkeyDefaultGlobalScopeGroupName));
}

TEST_F(GlicEnablingProfileEligibilityTest,
       DefaultHotkeyScopeSyntheticTrial_NotMigrated) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicHotkeyLocalScope);

  ScopedMetricsServiceForSyntheticTrials metrics_service(
      TestingBrowserProcess::GetGlobal());
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();
  local_state->SetInteger(
      prefs::kGlicDefaultHotkeyScope,
      std::to_underlying(prefs::DefaultHotkeyScope::kNotMigrated));

  glic::GlicKeyedService::Get(profile())->enabling().SetCompletedFre(
      prefs::FreStatus::kCompleted);

  EXPECT_FALSE(variations::ContainsTrialName(
      metrics_service.Get()
          ->GetSyntheticTrialRegistry()
          ->GetCurrentSyntheticFieldTrialsForTest(),
      kGlicHotkeyDefaultScopeTrialName));
}

TEST_F(GlicEnablingProfileEligibilityTest,
       DefaultHotkeyScopeSyntheticTrial_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kGlicHotkeyLocalScope);

  ScopedMetricsServiceForSyntheticTrials metrics_service(
      TestingBrowserProcess::GetGlobal());
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();
  local_state->SetInteger(
      prefs::kGlicDefaultHotkeyScope,
      std::to_underlying(prefs::DefaultHotkeyScope::kLocal));

  glic::GlicKeyedService::Get(profile())->enabling().SetCompletedFre(
      prefs::FreStatus::kCompleted);

  EXPECT_FALSE(variations::ContainsTrialName(
      metrics_service.Get()
          ->GetSyntheticTrialRegistry()
          ->GetCurrentSyntheticFieldTrialsForTest(),
      kGlicHotkeyDefaultScopeTrialName));
}
#endif

// Enables GEiC in the constructor, i.e. before `SetUp()` creates the profile:
// creating the profile latches its Glic provider (`GlicProviderLatch`), so
// enabling GEiC later would have no effect.
class GlicEnablingGeicTestBase : public GlicEnablingProfileEligibilityTest {
 protected:
  explicit GlicEnablingGeicTestBase(bool geic_fre_enabled) {
    std::vector<base::test::FeatureRefAndParams> enabled = {
        {features::kGeic,
         {{"enabled", "true"},
          {"geic-guest-url",
           "https://business.gemini.google/side-panel?configId=abc"}}}};
    std::vector<base::test::FeatureRef> disabled;
    if (geic_fre_enabled) {
      enabled.push_back({features::kGeicFre, {}});
    } else {
      disabled.push_back(features::kGeicFre);
    }
    geic_feature_list_.InitWithFeaturesAndParameters(enabled, disabled);
  }

 private:
  base::test::ScopedFeatureList geic_feature_list_;
};

class GlicEnablingGeicFreTest : public GlicEnablingGeicTestBase {
 protected:
  GlicEnablingGeicFreTest()
      : GlicEnablingGeicTestBase(/*geic_fre_enabled=*/true) {}
};

class GlicEnablingGeicWithoutGeicFreTest : public GlicEnablingGeicTestBase {
 protected:
  GlicEnablingGeicWithoutGeicFreTest()
      : GlicEnablingGeicTestBase(/*geic_fre_enabled=*/false) {}
};

TEST_F(GlicEnablingProfileEligibilityTest, FreStoreIsGemini) {
  const GlicFreStore& store = GetGlicProviderSpec(profile()).GetFreStore();
  EXPECT_FALSE(store.HidesSettingsUntilCompleted());
  EXPECT_EQ(store.GetStatusPrefName(), prefs::kGlicCompletedFre);
  EXPECT_EQ(GetGlicProviderSpec(nullptr).GetFreStore().GetStatusPrefName(),
            prefs::kGlicCompletedFre);
}

TEST_F(GlicEnablingGeicFreTest, FreStoreIsGeminiEnterprise) {
  const GlicFreStore& store = GetGlicProviderSpec(profile()).GetFreStore();
  // The GEiC settings section stays hidden until the GEiC FRE is completed.
  EXPECT_TRUE(store.HidesSettingsUntilCompleted());
  EXPECT_EQ(store.GetStatusPrefName(), geic::prefs::kGeicCompletedFre);
}

TEST_F(GlicEnablingGeicWithoutGeicFreTest, FreStoreIsGeminiEnterprise) {
  // Without `features::kGeicFre`, GEiC still uses the GEiC store.
  EXPECT_EQ(GetGlicProviderSpec(profile()).GetFreStore().GetStatusPrefName(),
            geic::prefs::kGeicCompletedFre);
}

TEST_F(GlicEnablingGeicFreTest, ResetSwitchClearsGeicFre) {
  base::test::ScopedCommandLine scoped_command_line;
  const GlicFreStore& store = GetGlicProviderSpec(profile()).GetFreStore();
  geic::CompleteFre(profile());
  // An entry for another GE instance.
  ScopedDictPrefUpdate(profile()->GetPrefs(), geic::prefs::kGeicCompletedFre)
      ->Set("https://business.gemini.google/side-panel?configId=other",
            base::DictValue().Set(
                "status",
                std::to_underlying(geic::prefs::GeicFreStatus::kCompleted)));
  ASSERT_TRUE(GlicEnabling::HasConsentedForProfile(profile()));

  // Without the switch, nothing changes.
  store.ApplyDevSwitches(profile());
  EXPECT_TRUE(GlicEnabling::HasConsentedForProfile(profile()));

  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kGeicResetFre);
  store.ApplyDevSwitches(profile());
  EXPECT_FALSE(GlicEnabling::HasConsentedForProfile(profile()));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetDict(geic::prefs::kGeicCompletedFre).empty());
}

TEST_F(GlicEnablingGeicFreTest, ResetAndSkipSwitchesLeaveGeicFreCompleted) {
  base::test::ScopedCommandLine scoped_command_line;
  geic::CompleteFre(profile());
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kGeicResetFre);
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kGeicAlwaysSkipFre);

  GetGlicProviderSpec(profile()).GetFreStore().ApplyDevSwitches(profile());
  EXPECT_TRUE(GlicEnabling::HasConsentedForProfile(profile()));
}

TEST_F(GlicEnablingGeicFreTest, ConsentUsesGeicPref) {
  // The consumer FRE doesn't count for GEiC.
  profile()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      std::to_underlying(prefs::FreStatus::kCompleted));
  EXPECT_FALSE(GlicEnabling::HasConsentedForProfile(profile()));

  geic::CompleteFre(profile());
  EXPECT_TRUE(GlicEnabling::HasConsentedForProfile(profile()));

  // GEiC completion doesn't touch the consumer FRE.
  profile()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      std::to_underlying(prefs::FreStatus::kNotStarted));
  EXPECT_TRUE(GlicEnabling::HasConsentedForProfile(profile()));
}

TEST_F(GlicEnablingGeicFreTest, NeverDismissed) {
  profile()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      std::to_underlying(prefs::FreStatus::kIncomplete));
  EXPECT_FALSE(GlicEnabling::DidDismissForProfile(profile()));
}

TEST_F(GlicEnablingGeicFreTest, GetCompletedFreReadsGeicFre) {
  EXPECT_EQ(GlicEnabling::GetCompletedFre(profile()),
            prefs::FreStatus::kNotStarted);

  // The consumer FRE pref doesn't complete the GEiC FRE.
  profile()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      std::to_underlying(prefs::FreStatus::kCompleted));
  EXPECT_EQ(GlicEnabling::GetCompletedFre(profile()),
            prefs::FreStatus::kNotStarted);

  geic::CompleteFre(profile());
  EXPECT_EQ(GlicEnabling::GetCompletedFre(profile()),
            prefs::FreStatus::kCompleted);
}

TEST_F(GlicEnablingGeicFreTest, SetCompletedFreCompletesGeicFre) {
  GlicKeyedService::Get(profile())->enabling().SetCompletedFre(
      prefs::FreStatus::kCompleted);

  EXPECT_EQ(geic::GetFreStatus(profile()),
            geic::prefs::GeicFreStatus::kCompleted);
  EXPECT_TRUE(GlicEnabling::HasConsentedForProfile(profile()));
  // The consumer FRE is untouched.
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(prefs::kGlicCompletedFre),
            std::to_underlying(prefs::FreStatus::kNotStarted));
}

TEST_F(GlicEnablingGeicFreTest, SetCompletedFreIgnoresOtherGeicStatuses) {
  GlicEnabling& enabling = GlicKeyedService::Get(profile())->enabling();
  enabling.SetCompletedFre(prefs::FreStatus::kIncomplete);
  EXPECT_EQ(geic::GetFreStatus(profile()),
            geic::prefs::GeicFreStatus::kNotStarted);

  enabling.SetCompletedFre(prefs::FreStatus::kCompleted);
  // The GEiC FRE can't be undone, except with `--geic-reset-fre`.
  enabling.SetCompletedFre(prefs::FreStatus::kNotStarted);
  EXPECT_EQ(geic::GetFreStatus(profile()),
            geic::prefs::GeicFreStatus::kCompleted);
  // The consumer FRE is untouched.
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(prefs::kGlicCompletedFre),
            std::to_underlying(prefs::FreStatus::kNotStarted));
}

TEST_F(GlicEnablingProfileEligibilityTest,
       SetCompletedFreCompletesConsumerFre) {
  GlicKeyedService::Get(profile())->enabling().SetCompletedFre(
      prefs::FreStatus::kCompleted);

  EXPECT_EQ(profile()->GetPrefs()->GetInteger(prefs::kGlicCompletedFre),
            std::to_underlying(prefs::FreStatus::kCompleted));
  EXPECT_TRUE(GlicEnabling::HasConsentedForProfile(profile()));
}

TEST_F(GlicEnablingGeicFreTest, ConsentChangedCallbackFiresOnGeicPref) {
  bool callback_called = false;
  auto subscription = glic::GlicKeyedService::Get(profile())
                          ->enabling()
                          .RegisterOnConsentChanged(base::BindLambdaForTesting(
                              [&]() { callback_called = true; }));

  geic::CompleteFre(profile());
  EXPECT_TRUE(callback_called);
}

TEST_F(GlicEnablingGeicFreTest, ConsentChangedCallbackIgnoresConsumerPref) {
  bool callback_called = false;
  auto subscription = glic::GlicKeyedService::Get(profile())
                          ->enabling()
                          .RegisterOnConsentChanged(base::BindLambdaForTesting(
                              [&]() { callback_called = true; }));

  profile()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      std::to_underlying(prefs::FreStatus::kCompleted));
  EXPECT_FALSE(callback_called);
}

TEST_F(GlicEnablingProfileEligibilityTest,
       ConsentChangedCallbackIgnoresGeicPref) {
  bool callback_called = false;
  auto subscription = glic::GlicKeyedService::Get(profile())
                          ->enabling()
                          .RegisterOnConsentChanged(base::BindLambdaForTesting(
                              [&]() { callback_called = true; }));

  ScopedDictPrefUpdate(profile()->GetPrefs(), geic::prefs::kGeicCompletedFre)
      ->Set("https://business.gemini.google/side-panel?configId=abc",
            base::DictValue().Set(
                "status",
                std::to_underlying(geic::prefs::GeicFreStatus::kCompleted)));
  EXPECT_FALSE(callback_called);
}

// Without `features::kGeicFre`, GEiC is always consented.
TEST_F(GlicEnablingGeicWithoutGeicFreTest, AlwaysConsented) {
  EXPECT_TRUE(GlicEnabling::HasConsentedForProfile(profile()));
  EXPECT_EQ(GlicEnabling::GetCompletedFre(profile()),
            prefs::FreStatus::kCompleted);

  // The consumer FRE doesn't affect GEiC.
  profile()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      std::to_underlying(prefs::FreStatus::kIncomplete));
  EXPECT_TRUE(GlicEnabling::HasConsentedForProfile(profile()));
  EXPECT_FALSE(GlicEnabling::DidDismissForProfile(profile()));
}

TEST_F(GlicEnablingGeicWithoutGeicFreTest, SetCompletedFreStoresNothing) {
  GlicEnabling& enabling = GlicKeyedService::Get(profile())->enabling();
  enabling.SetCompletedFre(prefs::FreStatus::kCompleted);
  enabling.SetCompletedFre(prefs::FreStatus::kNotStarted);

  EXPECT_TRUE(GlicEnabling::HasConsentedForProfile(profile()));
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(prefs::kGlicCompletedFre),
            std::to_underlying(prefs::FreStatus::kNotStarted));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetDict(geic::prefs::kGeicCompletedFre).empty());
}

// Appends a consumer FRE developer switch before the profile and its
// `GlicKeyedService` are created, so that the switch is applied at startup.
class GlicEnablingGeicFreSwitchTestBase
    : public GlicEnablingGeicTestBase,
      public testing::WithParamInterface<const char*> {
 protected:
  explicit GlicEnablingGeicFreSwitchTestBase(bool geic_fre_enabled)
      : GlicEnablingGeicTestBase(geic_fre_enabled) {}

  void SetUp() override {
    scoped_command_line_.GetProcessCommandLine()->AppendSwitch(GetParam());
    GlicEnablingGeicTestBase::SetUp();
  }

 private:
  base::test::ScopedCommandLine scoped_command_line_;
};

class GlicEnablingGeicFreSkipSwitchTest
    : public GlicEnablingGeicFreSwitchTestBase {
 protected:
  GlicEnablingGeicFreSkipSwitchTest()
      : GlicEnablingGeicFreSwitchTestBase(/*geic_fre_enabled=*/true) {}
};

// The consumer FRE skip switches don't skip the GEiC FRE.
TEST_P(GlicEnablingGeicFreSkipSwitchTest, DoesNotCompleteGeicFre) {
  ASSERT_TRUE(GlicKeyedService::Get(profile()));
  EXPECT_FALSE(GlicEnabling::HasConsentedForProfile(profile()));
  EXPECT_EQ(geic::GetFreStatus(profile()),
            geic::prefs::GeicFreStatus::kNotStarted);
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(prefs::kGlicCompletedFre),
            std::to_underlying(prefs::FreStatus::kNotStarted));
}

INSTANTIATE_TEST_SUITE_P(All,
                         GlicEnablingGeicFreSkipSwitchTest,
                         testing::Values(switches::kGlicAutomation,
                                         switches::kGlicAlwaysSkipFre));

class GlicEnablingGeicWithoutGeicFreSkipSwitchTest
    : public GlicEnablingGeicFreSwitchTestBase {
 protected:
  GlicEnablingGeicWithoutGeicFreSkipSwitchTest()
      : GlicEnablingGeicFreSwitchTestBase(/*geic_fre_enabled=*/false) {}
};

// Without `features::kGeicFre`, the consumer FRE switches don't apply to GEiC.
TEST_P(GlicEnablingGeicWithoutGeicFreSkipSwitchTest,
       DoesNotCompleteConsumerFre) {
  ASSERT_TRUE(GlicKeyedService::Get(profile()));
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(prefs::kGlicCompletedFre),
            std::to_underlying(prefs::FreStatus::kNotStarted));
}

INSTANTIATE_TEST_SUITE_P(All,
                         GlicEnablingGeicWithoutGeicFreSkipSwitchTest,
                         testing::Values(switches::kGlicAutomation,
                                         switches::kGlicAlwaysSkipFre));

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

TEST_F(GlicEnablingProfileReadyStateTestBase,
       GetExperimentalTriggeringCapabilities_UnavailableReturnsEmpty) {
  auto& enabling = glic::GlicKeyedService::Get(profile())->enabling();
  // Fully opt in and enable the screenshot feature. kGlicExperimentalTriggering
  // is disabled by default in this fixture, so experimental triggering is
  // still unavailable and no capability (including screenshot) may leak.
  enabling.SetCompletedFre(prefs::FreStatus::kCompleted);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicUserEnabledActuationOnWeb,
                                    true);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicExperimentalTriggeringEnabled,
                                    true);
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      features::kGlicExperimentalTriggeringScreenshot);

  EXPECT_EQ(enabling.GetExperimentalTriggeringState(),
            syncer::DeviceInfo::GlicExperimentalTriggeringState::kUnavailable);
  EXPECT_TRUE(enabling.GetExperimentalTriggeringCapabilities().empty());
}

TEST_F(GlicEnablingProfileReadyStateTestBase,
       GetExperimentalTriggeringCapabilities_ScreenshotFlag) {
  auto& enabling = glic::GlicKeyedService::Get(profile())->enabling();
  enabling.SetCompletedFre(prefs::FreStatus::kCompleted);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicUserEnabledActuationOnWeb,
                                    true);
  profile()->GetPrefs()->SetBoolean(prefs::kGlicExperimentalTriggeringEnabled,
                                    true);

  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        /*enabled_features=*/{features::kGlicExperimentalTriggering},
        /*disabled_features=*/{
            features::kGlicExperimentalTriggeringScreenshot});

    EXPECT_EQ(enabling.GetExperimentalTriggeringState(),
              syncer::DeviceInfo::GlicExperimentalTriggeringState::kReady);
    EXPECT_EQ(enabling.GetExperimentalTriggeringCapabilities(),
              (base::flat_set<std::string>{kGlicCapabilityStructuredYields,
                                           kGlicCapabilityPaymentMetadata}));
  }

  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        /*enabled_features=*/{features::kGlicExperimentalTriggering,
                              features::kGlicExperimentalTriggeringScreenshot},
        /*disabled_features=*/{});

    EXPECT_EQ(enabling.GetExperimentalTriggeringState(),
              syncer::DeviceInfo::GlicExperimentalTriggeringState::kReady);
    EXPECT_EQ(enabling.GetExperimentalTriggeringCapabilities(),
              (base::flat_set<std::string>{kGlicCapabilityScreenshot,
                                           kGlicCapabilityStructuredYields,
                                           kGlicCapabilityPaymentMetadata}));
  }
}

TEST_F(GlicEnablingProfileReadyStateTestBase,
       GetExperimentalTriggeringCapabilities_NeedsOptIn) {
  auto& enabling = glic::GlicKeyedService::Get(profile())->enabling();
  // Leave the opt-in incomplete.
  enabling.SetCompletedFre(prefs::FreStatus::kIncomplete);
  enabling.SetUserEnabledActuationOnWeb(false);
  enabling.SetExperimentalTriggeringEnabled(false);
  // Bypass the enterprise policy check which defaults to disabled.
  profile()->GetPrefs()->SetInteger(
      prefs::kGlicSparkPolicySettings,
      std::to_underlying(glic::prefs::GlicSparkPolicyState::kEnabled));

  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        /*enabled_features=*/{features::kGlic,
                              features::kGlicExperimentalTriggering},
        /*disabled_features=*/{
            features::kGlicExperimentalTriggeringOptInBypass,
            features::kGlicExperimentalTriggeringScreenshot});

    EXPECT_EQ(enabling.GetExperimentalTriggeringState(),
              syncer::DeviceInfo::GlicExperimentalTriggeringState::kNeedsOptIn);
    EXPECT_EQ(enabling.GetExperimentalTriggeringCapabilities(),
              (base::flat_set<std::string>{kGlicCapabilityStructuredYields,
                                           kGlicCapabilityPaymentMetadata}));
  }

  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        /*enabled_features=*/{features::kGlic,
                              features::kGlicExperimentalTriggering,
                              features::kGlicExperimentalTriggeringScreenshot},
        /*disabled_features=*/{
            features::kGlicExperimentalTriggeringOptInBypass});

    EXPECT_EQ(enabling.GetExperimentalTriggeringState(),
              syncer::DeviceInfo::GlicExperimentalTriggeringState::kNeedsOptIn);
    EXPECT_EQ(enabling.GetExperimentalTriggeringCapabilities(),
              (base::flat_set<std::string>{kGlicCapabilityScreenshot,
                                           kGlicCapabilityStructuredYields,
                                           kGlicCapabilityPaymentMetadata}));
  }
}

// Fixture without the actor policy control exemption, so that the experimental
// triggering state reflects whether the profile can actually act on the web.
class GlicEnablingExperimentalTriggeringCanActOnWebTest
    : public GlicEnablingProfileReadyStateTestBase {
 public:
  GlicEnablingExperimentalTriggeringCanActOnWebTest()
      : GlicEnablingProfileReadyStateTestBase(
            /*extra_enabled_features=*/{features::kGlicExperimentalTriggering},
            /*extra_disabled_features=*/{},
            /*actor_policy_control_exemption=*/false) {}

 protected:
  GlicEnabling& enabling() {
    return glic::GlicKeyedService::Get(profile())->enabling();
  }

  void OptIn() {
    enabling().SetCompletedFre(prefs::FreStatus::kCompleted);
    enabling().SetUserEnabledActuationOnWeb(true);
    enabling().SetExperimentalTriggeringEnabled(true);
  }
};

TEST_F(GlicEnablingExperimentalTriggeringCanActOnWebTest,
       UnavailableWhenCannotActOnWeb) {
  OptIn();

  // The account does not have an eligible subscription, so it cannot act on
  // the web.
  profile()->GetPrefs()->SetInteger(
      subscription_eligibility::prefs::kAiSubscriptionTier, 0);
  ASSERT_FALSE(
      GlicKeyedService::Get(profile())->actor_policy_checker().CanActOnWeb());
  EXPECT_EQ(enabling().GetExperimentalTriggeringState(),
            syncer::DeviceInfo::GlicExperimentalTriggeringState::kUnavailable);
  EXPECT_EQ(enabling().GetExperimentalTriggeringVersion(), std::nullopt);
  EXPECT_TRUE(enabling().GetExperimentalTriggeringCapabilities().empty());

  // With an eligible subscription the profile can act on the web.
  profile()->GetPrefs()->SetInteger(
      subscription_eligibility::prefs::kAiSubscriptionTier, 1);
  ASSERT_TRUE(
      GlicKeyedService::Get(profile())->actor_policy_checker().CanActOnWeb());
  EXPECT_EQ(enabling().GetExperimentalTriggeringState(),
            syncer::DeviceInfo::GlicExperimentalTriggeringState::kReady);
  EXPECT_NE(enabling().GetExperimentalTriggeringVersion(), std::nullopt);
  EXPECT_FALSE(enabling().GetExperimentalTriggeringCapabilities().empty());
}

TEST_F(GlicEnablingExperimentalTriggeringCanActOnWebTest,
       StateChangedCallbackOnActOnWebPolicyChange) {
  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      policy::EnterpriseManagementAuthority::CLOUD);
  profile()->GetPrefs()->SetInteger(
      prefs::kGlicSparkPolicySettings,
      std::to_underlying(glic::prefs::GlicSparkPolicyState::kEnabled));
  profile()->GetPrefs()->SetInteger(
      glic::prefs::kGlicActuationOnWeb,
      std::to_underlying(glic::prefs::GlicActuationOnWebPolicyState::kEnabled));
  profile()->GetPrefs()->SetInteger(
      subscription_eligibility::prefs::kAiSubscriptionTier, 1);
  OptIn();
  ASSERT_EQ(enabling().GetExperimentalTriggeringState(),
            syncer::DeviceInfo::GlicExperimentalTriggeringState::kReady);

  bool callback_called = false;
  auto subscription = enabling().RegisterOnExperimentalTriggeringStateChanged(
      base::BindLambdaForTesting([&]() { callback_called = true; }));

  // GlicEnabling doesn't observe the actuation policy itself, so this
  // exercises the notification from GlicActorPolicyChecker.
  profile()->GetPrefs()->SetInteger(
      glic::prefs::kGlicActuationOnWeb,
      std::to_underlying(
          glic::prefs::GlicActuationOnWebPolicyState::kDisabled));
  EXPECT_TRUE(callback_called);
  EXPECT_EQ(enabling().GetExperimentalTriggeringState(),
            syncer::DeviceInfo::GlicExperimentalTriggeringState::kUnavailable);

  callback_called = false;
  profile()->GetPrefs()->SetInteger(
      glic::prefs::kGlicActuationOnWeb,
      std::to_underlying(glic::prefs::GlicActuationOnWebPolicyState::kEnabled));
  EXPECT_TRUE(callback_called);
  EXPECT_EQ(enabling().GetExperimentalTriggeringState(),
            syncer::DeviceInfo::GlicExperimentalTriggeringState::kReady);
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
  SetGlicCapability(mutator, false);
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
  SetGlicCapability(mutator, true);
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
  SetGlicCapability(mutator, true);
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
class GlicEnablingWebActuationToggleGeicTest
    : public GlicEnablingProfileEligibilityTest {
 public:
  GlicEnablingWebActuationToggleGeicTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlicActor, {}},
         {features::kGeic,
          {{"enabled", "true"},
           {"geic-guest-url", "https://business.gemini.google/side-panel"}}}},
        {});
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(GlicEnablingWebActuationToggleGeicTest,
       ManagedProfile_Geic_CannotActOnWeb) {
  auto* identity_test_env = identity_test_env_adaptor_->identity_test_env();
  AccountInfo account_info = identity_test_env->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info);
  SetGlicCapability(mutator, true);
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
  EXPECT_FALSE(glic_service->provider_spec().IsActuationSupported());
  EXPECT_FALSE(glic_service->actor_policy_checker().CanActOnWeb());
  EXPECT_FALSE(glic_service->actor_policy_checker().GlicApiCanActOnWeb());
  EXPECT_FALSE(glic_service->enabling().ShouldShowWebActuationToggle());
}

TEST_F(GlicEnablingWebActuationToggleGeicTest,
       ManagedProfile_Geic_CanActOnWeb_WithFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGeicActuation);

  auto* identity_test_env = identity_test_env_adaptor_->identity_test_env();
  AccountInfo account_info = identity_test_env->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info);
  SetGlicCapability(mutator, true);
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
  EXPECT_TRUE(glic_service->provider_spec().IsActuationSupported());
  EXPECT_TRUE(glic_service->actor_policy_checker().CanActOnWeb());
  EXPECT_TRUE(glic_service->actor_policy_checker().GlicApiCanActOnWeb());
  EXPECT_TRUE(glic_service->enabling().ShouldShowWebActuationToggle());
}

// Tests for the subscription benefits priority pref
// (`kSubscriptionBenefits`), which replaces the AI subscription tier check
// when `kGlicSubscriptionBenefitsEligibility` is enabled.
namespace {

constexpr char kEligibleBenefit[] = "chrome_autobrowse";
constexpr char kIneligibleBenefit[] = "some_other_benefit";

void SetSubscriptionBenefits(Profile* profile,
                             std::vector<std::string> benefits) {
  base::ListValue benefit_list;
  for (const std::string& benefit : benefits) {
    benefit_list.Append(benefit);
  }
  profile->GetPrefs()->SetList(
      subscription_eligibility::prefs::kSubscriptionBenefits,
      std::move(benefit_list));
}

void SetUpCapableAccount(signin::IdentityTestEnvironment* identity_test_env) {
  AccountInfo account_info = identity_test_env->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_can_use_model_execution_features(true);
  signin::UpdateAccountInfoForAccount(identity_test_env->identity_manager(),
                                      account_info);
}

}  // namespace

TEST_F(GlicEnablingWebActuationToggleTest,
       ManagedProfile_CanActOnWeb_SubscriptionBenefits) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeatureWithParameters(
      features::kGlicSubscriptionBenefitsEligibility,
      {{features::kGlicActorEligibleBenefits.name, kEligibleBenefit}});

  SetUpCapableAccount(identity_test_env_adaptor_->identity_test_env());

  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);

  profile()->GetPrefs()->SetInteger(
      glic::prefs::kGlicActuationOnWeb,
      std::to_underlying(glic::prefs::GlicActuationOnWebPolicyState::kEnabled));
  // The tier pref is intentionally left unset: eligibility must come from the
  // benefits pref alone.
  SetSubscriptionBenefits(profile(), {kEligibleBenefit});

  auto* glic_service = GlicKeyedService::Get(profile());
  EXPECT_TRUE(glic_service->enabling().ShouldShowWebActuationToggle());
}

TEST_F(GlicEnablingWebActuationToggleTest,
       ManagedProfile_CannotActOnWeb_SubscriptionBenefitsMismatch) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeatureWithParameters(
      features::kGlicSubscriptionBenefitsEligibility,
      {{features::kGlicActorEligibleBenefits.name, kEligibleBenefit}});

  SetUpCapableAccount(identity_test_env_adaptor_->identity_test_env());

  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);

  profile()->GetPrefs()->SetInteger(
      glic::prefs::kGlicActuationOnWeb,
      std::to_underlying(glic::prefs::GlicActuationOnWebPolicyState::kEnabled));
  // An eligible tier must not grant access while the benefits check is active.
  profile()->GetPrefs()->SetInteger(
      subscription_eligibility::prefs::kAiSubscriptionTier, 1);

  auto* glic_service = GlicKeyedService::Get(profile());
  // Profile has an eligible tier, but no benefits set: must not be enabled.
  EXPECT_FALSE(glic_service->enabling().ShouldShowWebActuationToggle());

  // Profile has an eligible tier, but wrong benefit: must not be enabled.
  SetSubscriptionBenefits(profile(), {kIneligibleBenefit});
  EXPECT_FALSE(glic_service->enabling().ShouldShowWebActuationToggle());
}

TEST_F(GlicEnablingWebActuationToggleTest,
       UnmanagedProfile_SubscriptionBenefits) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{features::kGlicSubscriptionBenefitsEligibility,
        {{features::kGlicActorEligibleBenefits.name, kEligibleBenefit}}},
       {features::kGlicWebActuationSettingsToggle, {}}},
      /*disabled_features=*/{});

  SetUpCapableAccount(identity_test_env_adaptor_->identity_test_env());

  auto* glic_service = GlicKeyedService::Get(profile());
  // An eligible tier must not grant access while the benefits check is active.
  profile()->GetPrefs()->SetInteger(
      subscription_eligibility::prefs::kAiSubscriptionTier, 1);

  // Profile has an eligible tier, but no benefits set yet: must not be enabled.
  EXPECT_FALSE(glic_service->enabling().ShouldShowWebActuationToggle());

  // Profile has an eligible tier, but wrong benefit: must not be enabled.
  SetSubscriptionBenefits(profile(), {kIneligibleBenefit});
  EXPECT_FALSE(glic_service->enabling().ShouldShowWebActuationToggle());

  // Profile has matching eligible benefit: now enabled.
  SetSubscriptionBenefits(profile(), {kIneligibleBenefit, kEligibleBenefit});
  EXPECT_TRUE(glic_service->enabling().ShouldShowWebActuationToggle());
}

TEST_F(GlicEnablingWebActuationToggleTest,
       TieredRollout_SubscriptionBenefitsPrecedenceOverV2Tier) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{features::kGlicSubscriptionBenefitsEligibility,
        {{features::kGlicEligibleBenefits.name, kEligibleBenefit}}},
       {features::kGlicTieredRolloutV2,
        {{features::kGlicTieredRolloutV2EligibleTiers.name, "1,2"}}}},
      /*disabled_features=*/{});

  // Profile has an eligible tier for V2 (tier 1).
  profile()->GetPrefs()->SetInteger(
      subscription_eligibility::prefs::kAiSubscriptionTier, 1);

  // 1. When benefits flag is enabled, having an eligible tier with no benefits
  // set must not grant eligibility under kGlicTieredRolloutV2.
  EXPECT_FALSE(GlicEnabling::IsEligibleForGlicTieredRollout(profile()));

  // 2. Profile with wrong benefit must not grant eligibility despite tier 1.
  SetSubscriptionBenefits(profile(), {kIneligibleBenefit});
  EXPECT_FALSE(GlicEnabling::IsEligibleForGlicTieredRollout(profile()));

  // 3. Profile with matching eligible benefit is eligible.
  SetSubscriptionBenefits(profile(), {kEligibleBenefit});
  EXPECT_TRUE(GlicEnabling::IsEligibleForGlicTieredRollout(profile()));
}

// Tests toggle visibility when a non-default preference is stored.
class GlicEnablingWebActuationStickyToggleTest
    : public GlicEnablingWebActuationToggleTest {
 public:
  GlicEnablingWebActuationStickyToggleTest() {
    settings_toggle_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kGlicActor,
          {{features::kGlicActorEligibleTiers.name, "1,2"}}},
         {features::kGlicWebActuationSettingsToggle, {}}},
        /*disabled_features=*/{});
  }

 protected:
  // Signs in an eligible consumer account.
  void SignInEligibleConsumerAccount() {
    auto* identity_test_env = identity_test_env_adaptor_->identity_test_env();
    AccountInfo account_info = identity_test_env->MakePrimaryAccountAvailable(
        "test@example.com", signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info);
    mutator.set_can_use_model_execution_features(true);
    signin::UpdateAccountInfoForAccount(identity_test_env->identity_manager(),
                                        account_info);
  }

  // Updates subscription tier and triggers policy recomputation.
  void SetSubscriptionTier(int32_t tier) {
    profile()->GetPrefs()->SetInteger(
        subscription_eligibility::prefs::kAiSubscriptionTier, tier);
  }

 private:
  base::test::ScopedFeatureList settings_toggle_feature_list_;
};

TEST_F(GlicEnablingWebActuationStickyToggleTest,
       PrefDefault_TierEligible_Visible) {
  SignInEligibleConsumerAccount();
  SetSubscriptionTier(1);

  auto* glic_service = GlicKeyedService::Get(profile());
  ASSERT_TRUE(glic_service->enabling().IsUserEnabledActuationOnWebDefault());
  EXPECT_TRUE(glic_service->enabling().ShouldShowWebActuationToggle());
}

// Subscription tier requirements apply when the preference is at default.
TEST_F(GlicEnablingWebActuationStickyToggleTest,
       PrefDefault_TierIneligible_Hidden) {
  SignInEligibleConsumerAccount();
  SetSubscriptionTier(999);

  auto* glic_service = GlicKeyedService::Get(profile());
  ASSERT_TRUE(glic_service->enabling().IsUserEnabledActuationOnWebDefault());
  EXPECT_FALSE(glic_service->enabling().ShouldShowWebActuationToggle());
}

// The toggle remains visible when the preference is set to true.
TEST_F(GlicEnablingWebActuationStickyToggleTest,
       OptedIn_TierIneligible_StaysVisible) {
  SignInEligibleConsumerAccount();
  SetSubscriptionTier(999);

  auto* glic_service = GlicKeyedService::Get(profile());
  glic_service->enabling().SetUserEnabledActuationOnWeb(true);

  ASSERT_FALSE(glic_service->enabling().IsUserEnabledActuationOnWebDefault());
  EXPECT_TRUE(glic_service->enabling().ShouldShowWebActuationToggle());
}

// The toggle remains visible when the preference is set to false.
TEST_F(GlicEnablingWebActuationStickyToggleTest,
       OptedOut_TierIneligible_StaysVisible) {
  SignInEligibleConsumerAccount();
  SetSubscriptionTier(999);

  auto* glic_service = GlicKeyedService::Get(profile());
  glic_service->enabling().SetUserEnabledActuationOnWeb(false);

  ASSERT_FALSE(glic_service->enabling().IsUserEnabledActuationOnWebDefault());
  EXPECT_TRUE(glic_service->enabling().ShouldShowWebActuationToggle());
}

// Tests for ShouldShowExperimentalTriggeringToggle(), which gates the
// "Gemini Spark" (experimental triggering) settings toggle shared by the
// desktop and Android settings UIs.
TEST_F(GlicEnablingWebActuationToggleTest,
       ExperimentalTriggeringToggle_FeatureDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kGlicExperimentalTriggering);
  scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
      switches::kGlicAlwaysShowWebActuationToggle);

  auto* glic_service = GlicKeyedService::Get(profile());
  EXPECT_FALSE(
      glic_service->enabling().ShouldShowExperimentalTriggeringToggle());
}

TEST_F(GlicEnablingWebActuationToggleTest,
       ExperimentalTriggeringToggle_HiddenWhenWebActuationToggleHidden) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{features::kGlicExperimentalTriggering},
      /*disabled_features=*/{features::kGlicWebActuationSetting});

  auto* glic_service = GlicKeyedService::Get(profile());
  // Precondition: the web-actuation toggle is hidden.
  ASSERT_FALSE(glic_service->enabling().ShouldShowWebActuationToggle());
  EXPECT_FALSE(
      glic_service->enabling().ShouldShowExperimentalTriggeringToggle());
}

TEST_F(GlicEnablingWebActuationToggleTest,
       ExperimentalTriggeringToggle_HiddenWhenPrefIsDefault) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kGlicExperimentalTriggering);
  scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
      switches::kGlicAlwaysShowWebActuationToggle);

  auto* glic_service = GlicKeyedService::Get(profile());
  ASSERT_TRUE(glic_service->enabling().ShouldShowWebActuationToggle());
  // The pref is at its default value, so the user has not opted in.
  EXPECT_FALSE(
      glic_service->enabling().ShouldShowExperimentalTriggeringToggle());
}

TEST_F(GlicEnablingWebActuationToggleTest,
       ExperimentalTriggeringToggle_HiddenWhenManaged) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kGlicExperimentalTriggering);
  scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
      switches::kGlicAlwaysShowWebActuationToggle);

  // A managed (policy-enforced) pref is not user-controlled.
  static_cast<TestingProfile*>(profile())
      ->GetTestingPrefService()
      ->SetManagedPref(prefs::kGlicExperimentalTriggeringEnabled,
                       std::make_unique<base::Value>(true));

  auto* glic_service = GlicKeyedService::Get(profile());
  ASSERT_TRUE(glic_service->enabling().ShouldShowWebActuationToggle());
  EXPECT_FALSE(
      glic_service->enabling().ShouldShowExperimentalTriggeringToggle());
}

TEST_F(GlicEnablingWebActuationToggleTest,
       ExperimentalTriggeringToggle_ShownWhenUserOptedIn) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kGlicExperimentalTriggering);
  scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
      switches::kGlicAlwaysShowWebActuationToggle);

  auto* glic_service = GlicKeyedService::Get(profile());
  // User explicitly set the pref (user-controlled and non-default).
  glic_service->enabling().SetExperimentalTriggeringEnabled(true);

  ASSERT_TRUE(glic_service->enabling().ShouldShowWebActuationToggle());
  EXPECT_TRUE(
      glic_service->enabling().ShouldShowExperimentalTriggeringToggle());
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
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kTestType);
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
  SetGlicCapability(mutator, false);
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
      "Glic.CountryFilteringResult2.Adult",
      GlicFilteringResult::kBlockedNotInInclusionList, 1);

  // 2. Subsequent check with no change in the delegate's country. Enablement
  // check returns false again, but the histogram should not be recorded a
  // second time.
  EXPECT_FALSE(global_enabling.IsCountryEnabled());
  histogram_tester.ExpectUniqueSample(
      "Glic.CountryFilteringResult2.Adult",
      GlicFilteringResult::kBlockedNotInInclusionList, 1);

  // 3. Update permanent country to another disabled one ("yy") and checks
  // country enablement twice. Should re-record kBlockedNotInInclusionList once.
  delegate->SetPermanentCountryCode("yy");
  EXPECT_FALSE(global_enabling.IsCountryEnabled());
  EXPECT_FALSE(global_enabling.IsCountryEnabled());
  histogram_tester.ExpectBucketCount(
      "Glic.CountryFilteringResult2.Adult",
      GlicFilteringResult::kBlockedNotInInclusionList, 2);
  histogram_tester.ExpectTotalCount("Glic.CountryFilteringResult2.Adult", 2);

  // 4. Update session country to another disabled one ("xx") and checks
  // country enablement twice. Should re-record kBlockedNotInInclusionList once.
  delegate->SetSessionCountryCode("xx");
  EXPECT_FALSE(global_enabling.IsCountryEnabled());
  EXPECT_FALSE(global_enabling.IsCountryEnabled());
  histogram_tester.ExpectBucketCount(
      "Glic.CountryFilteringResult2.Adult",
      GlicFilteringResult::kBlockedNotInInclusionList, 3);
  histogram_tester.ExpectTotalCount("Glic.CountryFilteringResult2.Adult", 3);

  // 5. Update country to an enabled one ("us"). Two country checks return true,
  // but only a single sample is recorded to another bucket of the histogram.
  delegate->SetPermanentCountryCode("us");
  EXPECT_TRUE(global_enabling.IsCountryEnabled());
  EXPECT_TRUE(global_enabling.IsCountryEnabled());
  histogram_tester.ExpectBucketCount(
      "Glic.CountryFilteringResult2.Adult",
      GlicFilteringResult::kAllowedInInclusionList, 1);
  histogram_tester.ExpectTotalCount("Glic.CountryFilteringResult2.Adult", 4);

  // 6. Reset cache and re-evaluate with "zz" again. Should re-record
  // kBlockedNotInInclusionList since cache was cleared.
  delegate->SetPermanentCountryCode("zz");
  global_enabling.UpdateStateForTesting(delegate->Clone());
  EXPECT_FALSE(global_enabling.IsCountryEnabled());
  histogram_tester.ExpectBucketCount(
      "Glic.CountryFilteringResult2.Adult",
      GlicFilteringResult::kBlockedNotInInclusionList, 4);
  histogram_tester.ExpectTotalCount("Glic.CountryFilteringResult2.Adult", 5);
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
  SetGlicCapability(mutator, true);
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
  SetGlicCapability(mutator, true);
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

// Tests that users who cannot use adult features (e.g. minors) are subject to
// the kGlicMinorCountryFiltering country list, and adults to
// kGlicCountryFiltering.
class GlicEnablingMinorCountryCheckTest : public GlicEnablingCountryCheckTest {
 public:
  GlicEnablingMinorCountryCheckTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlicRollout, {}},
         {features::kGlicCountryFiltering, {{"enabled_countries", "us,ca"}}},
         {features::kGlicMinorCountryFiltering, {{"enabled_countries", "us"}}}},
        {});
  }

 protected:
  void SignIn(bool is_adult) {
    auto* identity_test_env = identity_test_env_adaptor_->identity_test_env();
    AccountInfo account_info = identity_test_env->MakePrimaryAccountAvailable(
        "test@example.com", signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info);
    mutator.set_can_use_gemini_in_chrome(true);
    mutator.set_can_use_model_execution_features(is_adult);
    signin::UpdateAccountInfoForAccount(identity_test_env->identity_manager(),
                                        account_info);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(GlicEnablingMinorCountryCheckTest, MinorAllowedInMinorCountry) {
  SetProfileCountryDelegate("us");
  SignIn(/*is_adult=*/false);

  GlicEnabling::ProfileEnablement enablement =
      GlicEnabling::EnablementForProfile(profile());
  EXPECT_TRUE(enablement.primary_account_is_capable);
  EXPECT_TRUE(enablement.allowed_by_country_filter);
  EXPECT_TRUE(enablement.IsEnabled());
}

TEST_F(GlicEnablingMinorCountryCheckTest, MinorNotCapableInAdultOnlyCountry) {
  SetProfileCountryDelegate("ca");
  SignIn(/*is_adult=*/false);

  // Minor users outside minor countries are reported as not capable, rather
  // than blocked by the (adult) country filter.
  GlicEnabling::ProfileEnablement enablement =
      GlicEnabling::EnablementForProfile(profile());
  EXPECT_FALSE(enablement.primary_account_is_capable);
  EXPECT_TRUE(enablement.allowed_by_country_filter);
  EXPECT_FALSE(enablement.IsEnabled());
  EXPECT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kIneligible);
}

TEST_F(GlicEnablingMinorCountryCheckTest, AdultAllowedInAdultOnlyCountry) {
  SetProfileCountryDelegate("ca");
  SignIn(/*is_adult=*/true);

  GlicEnabling::ProfileEnablement enablement =
      GlicEnabling::EnablementForProfile(profile());
  EXPECT_TRUE(enablement.allowed_by_country_filter);
  EXPECT_TRUE(enablement.IsEnabled());
}

TEST_F(GlicEnablingMinorCountryCheckTest, AdultBlockedInNonAdultCountry) {
  SetProfileCountryDelegate("zz");
  SignIn(/*is_adult=*/true);

  EXPECT_FALSE(
      GlicEnabling::EnablementForProfile(profile()).allowed_by_country_filter);
}

TEST_F(GlicEnablingMinorCountryCheckTest, SignedOutUsesAdultCountryList) {
  SetProfileCountryDelegate("ca");

  EXPECT_TRUE(
      GlicEnabling::EnablementForProfile(profile()).allowed_by_country_filter);
}

TEST_F(GlicEnablingMinorCountryCheckTest,
       IneligibleAccountUsesAdultCountryList) {
  SetProfileCountryDelegate("ca");
  auto* identity_test_env = identity_test_env_adaptor_->identity_test_env();
  AccountInfo account_info = identity_test_env->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_can_use_gemini_in_chrome(false);
  mutator.set_can_use_model_execution_features(false);
  signin::UpdateAccountInfoForAccount(identity_test_env->identity_manager(),
                                      account_info);

  GlicEnabling::ProfileEnablement enablement =
      GlicEnabling::EnablementForProfile(profile());
  EXPECT_FALSE(enablement.primary_account_is_capable);
  EXPECT_TRUE(enablement.allowed_by_country_filter);
}

TEST_F(GlicEnablingMinorCountryCheckTest,
       MinorNotCapableEverywhereWhenMinorFilteringDisabled) {
  base::test::ScopedFeatureList minor_filter_disabled;
  minor_filter_disabled.InitAndDisableFeature(
      features::kGlicMinorCountryFiltering);

  SetProfileCountryDelegate("us");
  SignIn(/*is_adult=*/false);

  GlicEnabling::ProfileEnablement enablement =
      GlicEnabling::EnablementForProfile(profile());
  EXPECT_FALSE(enablement.primary_account_is_capable);
  EXPECT_TRUE(enablement.allowed_by_country_filter);
  EXPECT_FALSE(enablement.IsEnabled());
}

TEST_F(GlicEnablingMinorCountryCheckTest,
       AdultUnaffectedWhenMinorFilteringDisabled) {
  base::test::ScopedFeatureList minor_filter_disabled;
  minor_filter_disabled.InitAndDisableFeature(
      features::kGlicMinorCountryFiltering);

  SetProfileCountryDelegate("us");
  SignIn(/*is_adult=*/true);

  EXPECT_TRUE(GlicEnabling::EnablementForProfile(profile()).IsEnabled());
}

TEST_F(GlicEnablingMinorCountryCheckTest, MinorCountryCheckCachedSeparately) {
  auto& global_enabling =
      g_browser_process->GetFeatures()->glic_global_enabling();

  // "ca" is enabled for adults only.
  TestDelegate* delegate = SetProfileCountryDelegate("ca");
  EXPECT_TRUE(global_enabling.IsCountryEnabled());
  EXPECT_FALSE(global_enabling.IsCountryEnabledForMinorAccounts());

  // Moving to "us" enables minor access.
  delegate->SetPermanentCountryCode("us");
  EXPECT_TRUE(global_enabling.IsCountryEnabledForMinorAccounts());

  // Once enabled, the minor result is sticky.
  delegate->SetPermanentCountryCode("zz");
  EXPECT_TRUE(global_enabling.IsCountryEnabledForMinorAccounts());

  // Resetting clears the minor cache too.
  global_enabling.UpdateStateForTesting(delegate->Clone());
  EXPECT_FALSE(global_enabling.IsCountryEnabledForMinorAccounts());
  EXPECT_FALSE(global_enabling.IsCountryEnabled());
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
  SetGlicCapability(mutator, true);
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
    SetGlicCapability(mutator, false);
    signin::UpdateAccountInfoForAccount(identity_test_env->identity_manager(),
                                        account_info);
  }

  // Verify we are in kIneligibleAccount
  ASSERT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kIneligibleAccount);

  // 3. Recover by enabling capabilities
  {
    AccountCapabilitiesTestMutator mutator(&account_info);
    SetGlicCapability(mutator, true);
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

TEST_F(GlicEnablingProfileEligibilityTest, IsAnyEntryPointEnabled) {
  profile()->GetPrefs()->SetBoolean(prefs::kGlicPinnedToTabstrip, false);
  TestingBrowserProcess::GetGlobal()->local_state()->SetBoolean(
      prefs::kGlicLauncherEnabled, false);
  EXPECT_FALSE(IsAnyEntryPointEnabled(profile()));

  profile()->GetPrefs()->SetBoolean(prefs::kGlicPinnedToTabstrip, true);
  EXPECT_TRUE(IsAnyEntryPointEnabled(profile()));

  profile()->GetPrefs()->SetBoolean(prefs::kGlicPinnedToTabstrip, false);
  TestingBrowserProcess::GetGlobal()->local_state()->SetBoolean(
      prefs::kGlicLauncherEnabled, true);
  EXPECT_TRUE(IsAnyEntryPointEnabled(profile()));

  profile()->GetPrefs()->SetBoolean(prefs::kGlicPinnedToTabstrip, true);
  EXPECT_TRUE(IsAnyEntryPointEnabled(profile()));
}

TEST_F(GlicEnablingProfileEligibilityTest, Provider_GeminiWhenGeicDisabled) {
  EXPECT_EQ(GlicProviderLatch::GetProvider(*profile()), GlicProvider::kGemini);
  EXPECT_TRUE(GlicEnabling::IsProfileEligible(profile()));
}

// Creates the test profile with `features::kGeic` enabled and a GEiC policy URL
// already set, so that the Glic provider latched when the profile's keyed
// services are created is Gemini Enterprise.
class GlicEnablingGeicPolicyTest : public GlicEnablingProfileEligibilityTest {
 public:
  GlicEnablingGeicPolicyTest() {
    geic_feature_list_.InitAndEnableFeature(features::kGeic);
  }

 protected:
  std::unique_ptr<sync_preferences::PrefServiceSyncable> CreatePrefService()
      override {
    auto pref_service =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(pref_service->registry());
    pref_service->SetManagedPref(
        prefs::kGlicGeminiEnterpriseUrl,
        base::Value("https://business.gemini.google/home/cid/abc123"));
    return pref_service;
  }

 private:
  base::test::ScopedFeatureList geic_feature_list_;
};

// Like `GlicEnablingGeicPolicyTest`, but with `features::kGeic` disabled when
// the profile is created.
class GlicEnablingGeicPolicyFeatureDisabledTest
    : public GlicEnablingGeicPolicyTest {
 public:
  GlicEnablingGeicPolicyFeatureDisabledTest() {
    geic_disabled_feature_list_.InitAndDisableFeature(features::kGeic);
  }

 private:
  base::test::ScopedFeatureList geic_disabled_feature_list_;
};

// Like `GlicEnablingGeicPolicyTest`, but with `features::kGlic` disabled when
// the profile is created, so the profile is ineligible for Glic.
class GlicEnablingGeicPolicyGlicDisabledTest
    : public GlicEnablingGeicPolicyTest {
 public:
  GlicEnablingGeicPolicyGlicDisabledTest() {
    glic_disabled_feature_list_.InitAndDisableFeature(features::kGlic);
  }

 private:
  base::test::ScopedFeatureList glic_disabled_feature_list_;
};

TEST_F(GlicEnablingGeicPolicyTest, Provider_GeminiEnterpriseWithPolicyUrl) {
  EXPECT_EQ(GlicProviderLatch::GetProvider(*profile()),
            GlicProvider::kGeminiEnterprise);
  EXPECT_TRUE(GlicEnabling::IsProfileEligible(profile()));
}

TEST_F(GlicEnablingGeicPolicyFeatureDisabledTest,
       Provider_GeminiWhenGeicFeatureDisabledWithPolicyUrl) {
  EXPECT_EQ(GlicProviderLatch::GetProvider(*profile()), GlicProvider::kGemini);
  EXPECT_TRUE(GlicEnabling::IsProfileEligible(profile()));
}

// The provider is latched independently of Glic eligibility.
TEST_F(GlicEnablingGeicPolicyGlicDisabledTest,
       Provider_GeminiEnterpriseLatchedButIneligibleForGlic) {
  EXPECT_FALSE(GlicEnabling::IsProfileEligible(profile()));
  EXPECT_EQ(GlicProviderLatch::GetProvider(*profile()),
            GlicProvider::kGeminiEnterprise);
  EXPECT_EQ(GlicKeyedService::Get(profile()), nullptr);
}

// Like `GlicEnablingGeicPolicyTest`, but with the `enabled` param of
// `features::kGeic` set to false when the profile is created.
class GlicEnablingGeicPolicyEnabledParamFalseTest
    : public GlicEnablingGeicPolicyTest {
 public:
  GlicEnablingGeicPolicyEnabledParamFalseTest() {
    geic_param_feature_list_.InitAndEnableFeatureWithParameters(
        features::kGeic, {{"enabled", "false"}});
  }

 private:
  base::test::ScopedFeatureList geic_param_feature_list_;
};

// Like `GlicEnablingProfileEligibilityTest`, but with `features::kGeic`
// enabled and no Gemini Enterprise guest URL configured when the profile is
// created.
class GlicEnablingGeicWithoutUrlTest
    : public GlicEnablingProfileEligibilityTest {
 public:
  GlicEnablingGeicWithoutUrlTest() {
    geic_feature_list_.InitAndEnableFeature(features::kGeic);
  }

 private:
  base::test::ScopedFeatureList geic_feature_list_;
};

// Expects `profile` to be latched to consumer Gemini and to get consumer
// enablement, including the Gemini account eligibility checks: the profile is
// signed out, so it isn't ready.
void ExpectConsumerProviderAndEnablement(Profile* profile) {
  ASSERT_TRUE(IdentityManagerFactory::GetForProfile(profile)
                  ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
                  .IsEmpty());
  EXPECT_EQ(GlicProviderLatch::GetProvider(*profile), GlicProvider::kGemini);
  EXPECT_TRUE(GetGlicProviderSpec(profile).RequiresGeminiAccountEligibility());

  GlicEnabling::ProfileEnablement enablement =
      GlicEnabling::EnablementForProfile(profile);
  EXPECT_TRUE(enablement.primary_account_needs_signed_in ||
              !enablement.primary_account_is_capable);
  EXPECT_NE(GlicEnabling::GetProfileReadyState(profile),
            mojom::ProfileReadyState::kReady);
}

// Enablement has no `features::kGeic` check of its own (see
// ComputeProfileEnablement()), because a profile is only latched to Gemini
// Enterprise if kGeic is fully enabled and a guest URL is configured. The
// following tests check each way the latch falls back to consumer Gemini, and
// that consumer enablement then applies.

// The Gemini Enterprise policy URL is set, but kGeic is disabled.
TEST_F(GlicEnablingGeicPolicyFeatureDisabledTest,
       ConsumerEnablementWhenGeicFeatureDisabled) {
  ExpectConsumerProviderAndEnablement(profile());
}

// The Gemini Enterprise policy URL is set, but kGeic's `enabled` param is
// false.
TEST_F(GlicEnablingGeicPolicyEnabledParamFalseTest,
       ConsumerEnablementWhenGeicEnabledParamFalse) {
  ExpectConsumerProviderAndEnablement(profile());
}

// kGeic is enabled, but no Gemini Enterprise guest URL is configured.
TEST_F(GlicEnablingGeicWithoutUrlTest, ConsumerEnablementWithoutGuestUrl) {
  ExpectConsumerProviderAndEnablement(profile());
}

// kGeic is enabled and the Gemini Enterprise policy URL is set, so the profile
// is latched to Gemini Enterprise and skips the Gemini account eligibility
// checks: a signed-out profile is enabled and ready.
TEST_F(GlicEnablingGeicPolicyTest,
       GeminiEnterpriseEnablementWithGeicFeatureAndUrl) {
  ASSERT_TRUE(IdentityManagerFactory::GetForProfile(profile())
                  ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
                  .IsEmpty());
  EXPECT_EQ(GlicProviderLatch::GetProvider(*profile()),
            GlicProvider::kGeminiEnterprise);
  EXPECT_FALSE(
      GetGlicProviderSpec(profile()).RequiresGeminiAccountEligibility());

  GlicEnabling::ProfileEnablement enablement =
      GlicEnabling::EnablementForProfile(profile());
  EXPECT_FALSE(enablement.primary_account_needs_signed_in);
  EXPECT_TRUE(enablement.primary_account_is_capable);
  EXPECT_TRUE(enablement.IsEnabled());
  EXPECT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kReady);
}

TEST_F(GlicEnablingGeicPolicyTest, Provider_LatchedAtCreation) {
  // Removing the policy after the profile was created does not change the
  // provider or the latched guest URL for the rest of the session.
  static_cast<sync_preferences::TestingPrefServiceSyncable*>(
      profile()->GetPrefs())
      ->RemoveManagedPref(prefs::kGlicGeminiEnterpriseUrl);

  EXPECT_EQ(GlicProviderLatch::GetProvider(*profile()),
            GlicProvider::kGeminiEnterprise);
  EXPECT_EQ(
      GetGlicProviderSpec(profile()).GetGuestUrl(profile()),
      GURL("https://business.gemini.google/side-panel?configId=abc123&hl=en"));
}

// Keyed services that read the provider while they are being built must be
// created after the provider is latched. Creating the profile in SetUp() has
// already built every service that is created with the browser context without
// hitting the latch CHECK; this checks that the Glic keyed service can be built
// for a GEiC profile and that the profile reports the GEiC provider.
TEST_F(GlicEnablingGeicPolicyTest, KeyedServicesBuildForGeicProfile) {
  EXPECT_TRUE(GlicKeyedServiceFactory::GetGlicKeyedService(profile(),
                                                           /*create=*/true));
  EXPECT_EQ(GlicProviderLatch::GetProvider(*profile()),
            GlicProvider::kGeminiEnterprise);
}

// Gemini Enterprise doesn't apply the consumer account checks, so a signed-out
// profile that never completed the consumer FRE is enabled, consented and
// ready.
TEST_F(GlicEnablingGeicPolicyTest, GeicEnabledWithoutConsumerAccountChecks) {
  ASSERT_EQ(GlicProviderLatch::GetProvider(*profile()),
            GlicProvider::kGeminiEnterprise);
  ASSERT_TRUE(identity_test_env_adaptor_->identity_test_env()
                  ->identity_manager()
                  ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
                  .IsEmpty());

  GlicEnabling::ProfileEnablement enablement =
      GlicEnabling::EnablementForProfile(profile());
  EXPECT_TRUE(enablement.IsEnabled());
  // This is true only because `features::kGeicFre` is disabled (the default),
  // so `GeminiEnterpriseGlicFreStore` reports the FRE as `kCompleted` (see
  // glic_fre_store_impl.cc). `GeicFreControlsConsentInEnablement` covers
  // consent with the GEiC FRE enabled.
  EXPECT_TRUE(enablement.IsEnabledAndConsented());
  EXPECT_TRUE(enablement.ShouldShowGlicButton());
  EXPECT_TRUE(enablement.ShouldShowSettingsPage());
  EXPECT_FALSE(enablement.primary_account_needs_signed_in);
  EXPECT_FALSE(enablement.DisallowedByAdmin());
  EXPECT_FALSE(enablement.EligibleForLive());
  EXPECT_FALSE(enablement.EligibleForShareImage());
  EXPECT_FALSE(enablement.live_allowed);
  EXPECT_FALSE(enablement.share_image_allowed);
  EXPECT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kReady);

  // Gemini Enterprise profiles are reported as consented by all consent
  // readers, without writing the consumer consent pref.
  EXPECT_TRUE(GlicEnabling::HasConsentedForProfile(profile()));
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(prefs::kGlicCompletedFre),
            std::to_underlying(prefs::FreStatus::kNotStarted));
  GlicKeyedService* service = GlicKeyedService::Get(profile());
  ASSERT_TRUE(service);
  EXPECT_NE(service->enabling().GetRequiredExperimentalOptIn(),
            RequiredExperimentalOptIn::kGlic);
}

// With `features::kGeicFre` enabled, Gemini Enterprise enablement requires
// completing the GEiC FRE rather than the consumer FRE.
TEST_F(GlicEnablingGeicPolicyTest, GeicFreControlsConsentInEnablement) {
  base::test::ScopedFeatureList geic_fre_feature;
  geic_fre_feature.InitAndEnableFeature(features::kGeicFre);

  // Completing the consumer FRE does not consent the GEiC profile.
  profile()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      std::to_underlying(prefs::FreStatus::kCompleted));

  {
    GlicEnabling::ProfileEnablement enablement =
        GlicEnabling::EnablementForProfile(profile());
    EXPECT_TRUE(enablement.IsEnabled());
    EXPECT_FALSE(enablement.fre_is_consented);
    EXPECT_FALSE(enablement.IsEnabledAndConsented());
    EXPECT_FALSE(GlicEnabling::IsEnabledAndConsentForProfile(profile()));
  }

  geic::CompleteFre(profile());

  {
    GlicEnabling::ProfileEnablement enablement =
        GlicEnabling::EnablementForProfile(profile());
    EXPECT_TRUE(enablement.IsEnabled());
    EXPECT_TRUE(enablement.fre_is_consented);
    EXPECT_TRUE(enablement.IsEnabledAndConsented());
    EXPECT_TRUE(GlicEnabling::IsEnabledAndConsentForProfile(profile()));
  }
}

// An off-the-record profile of a Gemini Enterprise profile is not eligible,
// enabled, or ready for Glic.
TEST_F(GlicEnablingGeicPolicyTest, GeicDisabledForOffTheRecordProfile) {
  ASSERT_TRUE(GlicEnabling::IsEnabledForProfile(profile()));
  ASSERT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kReady);

  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_FALSE(GlicEnabling::IsProfileEligible(otr_profile));
  EXPECT_FALSE(GlicEnabling::IsEnabledForProfile(otr_profile));
  EXPECT_FALSE(GlicEnabling::EnablementForProfile(otr_profile).IsEnabled());
  EXPECT_EQ(GlicEnabling::GetProfileReadyState(otr_profile),
            mojom::ProfileReadyState::kIneligible);
}

// Gemini Enterprise ignores the consumer GeminiSettings policy and does not
// observe it, so changing it does not trigger enablement callbacks.
TEST_F(GlicEnablingGeicPolicyTest, GeicIgnoresGeminiSettingsPolicy) {
  GlicKeyedService* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(profile(), /*create=*/true);
  ASSERT_TRUE(service);

  int callback_count = 0;
  base::CallbackListSubscription subscription =
      service->enabling().RegisterAllowedChanged(
          base::BindRepeating([](int* count) { (*count)++; }, &callback_count));

  profile()->GetPrefs()->SetInteger(
      optimization_guide::prefs::kGeminiSettings,
      std::to_underlying(
          optimization_guide::prefs::GeminiSettingsPolicyState::kDisabled));

  EXPECT_EQ(callback_count, 0);

  GlicEnabling::ProfileEnablement enablement =
      GlicEnabling::EnablementForProfile(profile());
  EXPECT_TRUE(enablement.IsEnabled());
  EXPECT_FALSE(enablement.DisallowedByAdmin());
  EXPECT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kReady);
}

// Gemini Enterprise does not observe primary account changes.
TEST_F(GlicEnablingGeicPolicyTest,
       GeicPrimaryAccountChangesDoNotNotifyAllowedChanged) {
  GlicKeyedService* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(profile(), /*create=*/true);
  ASSERT_TRUE(service);

  int callback_count = 0;
  base::CallbackListSubscription subscription =
      service->enabling().RegisterAllowedChanged(
          base::BindRepeating([](int* count) { (*count)++; }, &callback_count));

  auto* identity_test_env = identity_test_env_adaptor_->identity_test_env();
  AccountInfo account_info = identity_test_env->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info);
  SetGlicCapability(mutator, true);
  signin::UpdateAccountInfoForAccount(identity_test_env->identity_manager(),
                                      account_info);
  identity_test_env->SetInvalidRefreshTokenForPrimaryAccount();
  EXPECT_EQ(callback_count, 0);

#if !BUILDFLAG(IS_CHROMEOS)
  identity_test_env->ClearPrimaryAccount();
  EXPECT_EQ(callback_count, 0);
#endif
}

// Gemini Enterprise is not anchored: without the Glic feature the profile is
// ineligible and disabled, even if the user completed the consumer FRE.
TEST_F(GlicEnablingGeicPolicyTest, GeicNotAnchoredWhenGlobalCriteriaFail) {
  profile()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      std::to_underlying(prefs::FreStatus::kCompleted));
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kGlicAnchorEntryPointForOnboardedUsers}, {features::kGlic});

  EXPECT_FALSE(GlicEnabling::IsProfileEligible(profile()));
  GlicEnabling::ProfileEnablement enablement =
      GlicEnabling::EnablementForProfile(profile());
  EXPECT_FALSE(enablement.IsEnabled());
  EXPECT_FALSE(enablement.ShouldShowGlicButton());
  EXPECT_FALSE(enablement.anchor_entrypoint_override_active);
  EXPECT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kIneligible);
}

// Like `GlicEnablingGeicPolicyTest`, but with the Glic country filter enabled.
class GlicEnablingGeicPolicyCountryCheckTest
    : public GlicEnablingGeicPolicyTest {
 public:
  GlicEnablingGeicPolicyCountryCheckTest() {
    country_feature_list_.InitAndEnableFeature(features::kGlicCountryFiltering);
  }

 protected:
  void SetCountry(const std::string& country) {
    auto delegate = std::make_unique<TestDelegate>();
    delegate->SetBothCountryCodes(country);
    g_browser_process->GetFeatures()
        ->glic_global_enabling()
        .UpdateStateForTesting(std::move(delegate));
  }

 private:
  base::test::ScopedFeatureList country_feature_list_;
};

// Gemini Enterprise uses the consumer country filter, at the same layer
// (profile enablement, not the provider latch).
TEST_F(GlicEnablingGeicPolicyCountryCheckTest, GeicUsesConsumerCountryFilter) {
  SetCountry("zz");
  EXPECT_EQ(GlicProviderLatch::GetProvider(*profile()),
            GlicProvider::kGeminiEnterprise);
  EXPECT_TRUE(GlicEnabling::IsProfileEligible(profile()));
  GlicEnabling::ProfileEnablement enablement =
      GlicEnabling::EnablementForProfile(profile());
  EXPECT_FALSE(enablement.allowed_by_country_filter);
  EXPECT_FALSE(enablement.IsEnabled());
  EXPECT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kIneligible);

  {
    GlicEnabling::ScopedBypassEnablementChecksForTesting bypass;
    EXPECT_TRUE(GlicEnabling::IsEnabledForProfile(profile()));
  }

  SetCountry("us");
  EXPECT_TRUE(GlicEnabling::IsEnabledForProfile(profile()));
  EXPECT_EQ(GlicEnabling::GetProfileReadyState(profile()),
            mojom::ProfileReadyState::kReady);
}

// Gemini Enterprise doesn't use the consumer First Run Gemini step, so the
// First Run check is false even for an eligible account in an allowed country.
TEST_F(GlicEnablingGeicPolicyCountryCheckTest,
       GeicIsNotEnabledForFirstRunProfile) {
  SetCountry("us");
  ASSERT_TRUE(GlicEnabling::IsEnabledForProfile(profile()));

  auto* identity_test_env = identity_test_env_adaptor_->identity_test_env();
  AccountInfo account_info = identity_test_env->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info);
  SetGlicCapability(mutator, true);
  signin::UpdateAccountInfoForAccount(identity_test_env->identity_manager(),
                                      account_info);

  EXPECT_FALSE(GlicEnabling::IsEnabledForFirstRunProfile(profile(), "us", "us",
                                                         account_info));
}

// Gemini Enterprise skips the server-side user status check, so it doesn't
// create the user status fetcher.
TEST_F(GlicEnablingGeicPolicyTest, GeicDoesNotCreateUserStatusFetcher) {
  ASSERT_TRUE(base::FeatureList::IsEnabled(features::kGlicUserStatusCheck));
  ASSERT_EQ(GlicProviderLatch::GetProvider(*profile()),
            GlicProvider::kGeminiEnterprise);
  GlicKeyedService* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(profile(), /*create=*/true);
  ASSERT_TRUE(service);
  EXPECT_FALSE(service->enabling().HasUserStatusFetcherForTesting());
  // Refresh requests are ignored rather than crashing.
  service->enabling().UpdateUserStatusWithThrottling();
}

// Consumer Gemini creates the user status fetcher.
TEST_F(GlicEnablingGeicPolicyFeatureDisabledTest,
       ConsumerCreatesUserStatusFetcher) {
  ASSERT_TRUE(base::FeatureList::IsEnabled(features::kGlicUserStatusCheck));
  ASSERT_EQ(GlicProviderLatch::GetProvider(*profile()), GlicProvider::kGemini);
  GlicKeyedService* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(profile(), /*create=*/true);
  ASSERT_TRUE(service);
  EXPECT_TRUE(service->enabling().HasUserStatusFetcherForTesting());
}

// Consumer Gemini observes the GeminiSettings policy and fires enablement
// callbacks when it changes.
TEST_F(GlicEnablingGeicPolicyFeatureDisabledTest,
       ConsumerObservesGeminiSettingsPolicy) {
  GlicKeyedService* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(profile(), /*create=*/true);
  ASSERT_TRUE(service);

  int callback_count = 0;
  base::CallbackListSubscription subscription =
      service->enabling().RegisterAllowedChanged(
          base::BindRepeating([](int* count) { (*count)++; }, &callback_count));

  profile()->GetPrefs()->SetInteger(
      optimization_guide::prefs::kGeminiSettings,
      std::to_underlying(
          optimization_guide::prefs::GeminiSettingsPolicyState::kDisabled));

  EXPECT_GT(callback_count, 0);
}

#if BUILDFLAG(IS_ANDROID)
TEST(GlicAndroidFormFactorTest, PhoneAndDesktopAllowedWithoutTabletFlag) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kGlicAndroidTablet);

  EXPECT_TRUE(GlicGlobalEnabling::IsAndroidFormFactorAllowed(
      ui::DEVICE_FORM_FACTOR_PHONE, /*is_foldable=*/false));
  EXPECT_TRUE(GlicGlobalEnabling::IsAndroidFormFactorAllowed(
      ui::DEVICE_FORM_FACTOR_DESKTOP, /*is_foldable=*/false));
}

TEST(GlicAndroidFormFactorTest, TabletRequiresTabletFlag) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kGlicAndroidTablet);
    EXPECT_FALSE(GlicGlobalEnabling::IsAndroidFormFactorAllowed(
        ui::DEVICE_FORM_FACTOR_TABLET, /*is_foldable=*/false));
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(features::kGlicAndroidTablet);
    EXPECT_TRUE(GlicGlobalEnabling::IsAndroidFormFactorAllowed(
        ui::DEVICE_FORM_FACTOR_TABLET, /*is_foldable=*/false));
  }
}

// An unfolded foldable reports DEVICE_FORM_FACTOR_TABLET, but should be allowed
// without the tablet flag.
TEST(GlicAndroidFormFactorTest, UnfoldedFoldableAllowedWithoutTabletFlag) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kGlicAndroidTablet);

  EXPECT_TRUE(GlicGlobalEnabling::IsAndroidFormFactorAllowed(
      ui::DEVICE_FORM_FACTOR_TABLET, /*is_foldable=*/true));
}

TEST(GlicAndroidFormFactorTest, FoldedFoldableAllowedWithoutTabletFlag) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kGlicAndroidTablet);

  EXPECT_TRUE(GlicGlobalEnabling::IsAndroidFormFactorAllowed(
      ui::DEVICE_FORM_FACTOR_PHONE, /*is_foldable=*/true));
}

TEST(GlicAndroidFormFactorTest, FoldableFormFactorAllowedWithoutTabletFlag) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kGlicAndroidTablet);

  EXPECT_TRUE(GlicGlobalEnabling::IsAndroidFormFactorAllowed(
      ui::DEVICE_FORM_FACTOR_FOLDABLE, /*is_foldable=*/false));
}

// TV, automotive, and XR are not supported, even with the tablet flag enabled.
TEST(GlicAndroidFormFactorTest, UnsupportedFormFactorsNotAllowed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicAndroidTablet);

  EXPECT_FALSE(GlicGlobalEnabling::IsAndroidFormFactorAllowed(
      ui::DEVICE_FORM_FACTOR_TV, /*is_foldable=*/false));
  EXPECT_FALSE(GlicGlobalEnabling::IsAndroidFormFactorAllowed(
      ui::DEVICE_FORM_FACTOR_AUTOMOTIVE, /*is_foldable=*/false));
  EXPECT_FALSE(GlicGlobalEnabling::IsAndroidFormFactorAllowed(
      ui::DEVICE_FORM_FACTOR_XR, /*is_foldable=*/false));
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace glic
