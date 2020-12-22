// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings.h"

#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/util/values/values_util.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Define an additional content setting value to simulate an unmanaged default
// content setting.
const ContentSetting kNoSetting = static_cast<ContentSetting>(-1);

struct CookieContentSettingException {
  std::string primary_pattern;
  std::string secondary_pattern;
  ContentSetting content_setting;
};

}  // namespace

class PrivacySandboxSettingsTest : public testing::Test {
 public:
  PrivacySandboxSettingsTest()
      : browser_task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    InitializePrefsBeforeStart();

    privacy_sandbox_settings_ = std::make_unique<PrivacySandboxSettings>(
        HostContentSettingsMapFactory::GetForProfile(profile()),
        CookieSettingsFactory::GetForProfile(profile()).get(),
        profile()->GetPrefs());
  }

  virtual void InitializePrefsBeforeStart() {}

  // Sets up preferences and content settings based on provided parameters.
  void SetupTestState(
      bool privacy_sandbox_available,
      bool privacy_sandbox_enabled,
      bool block_third_party_cookies,
      ContentSetting default_cookie_setting,
      std::vector<CookieContentSettingException> user_cookie_exceptions,
      ContentSetting managed_cookie_setting,
      std::vector<CookieContentSettingException> managed_cookie_exceptions) {
    // Setup block-third-party-cookies settings.
    if (block_third_party_cookies) {
      profile()->GetTestingPrefService()->SetUserPref(
          prefs::kCookieControlsMode,
          std::make_unique<base::Value>(static_cast<int>(
              content_settings::CookieControlsMode::kBlockThirdParty)));
    }

    // Setup cookie content settings.
    auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
    auto user_provider = std::make_unique<content_settings::MockProvider>();
    auto managed_provider = std::make_unique<content_settings::MockProvider>();

    if (default_cookie_setting != kNoSetting) {
      user_provider->SetWebsiteSetting(
          ContentSettingsPattern::Wildcard(),
          ContentSettingsPattern::Wildcard(), ContentSettingsType::COOKIES,
          std::make_unique<base::Value>(default_cookie_setting));
    }

    for (const auto& exception : user_cookie_exceptions) {
      user_provider->SetWebsiteSetting(
          ContentSettingsPattern::FromString(exception.primary_pattern),
          ContentSettingsPattern::FromString(exception.secondary_pattern),
          ContentSettingsType::COOKIES,
          std::make_unique<base::Value>(exception.content_setting));
    }

    if (managed_cookie_setting != kNoSetting) {
      managed_provider->SetWebsiteSetting(
          ContentSettingsPattern::Wildcard(),
          ContentSettingsPattern::Wildcard(), ContentSettingsType::COOKIES,
          std::make_unique<base::Value>(managed_cookie_setting));
    }

    for (const auto& exception : managed_cookie_exceptions) {
      managed_provider->SetWebsiteSetting(
          ContentSettingsPattern::FromString(exception.primary_pattern),
          ContentSettingsPattern::FromString(exception.secondary_pattern),
          ContentSettingsType::COOKIES,
          std::make_unique<base::Value>(exception.content_setting));
    }

    content_settings::TestUtils::OverrideProvider(
        map, std::move(user_provider),
        HostContentSettingsMap::DEFAULT_PROVIDER);
    content_settings::TestUtils::OverrideProvider(
        map, std::move(managed_provider),
        HostContentSettingsMap::POLICY_PROVIDER);

    // Setup privacy sandbox feature & preference.
    feature_list()->Reset();
    if (privacy_sandbox_available)
      feature_list()->InitAndEnableFeature(features::kPrivacySandboxSettings);
    else
      feature_list()->InitAndDisableFeature(features::kPrivacySandboxSettings);

    profile()->GetTestingPrefService()->SetUserPref(
        prefs::kPrivacySandboxApisEnabled,
        std::make_unique<base::Value>(privacy_sandbox_enabled));
  }

  TestingProfile* profile() { return &profile_; }
  PrivacySandboxSettings* privacy_sandbox_settings() {
    return privacy_sandbox_settings_.get();
  }
  base::test::ScopedFeatureList* feature_list() { return &feature_list_; }

 protected:
  std::unique_ptr<PrivacySandboxSettings> privacy_sandbox_settings_;

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  TestingProfile profile_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(PrivacySandboxSettingsTest, CookieSettingAppliesWhenUiDisabled) {
  // When the Privacy Sandbox UI is unavailable, the cookie setting should
  // apply directly.
  SetupTestState(
      /*privacy_sandbox_available=*/false,
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});
  EXPECT_TRUE(privacy_sandbox_settings()->IsFlocAllowed(
      GURL("https://embedded.com"),
      url::Origin::Create(GURL("https://test.com"))));

  EXPECT_TRUE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  SetupTestState(
      /*privacy_sandbox_available=*/false,
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_ALLOW},
       {"https://another-test.com", "*",
        ContentSetting::CONTENT_SETTING_BLOCK}},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  EXPECT_TRUE(privacy_sandbox_settings()->IsFlocAllowed(
      GURL("https://embedded.com"),
      url::Origin::Create(GURL("https://test.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->IsFlocAllowed(
      GURL("https://another-test.com"), base::nullopt));

  EXPECT_TRUE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  SetupTestState(
      /*privacy_sandbox_available=*/false,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_ALLOW}},
      /*managed_cookie_setting=*/kNoSetting,
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_BLOCK}});

  EXPECT_FALSE(privacy_sandbox_settings()->IsFlocAllowed(
      GURL("https://embedded.com"),
      url::Origin::Create(GURL("https://test.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->IsFlocAllowed(
      GURL("https://embedded.com"), base::nullopt));

  EXPECT_FALSE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
}

TEST_F(PrivacySandboxSettingsTest, PreferenceOverridesDefaultContentSetting) {
  // When the Privacy Sandbox UI is available, the sandbox preference should
  // override the default cookie content setting.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  EXPECT_TRUE(privacy_sandbox_settings()->IsFlocAllowed(
      GURL("https://embedded.com"),
      url::Origin::Create(GURL("https://test.com"))));

  EXPECT_TRUE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  // An allow exception should not override the preference value.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_ALLOW},
       {"https://embedded.com", "https://another-test.com",
        ContentSetting::CONTENT_SETTING_ALLOW}},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  EXPECT_FALSE(privacy_sandbox_settings()->IsFlocAllowed(
      GURL("https://embedded.com"),
      url::Origin::Create(GURL("https://test.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
}

TEST_F(PrivacySandboxSettingsTest, CookieBlockExceptionsApply) {
  // When the Privacy Sandbox UI & preference are both enabled, targeted cookie
  // block exceptions should still apply.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_BLOCK}},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  EXPECT_FALSE(privacy_sandbox_settings()->IsFlocAllowed(
      GURL("https://embedded.com"),
      url::Origin::Create(GURL("https://test.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  // User created exceptions should not apply if a managed default coookie
  // setting exists. What the managed default setting actually is should *not*
  // affect whether APIs are enabled. The cookie managed state is reflected in
  // the privacy sandbox preferences directly.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_BLOCK},
       {"https://embedded.com", "https://another-test.com",
        ContentSetting::CONTENT_SETTING_BLOCK}},
      /*managed_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*managed_cookie_exceptions=*/{});

  EXPECT_TRUE(privacy_sandbox_settings()->IsFlocAllowed(
      GURL("https://embedded.com"),
      url::Origin::Create(GURL("https://test.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  // Managed content setting exceptions should override both the privacy
  // sandbox pref and any user settings.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_ALLOW},
       {"https://embedded.com", "https://another-test.com",
        ContentSetting::CONTENT_SETTING_ALLOW}},
      /*managed_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_BLOCK}});

  EXPECT_FALSE(privacy_sandbox_settings()->IsFlocAllowed(
      GURL("https://embedded.com"),
      url::Origin::Create(GURL("https://test.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->IsFlocAllowed(
      GURL("https://unrelated.com"),
      url::Origin::Create(GURL("https://unrelated.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://unrelated-a.com")),
      url::Origin::Create(GURL("https://unrelated-b.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://unrelated-c.com")),
      url::Origin::Create(GURL("https://unrelated-d.com")),
      url::Origin::Create(GURL("https://unrelated-e.com"))));

  // A less specific block exception should not override a more specific allow
  // exception. The effective content setting in this scenario is still allow,
  // even though a block exception exists.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_ALLOW},
       {"https://embedded.com", "https://another-test.com",
        ContentSetting::CONTENT_SETTING_ALLOW},
       {"https://[*.]embedded.com", "https://[*.]test.com",
        ContentSetting::CONTENT_SETTING_BLOCK},
       {"https://[*.]embedded.com", "https://[*.]another-test.com",
        ContentSetting::CONTENT_SETTING_BLOCK}},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});
  EXPECT_TRUE(privacy_sandbox_settings()->IsFlocAllowed(
      GURL("https://embedded.com"),
      url::Origin::Create(GURL("https://test.com"))));

  // Exceptions which specify a top frame origin should not match against other
  // top frame origins, or an empty origin.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_BLOCK}},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_BLOCK}});

  EXPECT_TRUE(privacy_sandbox_settings()->IsFlocAllowed(
      GURL("https://embedded.com"), base::nullopt));

  EXPECT_TRUE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://yet-another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  // Exceptions which specify a wildcard top frame origin should match both
  // empty top frames and non empty top frames.
  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/
      {{"https://embedded.com", "*", ContentSetting::CONTENT_SETTING_BLOCK}},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  EXPECT_FALSE(privacy_sandbox_settings()->IsFlocAllowed(
      GURL("https://embedded.com"), base::nullopt));
  EXPECT_FALSE(privacy_sandbox_settings()->IsFlocAllowed(
      GURL("https://embedded.com"),
      url::Origin::Create(GURL("https://test.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
}

TEST_F(PrivacySandboxSettingsTest, ThirdPartyByDefault) {
  // Check that when the UI is not enabled, all requests are considered
  // as third party requests.
  SetupTestState(
      /*privacy_sandbox_available=*/false,
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});

  EXPECT_FALSE(privacy_sandbox_settings()->IsFlocAllowed(
      GURL("https://embedded.com"),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->IsFlocAllowed(
      GURL("https://embedded.com"), base::nullopt));

  EXPECT_FALSE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://embedded.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://embedded.com")),
      url::Origin::Create(GURL("https://embedded.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
}

TEST_F(PrivacySandboxSettingsTest, IsPrivacySandboxAllowed) {
  SetupTestState(
      /*privacy_sandbox_available=*/false,
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});
  EXPECT_TRUE(privacy_sandbox_settings()->IsPrivacySandboxAllowed());

  SetupTestState(
      /*privacy_sandbox_available=*/false,
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});
  EXPECT_FALSE(privacy_sandbox_settings()->IsPrivacySandboxAllowed());

  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});
  EXPECT_FALSE(privacy_sandbox_settings()->IsPrivacySandboxAllowed());

  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});
  EXPECT_FALSE(privacy_sandbox_settings()->IsPrivacySandboxAllowed());

  SetupTestState(
      /*privacy_sandbox_available=*/true,
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/kNoSetting,
      /*managed_cookie_exceptions=*/{});
  EXPECT_TRUE(privacy_sandbox_settings()->IsPrivacySandboxAllowed());
}

TEST_F(PrivacySandboxSettingsTest, FlocDataAccessibleSince) {
  ASSERT_NE(base::Time(), base::Time::Now());

  EXPECT_EQ(base::Time(),
            privacy_sandbox_settings()->FlocDataAccessibleSince());

  privacy_sandbox_settings()->OnCookiesCleared();

  EXPECT_EQ(base::Time::Now(),
            privacy_sandbox_settings()->FlocDataAccessibleSince());
}

class PrivacySandboxSettingsTestCookiesClearOnExitTurnedOff
    : public PrivacySandboxSettingsTest {
 public:
  void InitializePrefsBeforeStart() override {
    profile()->GetTestingPrefService()->SetUserPref(
        prefs::kPrivacySandboxFlocDataAccessibleSince,
        std::make_unique<base::Value>(
            ::util::TimeToValue(base::Time::FromTimeT(12345))));
  }
};

TEST_F(PrivacySandboxSettingsTestCookiesClearOnExitTurnedOff,
       UseLastFlocDataAccessibleSince) {
  EXPECT_EQ(base::Time::FromTimeT(12345),
            privacy_sandbox_settings()->FlocDataAccessibleSince());
}

class PrivacySandboxSettingsTestCookiesClearOnExitTurnedOn
    : public PrivacySandboxSettingsTest {
 public:
  void InitializePrefsBeforeStart() override {
    auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
    map->SetDefaultContentSetting(ContentSettingsType::COOKIES,
                                  ContentSetting::CONTENT_SETTING_SESSION_ONLY);

    profile()->GetTestingPrefService()->SetUserPref(
        prefs::kPrivacySandboxFlocDataAccessibleSince,
        std::make_unique<base::Value>(
            ::util::TimeToValue(base::Time::FromTimeT(12345))));
  }
};

TEST_F(PrivacySandboxSettingsTestCookiesClearOnExitTurnedOn,
       UpdateFlocDataAccessibleSince) {
  EXPECT_EQ(base::Time::Now(),
            privacy_sandbox_settings()->FlocDataAccessibleSince());
}
