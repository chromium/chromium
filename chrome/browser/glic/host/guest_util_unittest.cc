// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/guest_util.h"

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_features.mojom-features.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/scoped_browser_locale.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"
#include "components/prefs/pref_service.h"
#include "components/skills/features.h"
#include "components/skills/public/skills_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/url_util.h"
#include "pdf/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace glic {

namespace {

using testing::Contains;
using testing::Not;

class GuestUtilTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

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

TEST_F(GuestUtilTest, GetLocalizedGuestURLDoesNotChangeLanguageParameter) {
  EXPECT_EQ(GURL("https://www.google.com?hl=es"),
            GetLocalizedGuestURL(GURL("https://www.google.com?hl=es")));
}

TEST_F(GuestUtilTest, GetLocalizedGuestURLForDifferentLocales) {
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
  EXPECT_EQ(GURL("https://www.example.com/glic?hl=en"),
            GetGuestURL(CreateTestingProfile()));
}

// When features::kGeic is enabled with a GEiC guest URL, GetGuestURL loads the
// GEiC guest URL for the profile.
TEST_F(GuestUtilMultiInstanceTest, GeicEnabledLoadsGeicGuestURL) {
  ScopedBrowserLocale scoped_locale("en");
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGeic,
        {{features::kGeicGuestURL.name,
          "https://business.gemini.google/side-panel"}}}},
      {});

  EXPECT_EQ(GURL("https://business.gemini.google/side-panel?hl=en"),
            GetGuestURL(CreateTestingProfile()));
}

TEST_F(GuestUtilMultiInstanceTest,
       GeicEnabledLoadsPolicyGuestURLAndAllowsOrigin) {
  ScopedBrowserLocale scoped_locale("en");
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kGeic, features::kGlicGuestUrlPresets}, {});

  // Even if a consumer Glic preset URL is active in local_state, GEiC must not
  // allow it to override the validated GEiC guest URL.
  TestingBrowserProcess::GetGlobal()->local_state()->SetString(
      prefs::kGlicGuestUrlPresetAutopush, "https://evil.com/preset");

  TestingProfile* profile = CreateTestingProfile();
  base::DictValue settings;
  settings.Set("url", "https://business.gemini.google/home/cid/abc123");
  profile->GetPrefs()->SetDict(glic::prefs::kGlicGeminiEnterpriseSettings,
                               std::move(settings));

  EXPECT_EQ(
      GURL("https://business.gemini.google/side-panel?configId=abc123&hl=en"),
      GetGuestURL(profile));
  const url::Origin geic_origin =
      url::Origin::Create(GURL("https://business.gemini.google"));
  EXPECT_TRUE(IsOriginAllowedGlicApi(geic_origin, profile));
  EXPECT_TRUE(IsGuestOriginAllowed(geic_origin, profile));
  EXPECT_FALSE(IsOriginAllowedGlicApi(
      url::Origin::Create(GURL("https://evil.com/preset")), profile));
}

// When features::kGeic is enabled without a GEiC guest URL configured, the
// profile does not enter GEiC mode and falls back to the consumer guest URL.
TEST_F(GuestUtilMultiInstanceTest,
       GeicFeatureWithoutGuestURLFallsBackToConsumerGuestURL) {
  ScopedBrowserLocale scoped_locale("en");
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGeic);

  EXPECT_EQ(GURL("https://www.example.com/glic?hl=en"),
            GetGuestURL(CreateTestingProfile()));
}

// Without GEiC enabled, a consumer guest URL override is respected.
TEST_F(GuestUtilTest, GeicDisabledKeepsConsumerGuestURL) {
  ScopedBrowserLocale scoped_locale("en");
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGlicURLConfig,
        {{features::kGlicGuestURL.name,
          "https://gemini.google.com/custom-panel"}}}},
      {});

  EXPECT_EQ(GURL("https://gemini.google.com/custom-panel?hl=en"),
            GetGuestURL(&profile_));
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

