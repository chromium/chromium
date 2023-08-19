// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_policy_provider.h"

#include <memory>
#include <string>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/content_settings_mock_observer.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;

namespace content_settings {

typedef std::vector<Rule> Rules;

class PolicyProviderTest : public testing::Test {
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(PolicyProviderTest, DefaultGeolocationContentSetting) {
  TestingProfile profile;
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile.GetTestingPrefService();
  PolicyProvider provider(prefs);

  std::unique_ptr<RuleIterator> rule_iterator(
      provider.GetRuleIterator(ContentSettingsType::GEOLOCATION, false));
  EXPECT_FALSE(rule_iterator);

  // Change the managed value of the default geolocation setting
  prefs->SetManagedPref(prefs::kManagedDefaultGeolocationSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));

  rule_iterator =
      provider.GetRuleIterator(ContentSettingsType::GEOLOCATION, false);
  ASSERT_TRUE(rule_iterator);
  EXPECT_TRUE(rule_iterator->HasNext());
  std::unique_ptr<Rule> rule = rule_iterator->Next();
  EXPECT_FALSE(rule_iterator->HasNext());

  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule->primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule->secondary_pattern);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, ValueToContentSetting(rule->value()));

  provider.ShutdownOnUIThread();
}

TEST_F(PolicyProviderTest, ManagedDefaultContentSettings) {
  TestingProfile profile;
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile.GetTestingPrefService();
  PolicyProvider provider(prefs);

  prefs->SetManagedPref(prefs::kManagedDefaultCookiesSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));

  std::unique_ptr<RuleIterator> rule_iterator(
      provider.GetRuleIterator(ContentSettingsType::COOKIES, false));
  EXPECT_TRUE(rule_iterator->HasNext());
  std::unique_ptr<Rule> rule = rule_iterator->Next();
  EXPECT_FALSE(rule_iterator->HasNext());

  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule->primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule->secondary_pattern);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, ValueToContentSetting(rule->value()));

  provider.ShutdownOnUIThread();
}

// When a default-content-setting is set to a managed setting a
// CONTENT_SETTINGS_CHANGED notification should be fired. The same should happen
// if the managed setting is removed.
TEST_F(PolicyProviderTest, ObserveManagedSettingsChange) {
  TestingProfile profile;
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile.GetTestingPrefService();
  PolicyProvider provider(prefs);

  MockObserver mock_observer;
  EXPECT_CALL(mock_observer,
              OnContentSettingChanged(_, _, ContentSettingsType::DEFAULT));
  provider.AddObserver(&mock_observer);

  // Set the managed default-content-setting.
  prefs->SetManagedPref(prefs::kManagedDefaultCookiesSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer);
  EXPECT_CALL(mock_observer,
              OnContentSettingChanged(_, _, ContentSettingsType::DEFAULT));
  // Remove the managed default-content-setting.
  prefs->RemoveManagedPref(prefs::kManagedDefaultCookiesSetting);
  provider.ShutdownOnUIThread();
}

TEST_F(PolicyProviderTest, GettingManagedContentSettings) {
  TestingProfile profile;
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile.GetTestingPrefService();

  base::Value::List list;
  list.Append("[*.]google.com");
  prefs->SetManagedPref(prefs::kManagedImagesBlockedForUrls, std::move(list));

  PolicyProvider provider(prefs);

  ContentSettingsPattern yt_url_pattern =
      ContentSettingsPattern::FromString("www.youtube.com");
  GURL youtube_url("http://www.youtube.com");
  GURL google_url("http://mail.google.com");

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            TestUtils::GetContentSetting(&provider, youtube_url, youtube_url,
                                         ContentSettingsType::COOKIES, false));
  EXPECT_EQ(base::Value(), TestUtils::GetContentSettingValue(
                               &provider, youtube_url, youtube_url,
                               ContentSettingsType::COOKIES, false));

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            TestUtils::GetContentSetting(&provider, google_url, google_url,
                                         ContentSettingsType::IMAGES, false));
  base::Value value = TestUtils::GetContentSettingValue(
      &provider, google_url, google_url, ContentSettingsType::IMAGES, false);

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            IntToContentSetting(value.GetIfInt().value_or(-1)));

  // The PolicyProvider does not allow setting content settings as they are
  // enforced via policies and not set by the user or extension. So a call to
  // SetWebsiteSetting does nothing.
  bool owned = provider.SetWebsiteSetting(yt_url_pattern, yt_url_pattern,
                                          ContentSettingsType::COOKIES,
                                          base::Value(CONTENT_SETTING_BLOCK));
  EXPECT_FALSE(owned);
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            TestUtils::GetContentSetting(&provider, youtube_url, youtube_url,
                                         ContentSettingsType::COOKIES, false));

  provider.ShutdownOnUIThread();
}

