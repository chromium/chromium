// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/content_settings/content_settings_mock_observer.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/content_settings_default_provider.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;

namespace content_settings {

class ContentSettingsDefaultProviderTest : public testing::Test {
 public:
  ContentSettingsDefaultProviderTest()
      : provider_(profile_.GetPrefs(), false) {}
  ~ContentSettingsDefaultProviderTest() override {
    provider_.ShutdownOnUIThread();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  DefaultProvider provider_;
};

TEST_F(ContentSettingsDefaultProviderTest, DefaultValues) {
  // Check setting defaults.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            TestUtils::GetContentSetting(&provider_, GURL(), GURL(),
                                         ContentSettingsType::COOKIES,
                                         std::string(), false));
  provider_.SetWebsiteSetting(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, std::string(),
      std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            TestUtils::GetContentSetting(&provider_, GURL(), GURL(),
                                         ContentSettingsType::COOKIES,
                                         std::string(), false));

  EXPECT_EQ(CONTENT_SETTING_ASK,
            TestUtils::GetContentSetting(&provider_, GURL(), GURL(),
                                         ContentSettingsType::GEOLOCATION,
                                         std::string(), false));
  provider_.SetWebsiteSetting(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::GEOLOCATION, std::string(),
      std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            TestUtils::GetContentSetting(&provider_, GURL(), GURL(),
                                         ContentSettingsType::GEOLOCATION,
                                         std::string(), false));

  std::unique_ptr<base::Value> value(TestUtils::GetContentSettingValue(
      &provider_, GURL("http://example.com/"), GURL("http://example.com/"),
      ContentSettingsType::AUTO_SELECT_CERTIFICATE, std::string(), false));
  EXPECT_FALSE(value.get());
}

TEST_F(ContentSettingsDefaultProviderTest, IgnoreNonDefaultSettings) {
  GURL primary_url("http://www.google.com");
  GURL secondary_url("http://www.google.com");

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            TestUtils::GetContentSetting(&provider_, primary_url, secondary_url,
                                         ContentSettingsType::COOKIES,
                                         std::string(), false));
  std::unique_ptr<base::Value> value(new base::Value(CONTENT_SETTING_BLOCK));
  bool owned = provider_.SetWebsiteSetting(
      ContentSettingsPattern::FromURL(primary_url),
      ContentSettingsPattern::FromURL(secondary_url),
      ContentSettingsType::COOKIES, std::string(), std::move(value));
  EXPECT_FALSE(owned);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            TestUtils::GetContentSetting(&provider_, primary_url, secondary_url,
                                         ContentSettingsType::COOKIES,
                                         std::string(), false));
}

TEST_F(ContentSettingsDefaultProviderTest, Observer) {
  MockObserver mock_observer;
  EXPECT_CALL(mock_observer,
              OnContentSettingChanged(_, _, ContentSettingsType::COOKIES, ""));
  provider_.AddObserver(&mock_observer);
  provider_.SetWebsiteSetting(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, std::string(),
      std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));

  EXPECT_CALL(mock_observer, OnContentSettingChanged(
                                 _, _, ContentSettingsType::GEOLOCATION, ""));
  provider_.SetWebsiteSetting(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::GEOLOCATION, std::string(),
      std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
}

TEST_F(ContentSettingsDefaultProviderTest, ObservePref) {
  PrefService* prefs = profile_.GetPrefs();

  provider_.SetWebsiteSetting(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, std::string(),
      std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            TestUtils::GetContentSetting(&provider_, GURL(), GURL(),
                                         ContentSettingsType::COOKIES,
                                         std::string(), false));
  const WebsiteSettingsInfo* info =
      WebsiteSettingsRegistry::GetInstance()->Get(ContentSettingsType::COOKIES);
  // Clearing the backing pref should also clear the internal cache.
  prefs->ClearPref(info->default_value_pref_name());
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            TestUtils::GetContentSetting(&provider_, GURL(), GURL(),
                                         ContentSettingsType::COOKIES,
                                         std::string(), false));
  // Reseting the pref to its previous value should update the cache.
  prefs->SetInteger(info->default_value_pref_name(), CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            TestUtils::GetContentSetting(&provider_, GURL(), GURL(),
                                         ContentSettingsType::COOKIES,
                                         std::string(), false));
}