TEST_F(GuestUtilMultiInstanceTest,
       PopulateGlobalClientInitialState_SkillsV2EnabledWhenFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kSkillsWebViewV2Enabled);

  TestingProfile* profile = CreateTestingProfile();

  auto state = mojom::WebClientInitialState::New();
  PopulateGlobalClientInitialState(state.get(), profile);

  bool found_capability = false;
  for (const mojom::HostCapability& capability : state->host_capabilities) {
    if (capability == mojom::HostCapability::kSkillsV2) {
      found_capability = true;
      break;
    }
  }
  EXPECT_TRUE(found_capability);
}

TEST_F(GuestUtilMultiInstanceTest,
       PopulateGlobalClientInitialState_SkillsV2DisabledWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kSkillsWebViewV2Enabled);

  TestingProfile* profile = CreateTestingProfile();

  auto state = mojom::WebClientInitialState::New();
  PopulateGlobalClientInitialState(state.get(), profile);

  bool found_capability = false;
  for (const mojom::HostCapability& capability : state->host_capabilities) {
    if (capability == mojom::HostCapability::kSkillsV2) {
      found_capability = true;
      break;
    }
  }
  EXPECT_FALSE(found_capability);
}

TEST_F(GuestUtilMultiInstanceTest,
       PopulateGlobalClientInitialState_EmbeddedPdfBytesExtractionDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      page_content_annotations::kGlicEmbeddedPdfBytesExtraction);

  TestingProfile* profile = CreateTestingProfile();
  auto state = mojom::WebClientInitialState::New();
  PopulateGlobalClientInitialState(state.get(), profile);

  EXPECT_THAT(
      state->host_capabilities,
      Not(Contains(mojom::HostCapability::kEmbeddedPdfBytesExtraction)));
}

TEST_F(GuestUtilMultiInstanceTest,
       PopulateGlobalClientInitialState_EmbeddedPdfBytesExtractionEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      page_content_annotations::kGlicEmbeddedPdfBytesExtraction);

  TestingProfile* profile = CreateTestingProfile();
  auto state = mojom::WebClientInitialState::New();
  PopulateGlobalClientInitialState(state.get(), profile);

#if BUILDFLAG(ENABLE_PDF)
  EXPECT_THAT(state->host_capabilities,
              Contains(mojom::HostCapability::kEmbeddedPdfBytesExtraction));
#else
  EXPECT_THAT(
      state->host_capabilities,
      Not(Contains(mojom::HostCapability::kEmbeddedPdfBytesExtraction)));
#endif
}

TEST_F(GuestUtilTest, IsOriginAllowedGlicApiWildcardMatching) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGlicURLConfig,
        {{features::kGlicGuestURL.name, "https://cat.fun/party"}}},
       {features::kGlicCSPConfig,
        {{features::kGlicApiAllowedOrigins.name,
          "https://*.mouse.org https://dog.com"}}}},
      {});

  EXPECT_TRUE(IsOriginAllowedGlicApi(
      url::Origin::Create(GURL("https://cat.fun/party")), &profile_));
  EXPECT_FALSE(IsOriginAllowedGlicApi(
      url::Origin::Create(GURL("https://cat.fun:42/party")), &profile_));
  EXPECT_TRUE(IsOriginAllowedGlicApi(
      url::Origin::Create(GURL("https://sub.mouse.org/party")), &profile_));
  EXPECT_TRUE(IsOriginAllowedGlicApi(
      url::Origin::Create(GURL("https://inner.sub.mouse.org/party")),
      &profile_));
  EXPECT_FALSE(IsOriginAllowedGlicApi(
      url::Origin::Create(GURL("https://sub.mouse.org:99/party")), &profile_));
  EXPECT_FALSE(IsOriginAllowedGlicApi(
      url::Origin::Create(GURL("https://mouse.org")), &profile_));
  EXPECT_FALSE(IsOriginAllowedGlicApi(
      url::Origin::Create(GURL("https://amouse.org")), &profile_));
  EXPECT_TRUE(IsOriginAllowedGlicApi(
      url::Origin::Create(GURL("https://dog.com/party")), &profile_));
  EXPECT_FALSE(IsOriginAllowedGlicApi(
      url::Origin::Create(GURL("https://dog.com:99/party")), &profile_));
  EXPECT_FALSE(IsOriginAllowedGlicApi(
      url::Origin::Create(GURL("http://dog.com/party")), &profile_));
}

