// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/guest_util.h"

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_features.mojom-features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/scoped_browser_locale.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/skills/features.h"
#include "components/skills/public/skills_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace glic {

namespace {

class GuestUtilTest : public testing::Test {};

// Test fixture for multi-instance feature.
class GuestUtilMultiInstanceTest : public testing::Test {
 public:
  GuestUtilMultiInstanceTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    enabled_features.push_back(
        {features::kGlicURLConfig,
         {{features::kGlicGuestURL.name, "https://www.example.com/glic"}}});

    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
  }

  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

  TestingProfile* CreateTestingProfile() {
    return profile_manager_.CreateTestingProfile("test_profile");
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
};

TEST(GuestUtilTest, GetLocalizedGuestURLDoesNotChangeLanguageParameter) {
  EXPECT_EQ(GURL("https://www.google.com?hl=es"),
            GetLocalizedGuestURL(GURL("https://www.google.com?hl=es")));
}

TEST(GuestUtilTest, GetLocalizedGuestURLForDifferentLocales) {
  struct LocaleTestCase {
    std::string locale;
    std::string expected_hl;
  } test_cases[] = {
      {"en", "en"},       {"es", "es"},       {"es-419", "es-419"},
      {"es-MX", "es-MX"}, {"en-GB", "en-GB"}, {"nb", "no"},
  };
  for (const auto& test_case : test_cases) {
    ScopedBrowserLocale scoped_locale(test_case.locale);
    EXPECT_EQ(GURL("https://www.google.com?hl=" + test_case.expected_hl),
              GetLocalizedGuestURL(GURL("https://www.google.com")));
  }
}

TEST_F(GuestUtilMultiInstanceTest, GetGlicGuestURLs) {
  EXPECT_EQ(GURL("https://www.example.com/glic?hl=en"), GetGuestURL());
}

TEST_F(GuestUtilMultiInstanceTest,
       PopulateGlobalClientInitialState_SkillsDisabledWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kSkillsEnabled);

  TestingProfile* profile = CreateTestingProfile();
  profile->GetPrefs()->SetBoolean(skills::prefs::kChromeSkillsEnabled, true);

  auto state = mojom::WebClientInitialState::New();
  PopulateGlobalClientInitialState(state.get(), profile);

  EXPECT_FALSE(state->enable_skills);
}

TEST_F(
    GuestUtilMultiInstanceTest,
    PopulateGlobalClientInitialState_SkillsEnabledWhenFeatureAndPrefEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kSkillsEnabled);

  TestingProfile* profile = CreateTestingProfile();
  profile->GetPrefs()->SetBoolean(skills::prefs::kChromeSkillsEnabled, true);

  auto state = mojom::WebClientInitialState::New();
  PopulateGlobalClientInitialState(state.get(), profile);

  EXPECT_TRUE(state->enable_skills);
}

TEST_F(GuestUtilMultiInstanceTest,
       PopulateGlobalClientInitialState_SkillsDisabledWhenPrefDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kSkillsEnabled);

  TestingProfile* profile = CreateTestingProfile();
  profile->GetPrefs()->SetBoolean(skills::prefs::kChromeSkillsEnabled, false);

  auto state = mojom::WebClientInitialState::New();
  PopulateGlobalClientInitialState(state.get(), profile);

  EXPECT_FALSE(state->enable_skills);
}

TEST(GuestUtilTest, IsOriginAllowedGlicApiWildcardMatching) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGlicURLConfig,
        {{features::kGlicGuestURL.name, "https://cat.fun/party"}}},
       {features::kGlicCSPConfig,
        {{features::kGlicApiAllowedOrigins.name,
          "https://*.mouse.org https://dog.com"}}}},
      {});

  EXPECT_TRUE(IsOriginAllowedGlicApi(
      url::Origin::Create(GURL("https://cat.fun/party"))));
  EXPECT_FALSE(IsOriginAllowedGlicApi(
      url::Origin::Create(GURL("https://cat.fun:42/party"))));
  EXPECT_TRUE(IsOriginAllowedGlicApi(
      url::Origin::Create(GURL("https://sub.mouse.org/party"))));
  EXPECT_TRUE(IsOriginAllowedGlicApi(
      url::Origin::Create(GURL("https://inner.sub.mouse.org/party"))));
  EXPECT_FALSE(IsOriginAllowedGlicApi(
      url::Origin::Create(GURL("https://sub.mouse.org:99/party"))));
  EXPECT_FALSE(
      IsOriginAllowedGlicApi(url::Origin::Create(GURL("https://mouse.org"))));
  EXPECT_FALSE(
      IsOriginAllowedGlicApi(url::Origin::Create(GURL("https://amouse.org"))));
  EXPECT_TRUE(IsOriginAllowedGlicApi(
      url::Origin::Create(GURL("https://dog.com/party"))));
  EXPECT_FALSE(IsOriginAllowedGlicApi(
      url::Origin::Create(GURL("https://dog.com:99/party"))));
  EXPECT_FALSE(IsOriginAllowedGlicApi(
      url::Origin::Create(GURL("http://dog.com/party"))));
}

TEST(GuestUtilTest, IsOriginAllowedGlicApiPortMatching) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGlicURLConfig,
        {{features::kGlicGuestURL.name, "https://cat.fun/party"}}},
       {features::kGlicCSPConfig,
        {{features::kGlicApiAllowedOrigins.name,
          "https://dog.com:8080 http://cat.fun"}}}},
      {});

  EXPECT_TRUE(IsOriginAllowedGlicApi(
      url::Origin::Create(GURL("https://dog.com:8080/party"))));
  EXPECT_FALSE(IsOriginAllowedGlicApi(
      url::Origin::Create(GURL("https://dog.com:99/party"))));
  EXPECT_FALSE(
      IsOriginAllowedGlicApi(url::Origin::Create(GURL("http://cat.fun:42"))));
  EXPECT_TRUE(
      IsOriginAllowedGlicApi(url::Origin::Create(GURL("http://cat.fun:80"))));
}

