// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_policy_provider.h"

#include <memory>
#include <string>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/content_settings_mock_observer.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile.h"
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

  std::unique_ptr<RuleIterator> rule_iterator(provider.GetRuleIterator(
      ContentSettingsType::GEOLOCATION, std::string(), false));
  EXPECT_FALSE(rule_iterator);

  // Change the managed value of the default geolocation setting
  prefs->SetManagedPref(prefs::kManagedDefaultGeolocationSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));

  rule_iterator = provider.GetRuleIterator(ContentSettingsType::GEOLOCATION,
                                           std::string(), false);
  ASSERT_TRUE(rule_iterator);
  EXPECT_TRUE(rule_iterator->HasNext());
  Rule rule = rule_iterator->Next();
  EXPECT_FALSE(rule_iterator->HasNext());

  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule.primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule.secondary_pattern);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, ValueToContentSetting(&rule.value));

  provider.ShutdownOnUIThread();
}

TEST_F(PolicyProviderTest, ManagedDefaultContentSettings) {
  TestingProfile profile;
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile.GetTestingPrefService();
  PolicyProvider provider(prefs);

  prefs->SetManagedPref(prefs::kManagedDefaultCookiesSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));

  std::unique_ptr<RuleIterator> rule_iterator(provider.GetRuleIterator(
      ContentSettingsType::COOKIES, std::string(), false));
  EXPECT_TRUE(rule_iterator->HasNext());
  Rule rule = rule_iterator->Next();
  EXPECT_FALSE(rule_iterator->HasNext());

  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule.primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule.secondary_pattern);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, ValueToContentSetting(&rule.value));

  provider.ShutdownOnUIThread();
}

TEST_F(PolicyProviderTest, ManagedDefaultPluginSettingsExperiment) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine("IgnoreDefaultPluginsSetting",
                                          std::string());

  TestingProfile profile;
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile.GetTestingPrefService();
  PolicyProvider provider(prefs);

  // ForceDefaultPluginsSettingAsk overrides this to ASK.
  prefs->SetManagedPref(prefs::kManagedDefaultPluginsSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  prefs->SetManagedPref(prefs::kManagedDefaultJavaScriptSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));

  std::unique_ptr<RuleIterator> plugin_rule_iterator(provider.GetRuleIterator(
      ContentSettingsType::PLUGINS, std::string(), false));
  // Policy should be removed when running under experiment.
  EXPECT_FALSE(plugin_rule_iterator);

  std::unique_ptr<RuleIterator> js_rule_iterator(provider.GetRuleIterator(
      ContentSettingsType::JAVASCRIPT, std::string(), false));
  // Other policies should be left alone.
  EXPECT_TRUE(js_rule_iterator->HasNext());
  Rule rule = js_rule_iterator->Next();
  EXPECT_FALSE(js_rule_iterator->HasNext());

  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule.primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule.secondary_pattern);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, ValueToContentSetting(&rule.value));

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
              OnContentSettingChanged(_, _, ContentSettingsType::DEFAULT, ""));
  provider.AddObserver(&mock_observer);

  // Set the managed default-content-setting.
  prefs->SetManagedPref(prefs::kManagedDefaultCookiesSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer);
  EXPECT_CALL(mock_observer,
              OnContentSettingChanged(_, _, ContentSettingsType::DEFAULT, ""));
  // Remove the managed default-content-setting.
  prefs->RemoveManagedPref(prefs::kManagedDefaultCookiesSetting);
  provider.ShutdownOnUIThread();
}

TEST_F(PolicyProviderTest, GettingManagedContentSettings) {
  TestingProfile profile;
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile.GetTestingPrefService();

  auto value = std::make_unique<base::ListValue>();
  value->AppendString("[*.]google.com");
  prefs->SetManagedPref(prefs::kManagedImagesBlockedForUrls, std::move(value));

  PolicyProvider provider(prefs);

  ContentSettingsPattern yt_url_pattern =
      ContentSettingsPattern::FromString("www.youtube.com");
  GURL youtube_url("http://www.youtube.com");
  GURL google_url("http://mail.google.com");

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            TestUtils::GetContentSetting(&provider, youtube_url, youtube_url,
                                         ContentSettingsType::COOKIES,
                                         std::string(), false));
  EXPECT_EQ(NULL, TestUtils::GetContentSettingValue(
                      &provider, youtube_url, youtube_url,
                      ContentSettingsType::COOKIES, std::string(), false));

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            TestUtils::GetContentSetting(&provider, google_url, google_url,
                                         ContentSettingsType::IMAGES,
                                         std::string(), false));
  std::unique_ptr<base::Value> value_ptr(TestUtils::GetContentSettingValue(
      &provider, google_url, google_url, ContentSettingsType::IMAGES,
      std::string(), false));

  int int_value = -1;
  value_ptr->GetAsInteger(&int_value);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, IntToContentSetting(int_value));

  // The PolicyProvider does not allow setting content settings as they are
  // enforced via policies and not set by the user or extension. So a call to
  // SetWebsiteSetting does nothing.
  std::unique_ptr<base::Value> value_block(
      new base::Value(CONTENT_SETTING_BLOCK));
  bool owned = provider.SetWebsiteSetting(
      yt_url_pattern, yt_url_pattern, ContentSettingsType::COOKIES,
      std::string(), std::move(value_block));
  EXPECT_FALSE(owned);
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            TestUtils::GetContentSetting(&provider, youtube_url, youtube_url,
                                         ContentSettingsType::COOKIES,
                                         std::string(), false));

  provider.ShutdownOnUIThread();
}

