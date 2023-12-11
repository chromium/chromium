// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/content_settings_default_provider.h"
#include "components/content_settings/core/browser/content_settings_mock_observer.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
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
      : provider_(profile_.GetPrefs(), false, false) {}
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
                                         ContentSettingsType::COOKIES, false));
  provider_.SetWebsiteSetting(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, base::Value(CONTENT_SETTING_BLOCK),
      /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            TestUtils::GetContentSetting(&provider_, GURL(), GURL(),
                                         ContentSettingsType::COOKIES, false));

  EXPECT_EQ(CONTENT_SETTING_ASK, TestUtils::GetContentSetting(
                                     &provider_, GURL(), GURL(),
                                     ContentSettingsType::GEOLOCATION, false));
  provider_.SetWebsiteSetting(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::GEOLOCATION, base::Value(CONTENT_SETTING_BLOCK),
      /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      TestUtils::GetContentSetting(&provider_, GURL(), GURL(),
                                   ContentSettingsType::GEOLOCATION, false));

  base::Value value = TestUtils::GetContentSettingValue(
      &provider_, GURL("http://example.com/"), GURL("http://example.com/"),
      ContentSettingsType::AUTO_SELECT_CERTIFICATE, false);
  EXPECT_TRUE(value.is_none());
}

TEST_F(ContentSettingsDefaultProviderTest, IgnoreNonDefaultSettings) {
  GURL primary_url("http://www.google.com");
  GURL secondary_url("http://www.google.com");

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            TestUtils::GetContentSetting(&provider_, primary_url, secondary_url,
                                         ContentSettingsType::COOKIES, false));
  bool owned = provider_.SetWebsiteSetting(
      ContentSettingsPattern::FromURL(primary_url),
      ContentSettingsPattern::FromURL(secondary_url),
      ContentSettingsType::COOKIES, base::Value(CONTENT_SETTING_BLOCK),
      /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_FALSE(owned);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            TestUtils::GetContentSetting(&provider_, primary_url, secondary_url,
                                         ContentSettingsType::COOKIES, false));
}

TEST_F(ContentSettingsDefaultProviderTest, Observer) {
  MockObserver mock_observer;
  EXPECT_CALL(mock_observer,
              OnContentSettingChanged(_, _, ContentSettingsType::COOKIES));
  provider_.AddObserver(&mock_observer);
  provider_.SetWebsiteSetting(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, base::Value(CONTENT_SETTING_BLOCK),
      /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());

  EXPECT_CALL(mock_observer,
              OnContentSettingChanged(_, _, ContentSettingsType::GEOLOCATION));
  provider_.SetWebsiteSetting(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::GEOLOCATION, base::Value(CONTENT_SETTING_BLOCK),
      /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
}

TEST_F(ContentSettingsDefaultProviderTest, ObservePref) {
  PrefService* prefs = profile_.GetPrefs();

  provider_.SetWebsiteSetting(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, base::Value(CONTENT_SETTING_BLOCK),
      /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            TestUtils::GetContentSetting(&provider_, GURL(), GURL(),
                                         ContentSettingsType::COOKIES, false));
  const WebsiteSettingsInfo* info =
      WebsiteSettingsRegistry::GetInstance()->Get(ContentSettingsType::COOKIES);
  // Clearing the backing pref should also clear the internal cache.
  prefs->ClearPref(info->default_value_pref_name());
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            TestUtils::GetContentSetting(&provider_, GURL(), GURL(),
                                         ContentSettingsType::COOKIES, false));
  // Resetting the pref to its previous value should update the cache.
  prefs->SetInteger(info->default_value_pref_name(), CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            TestUtils::GetContentSetting(&provider_, GURL(), GURL(),
                                         ContentSettingsType::COOKIES, false));
}