TEST(GuestUtilTest, IsOriginAllowedGlicApiDevMode) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGlicURLConfig,
        {{features::kGlicGuestURL.name, "https://cat.fun/party"}}},
       {features::kGlicCSPConfig,
        {{features::kGlicApiAllowedOrigins.name, ""}}}},
      {});
  base::CommandLine::ForCurrentProcess()->AppendSwitch(::switches::kGlicDev);

  EXPECT_TRUE(IsOriginAllowedGlicApi(
      url::Origin::Create(GURL("https://cat.fun/party"))));
  EXPECT_TRUE(
      IsOriginAllowedGlicApi(url::Origin::Create(GURL("https://dog.fun/"))));
  EXPECT_FALSE(IsOriginAllowedGlicApi(
      url::Origin::Create(GURL("data:text/html,hello"))));
}

TEST(GuestUtilTest, IsOriginAllowedGlicApiHttp) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGlicURLConfig,
        {{features::kGlicGuestURL.name, "http://test.com"}}}},
      {});

  EXPECT_TRUE(
      IsOriginAllowedGlicApi(url::Origin::Create(GURL("http://test.com"))));
  EXPECT_FALSE(
      IsOriginAllowedGlicApi(url::Origin::Create(GURL("https://test.com"))));
  EXPECT_FALSE(
      IsOriginAllowedGlicApi(url::Origin::Create(GURL("http://other.com"))));
}

TEST(GuestUtilTest, IsGuestOriginAllowedWildcardMatching) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGlicURLConfig,
        {{features::kGlicGuestURL.name, "https://cat.fun/party"}}},
       {features::kGlicCSPConfig,
        {{features::kGlicAllowedOriginsOverride.name,
          "https://*.mouse.org https://dog.com"}}}},
      {});

  // Primary guest URL is allowed via IsOriginAllowedGlicApi.
  EXPECT_TRUE(
      IsGuestOriginAllowed(url::Origin::Create(GURL("https://cat.fun/party"))));
  EXPECT_TRUE(
      IsGuestOriginAllowed(url::Origin::Create(GURL("https://cat.fun/other"))));
  // Allowed origins wildcard matching.
  EXPECT_TRUE(IsGuestOriginAllowed(
      url::Origin::Create(GURL("https://sub.mouse.org/party"))));
  EXPECT_TRUE(IsGuestOriginAllowed(
      url::Origin::Create(GURL("https://inner.sub.mouse.org/party"))));
  EXPECT_FALSE(
      IsGuestOriginAllowed(url::Origin::Create(GURL("https://mouse.org"))));
  EXPECT_FALSE(
      IsGuestOriginAllowed(url::Origin::Create(GURL("https://amouse.org"))));
  EXPECT_TRUE(
      IsGuestOriginAllowed(url::Origin::Create(GURL("https://dog.com/party"))));
  EXPECT_FALSE(IsGuestOriginAllowed(
      url::Origin::Create(GURL("https://dog.com:99/party"))));
  EXPECT_FALSE(
      IsGuestOriginAllowed(url::Origin::Create(GURL("http://dog.com/party"))));
  EXPECT_FALSE(
      IsGuestOriginAllowed(url::Origin::Create(GURL("https://evil.com"))));
}

TEST(GuestUtilTest, IsGuestOriginAllowedAuthOrigins) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGlicURLConfig,
        {{features::kGlicGuestURL.name, "https://cat.fun/party"}}},
       {features::kGlicCSPConfig,
        {{features::kGlicAllowedOriginsOverride.name, ""},
         {features::kGlicApiAllowedOrigins.name, ""}}}},
      {});

  EXPECT_TRUE(IsGuestOriginAllowed(
      url::Origin::Create(GURL("https://login.corp.google.com"))));
  EXPECT_TRUE(IsGuestOriginAllowed(
      url::Origin::Create(GURL("https://accounts.google.com"))));
  EXPECT_TRUE(IsGuestOriginAllowed(
      url::Origin::Create(GURL("https://accounts.googlers.com"))));
  EXPECT_TRUE(IsGuestOriginAllowed(
      url::Origin::Create(GURL("https://gaiastaging.corp.google.com"))));

  // Disallow HTTP auth origins.
  EXPECT_FALSE(IsGuestOriginAllowed(
      url::Origin::Create(GURL("http://accounts.google.com"))));

  // Disallow attacker spoofing domain.
  EXPECT_FALSE(IsGuestOriginAllowed(
      url::Origin::Create(GURL("https://accounts.google.com.attacker.com"))));
}

TEST(GuestUtilTest, IsGuestOriginAllowedOpaqueOrigin) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGlicURLConfig,
        {{features::kGlicGuestURL.name, "https://cat.fun/party"}}},
       {features::kGlicCSPConfig,
        {{features::kGlicAllowedOriginsOverride.name, "https://dog.com"}}}},
      {});
  base::CommandLine::ForCurrentProcess()->AppendSwitch(::switches::kGlicDev);

  EXPECT_FALSE(
      IsGuestOriginAllowed(url::Origin::Create(GURL("data:text/html,hello"))));
}

}  // namespace

}  // namespace glic