TEST_F(GuestUtilTest, IsOriginAllowedGlicApiPortMatching) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGlicURLConfig,
        {{features::kGlicGuestURL.name, "https://cat.fun/party"}}},
       {features::kGlicCSPConfig,
        {{features::kGlicApiAllowedOrigins.name,
          "https://dog.com:8080 http://cat.fun"}}}},
      {});

  EXPECT_TRUE(IsOriginAllowedGlicApi(
      url::Origin::Create(GURL("https://dog.com:8080/party")), &profile_));
  EXPECT_FALSE(IsOriginAllowedGlicApi(
      url::Origin::Create(GURL("https://dog.com:99/party")), &profile_));
  EXPECT_FALSE(IsOriginAllowedGlicApi(
      url::Origin::Create(GURL("http://cat.fun:42")), &profile_));
  EXPECT_TRUE(IsOriginAllowedGlicApi(
      url::Origin::Create(GURL("http://cat.fun:80")), &profile_));
}

TEST_F(GuestUtilTest, IsOriginAllowedGlicApiDevMode) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGlicURLConfig,
        {{features::kGlicGuestURL.name, "https://cat.fun/party"}}},
       {features::kGlicCSPConfig,
        {{features::kGlicApiAllowedOrigins.name, ""}}}},
      {});
  base::CommandLine::ForCurrentProcess()->AppendSwitch(::switches::kGlicDev);

  EXPECT_TRUE(IsOriginAllowedGlicApi(
      url::Origin::Create(GURL("https://cat.fun/party")), &profile_));
  EXPECT_TRUE(IsOriginAllowedGlicApi(
      url::Origin::Create(GURL("https://dog.fun/")), &profile_));
  EXPECT_FALSE(IsOriginAllowedGlicApi(
      url::Origin::Create(GURL("data:text/html,hello")), &profile_));
}

TEST_F(GuestUtilTest, IsOriginAllowedGlicApiHttp) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGlicURLConfig,
        {{features::kGlicGuestURL.name, "http://test.com"}}}},
      {});

  EXPECT_TRUE(IsOriginAllowedGlicApi(
      url::Origin::Create(GURL("http://test.com")), &profile_));
  EXPECT_FALSE(IsOriginAllowedGlicApi(
      url::Origin::Create(GURL("https://test.com")), &profile_));
  EXPECT_FALSE(IsOriginAllowedGlicApi(
      url::Origin::Create(GURL("http://other.com")), &profile_));
}

TEST_F(GuestUtilTest, IsGuestOriginAllowedWildcardMatching) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGlicURLConfig,
        {{features::kGlicGuestURL.name, "https://cat.fun/party"}}},
       {features::kGlicCSPConfig,
        {{features::kGlicAllowedOriginsOverride.name,
          "https://*.mouse.org https://dog.com"}}}},
      {});

  // Primary guest URL is allowed via IsOriginAllowedGlicApi.
  EXPECT_TRUE(IsGuestOriginAllowed(
      url::Origin::Create(GURL("https://cat.fun/party")), &profile_));
  EXPECT_TRUE(IsGuestOriginAllowed(
      url::Origin::Create(GURL("https://cat.fun/other")), &profile_));
  // Allowed origins wildcard matching.
  EXPECT_TRUE(IsGuestOriginAllowed(
      url::Origin::Create(GURL("https://sub.mouse.org/party")), &profile_));
  EXPECT_TRUE(IsGuestOriginAllowed(
      url::Origin::Create(GURL("https://inner.sub.mouse.org/party")),
      &profile_));
  EXPECT_FALSE(IsGuestOriginAllowed(
      url::Origin::Create(GURL("https://mouse.org")), &profile_));
  EXPECT_FALSE(IsGuestOriginAllowed(
      url::Origin::Create(GURL("https://amouse.org")), &profile_));
  EXPECT_TRUE(IsGuestOriginAllowed(
      url::Origin::Create(GURL("https://dog.com/party")), &profile_));
  EXPECT_FALSE(IsGuestOriginAllowed(
      url::Origin::Create(GURL("https://dog.com:99/party")), &profile_));
  EXPECT_FALSE(IsGuestOriginAllowed(
      url::Origin::Create(GURL("http://dog.com/party")), &profile_));
  EXPECT_FALSE(IsGuestOriginAllowed(
      url::Origin::Create(GURL("https://evil.com")), &profile_));
}