TEST_F(PolicyProviderTest, ResourceIdentifier) {
  TestingProfile profile;
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile.GetTestingPrefService();

  auto value = std::make_unique<base::ListValue>();
  value->AppendString("[*.]google.com");
  prefs->SetManagedPref(prefs::kManagedPluginsAllowedForUrls, std::move(value));

  PolicyProvider provider(prefs);

  GURL youtube_url("http://www.youtube.com");
  GURL google_url("http://mail.google.com");

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            TestUtils::GetContentSetting(&provider, youtube_url, youtube_url,
                                         ContentSettingsType::PLUGINS,
                                         "someplugin", false));

  // There is currently no policy support for resource content settings.
  // Resource identifiers are simply ignored by the PolicyProvider.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            TestUtils::GetContentSetting(&provider, google_url, google_url,
                                         ContentSettingsType::PLUGINS,
                                         std::string(), false));

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            TestUtils::GetContentSetting(&provider, google_url, google_url,
                                         ContentSettingsType::PLUGINS,
                                         "someplugin", false));

  provider.ShutdownOnUIThread();
}

TEST_F(PolicyProviderTest, AutoSelectCertificateList) {
  TestingProfile profile;
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile.GetTestingPrefService();

  PolicyProvider provider(prefs);
  GURL google_url("https://mail.google.com");
  // Tests the default setting for auto selecting certificates
  EXPECT_EQ(NULL, TestUtils::GetContentSettingValue(
                      &provider, google_url, google_url,
                      ContentSettingsType::AUTO_SELECT_CERTIFICATE,
                      std::string(), false));

  // Set the content settings pattern list for origins to auto select
  // certificates.
  std::string pattern_str("\"pattern\":\"[*.]google.com\"");
  std::string filter_str("\"filter\":{\"ISSUER\":{\"CN\":\"issuer name\"}}");
  auto value = std::make_unique<base::ListValue>();
  value->AppendString("{" + pattern_str + "," + filter_str + "}");
  prefs->SetManagedPref(prefs::kManagedAutoSelectCertificateForUrls,
                        std::move(value));
  GURL youtube_url("https://www.youtube.com");
  EXPECT_EQ(NULL, TestUtils::GetContentSettingValue(
                      &provider, youtube_url, youtube_url,
                      ContentSettingsType::AUTO_SELECT_CERTIFICATE,
                      std::string(), false));
  std::unique_ptr<base::Value> cert_filter_setting(
      TestUtils::GetContentSettingValue(
          &provider, google_url, google_url,
          ContentSettingsType::AUTO_SELECT_CERTIFICATE, std::string(), false));

  ASSERT_EQ(base::Value::Type::DICTIONARY, cert_filter_setting->type());
  base::Value* cert_filters =
      cert_filter_setting->FindKeyOfType("filters", base::Value::Type::LIST);
  ASSERT_TRUE(cert_filters);
  ASSERT_FALSE(cert_filters->GetList().empty());
  base::DictionaryValue* filter;
  ASSERT_TRUE(cert_filters->GetList().front().GetAsDictionary(&filter));
  std::string actual_common_name;
  ASSERT_TRUE(filter->GetString("ISSUER.CN", &actual_common_name));
  EXPECT_EQ("issuer name", actual_common_name);
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
  std::unique_ptr<RuleIterator> rule_iterator(provider.GetRuleIterator(
      ContentSettingsType::COOKIES, std::string(), false));
  EXPECT_FALSE(rule_iterator);

  provider.ShutdownOnUIThread();
}

}  // namespace content_settings