// Tests that fullscreen, obsolete NFC (with the old semantics, see
// crbug.com/1275576), and obsolete content settings (plugins, mouselock,
// installed web app metadata) are cleared.
TEST_F(ContentSettingsDefaultProviderTest, DiscardObsoletePreferences) {
  static const char kNfcPrefPath[] =
      "profile.default_content_setting_values.nfc";
#if !BUILDFLAG(IS_ANDROID)
  static const char kMouselockPrefPath[] =
      "profile.default_content_setting_values.mouselock";
  const char kObsoletePluginsDefaultPref[] =
      "profile.default_content_setting_values.plugins";
  const char kObsoletePluginsDataDefaultPref[] =
      "profile.default_content_setting_values.flash_data";
  const char kObsoleteFileHandlingDefaultPref[] =
      "profile.default_content_setting_values.file_handling";
  const char kObsoleteInstalledWebAppMetadataDefaultPref[] =
      "profile.default_content_setting_values.installed_web_app_metadata";
#endif
  static const char kGeolocationPrefPath[] =
      "profile.default_content_setting_values.geolocation";

  PrefService* prefs = profile_.GetPrefs();
  // Set some pref data.
#if !BUILDFLAG(IS_ANDROID)
  prefs->SetInteger(kMouselockPrefPath, CONTENT_SETTING_ALLOW);
  prefs->SetInteger(kObsoletePluginsDefaultPref, CONTENT_SETTING_ALLOW);
  prefs->SetInteger(kObsoletePluginsDataDefaultPref, CONTENT_SETTING_ALLOW);
  prefs->SetInteger(kObsoleteFileHandlingDefaultPref, CONTENT_SETTING_ALLOW);
  prefs->SetInteger(kObsoleteInstalledWebAppMetadataDefaultPref,
                    CONTENT_SETTING_ALLOW);
#endif
  prefs->SetInteger(kGeolocationPrefPath, CONTENT_SETTING_BLOCK);

  // Instantiate a new DefaultProvider; can't use |provider_| because we want to
  // test the constructor's behavior after setting the above.
  DefaultProvider provider(prefs, false, false);

  // Check that obsolete prefs have been deleted.
  EXPECT_FALSE(prefs->HasPrefPath(kNfcPrefPath));
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(prefs->HasPrefPath(kMouselockPrefPath));
  EXPECT_FALSE(prefs->HasPrefPath(kObsoletePluginsDefaultPref));
  EXPECT_FALSE(prefs->HasPrefPath(kObsoletePluginsDataDefaultPref));
  EXPECT_FALSE(prefs->HasPrefPath(kObsoleteFileHandlingDefaultPref));
  EXPECT_FALSE(prefs->HasPrefPath(kObsoleteInstalledWebAppMetadataDefaultPref));
#endif
  // Check that non-obsolete prefs have not been touched.
  EXPECT_TRUE(prefs->HasPrefPath(kGeolocationPrefPath));
  EXPECT_EQ(CONTENT_SETTING_BLOCK, prefs->GetInteger(kGeolocationPrefPath));
}

TEST_F(ContentSettingsDefaultProviderTest, OffTheRecord) {
  DefaultProvider otr_provider(profile_.GetPrefs(), true /* incognito */,
                               false /* should_record_metrics */);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            TestUtils::GetContentSetting(&provider_, GURL(), GURL(),
                                         ContentSettingsType::COOKIES,
                                         false /* include_incognito */));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            TestUtils::GetContentSetting(&otr_provider, GURL(), GURL(),
                                         ContentSettingsType::COOKIES,
                                         true /* include_incognito */));

  // Changing content settings on the main provider should also affect the
  // incognito map.
  provider_.SetWebsiteSetting(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, base::Value(CONTENT_SETTING_BLOCK),
      /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            TestUtils::GetContentSetting(&provider_, GURL(), GURL(),
                                         ContentSettingsType::COOKIES,
                                         false /* include_incognito */));

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            TestUtils::GetContentSetting(&otr_provider, GURL(), GURL(),
                                         ContentSettingsType::COOKIES,
                                         true /* include_incognito */));

  // Changing content settings on the incognito provider should be ignored.
  bool owned = otr_provider.SetWebsiteSetting(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, base::Value(CONTENT_SETTING_ALLOW),
      /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_TRUE(owned);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            TestUtils::GetContentSetting(&provider_, GURL(), GURL(),
                                         ContentSettingsType::COOKIES,
                                         false /* include_incognito */));

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            TestUtils::GetContentSetting(&otr_provider, GURL(), GURL(),
                                         ContentSettingsType::COOKIES,
                                         true /* include_incognito */));

  // Check that new OTR DefaultProviders also inherit the correct value.
  DefaultProvider otr_provider2(profile_.GetPrefs(), true /* incognito */,
                                false /* should_record_metrics */);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            TestUtils::GetContentSetting(&otr_provider2, GURL(), GURL(),
                                         ContentSettingsType::COOKIES,
                                         true /* include_incognito */));

  otr_provider.ShutdownOnUIThread();
  otr_provider2.ShutdownOnUIThread();
}

}  // namespace content_settings