TEST_F(PolicyProviderTest, AutoSelectCertificateList) {
  TestingProfile profile;
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile.GetTestingPrefService();

  PolicyProvider provider(prefs);
  GURL google_url("https://mail.google.com");
  // Tests the default setting for auto selecting certificates
  EXPECT_EQ(base::Value(),
            TestUtils::GetContentSettingValue(
                &provider, google_url, google_url,
                ContentSettingsType::AUTO_SELECT_CERTIFICATE, false));

  // Set the content settings pattern list for origins to auto select
  // certificates.
  std::string pattern_str("\"pattern\":\"[*.]google.com\"");
  std::string filter_str("\"filter\":{\"ISSUER\":{\"CN\":\"issuer name\"}}");
  base::Value::List list;
  list.Append("{" + pattern_str + "," + filter_str + "}");
  prefs->SetManagedPref(prefs::kManagedAutoSelectCertificateForUrls,
                        std::move(list));
  GURL youtube_url("https://www.youtube.com");
  EXPECT_EQ(base::Value(),
            TestUtils::GetContentSettingValue(
                &provider, youtube_url, youtube_url,
                ContentSettingsType::AUTO_SELECT_CERTIFICATE, false));
  base::Value cert_filter_setting = TestUtils::GetContentSettingValue(
      &provider, google_url, google_url,
      ContentSettingsType::AUTO_SELECT_CERTIFICATE, false);

  ASSERT_EQ(base::Value::Type::DICT, cert_filter_setting.type());
  base::Value::List* cert_filters =
      cert_filter_setting.GetDict().FindList("filters");
  ASSERT_TRUE(cert_filters);
  ASSERT_FALSE(cert_filters->empty());
  auto& filter = cert_filters->front();
  ASSERT_TRUE(filter.is_dict());
  const std::string* actual_common_name =
      filter.GetDict().FindStringByDottedPath("ISSUER.CN");
  ASSERT_TRUE(actual_common_name);
  EXPECT_EQ("issuer name", *actual_common_name);
  provider.ShutdownOnUIThread();
}

TEST_F(PolicyProviderTest, InvalidAutoSelectCertificateList) {
  TestingProfile profile;
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile.GetTestingPrefService();

  PolicyProvider provider(prefs);
  GURL google_url("https://mail.google.com");

  std::string pattern_str("\"pattern\":\"[*.]google.com\"");
  std::string filter_str("\"filter\":{\"ISSUER\":{\"CN\":\"issuer name\"}}");

  // Missing pattern should be rejected.
  base::Value::List missing_pattern_value;
  missing_pattern_value.Append("{" + filter_str + "}");
  prefs->SetManagedPref(prefs::kManagedAutoSelectCertificateForUrls,
                        std::move(missing_pattern_value));
  EXPECT_EQ(base::Value(),
            TestUtils::GetContentSettingValue(
                &provider, google_url, google_url,
                ContentSettingsType::AUTO_SELECT_CERTIFICATE, false));

  // Non-dict value should be rejected.
  base::Value::List no_dict_value;
  no_dict_value.Append(pattern_str + "," + filter_str);
  prefs->SetManagedPref(prefs::kManagedAutoSelectCertificateForUrls,
                        std::move(no_dict_value));
  EXPECT_EQ(base::Value(),
            TestUtils::GetContentSettingValue(
                &provider, google_url, google_url,
                ContentSettingsType::AUTO_SELECT_CERTIFICATE, false));

  // Missing filter should be rejected.
  base::Value::List missing_filter_value;
  missing_filter_value.Append("{" + pattern_str + "}");
  prefs->SetManagedPref(prefs::kManagedAutoSelectCertificateForUrls,
                        std::move(missing_filter_value));
  EXPECT_EQ(base::Value(),
            TestUtils::GetContentSettingValue(
                &provider, google_url, google_url,
                ContentSettingsType::AUTO_SELECT_CERTIFICATE, false));

  // Valid configuration should not be rejected.
  base::Value::List valid_value;
  valid_value.Append("{" + pattern_str + "," + filter_str + "}");
  prefs->SetManagedPref(prefs::kManagedAutoSelectCertificateForUrls,
                        std::move(valid_value));
  EXPECT_NE(base::Value(),
            TestUtils::GetContentSettingValue(
                &provider, google_url, google_url,
                ContentSettingsType::AUTO_SELECT_CERTIFICATE, false));

  provider.ShutdownOnUIThread();
}

TEST_F(PolicyProviderTest, InvalidManagedDefaultContentSetting) {
  TestingProfile profile;
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile.GetTestingPrefService();
  PolicyProvider provider(prefs);

  prefs->SetManagedPref(
      prefs::kManagedDefaultCookiesSetting,
      std::make_unique<base::Value>(CONTENT_SETTING_DETECT_IMPORTANT_CONTENT));

  // The setting provided in the cookies pref is not valid for cookies. It
  // should be ignored.
  std::unique_ptr<RuleIterator> rule_iterator(
      provider.GetRuleIterator(ContentSettingsType::COOKIES, false));
  EXPECT_FALSE(rule_iterator);

  provider.ShutdownOnUIThread();
}

}  // namespace content_settings