// Tests that fullscreen and mouselock content settings are cleared.
TEST_F(ContentSettingsDefaultProviderTest, DiscardObsoletePreferences) {
  static const char kFullscreenPrefPath[] =
      "profile.default_content_setting_values.fullscreen";
#if !defined(OS_ANDROID)
  static const char kMouselockPrefPath[] =
      "profile.default_content_setting_values.mouselock";
#endif
  static const char kGeolocationPrefPath[] =
      "profile.default_content_setting_values.geolocation";

  PrefService* prefs = profile_.GetPrefs();
  // Set some pref data.
  prefs->SetInteger(kFullscreenPrefPath, CONTENT_SETTING_BLOCK);
#if !defined(OS_ANDROID)
  prefs->SetInteger(kMouselockPrefPath, CONTENT_SETTING_ALLOW);
#endif
  prefs->SetInteger(kGeolocationPrefPath, CONTENT_SETTING_BLOCK);

  // Instantiate a new DefaultProvider; can't use |provider_| because we want to
  // test the constructor's behavior after setting the above.
  DefaultProvider provider(prefs, false);

  // Check that fullscreen and mouselock have been deleted.
  EXPECT_FALSE(prefs->HasPrefPath(kFullscreenPrefPath));
#if !defined(OS_ANDROID)
  EXPECT_FALSE(prefs->HasPrefPath(kMouselockPrefPath));
#endif
  EXPECT_TRUE(prefs->HasPrefPath(kGeolocationPrefPath));
  EXPECT_EQ(CONTENT_SETTING_BLOCK, prefs->GetInteger(kGeolocationPrefPath));
}

#if !defined(OS_ANDROID)
TEST_F(ContentSettingsDefaultProviderTest, DiscardObsoletePluginsAllow) {
  PrefService* prefs = profile_.GetPrefs();
  const std::string& plugins_pref_path = WebsiteSettingsRegistry::GetInstance()
                                             ->Get(ContentSettingsType::PLUGINS)
                                             ->default_value_pref_name();

  // The ALLOW value of the plugins content setting should be discarded.
  {
    prefs->SetInteger(plugins_pref_path, CONTENT_SETTING_ALLOW);
    DefaultProvider provider(prefs, false);
    EXPECT_FALSE(prefs->HasPrefPath(plugins_pref_path));
  }

  // Other values of the plugins content setting should be preserved.
  {
    prefs->SetInteger(plugins_pref_path, CONTENT_SETTING_BLOCK);
    DefaultProvider provider(prefs, false);
    EXPECT_TRUE(prefs->HasPrefPath(plugins_pref_path));
    EXPECT_EQ(CONTENT_SETTING_BLOCK, prefs->GetInteger(plugins_pref_path));
  }

  {
    prefs->SetInteger(plugins_pref_path,
                      CONTENT_SETTING_DETECT_IMPORTANT_CONTENT);
    DefaultProvider provider(prefs, false);

    EXPECT_TRUE(prefs->HasPrefPath(plugins_pref_path));
    EXPECT_EQ(CONTENT_SETTING_DETECT_IMPORTANT_CONTENT,
              prefs->GetInteger(plugins_pref_path));
  }
}
#endif  // !defined(OS_ANDROID)

TEST_F(ContentSettingsDefaultProviderTest, OffTheRecord) {
  DefaultProvider otr_provider(profile_.GetPrefs(), true /* incognito */);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            TestUtils::GetContentSetting(
                &provider_, GURL(), GURL(), ContentSettingsType::COOKIES,
                std::string(), false /* include_incognito */));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            TestUtils::GetContentSetting(
                &otr_provider, GURL(), GURL(), ContentSettingsType::COOKIES,
                std::string(), true /* include_incognito */));

  // Changing content settings on the main provider should also affect the
  // incognito map.
  provider_.SetWebsiteSetting(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, std::string(),
      std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            TestUtils::GetContentSetting(
                &provider_, GURL(), GURL(), ContentSettingsType::COOKIES,
                std::string(), false /* include_incognito */));

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            TestUtils::GetContentSetting(
                &otr_provider, GURL(), GURL(), ContentSettingsType::COOKIES,
                std::string(), true /* include_incognito */));

  // Changing content settings on the incognito provider should be ignored.
  std::unique_ptr<base::Value> value(new base::Value(CONTENT_SETTING_ALLOW));
  bool owned = otr_provider.SetWebsiteSetting(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, std::string(), std::move(value));
  EXPECT_TRUE(owned);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            TestUtils::GetContentSetting(
                &provider_, GURL(), GURL(), ContentSettingsType::COOKIES,
                std::string(), false /* include_incognito */));

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            TestUtils::GetContentSetting(
                &otr_provider, GURL(), GURL(), ContentSettingsType::COOKIES,
                std::string(), true /* include_incognito */));

  // Check that new OTR DefaultProviders also inherit the correct value.
  DefaultProvider otr_provider2(profile_.GetPrefs(), true /* incognito */);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            TestUtils::GetContentSetting(
                &otr_provider2, GURL(), GURL(), ContentSettingsType::COOKIES,
                std::string(), true /* include_incognito */));

  otr_provider.ShutdownOnUIThread();
  otr_provider2.ShutdownOnUIThread();
}

}  // namespace content_settings