TEST_F(GuestUtilTest, IsGuestOriginAllowedAuthOrigins) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGlicURLConfig,
        {{features::kGlicGuestURL.name, "https://cat.fun/party"}}},
       {features::kGlicCSPConfig,
        {{features::kGlicAllowedOriginsOverride.name, ""},
         {features::kGlicApiAllowedOrigins.name, ""}}}},
      {});

  EXPECT_TRUE(IsGuestOriginAllowed(
      url::Origin::Create(GURL("https://login.corp.google.com")), &profile_));
  EXPECT_TRUE(IsGuestOriginAllowed(
      url::Origin::Create(GURL("https://accounts.google.com")), &profile_));
  EXPECT_TRUE(IsGuestOriginAllowed(
      url::Origin::Create(GURL("https://accounts.googlers.com")), &profile_));
  EXPECT_TRUE(IsGuestOriginAllowed(
      url::Origin::Create(GURL("https://gaiastaging.corp.google.com")),
      &profile_));

  // Disallow HTTP auth origins.
  EXPECT_FALSE(IsGuestOriginAllowed(
      url::Origin::Create(GURL("http://accounts.google.com")), &profile_));

  // Disallow attacker spoofing domain.
  EXPECT_FALSE(IsGuestOriginAllowed(
      url::Origin::Create(GURL("https://accounts.google.com.attacker.com")),
      &profile_));
}

TEST_F(GuestUtilTest, IsGuestOriginAllowedOpaqueOrigin) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGlicURLConfig,
        {{features::kGlicGuestURL.name, "https://cat.fun/party"}}},
       {features::kGlicCSPConfig,
        {{features::kGlicAllowedOriginsOverride.name, "https://dog.com"}}}},
      {});
  base::CommandLine::ForCurrentProcess()->AppendSwitch(::switches::kGlicDev);

  EXPECT_FALSE(IsGuestOriginAllowed(
      url::Origin::Create(GURL("data:text/html,hello")), &profile_));
}

TEST_F(GuestUtilTest, IsGuestOriginAllowedCorpOrigins) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGlicURLConfig,
        {{features::kGlicGuestURL.name, "https://cat.fun/party"}}},
       {features::kGlicCSPConfig,
        {{features::kGlicAllowedOriginsOverride.name, ""},
         {features::kGlicApiAllowedOrigins.name, ""}}}},
      {});

  EXPECT_TRUE(IsGuestOriginAllowed(
      url::Origin::Create(GURL("https://gemini.corp.google.com")), &profile_));
  EXPECT_TRUE(IsGuestOriginAllowed(
      url::Origin::Create(GURL("https://subdomain.corp.google.com")),
      &profile_));
  EXPECT_FALSE(IsGuestOriginAllowed(
      url::Origin::Create(GURL("http://gemini.corp.google.com")), &profile_));
  EXPECT_FALSE(IsGuestOriginAllowed(
      url::Origin::Create(GURL("https://corp.google.com.attacker.com")),
      &profile_));
}

}  // namespace

}  // namespace glic
