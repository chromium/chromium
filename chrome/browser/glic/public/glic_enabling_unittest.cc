// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_enabling.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
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
            features::kTabstripComboButton,
#if BUILDFLAG(IS_CHROMEOS)
            chromeos::features::kFeatureManagementGlic,
#endif  // BUILDFLAG(IS_CHROMEOS)
        },
        {});
  }

  void TearDown() override {
    scoped_feature_list_.Reset();
    testing::Test::TearDown();
  }

 protected:
  TestDelegate delegate_;
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
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

TEST_F(GlicEnablingTest, TabStripComboButtonFeatureNotEnabledTest) {
  // Turn tab strip combo button feature flag off
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures({}, {features::kTabstripComboButton});
  EXPECT_EQ(GlicGlobalEnabling(delegate_).IsEnabledByFlags(), false);
}

TEST_F(GlicEnablingTest, CountryFilteringNotEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kGlicCountryFiltering);
  delegate_.SetCountryCode("zz");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByFlags());
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
}

TEST_F(GlicEnablingTest, LocaleFilteringNotEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kGlicLocaleFiltering);
  delegate_.SetLocale("foobar");
  EXPECT_TRUE(GlicGlobalEnabling(delegate_).IsEnabledByFlags());
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
}

// Test for `glic::GlicEnabling::IsProfileEligible`.
class GlicEnablingProfileEligibilityTest : public testing::Test {
 public:
  GlicEnablingProfileEligibilityTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            features::kGlic,
            features::kTabstripComboButton,
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

}  // namespace
}  // namespace glic
