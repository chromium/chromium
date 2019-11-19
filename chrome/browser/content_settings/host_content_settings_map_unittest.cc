// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/content_settings_mock_observer.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/mock_settings_observer.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/content_settings_details.h"
#include "components/content_settings/core/browser/content_settings_ephemeral_provider.h"
#include "components/content_settings/core/browser/content_settings_pref_provider.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/user_modifiable_provider.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/static_cookie_policy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::MockFunction;
using ::testing::Return;

namespace {

bool MatchPrimaryPattern(const ContentSettingsPattern& expected_primary,
                         const ContentSettingsPattern& primary_pattern,
                         const ContentSettingsPattern& secondary_pattern) {
  return expected_primary == primary_pattern;
}

}  // namespace

class MockUserModifiableProvider
    : public content_settings::UserModifiableProvider {
 public:
  ~MockUserModifiableProvider() override = default;
  MOCK_CONST_METHOD3(GetRuleIterator,
                     std::unique_ptr<content_settings::RuleIterator>(
                         ContentSettingsType,
                         const content_settings::ResourceIdentifier&,
                         bool));

  MOCK_METHOD5(SetWebsiteSetting,
               bool(const ContentSettingsPattern&,
                    const ContentSettingsPattern&,
                    ContentSettingsType,
                    const content_settings::ResourceIdentifier&,
                    std::unique_ptr<base::Value>&&));

  MOCK_METHOD1(ClearAllContentSettingsRules, void(ContentSettingsType));

  MOCK_METHOD0(ShutdownOnUIThread, void());

  MOCK_METHOD4(GetWebsiteSettingLastModified,
               base::Time(const ContentSettingsPattern&,
                          const ContentSettingsPattern&,
                          ContentSettingsType,
                          const content_settings::ResourceIdentifier&));
};

class HostContentSettingsMapTest : public testing::Test {
 public:
  HostContentSettingsMapTest() = default;

 protected:
  const std::string& GetPrefName(ContentSettingsType type) {
    return content_settings::WebsiteSettingsRegistry::GetInstance()
        ->Get(type)
        ->pref_name();
  }

  content::BrowserTaskEnvironment task_environment_;
};

// Wrapper to TestingProfile to reduce test boilerplates, by keeping a fixed
// |content_type| so caller only need to specify it once.
class TesterForType {
 public:
  TesterForType(TestingProfile *profile, ContentSettingsType content_type)
      : prefs_(profile->GetTestingPrefService()),
        host_content_settings_map_(
            HostContentSettingsMapFactory::GetForProfile(profile)),
        content_type_(content_type) {
    switch (content_type_) {
      case ContentSettingsType::COOKIES:
        policy_default_setting_ = prefs::kManagedDefaultCookiesSetting;
        break;
      case ContentSettingsType::POPUPS:
        policy_default_setting_ = prefs::kManagedDefaultPopupsSetting;
        break;
      case ContentSettingsType::ADS:
        policy_default_setting_ = prefs::kManagedDefaultAdsSetting;
        break;
      default:
        // Add support as needed.
        NOTREACHED();
    }
  }

  void ClearPolicyDefault() {
    prefs_->RemoveManagedPref(policy_default_setting_);
  }

  void SetPolicyDefault(ContentSetting setting) {
    prefs_->SetManagedPref(policy_default_setting_,
                           std::make_unique<base::Value>(setting));
  }

  void AddUserException(std::string exception,
                        ContentSetting content_settings) {
    ContentSettingsPattern pattern =
        ContentSettingsPattern::FromString(exception);
    host_content_settings_map_->SetContentSettingCustomScope(
        pattern, pattern, content_type_, std::string(), content_settings);
  }

  // Wrapper to query GetWebsiteSetting(), and only return the source.
  content_settings::SettingSource GetSettingSourceForURL(
      const std::string& url_str) {
    GURL url(url_str);
    content_settings::SettingInfo setting_info;
    std::unique_ptr<base::Value> result =
        host_content_settings_map_->GetWebsiteSetting(
            url, url, content_type_, std::string(), &setting_info);
    return setting_info.source;
  }

 private:
  sync_preferences::TestingPrefServiceSyncable* prefs_;
  HostContentSettingsMap* host_content_settings_map_;
  ContentSettingsType content_type_;
  const char* policy_default_setting_;

  DISALLOW_COPY_AND_ASSIGN(TesterForType);
};

TEST_F(HostContentSettingsMapTest, DefaultValues) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  // Check setting defaults.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::JAVASCRIPT, NULL));
  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::JAVASCRIPT, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::JAVASCRIPT, NULL));
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      host_content_settings_map->GetContentSetting(
          GURL(chrome::kChromeUINewTabURL), GURL(chrome::kChromeUINewTabURL),
          ContentSettingsType::JAVASCRIPT, std::string()));

#if BUILDFLAG(ENABLE_PLUGINS)
  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::PLUGINS, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::PLUGINS, NULL));
  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::PLUGINS, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::PLUGINS, NULL));
  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::PLUGINS, CONTENT_SETTING_DETECT_IMPORTANT_CONTENT);
  EXPECT_EQ(CONTENT_SETTING_DETECT_IMPORTANT_CONTENT,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::PLUGINS, NULL));
#endif

  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::POPUPS, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::POPUPS, NULL));
}

TEST_F(HostContentSettingsMapTest, IndividualSettings) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  // Check returning individual settings.
  GURL host("http://example.com/");
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES, std::string()));
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::COOKIES, std::string(),
      CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES, std::string()));
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::COOKIES, std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES, std::string()));
#if BUILDFLAG(ENABLE_PLUGINS)
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::PLUGINS, std::string()));
#endif

  // Check returning all settings for a host.
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::JAVASCRIPT, std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::JAVASCRIPT, std::string()));
#if BUILDFLAG(ENABLE_PLUGINS)
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::PLUGINS, std::string(),
      CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::PLUGINS, std::string()));
#endif
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::POPUPS, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::GEOLOCATION, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::NOTIFICATIONS, std::string()));

  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::AUTOPLAY, std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::AUTOPLAY, std::string()));

  // Check returning all hosts for a setting.
  GURL host2("http://example.org/");
  host_content_settings_map->SetContentSettingDefaultScope(
      host2, GURL(), ContentSettingsType::JAVASCRIPT, std::string(),
      CONTENT_SETTING_BLOCK);
#if BUILDFLAG(ENABLE_PLUGINS)
  host_content_settings_map->SetContentSettingDefaultScope(
      host2, GURL(), ContentSettingsType::PLUGINS, std::string(),
      CONTENT_SETTING_BLOCK);
#endif
  ContentSettingsForOneType host_settings;
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::JAVASCRIPT, std::string(), &host_settings);
  // |host_settings| contains the default setting and 2 exception.
  EXPECT_EQ(3U, host_settings.size());
#if BUILDFLAG(ENABLE_PLUGINS)
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::PLUGINS, std::string(), &host_settings);
  // |host_settings| contains the default setting and 2 exceptions.
  EXPECT_EQ(3U, host_settings.size());
#endif
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::POPUPS, std::string(), &host_settings);
  // |host_settings| contains only the default setting.
  EXPECT_EQ(1U, host_settings.size());
}

TEST_F(HostContentSettingsMapTest, GetWebsiteSettingsForOneType) {
  TestingProfile profile;
  GURL hosts[] = {GURL("https://example1.com/"), GURL("https://example2.com/")};
  ContentSettingsForOneType client_hints_settings;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::CLIENT_HINTS, std::string(), &client_hints_settings);
  EXPECT_EQ(0U, client_hints_settings.size());

  // Add setting for hosts[0].
  const double expiration_time =
      (base::Time::Now() + base::TimeDelta::FromDays(1)).ToDoubleT();
  std::unique_ptr<base::ListValue> expiration_times_list =
      std::make_unique<base::ListValue>();
  expiration_times_list->AppendInteger(42 /* client hint  value */);
  auto expiration_times_dictionary = std::make_unique<base::DictionaryValue>();
  expiration_times_dictionary->SetList("client_hints",
                                       std::move(expiration_times_list));
  expiration_times_dictionary->SetDouble("expiration_time", expiration_time);
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      hosts[0], GURL(), ContentSettingsType::CLIENT_HINTS, std::string(),
      std::make_unique<base::Value>(expiration_times_dictionary->Clone()));

  // Reading the settings should now return one setting.
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::CLIENT_HINTS, std::string(), &client_hints_settings);
  EXPECT_EQ(1U, client_hints_settings.size());
  for (size_t i = 0; i < client_hints_settings.size(); ++i) {
    EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(hosts[i]),
              client_hints_settings.at(i).primary_pattern);
    EXPECT_EQ(ContentSettingsPattern::Wildcard(),
              client_hints_settings.at(i).secondary_pattern);
    EXPECT_EQ(*expiration_times_dictionary,
              client_hints_settings.at(i).setting_value);
  }

  // Add setting for hosts[1].
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      hosts[1], GURL(), ContentSettingsType::CLIENT_HINTS, std::string(),
      std::make_unique<base::Value>(expiration_times_dictionary->Clone()));

  // Reading the settings should now return two settings.
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::CLIENT_HINTS, std::string(), &client_hints_settings);
  EXPECT_EQ(2U, client_hints_settings.size());
  for (size_t i = 0; i < client_hints_settings.size(); ++i) {
    EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(hosts[i]),
              client_hints_settings.at(i).primary_pattern);
    EXPECT_EQ(ContentSettingsPattern::Wildcard(),
              client_hints_settings.at(i).secondary_pattern);
    EXPECT_EQ(*expiration_times_dictionary,
              client_hints_settings.at(i).setting_value);
  }

  // Add settings again for hosts[0].
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      hosts[0], GURL(), ContentSettingsType::CLIENT_HINTS, std::string(),
      std::make_unique<base::Value>(expiration_times_dictionary->Clone()));

  // Reading the settings should still return two settings.
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::CLIENT_HINTS, std::string(), &client_hints_settings);
  EXPECT_EQ(2U, client_hints_settings.size());
  for (size_t i = 0; i < client_hints_settings.size(); ++i) {
    EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(hosts[i]),
              client_hints_settings.at(i).primary_pattern);
    EXPECT_EQ(ContentSettingsPattern::Wildcard(),
              client_hints_settings.at(i).secondary_pattern);
    EXPECT_EQ(*expiration_times_dictionary,
              client_hints_settings.at(i).setting_value);
  }
}

TEST_F(HostContentSettingsMapTest, Clear) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  // Check clearing one type.
  GURL host("http://example.org/");
  GURL host2("http://example.net/");
  host_content_settings_map->SetContentSettingDefaultScope(
      host2, GURL(), ContentSettingsType::COOKIES, std::string(),
      CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::COOKIES, std::string(),
      CONTENT_SETTING_BLOCK);
#if BUILDFLAG(ENABLE_PLUGINS)
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::PLUGINS, std::string(),
      CONTENT_SETTING_BLOCK);
#endif
  host_content_settings_map->SetContentSettingDefaultScope(
      host2, GURL(), ContentSettingsType::COOKIES, std::string(),
      CONTENT_SETTING_BLOCK);
  host_content_settings_map->ClearSettingsForOneType(
      ContentSettingsType::COOKIES);
  ContentSettingsForOneType host_settings;
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::COOKIES, std::string(), &host_settings);
  // |host_settings| contains only the default setting.
  EXPECT_EQ(1U, host_settings.size());
#if BUILDFLAG(ENABLE_PLUGINS)
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::PLUGINS, std::string(), &host_settings);
  // |host_settings| contains the default setting and an exception.
  EXPECT_EQ(2U, host_settings.size());
#endif
}

TEST_F(HostContentSettingsMapTest, Patterns) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  GURL host1("http://example.com/");
  GURL host2("http://www.example.com/");
  GURL host3("http://example.org/");
  ContentSettingsPattern pattern1 =
      ContentSettingsPattern::FromString("[*.]example.com");
  ContentSettingsPattern pattern2 =
      ContentSettingsPattern::FromString("example.org");
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host1, host1, ContentSettingsType::COOKIES, std::string()));
  host_content_settings_map->SetContentSettingCustomScope(
      pattern1, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, std::string(), CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host1, host1, ContentSettingsType::COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host2, host2, ContentSettingsType::COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host3, host3, ContentSettingsType::COOKIES, std::string()));
  host_content_settings_map->SetContentSettingCustomScope(
      pattern2, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, std::string(), CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host3, host3, ContentSettingsType::COOKIES, std::string()));
}

// Changing a setting for one origin doesn't affect subdomains.
TEST_F(HostContentSettingsMapTest, Origins) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  GURL host1("http://example.com/");
  GURL host2("http://www.example.com/");
  GURL host3("http://example.org/");
  GURL host4("http://example.com:8080");
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromURLNoWildcard(host1);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host1, host1, ContentSettingsType::COOKIES, std::string()));
  host_content_settings_map->SetContentSettingCustomScope(
      pattern, ContentSettingsPattern::Wildcard(), ContentSettingsType::COOKIES,
      std::string(), CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host1, host1, ContentSettingsType::COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host2, host2, ContentSettingsType::COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host3, host3, ContentSettingsType::COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host4, host4, ContentSettingsType::COOKIES, std::string()));
}

TEST_F(HostContentSettingsMapTest, Observer) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  MockSettingsObserver observer(host_content_settings_map);

  GURL host("http://example.com/");
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString("[*.]example.com");
  ContentSettingsPattern secondary_pattern =
      ContentSettingsPattern::Wildcard();
  EXPECT_CALL(observer, OnContentSettingsChanged(host_content_settings_map,
                                                 ContentSettingsType::COOKIES,
                                                 false, primary_pattern,
                                                 secondary_pattern, false));
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::COOKIES, std::string(),
      CONTENT_SETTING_ALLOW);
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnContentSettingsChanged(host_content_settings_map,
                                                 ContentSettingsType::COOKIES,
                                                 false, _, _, true));
  host_content_settings_map->ClearSettingsForOneType(
      ContentSettingsType::COOKIES);
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnContentSettingsChanged(host_content_settings_map,
                                                 ContentSettingsType::COOKIES,
                                                 false, _, _, true));
  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::COOKIES, CONTENT_SETTING_BLOCK);
}

TEST_F(HostContentSettingsMapTest, ObserveDefaultPref) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  PrefService* prefs = profile.GetPrefs();
  GURL host("http://example.com");

  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::COOKIES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES, std::string()));

  const content_settings::WebsiteSettingsInfo* info =
      content_settings::WebsiteSettingsRegistry::GetInstance()->Get(
          ContentSettingsType::COOKIES);
  // Clearing the backing pref should also clear the internal cache.
  prefs->ClearPref(info->default_value_pref_name());
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES, std::string()));

  // Reseting the pref to its previous value should update the cache.
  prefs->SetInteger(info->default_value_pref_name(), CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES, std::string()));
}

TEST_F(HostContentSettingsMapTest, ObserveExceptionPref) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  PrefService* prefs = profile.GetPrefs();

  // Make a copy of the default pref value so we can reset it later.
  std::unique_ptr<base::Value> default_value(
      prefs->FindPreference(GetPrefName(ContentSettingsType::COOKIES))
          ->GetValue()
          ->DeepCopy());

  GURL host("http://example.com");

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES, std::string()));

  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::COOKIES, std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES, std::string()));

  // Make a copy of the pref's new value so we can reset it later.
  std::unique_ptr<base::Value> new_value(
      prefs->FindPreference(GetPrefName(ContentSettingsType::COOKIES))
          ->GetValue()
          ->DeepCopy());

  // Clearing the backing pref should also clear the internal cache.
  prefs->Set(GetPrefName(ContentSettingsType::COOKIES), *default_value);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES, std::string()));

  // Reseting the pref to its previous value should update the cache.
  prefs->Set(GetPrefName(ContentSettingsType::COOKIES), *new_value);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES, std::string()));
}

TEST_F(HostContentSettingsMapTest, HostTrimEndingDotCheck) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  content_settings::CookieSettings* cookie_settings =
      CookieSettingsFactory::GetForProfile(&profile).get();

  GURL host_ending_with_dot("http://example.com./");

  EXPECT_TRUE(cookie_settings->IsCookieAccessAllowed(host_ending_with_dot,
                                                     host_ending_with_dot));
  host_content_settings_map->SetContentSettingDefaultScope(
      host_ending_with_dot, GURL(), ContentSettingsType::COOKIES, std::string(),
      CONTENT_SETTING_DEFAULT);
  EXPECT_TRUE(cookie_settings->IsCookieAccessAllowed(host_ending_with_dot,
                                                     host_ending_with_dot));
  host_content_settings_map->SetContentSettingDefaultScope(
      host_ending_with_dot, GURL(), ContentSettingsType::COOKIES, std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(cookie_settings->IsCookieAccessAllowed(host_ending_with_dot,
                                                      host_ending_with_dot));

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, host_ending_with_dot,
                ContentSettingsType::JAVASCRIPT, std::string()));
  host_content_settings_map->SetContentSettingDefaultScope(
      host_ending_with_dot, GURL(), ContentSettingsType::JAVASCRIPT,
      std::string(), CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, host_ending_with_dot,
                ContentSettingsType::JAVASCRIPT, std::string()));
  host_content_settings_map->SetContentSettingDefaultScope(
      host_ending_with_dot, GURL(), ContentSettingsType::JAVASCRIPT,
      std::string(), CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, host_ending_with_dot,
                ContentSettingsType::JAVASCRIPT, std::string()));

#if BUILDFLAG(ENABLE_PLUGINS)
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, host_ending_with_dot,
                ContentSettingsType::PLUGINS, std::string()));
  host_content_settings_map->SetContentSettingDefaultScope(
      host_ending_with_dot, GURL(), ContentSettingsType::PLUGINS, std::string(),
      CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, host_ending_with_dot,
                ContentSettingsType::PLUGINS, std::string()));
#endif

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, host_ending_with_dot,
                ContentSettingsType::POPUPS, std::string()));
  host_content_settings_map->SetContentSettingDefaultScope(
      host_ending_with_dot, GURL(), ContentSettingsType::POPUPS, std::string(),
      CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, host_ending_with_dot,
                ContentSettingsType::POPUPS, std::string()));
  host_content_settings_map->SetContentSettingDefaultScope(
      host_ending_with_dot, GURL(), ContentSettingsType::POPUPS, std::string(),
      CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, host_ending_with_dot,
                ContentSettingsType::POPUPS, std::string()));

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, host_ending_with_dot,
                ContentSettingsType::AUTOPLAY, std::string()));
  host_content_settings_map->SetContentSettingDefaultScope(
      host_ending_with_dot, GURL(), ContentSettingsType::AUTOPLAY,
      std::string(), CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, host_ending_with_dot,
                ContentSettingsType::AUTOPLAY, std::string()));
  host_content_settings_map->SetContentSettingDefaultScope(
      host_ending_with_dot, GURL(), ContentSettingsType::AUTOPLAY,
      std::string(), CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, host_ending_with_dot,
                ContentSettingsType::AUTOPLAY, std::string()));
}

TEST_F(HostContentSettingsMapTest, NestedSettings) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  GURL host1("http://example.com/");
  GURL host2("http://b.example.com/");
  GURL host3("http://a.b.example.com/");
  GURL host4("http://a.example.com/");
  GURL host5("http://b.b.example.com/");
  ContentSettingsPattern pattern1 =
      ContentSettingsPattern::FromString("[*.]example.com");
  ContentSettingsPattern pattern2 =
      ContentSettingsPattern::FromString("[*.]b.example.com");
  ContentSettingsPattern pattern3 =
      ContentSettingsPattern::FromString("a.b.example.com");

  // Test nested patterns for one type.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::COOKIES, nullptr));
  host_content_settings_map->SetContentSettingCustomScope(
      pattern1, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, std::string(), CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetContentSettingCustomScope(
      pattern2, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, std::string(), CONTENT_SETTING_ALLOW);
  host_content_settings_map->SetContentSettingCustomScope(
      pattern3, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, std::string(), CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host1, host1, ContentSettingsType::COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host2, host2, ContentSettingsType::COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host3, host3, ContentSettingsType::COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host4, host4, ContentSettingsType::COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host5, host5, ContentSettingsType::COOKIES, std::string()));

  host_content_settings_map->ClearSettingsForOneType(
      ContentSettingsType::COOKIES);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::COOKIES, nullptr));

  GURL https_host1("https://b.example.com/");
  GURL https_host2("https://a.b.example.com/");
  ContentSettingsPattern pattern4 =
      ContentSettingsPattern::FromString("b.example.com");

  host_content_settings_map->SetContentSettingCustomScope(
      pattern4, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, std::string(), CONTENT_SETTING_BLOCK);
  // Pattern "b.example.com" will affect (http|https)://b.example.com
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host2, host2, ContentSettingsType::COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                https_host1, https_host1, ContentSettingsType::COOKIES,
                std::string()));
  // Pattern "b.example.com" will not affect (http|https)://a.b.example.com
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host3, host3, ContentSettingsType::COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                https_host2, https_host2, ContentSettingsType::COOKIES,
                std::string()));
}

TEST_F(HostContentSettingsMapTest, TypeIsolatedSettings) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  GURL host("http://example.com/");

  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::COOKIES, std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::GEOLOCATION, std::string()));
}

TEST_F(HostContentSettingsMapTest, IncognitoInheritInitialAllow) {
  // The cookie setting has an initial value of ALLOW, so all changes should be
  // inherited from regular to incognito mode.
  TestingProfile profile;
  Profile* otr_profile = profile.GetOffTheRecordProfile();
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  HostContentSettingsMap* otr_map =
      HostContentSettingsMapFactory::GetForProfile(otr_profile);

  GURL host("http://example.com/");

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            otr_map->GetContentSetting(host, host, ContentSettingsType::COOKIES,
                                       std::string()));

  // Changing content settings on the main map should also affect the
  // incognito map.
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::COOKIES, std::string(),
      CONTENT_SETTING_SESSION_ONLY);
  EXPECT_EQ(CONTENT_SETTING_SESSION_ONLY,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_SESSION_ONLY,
            otr_map->GetContentSetting(host, host, ContentSettingsType::COOKIES,
                                       std::string()));

  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::COOKIES, std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            otr_map->GetContentSetting(host, host, ContentSettingsType::COOKIES,
                                       std::string()));

  // Changing content settings on the incognito map should NOT affect the
  // main map.
  otr_map->SetContentSettingDefaultScope(host, GURL(),
                                         ContentSettingsType::COOKIES,
                                         std::string(), CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            otr_map->GetContentSetting(host, host, ContentSettingsType::COOKIES,
                                       std::string()));
}

TEST_F(HostContentSettingsMapTest, IncognitoInheritPopups) {
  // The popup setting has an initial value of BLOCK, but it is allowed
  // to inherit ALLOW settings because it doesn't provide access to user data.
  TestingProfile profile;
  Profile* otr_profile = profile.GetOffTheRecordProfile();
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  HostContentSettingsMap* otr_map =
      HostContentSettingsMapFactory::GetForProfile(otr_profile);

  GURL host("http://example.com/");

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::POPUPS, std::string()));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            otr_map->GetContentSetting(host, host, ContentSettingsType::POPUPS,
                                       std::string()));

  // Changing content settings on the main map should affect the
  // incognito map.
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::POPUPS, std::string(),
      CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::POPUPS, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            otr_map->GetContentSetting(host, host, ContentSettingsType::POPUPS,
                                       std::string()));

  // Changing content settings on the incognito map should NOT affect the
  // main map.
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::POPUPS, std::string(),
      CONTENT_SETTING_BLOCK);
  otr_map->SetContentSettingDefaultScope(host, GURL(),
                                         ContentSettingsType::POPUPS,
                                         std::string(), CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::POPUPS, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            otr_map->GetContentSetting(host, host, ContentSettingsType::POPUPS,
                                       std::string()));
}

TEST_F(HostContentSettingsMapTest, IncognitoPartialInheritPref) {
  // Permissions marked INHERIT_IF_LESS_PERMISSIVE in
  // ContentSettingsRegistry only inherit BLOCK and ASK settings from regular
  // to incognito if the initial value is ASK.
  TestingProfile profile;
  Profile* otr_profile = profile.GetOffTheRecordProfile();
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  HostContentSettingsMap* otr_map =
      HostContentSettingsMapFactory::GetForProfile(otr_profile);

  GURL host("http://example.com/");

  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      host_content_settings_map->GetContentSetting(
          host, host, ContentSettingsType::MEDIASTREAM_MIC, std::string()));
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      otr_map->GetContentSetting(
          host, host, ContentSettingsType::MEDIASTREAM_MIC, std::string()));

  // BLOCK should be inherited from the main map to the incognito map.
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::MEDIASTREAM_MIC, std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      host_content_settings_map->GetContentSetting(
          host, host, ContentSettingsType::MEDIASTREAM_MIC, std::string()));
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      otr_map->GetContentSetting(
          host, host, ContentSettingsType::MEDIASTREAM_MIC, std::string()));

  // ALLOW should not be inherited from the main map to the incognito map (but
  // it still overwrites the BLOCK, hence incognito reverts to ASK).
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::MEDIASTREAM_MIC, std::string(),
      CONTENT_SETTING_ALLOW);
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      host_content_settings_map->GetContentSetting(
          host, host, ContentSettingsType::MEDIASTREAM_MIC, std::string()));
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      otr_map->GetContentSetting(
          host, host, ContentSettingsType::MEDIASTREAM_MIC, std::string()));
}

TEST_F(HostContentSettingsMapTest, IncognitoPartialInheritDefault) {
  // Permissions marked INHERIT_IF_LESS_PERMISSIVE in
  // ContentSettingsRegistry only inherit BLOCK and ASK settings from regular
  // to incognito if the initial value is ASK.
  TestingProfile profile;
  Profile* otr_profile = profile.GetOffTheRecordProfile();
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  HostContentSettingsMap* otr_map =
      HostContentSettingsMapFactory::GetForProfile(otr_profile);

  GURL host("http://example.com/");

  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::GEOLOCATION, NULL));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::GEOLOCATION, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ASK, otr_map->GetDefaultContentSetting(
                                     ContentSettingsType::GEOLOCATION, NULL));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            otr_map->GetContentSetting(
                host, host, ContentSettingsType::GEOLOCATION, std::string()));

  // BLOCK should be inherited from the main map to the incognito map.
  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::GEOLOCATION, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::GEOLOCATION, NULL));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::GEOLOCATION, std::string()));
  EXPECT_EQ(CONTENT_SETTING_BLOCK, otr_map->GetDefaultContentSetting(
                                       ContentSettingsType::GEOLOCATION, NULL));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            otr_map->GetContentSetting(
                host, host, ContentSettingsType::GEOLOCATION, std::string()));

  // ALLOW should not be inherited from the main map to the incognito map (but
  // it still overwrites the BLOCK, hence incognito reverts to ASK).
  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::GEOLOCATION, NULL));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::GEOLOCATION, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ASK, otr_map->GetDefaultContentSetting(
                                     ContentSettingsType::GEOLOCATION, NULL));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            otr_map->GetContentSetting(
                host, host, ContentSettingsType::GEOLOCATION, std::string()));
}

TEST_F(HostContentSettingsMapTest, IncognitoDontInheritSetting) {
  // Website settings marked DONT_INHERIT_IN_INCOGNITO in
  // WebsiteSettingsRegistry (e.g. usb chooser data) don't inherit any values
  // from from regular to incognito.
  TestingProfile profile;
  Profile* otr_profile = profile.GetOffTheRecordProfile();
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  HostContentSettingsMap* otr_map =
      HostContentSettingsMapFactory::GetForProfile(otr_profile);

  GURL host("http://example.com/");

  // USB chooser data defaults to |nullptr|.
  EXPECT_EQ(nullptr, host_content_settings_map->GetWebsiteSetting(
                         host, host, ContentSettingsType::USB_CHOOSER_DATA,
                         std::string(), nullptr));
  EXPECT_EQ(nullptr, otr_map->GetWebsiteSetting(
                         host, host, ContentSettingsType::USB_CHOOSER_DATA,
                         std::string(), nullptr));

  base::DictionaryValue test_value;
  test_value.SetString("test", "value");
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      host, host, ContentSettingsType::USB_CHOOSER_DATA, std::string(),
      base::WrapUnique(test_value.DeepCopy()));

  // The setting is not inherted by |otr_map|.
  std::unique_ptr<base::Value> stored_value =
      host_content_settings_map->GetWebsiteSetting(
          host, host, ContentSettingsType::USB_CHOOSER_DATA, std::string(),
          nullptr);
  EXPECT_TRUE(stored_value && stored_value->Equals(&test_value));
  EXPECT_EQ(nullptr, otr_map->GetWebsiteSetting(
                         host, host, ContentSettingsType::USB_CHOOSER_DATA,
                         std::string(), nullptr));
}

TEST_F(HostContentSettingsMapTest, PrefExceptionsOperation) {
  using content_settings::SETTING_SOURCE_POLICY;
  using content_settings::SETTING_SOURCE_USER;

  const char kUrl1[] = "http://user_exception_allow.com";
  const char kUrl2[] = "http://user_exception_block.com";
  const char kUrl3[] = "http://non_exception.com";

  TestingProfile profile;
  // Arbitrarily using cookies as content type to test.
  TesterForType tester(&profile, ContentSettingsType::COOKIES);

  // Add |kUrl1| and |kUrl2| only.
  tester.AddUserException(kUrl1, CONTENT_SETTING_ALLOW);
  tester.AddUserException(kUrl2, CONTENT_SETTING_BLOCK);

  // No policy setting: follow users settings.
  tester.ClearPolicyDefault();
  // User exceptions.
  EXPECT_EQ(SETTING_SOURCE_USER, tester.GetSettingSourceForURL(kUrl1));
  EXPECT_EQ(SETTING_SOURCE_USER, tester.GetSettingSourceForURL(kUrl2));
  // User default.
  EXPECT_EQ(SETTING_SOURCE_USER, tester.GetSettingSourceForURL(kUrl3));

  // Policy overrides users always.
  tester.SetPolicyDefault(CONTENT_SETTING_ALLOW);
  EXPECT_EQ(SETTING_SOURCE_POLICY, tester.GetSettingSourceForURL(kUrl1));
  EXPECT_EQ(SETTING_SOURCE_POLICY, tester.GetSettingSourceForURL(kUrl2));
  EXPECT_EQ(SETTING_SOURCE_POLICY, tester.GetSettingSourceForURL(kUrl3));
  tester.SetPolicyDefault(CONTENT_SETTING_BLOCK);
  EXPECT_EQ(SETTING_SOURCE_POLICY, tester.GetSettingSourceForURL(kUrl1));
  EXPECT_EQ(SETTING_SOURCE_POLICY, tester.GetSettingSourceForURL(kUrl2));
  EXPECT_EQ(SETTING_SOURCE_POLICY, tester.GetSettingSourceForURL(kUrl3));
}

TEST_F(HostContentSettingsMapTest, GetUserModifiableContentSetting) {
  GURL url("http://user_exception_allow.com");

  TestingProfile profile;
  // Arbitrarily using cookies as content type to test.
  profile.GetTestingPrefService()->SetManagedPref(
      prefs::kManagedDefaultCookiesSetting,
      std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  map->SetContentSettingDefaultScope(url, url, ContentSettingsType::COOKIES,
                                     std::string(), CONTENT_SETTING_ALLOW);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetUserModifiableContentSetting(
                url, url, ContentSettingsType::COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            map->GetContentSetting(url, url, ContentSettingsType::COOKIES,
                                   std::string()));
}

// For a single Unicode encoded pattern, check if it gets converted to punycode
// and old pattern gets deleted.
TEST_F(HostContentSettingsMapTest, CanonicalizeExceptionsUnicodeOnly) {
  TestingProfile profile;
  PrefService* prefs = profile.GetPrefs();

  // Set utf-8 data.
  {
    DictionaryPrefUpdate update(prefs,
                                GetPrefName(ContentSettingsType::COOKIES));
    base::DictionaryValue* all_settings_dictionary = update.Get();
    ASSERT_TRUE(NULL != all_settings_dictionary);

    auto dummy_payload = std::make_unique<base::DictionaryValue>();
    dummy_payload->SetInteger("setting", CONTENT_SETTING_ALLOW);
    all_settings_dictionary->SetWithoutPathExpansion("[*.]\xC4\x87ira.com,*",
                                                     std::move(dummy_payload));
  }

  HostContentSettingsMapFactory::GetForProfile(&profile);

  const base::DictionaryValue* all_settings_dictionary =
      prefs->GetDictionary(GetPrefName(ContentSettingsType::COOKIES));
  const base::DictionaryValue* result = NULL;
  EXPECT_FALSE(all_settings_dictionary->GetDictionaryWithoutPathExpansion(
      "[*.]\xC4\x87ira.com,*", &result));
  EXPECT_TRUE(all_settings_dictionary->GetDictionaryWithoutPathExpansion(
      "[*.]xn--ira-ppa.com,*", &result));
}

// If both Unicode and its punycode pattern exist, make sure we don't touch the
// settings for the punycode, and that Unicode pattern gets deleted.
TEST_F(HostContentSettingsMapTest, CanonicalizeExceptionsUnicodeAndPunycode) {
  TestingProfile profile;

  std::unique_ptr<base::Value> value = base::JSONReader::ReadDeprecated(
      "{\"[*.]\\xC4\\x87ira.com,*\":{\"setting\":1}}");
  profile.GetPrefs()->Set(GetPrefName(ContentSettingsType::COOKIES), *value);

  // Set punycode equivalent, with different setting.
  std::unique_ptr<base::Value> puny_value = base::JSONReader::ReadDeprecated(
      "{\"[*.]xn--ira-ppa.com,*\":{\"setting\":2}}");
  profile.GetPrefs()->Set(GetPrefName(ContentSettingsType::COOKIES),
                          *puny_value);

  // Initialize the content map.
  HostContentSettingsMapFactory::GetForProfile(&profile);

  const base::DictionaryValue& content_setting_prefs =
      *profile.GetPrefs()->GetDictionary(
          GetPrefName(ContentSettingsType::COOKIES));
  std::string prefs_as_json;
  base::JSONWriter::Write(content_setting_prefs, &prefs_as_json);
  EXPECT_STREQ("{\"[*.]xn--ira-ppa.com,*\":{\"setting\":2}}",
               prefs_as_json.c_str());
}

// If a default-content-setting is managed, the managed value should be used
// instead of the default value.
TEST_F(HostContentSettingsMapTest, ManagedDefaultContentSetting) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile.GetTestingPrefService();

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::JAVASCRIPT, NULL));

  // Set managed-default-content-setting through the coresponding preferences.
  prefs->SetManagedPref(prefs::kManagedDefaultJavaScriptSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::JAVASCRIPT, NULL));

  // Remove managed-default-content-settings-preferences.
  prefs->RemoveManagedPref(prefs::kManagedDefaultJavaScriptSetting);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::JAVASCRIPT, NULL));

#if BUILDFLAG(ENABLE_PLUGINS)
  // Set preference to manage the default-content-setting for Plugins.
  prefs->SetManagedPref(prefs::kManagedDefaultPluginsSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::PLUGINS, NULL));

  // Remove the preference to manage the default-content-setting for Plugins.
  prefs->RemoveManagedPref(prefs::kManagedDefaultPluginsSetting);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::PLUGINS, NULL));
#endif
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::ADS, NULL));

  // Set managed-default-content-setting through the coresponding preferences.
  prefs->SetManagedPref(prefs::kManagedDefaultAdsSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::ADS, NULL));

  // Remove managed-default-content-settings-preferences.
  prefs->RemoveManagedPref(prefs::kManagedDefaultAdsSetting);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::ADS, NULL));
}

TEST_F(HostContentSettingsMapTest,
       GetNonDefaultContentSettingsIfTypeManaged) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile.GetTestingPrefService();

  // Set url for JavaScript setting.
  GURL host("http://example.com/");
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::JAVASCRIPT, std::string(),
      CONTENT_SETTING_BLOCK);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::JAVASCRIPT, NULL));

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::JAVASCRIPT, std::string()));

  // Set managed-default-content-setting for content-settings-type JavaScript.
  prefs->SetManagedPref(prefs::kManagedDefaultJavaScriptSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::JAVASCRIPT, std::string()));
}

// Managed default content setting should have higher priority
// than user defined patterns.
TEST_F(HostContentSettingsMapTest,
       ManagedDefaultContentSettingIgnoreUserPattern) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile.GetTestingPrefService();

  // Block all JavaScript.
  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::JAVASCRIPT, CONTENT_SETTING_BLOCK);

  // Set an exception to allow "[*.]example.com"
  GURL host("http://example.com/");

  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::JAVASCRIPT, std::string(),
      CONTENT_SETTING_ALLOW);

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::JAVASCRIPT, NULL));

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::JAVASCRIPT, std::string()));

  // Set managed-default-content-settings-preferences.
  prefs->SetManagedPref(prefs::kManagedDefaultJavaScriptSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::JAVASCRIPT, std::string()));

  // Remove managed-default-content-settings-preferences.
  prefs->RemoveManagedPref(prefs::kManagedDefaultJavaScriptSetting);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::JAVASCRIPT, std::string()));
}

// If a default-content-setting is set to managed setting, the user defined
// setting should be preserved.
TEST_F(HostContentSettingsMapTest, OverwrittenDefaultContentSetting) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile.GetTestingPrefService();

  // Set user defined default-content-setting for Cookies.
  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::COOKIES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::COOKIES, NULL));

  // Set preference to manage the default-content-setting for Cookies.
  prefs->SetManagedPref(prefs::kManagedDefaultCookiesSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::COOKIES, NULL));

  // Remove the preference to manage the default-content-setting for Cookies.
  prefs->RemoveManagedPref(prefs::kManagedDefaultCookiesSetting);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::COOKIES, NULL));
}

// If a setting for a default-content-setting-type is set while the type is
// managed, then the new setting should be preserved and used after the
// default-content-setting-type is not managed anymore.
TEST_F(HostContentSettingsMapTest, SettingDefaultContentSettingsWhenManaged) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile.GetTestingPrefService();

  prefs->SetManagedPref(prefs::kManagedDefaultCookiesSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::COOKIES, NULL));

  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::COOKIES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::COOKIES, NULL));

  prefs->RemoveManagedPref(prefs::kManagedDefaultCookiesSetting);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::COOKIES, NULL));
}

TEST_F(HostContentSettingsMapTest, GetContentSetting) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  GURL host("http://example.com/");
  GURL embedder("chrome://foo");
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::COOKIES, std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                embedder, host, ContentSettingsType::COOKIES, std::string()));
}

TEST_F(HostContentSettingsMapTest, AddContentSettingsObserver) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  content_settings::MockObserver mock_observer;

  GURL host("http://example.com/");
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]example.com");
  EXPECT_CALL(mock_observer, OnContentSettingChanged(
                                 pattern, ContentSettingsPattern::Wildcard(),
                                 ContentSettingsType::COOKIES, ""));

  host_content_settings_map->AddObserver(&mock_observer);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES, std::string()));
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::COOKIES, std::string(),
      CONTENT_SETTING_DEFAULT);
}

// Guest profiles do not exist on Android, so don't run these tests there.
#if !defined(OS_ANDROID)
TEST_F(HostContentSettingsMapTest, GuestProfile) {
  TestingProfile::Builder profile_builder;
  profile_builder.SetGuestSession();
  std::unique_ptr<Profile> profile = profile_builder.Build();
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile.get());

  GURL host("http://example.com/");
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]example.com");

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES, std::string()));

  // Changing content settings should not result in any prefs being stored
  // however the value should be set in memory.
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::COOKIES, std::string(),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES, std::string()));

  const base::DictionaryValue* all_settings_dictionary =
      profile->GetPrefs()->GetDictionary(
          GetPrefName(ContentSettingsType::COOKIES));
  EXPECT_TRUE(all_settings_dictionary->empty());
}

// Default settings should not be modifiable for the guest profile (there is no
// UI to do this).
TEST_F(HostContentSettingsMapTest, GuestProfileDefaultSetting) {
  TestingProfile::Builder profile_builder;
  profile_builder.SetGuestSession();
  std::unique_ptr<Profile> profile = profile_builder.Build();
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile.get());

  GURL host("http://example.com/");

  // There are no custom rules, so this should be the default.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES, std::string()));

  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::COOKIES, CONTENT_SETTING_BLOCK);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES, std::string()));
}
#endif  // !defined(OS_ANDROID)

TEST_F(HostContentSettingsMapTest, InvalidPattern) {
  // This is a regression test for crbug.com/618529, which fixed a memory leak
  // when a website setting was set under a URL that mapped to an invalid
  // pattern.
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  GURL unsupported_url = GURL("view-source:http://www.google.com");
  base::DictionaryValue test_value;
  test_value.SetString("test", "value");
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      unsupported_url, unsupported_url, ContentSettingsType::APP_BANNER,
      std::string(), base::WrapUnique(test_value.DeepCopy()));
  EXPECT_EQ(nullptr,
            host_content_settings_map->GetWebsiteSetting(
                unsupported_url, unsupported_url,
                ContentSettingsType::APP_BANNER, std::string(), nullptr));
}

TEST_F(HostContentSettingsMapTest, ClearSettingsForOneTypeWithPredicate) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  ContentSettingsForOneType host_settings;

  // Patterns with wildcards.
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]example.org");
  ContentSettingsPattern pattern2 =
      ContentSettingsPattern::FromString("[*.]example.net");

  // Patterns without wildcards.
  GURL url1("https://www.google.com/");
  GURL url2("https://www.google.com/maps");
  GURL url3("http://www.google.com/maps");
  GURL url3_origin_only("http://www.google.com/");

  host_content_settings_map->SetContentSettingCustomScope(
      pattern2, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, std::string(), CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetContentSettingCustomScope(
      pattern, ContentSettingsPattern::Wildcard(), ContentSettingsType::COOKIES,
      std::string(), CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetWebsiteSettingCustomScope(
      pattern2, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::APP_BANNER, std::string(),
      base::WrapUnique(new base::DictionaryValue()));

  // First, test that we clear only COOKIES (not APP_BANNER), and pattern2.
  host_content_settings_map->ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType::COOKIES, base::Time(), base::Time::Max(),
      base::Bind(&MatchPrimaryPattern, pattern2));
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::COOKIES, std::string(), &host_settings);
  // |host_settings| contains default & block.
  EXPECT_EQ(2U, host_settings.size());
  EXPECT_EQ(pattern, host_settings[0].primary_pattern);
  EXPECT_EQ("*", host_settings[0].secondary_pattern.ToString());
  EXPECT_EQ("*", host_settings[1].primary_pattern.ToString());
  EXPECT_EQ("*", host_settings[1].secondary_pattern.ToString());

  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::APP_BANNER, std::string(), &host_settings);
  // |host_settings| still contains the value for APP_BANNER.
  EXPECT_EQ(1U, host_settings.size());
  EXPECT_EQ(pattern2, host_settings[0].primary_pattern);
  EXPECT_EQ("*", host_settings[0].secondary_pattern.ToString());

  // Next, test that we do correct pattern matching w/ an origin policy item.
  // We verify that we have no settings stored.
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::SITE_ENGAGEMENT, std::string(), &host_settings);
  EXPECT_EQ(0u, host_settings.size());
  // Add settings.
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      url1, GURL(), ContentSettingsType::SITE_ENGAGEMENT, std::string(),
      base::WrapUnique(new base::DictionaryValue()));
  // This setting should override the one above, as it's the same origin.
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      url2, GURL(), ContentSettingsType::SITE_ENGAGEMENT, std::string(),
      base::WrapUnique(new base::DictionaryValue()));
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      url3, GURL(), ContentSettingsType::SITE_ENGAGEMENT, std::string(),
      base::WrapUnique(new base::DictionaryValue()));
  // Verify we only have two.
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::SITE_ENGAGEMENT, std::string(), &host_settings);
  EXPECT_EQ(2u, host_settings.size());

  // Clear the http one, which we should be able to do w/ the origin only, as
  // the scope of ContentSettingsType::SITE_ENGAGEMENT is
  // REQUESTING_ORIGIN_ONLY_SCOPE.
  ContentSettingsPattern http_pattern =
      ContentSettingsPattern::FromURLNoWildcard(url3_origin_only);
  host_content_settings_map->ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType::SITE_ENGAGEMENT, base::Time(), base::Time::Max(),
      base::Bind(&MatchPrimaryPattern, http_pattern));
  // Verify we only have one, and it's url1.
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::SITE_ENGAGEMENT, std::string(), &host_settings);
  EXPECT_EQ(1u, host_settings.size());
  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(url1),
            host_settings[0].primary_pattern);
}

TEST_F(HostContentSettingsMapTest, ClearSettingsWithTimePredicate) {
  TestingProfile profile;
  auto* map = HostContentSettingsMapFactory::GetForProfile(&profile);
  base::Time now = base::Time::Now();
  base::Time back_1_hour = now - base::TimeDelta::FromHours(1);
  base::Time back_30_days = now - base::TimeDelta::FromDays(30);
  base::Time back_31_days = now - base::TimeDelta::FromDays(31);

  base::SimpleTestClock test_clock;
  test_clock.SetNow(now);
  map->SetClockForTesting(&test_clock);

  ContentSettingsForOneType host_settings;

  GURL url1("https://www.google.com/");
  GURL url2("https://maps.google.com/");
  GURL url3("https://photos.google.com");

  // Add setting for url1.
  map->SetContentSettingDefaultScope(url1, GURL(), ContentSettingsType::POPUPS,
                                     std::string(), CONTENT_SETTING_BLOCK);

  // Add setting for url2.
  test_clock.SetNow(back_1_hour);
  map->SetContentSettingDefaultScope(url2, GURL(), ContentSettingsType::POPUPS,
                                     std::string(), CONTENT_SETTING_BLOCK);

  // Add setting for url3 with the timestamp of 31 days old.
  test_clock.SetNow(back_31_days);
  map->SetContentSettingDefaultScope(url3, GURL(), ContentSettingsType::POPUPS,
                                     std::string(), CONTENT_SETTING_BLOCK);

  // Verify we have three pattern and the default.
  map->GetSettingsForOneType(ContentSettingsType::POPUPS, std::string(),
                             &host_settings);
  EXPECT_EQ(4u, host_settings.size());

  // Clear all settings since |now|.
  map->ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType::POPUPS, now, base::Time::Max(),
      HostContentSettingsMap::PatternSourcePredicate());

  // Verify we have two pattern (url2, url3) and the default.
  map->GetSettingsForOneType(ContentSettingsType::POPUPS, std::string(),
                             &host_settings);
  EXPECT_EQ(3u, host_settings.size());
  EXPECT_EQ("https://maps.google.com:443",
            host_settings[0].primary_pattern.ToString());
  EXPECT_EQ("https://photos.google.com:443",
            host_settings[1].primary_pattern.ToString());
  EXPECT_EQ("*", host_settings[2].primary_pattern.ToString());

  // Clear all settings since the beginning of time to 30 days old.
  map->ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType::POPUPS, base::Time(), back_30_days,
      HostContentSettingsMap::PatternSourcePredicate());

  // Verify we only have one pattern (url2) and the default.
  map->GetSettingsForOneType(ContentSettingsType::POPUPS, std::string(),
                             &host_settings);
  EXPECT_EQ(2u, host_settings.size());
  EXPECT_EQ("https://maps.google.com:443",
            host_settings[0].primary_pattern.ToString());
  EXPECT_EQ("*", host_settings[1].primary_pattern.ToString());

  // Clear all settings since the beginning of time.
  map->ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType::POPUPS, base::Time(), base::Time::Max(),
      HostContentSettingsMap::PatternSourcePredicate());

  // Verify we only have the default setting.
  map->GetSettingsForOneType(ContentSettingsType::POPUPS, std::string(),
                             &host_settings);
  EXPECT_EQ(1u, host_settings.size());
  EXPECT_EQ("*", host_settings[0].primary_pattern.ToString());
}

TEST_F(HostContentSettingsMapTest, GetSettingLastModified) {
  TestingProfile profile;
  auto* map = HostContentSettingsMapFactory::GetForProfile(&profile);

  base::SimpleTestClock test_clock;
  test_clock.SetNow(base::Time::Now());
  map->SetClockForTesting(&test_clock);

  ContentSettingsForOneType host_settings;

  GURL url("https://www.google.com/");
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromURLNoWildcard(url);

  // Last modified date for non existant settings should be base::Time().
  base::Time t = map->GetSettingLastModifiedDate(
      pattern, ContentSettingsPattern::Wildcard(), ContentSettingsType::POPUPS);
  EXPECT_EQ(base::Time(), t);

  // Add setting for url.
  map->SetContentSettingDefaultScope(url, GURL(), ContentSettingsType::POPUPS,
                                     std::string(), CONTENT_SETTING_BLOCK);
  t = map->GetSettingLastModifiedDate(
      pattern, ContentSettingsPattern::Wildcard(), ContentSettingsType::POPUPS);
  EXPECT_EQ(t, test_clock.Now());

  test_clock.Advance(base::TimeDelta::FromSeconds(1));
  // Modify setting.
  map->SetContentSettingDefaultScope(url, GURL(), ContentSettingsType::POPUPS,
                                     std::string(), CONTENT_SETTING_ALLOW);

  t = map->GetSettingLastModifiedDate(
      pattern, ContentSettingsPattern::Wildcard(), ContentSettingsType::POPUPS);
  EXPECT_EQ(t, test_clock.Now());
}

TEST_F(HostContentSettingsMapTest, LastModifiedMultipleModifiableProviders) {
  TestingProfile profile;
  auto* map = HostContentSettingsMapFactory::GetForProfile(&profile);
  GURL url("https://www.google.com/");
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromURLNoWildcard(url);

  base::Time t1 = base::Time::Now();
  auto test_clock = std::make_unique<base::SimpleTestClock>();
  test_clock->SetNow(t1);

  base::SimpleTestClock* clock = test_clock.get();
  clock->Advance(base::TimeDelta::FromSeconds(1));
  base::Time t2 = clock->Now();

  // Register a provider which reports a modification time of t1.
  std::unique_ptr<MockUserModifiableProvider> provider =
      std::make_unique<MockUserModifiableProvider>();
  EXPECT_CALL(*provider, GetWebsiteSettingLastModified(
                             _, _, ContentSettingsType::NOTIFICATIONS, _))
      .WillOnce(Return(t1));
  MockUserModifiableProvider* weak_provider = provider.get();
  map->RegisterUserModifiableProvider(
      HostContentSettingsMap::PROVIDER_FOR_TESTS, std::move(provider));

  // Register another provider which reports a modification time of t2.
  std::unique_ptr<MockUserModifiableProvider> other_provider =
      std::make_unique<MockUserModifiableProvider>();
  EXPECT_CALL(*other_provider, GetWebsiteSettingLastModified(
                                   _, _, ContentSettingsType::NOTIFICATIONS, _))
      .WillRepeatedly(Return(t2));
  MockUserModifiableProvider* weak_other_provider = other_provider.get();
  map->RegisterUserModifiableProvider(
      HostContentSettingsMap::OTHER_PROVIDER_FOR_TESTS,
      std::move(other_provider));

  // Expect the more recent modification time to be reported.
  EXPECT_EQ(t2, map->GetSettingLastModifiedDate(
                    pattern, ContentSettingsPattern::Wildcard(),
                    ContentSettingsType::NOTIFICATIONS));

  // Now have original provider report a more recent modification time.
  clock->Advance(base::TimeDelta::FromSeconds(1));
  base::Time t3 = clock->Now();
  EXPECT_CALL(*weak_provider, GetWebsiteSettingLastModified(
                                  _, _, ContentSettingsType::NOTIFICATIONS, _))
      .WillOnce(Return(t3));

  // Expect the timestamp from the registered provider to be reported now.
  EXPECT_EQ(t3, map->GetSettingLastModifiedDate(
                    pattern, ContentSettingsPattern::Wildcard(),
                    ContentSettingsType::NOTIFICATIONS));
  weak_provider->RemoveObserver(map);
  weak_other_provider->RemoveObserver(map);
}

TEST_F(HostContentSettingsMapTest, IsRestrictedToSecureOrigins) {
  TestingProfile profile;
  const auto* map = HostContentSettingsMapFactory::GetForProfile(&profile);
  EXPECT_TRUE(
      map->IsRestrictedToSecureOrigins(ContentSettingsType::GEOLOCATION));

  EXPECT_FALSE(
      map->IsRestrictedToSecureOrigins(ContentSettingsType::JAVASCRIPT));
}

TEST_F(HostContentSettingsMapTest, CanSetNarrowestSetting) {
  TestingProfile profile;
  const auto* map = HostContentSettingsMapFactory::GetForProfile(&profile);

  GURL valid_url("http://google.com");
  EXPECT_TRUE(map->CanSetNarrowestContentSetting(valid_url, valid_url,
                                                 ContentSettingsType::POPUPS));

  GURL invalid_url("about:blank");
  EXPECT_FALSE(map->CanSetNarrowestContentSetting(invalid_url, invalid_url,
                                                  ContentSettingsType::POPUPS));
}

TEST_F(HostContentSettingsMapTest, MigrateRequestingAndTopLevelOriginSettings) {
  TestingProfile profile;
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  GURL requesting_origin("https://requester.com");
  GURL embedding_origin("https://embedder.com");

  ContentSettingsPattern requesting_pattern =
      ContentSettingsPattern::FromURLNoWildcard(requesting_origin);
  ContentSettingsPattern embedding_pattern =
      ContentSettingsPattern::FromURLNoWildcard(embedding_origin);

  // Set content settings for 2 types that use requesting and top level
  // origin as well as one for a type that doesn't.
  map->SetContentSettingCustomScope(requesting_pattern, embedding_pattern,
                                    ContentSettingsType::GEOLOCATION,
                                    std::string(), CONTENT_SETTING_ALLOW);
  map->SetContentSettingCustomScope(requesting_pattern, embedding_pattern,
                                    ContentSettingsType::MIDI_SYSEX,
                                    std::string(), CONTENT_SETTING_ALLOW);

  map->SetContentSettingCustomScope(requesting_pattern, embedding_pattern,
                                    ContentSettingsType::COOKIES, std::string(),
                                    CONTENT_SETTING_ALLOW);

  map->MigrateRequestingAndTopLevelOriginSettings();

  ContentSettingsForOneType host_settings;
  // Verify that all the settings are deleted except the cookies setting.
  map->GetSettingsForOneType(ContentSettingsType::GEOLOCATION, std::string(),
                             &host_settings);
  EXPECT_EQ(1u, host_settings.size());
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            host_settings[0].primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            host_settings[0].secondary_pattern);

  map->GetSettingsForOneType(ContentSettingsType::MIDI_SYSEX, std::string(),
                             &host_settings);
  EXPECT_EQ(1u, host_settings.size());
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            host_settings[0].primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            host_settings[0].secondary_pattern);

  map->GetSettingsForOneType(ContentSettingsType::COOKIES, std::string(),
                             &host_settings);
  EXPECT_EQ(2u, host_settings.size());
  EXPECT_EQ(requesting_pattern, host_settings[0].primary_pattern);
  EXPECT_EQ(embedding_pattern, host_settings[0].secondary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            host_settings[1].primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            host_settings[1].secondary_pattern);
}

TEST_F(HostContentSettingsMapTest,
       MigrateRequestingAndTopLevelOriginSettingsResetsEmbeddedSetting) {
  TestingProfile profile;
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  GURL requesting_origin("https://requester.com");
  GURL embedding_origin("https://embedder.com");

  ContentSettingsPattern requesting_pattern =
      ContentSettingsPattern::FromURLNoWildcard(requesting_origin);
  ContentSettingsPattern embedding_pattern =
      ContentSettingsPattern::FromURLNoWildcard(embedding_origin);

  map->SetContentSettingCustomScope(requesting_pattern, embedding_pattern,
                                    ContentSettingsType::GEOLOCATION,
                                    std::string(), CONTENT_SETTING_BLOCK);
  map->SetContentSettingCustomScope(embedding_pattern, embedding_pattern,
                                    ContentSettingsType::GEOLOCATION,
                                    std::string(), CONTENT_SETTING_ALLOW);

  map->MigrateRequestingAndTopLevelOriginSettings();

  ContentSettingsForOneType host_settings;
  // Verify that all settings for the embedding origin are deleted. This is
  // important so that a user is repropmted if a permission request from an
  // embedded site they had previously blocked makes a new request.
  map->GetSettingsForOneType(ContentSettingsType::GEOLOCATION, std::string(),
                             &host_settings);
  EXPECT_EQ(1u, host_settings.size());
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            host_settings[0].primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            host_settings[0].secondary_pattern);
}

#if BUILDFLAG(ENABLE_PLUGINS)
// Test that existing Flash preferences should get copied into the
// |ContentSettingsType::PLUGINS_DATA| setting on the creation of a new
// |HostContentSettingsMap|.
TEST_F(HostContentSettingsMapTest, PluginDataMigration) {
  // Avoid the test if Flash permissions are ephemeral.
  if (content_settings::ContentSettingsRegistry::GetInstance()
          ->Get(ContentSettingsType::PLUGINS)
          ->storage_behavior() ==
      content_settings::ContentSettingsInfo::EPHEMERAL) {
    return;
  }
  TestingProfile profile;
  // Set a website-specific Flash preference and a pattern exception.
  std::unique_ptr<base::Value> value = base::JSONReader::ReadDeprecated(
      "{\"https://urlwithflashchanged.com:443,*\":{\"setting\":1}, "
      "\"[*.]patternurl.com:443,*\":{\"setting\":1}}");
  profile.GetPrefs()->Set(GetPrefName(ContentSettingsType::PLUGINS), *value);

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  // Check it was copied successfully.
  const GURL url1("https://urlwithflashchanged.com");
  EXPECT_NE(nullptr, map->GetWebsiteSetting(url1, url1,
                                            ContentSettingsType::PLUGINS_DATA,
                                            std::string(), nullptr));
  // Check other urls were not affected.
  const GURL url2("https://urlwithflashdefault.com");
  EXPECT_EQ(nullptr, map->GetWebsiteSetting(url2, url2,
                                            ContentSettingsType::PLUGINS_DATA,
                                            std::string(), nullptr));
  // Check patterns are also unaffected.
  const GURL pattern("[*.]patternurl.com");
  EXPECT_EQ(nullptr, map->GetWebsiteSetting(pattern, pattern,
                                            ContentSettingsType::PLUGINS_DATA,
                                            std::string(), nullptr));
}

// If there are existing |ContentSettingsType::PLUGINS_DATA| preferences
// stored, check we skip the migration.
TEST_F(HostContentSettingsMapTest, PluginDataMigrated) {
  TestingProfile profile;
  // Set a website-specific Flash preference and another preference indicating
  // that the Flash setting has changed for a different website.
  std::unique_ptr<base::Value> value = base::JSONReader::ReadDeprecated(
      "{\"https://unmigratedurl.com:443,*\":{\"setting\":1}}");
  profile.GetPrefs()->Set(GetPrefName(ContentSettingsType::PLUGINS), *value);
  value = base::JSONReader::ReadDeprecated(
      "{\"https://"
      "example.com:443,*\":{\"setting\":{\"flashPreviouslyChanged\":true}}}");
  profile.GetPrefs()->Set(GetPrefName(ContentSettingsType::PLUGINS_DATA),
                          *value);

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  // Check it was copied successfully.
  const GURL flash_data_url("https://example.com");
  EXPECT_NE(nullptr, map->GetWebsiteSetting(flash_data_url, flash_data_url,
                                            ContentSettingsType::PLUGINS_DATA,
                                            std::string(), nullptr));
  // Check the migration code was not run (i.e. the other Flash preference set
  // above was not migrated). Theoretically this should never happen, but this
  // scenario is useful for testing.
  const GURL unmigrated_url("https://unmigratedurl.com");
  EXPECT_EQ(nullptr, map->GetWebsiteSetting(unmigrated_url, unmigrated_url,
                                            ContentSettingsType::PLUGINS_DATA,
                                            std::string(), nullptr));
}

// Creates new instances of PrefProvider and EphemeralProvider and overrides
// them in |host_content_settings_map|.
void ReloadProviders(PrefService* pref_service,
                     HostContentSettingsMap* host_content_settings_map) {
  auto pref_provider = std::make_unique<content_settings::PrefProvider>(
      pref_service, false, true);
  content_settings::TestUtils::OverrideProvider(
      host_content_settings_map, std::move(pref_provider),
      HostContentSettingsMap::PREF_PROVIDER);

  auto ephemeral_provider =
      std::make_unique<content_settings::EphemeralProvider>(true);
  content_settings::TestUtils::OverrideProvider(
      host_content_settings_map, std::move(ephemeral_provider),
      HostContentSettingsMap::EPHEMERAL_PROVIDER);
}

// Tests that Flash permissions are reset after restarting.
// Flash, and consequently, Flash permissions are not available on Android.
#if !defined(OS_ANDROID)
TEST_F(HostContentSettingsMapTest, FlashPermissionsAreEphemeral) {
  TestingProfile profile;
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  const GURL url("https://example.com");

  map->SetDefaultContentSetting(ContentSettingsType::PLUGINS,
                                CONTENT_SETTING_ASK);

  base::test::ScopedFeatureList feature_list;
  content_settings::ContentSettingsRegistry::GetInstance()->ResetForTest();

  ReloadProviders(profile.GetPrefs(), map);
  map->SetContentSettingDefaultScope(url, url, ContentSettingsType::PLUGINS,
                                     std::string(), CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(url, url, ContentSettingsType::PLUGINS,
                                   std::string()));

  ReloadProviders(profile.GetPrefs(), map);
  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(url, url, ContentSettingsType::PLUGINS,
                                   std::string()));
}
#endif  // !defined(OS_ANDROID)

// Tests that restarting only removes ephemeral permissions. Flash, and
// consequently, Flash permissions are not available on Android.
#if !defined(OS_ANDROID)
TEST_F(HostContentSettingsMapTest, MixedEphemeralAndPersistentPermissions) {
  TestingProfile profile;
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  content_settings::ContentSettingsRegistry::GetInstance()->ResetForTest();
  ReloadProviders(profile.GetPrefs(), map);

  // The following two types are used as samples of ephemeral and persistent
  // permission types. They can be replaced with any other type if required.
  const ContentSettingsType ephemeral_type = ContentSettingsType::PLUGINS;
  const ContentSettingsType persistent_type = ContentSettingsType::GEOLOCATION;

  EXPECT_EQ(content_settings::ContentSettingsInfo::EPHEMERAL,
            content_settings::ContentSettingsRegistry::GetInstance()
                ->Get(ephemeral_type)
                ->storage_behavior());
  EXPECT_EQ(content_settings::ContentSettingsInfo::PERSISTENT,
            content_settings::ContentSettingsRegistry::GetInstance()
                ->Get(persistent_type)
                ->storage_behavior());

  const GURL url("https://example.com");

  // Set default permission of both to ASK and expect it for a website.
  map->SetDefaultContentSetting(ephemeral_type, CONTENT_SETTING_ASK);
  map->SetDefaultContentSetting(persistent_type, CONTENT_SETTING_ASK);

  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(url, url, ephemeral_type, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(url, url, persistent_type, std::string()));

  // Set permission for both types and expect receiving it correctly.
  map->SetContentSettingDefaultScope(url, url, ephemeral_type, std::string(),
                                     CONTENT_SETTING_ALLOW);
  map->SetContentSettingDefaultScope(url, url, persistent_type, std::string(),
                                     CONTENT_SETTING_BLOCK);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            map->GetContentSetting(url, url, ephemeral_type, std::string()));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            map->GetContentSetting(url, url, persistent_type, std::string()));

  // Restart and expect reset of ephemeral permission to ASK, while keeping
  // the permission of persistent type.
  ReloadProviders(profile.GetPrefs(), map);

  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(url, url, ephemeral_type, std::string()));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            map->GetContentSetting(url, url, persistent_type, std::string()));
}
#endif  // !defined(OS_ANDROID)

// Test that directly writing a value to PrefProvider doesn't affect ephmeral
// types. Flash, and consequently, Flash permissions are not available on
// Android.
#if !defined(OS_ANDROID)
TEST_F(HostContentSettingsMapTest, EphemeralTypeDoesntReadFromPrefProvider) {
  TestingProfile profile;
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  content_settings::ContentSettingsRegistry::GetInstance()->ResetForTest();
  ReloadProviders(profile.GetPrefs(), map);

  // ContentSettingsType::PLUGINS is used as a sample of ephemeral permission
  // type. It can be replaced with any other type if required.
  const ContentSettingsType ephemeral_type = ContentSettingsType::PLUGINS;

  EXPECT_EQ(content_settings::ContentSettingsInfo::EPHEMERAL,
            content_settings::ContentSettingsRegistry::GetInstance()
                ->Get(ephemeral_type)
                ->storage_behavior());

  const GURL url("https://example.com");
  const ContentSettingsPattern pattern = ContentSettingsPattern::FromURL(url);

  map->SetDefaultContentSetting(ephemeral_type, CONTENT_SETTING_ASK);

  content_settings::PrefProvider pref_provider(profile.GetPrefs(), true, true);
  pref_provider.SetWebsiteSetting(
      pattern, pattern, ephemeral_type, std::string(),
      std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));

  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(url, url, ephemeral_type, std::string()));

  ReloadProviders(profile.GetPrefs(), map);

  EXPECT_EQ(CONTENT_SETTING_ASK,
            map->GetContentSetting(url, url, ephemeral_type, std::string()));

  pref_provider.ShutdownOnUIThread();
}

#endif  // !defined(OS_ANDROID)

#endif  // BUILDFLAG(ENABLE_PLUGINS)

TEST_F(HostContentSettingsMapTest, GetPatternsFromScopingType) {
  const GURL primary_url("http://a.b.example1.com:8080");
  const GURL secondary_url("http://a.b.example2.com:8080");

  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  // Testing case: WebsiteSettingsInfo::REQUESTING_DOMAIN_ONLY_SCOPE.
  host_content_settings_map->SetContentSettingDefaultScope(
      primary_url, secondary_url, ContentSettingsType::COOKIES, std::string(),
      CONTENT_SETTING_ALLOW);

  ContentSettingsForOneType settings;

  host_content_settings_map->GetSettingsForOneType(ContentSettingsType::COOKIES,
                                                   std::string(), &settings);

  EXPECT_EQ(settings[0].primary_pattern,
            ContentSettingsPattern::FromURL(primary_url));
  EXPECT_EQ(settings[0].secondary_pattern, ContentSettingsPattern::Wildcard());

  // Testing case: WebsiteSettingsInfo::TOP_LEVEL_ORIGIN_ONLY_SCOPE.
  host_content_settings_map->SetContentSettingDefaultScope(
      primary_url, secondary_url, ContentSettingsType::JAVASCRIPT,
      std::string(), CONTENT_SETTING_ALLOW);

  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::JAVASCRIPT, std::string(), &settings);

  EXPECT_EQ(settings[0].primary_pattern,
            ContentSettingsPattern::FromURLNoWildcard(primary_url));
  EXPECT_EQ(settings[0].secondary_pattern, ContentSettingsPattern::Wildcard());

  // Testing case: WebsiteSettingsInfo::REQUESTING_ORIGIN_ONLY_SCOPE.
  host_content_settings_map->SetContentSettingDefaultScope(
      primary_url, secondary_url, ContentSettingsType::NOTIFICATIONS,
      std::string(), CONTENT_SETTING_ALLOW);

  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::NOTIFICATIONS, std::string(), &settings);

  EXPECT_EQ(settings[0].primary_pattern,
            ContentSettingsPattern::FromURLNoWildcard(primary_url));
  EXPECT_EQ(settings[0].secondary_pattern, ContentSettingsPattern::Wildcard());

  // Testing case:
  // WebsiteSettingsInfo::REQUESTING_ORIGIN_AND_TOP_LEVEL_ORIGIN_SCOPE.
  host_content_settings_map->SetContentSettingDefaultScope(
      primary_url, secondary_url, ContentSettingsType::GEOLOCATION,
      std::string(), CONTENT_SETTING_ASK);

  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::GEOLOCATION, std::string(), &settings);

  EXPECT_EQ(settings[0].primary_pattern,
            ContentSettingsPattern::FromURLNoWildcard(primary_url));
  EXPECT_EQ(settings[0].secondary_pattern,
            ContentSettingsPattern::FromURLNoWildcard(secondary_url));
}

// Tests if changing a settings in incognito mode does not affects the regular
// mode.
TEST_F(HostContentSettingsMapTest, IncognitoChangesDoNotPersist) {
  TestingProfile profile;
  auto* regular_map = HostContentSettingsMapFactory::GetForProfile(&profile);
  auto* incognito_map = HostContentSettingsMapFactory::GetForProfile(
      profile.GetOffTheRecordProfile());
  auto* registry = content_settings::WebsiteSettingsRegistry::GetInstance();
  auto* content_setting_registry =
      content_settings::ContentSettingsRegistry::GetInstance();

  GURL url("https://example.com");
  ContentSettingsPattern pattern = ContentSettingsPattern::FromURL(url);
  content_settings::SettingInfo setting_info;

  for (const content_settings::WebsiteSettingsInfo* info : *registry) {
    SCOPED_TRACE(info->name());

    // Get regular profile default value.
    std::unique_ptr<base::Value> original_value =
        regular_map->GetWebsiteSetting(url, url, info->type(), std::string(),
                                       &setting_info);
    // Get a different valid value for incognito mode.
    std::unique_ptr<base::Value> new_value;
    if (content_setting_registry->Get(info->type())) {
      // If no original value is available, the settings does not have any valid
      // values and no more steps are required.
      if (!original_value)
        continue;
      int current_value;
      original_value->GetAsInteger(&current_value);

      for (int another_value = 0;
           another_value < ContentSetting::CONTENT_SETTING_NUM_SETTINGS;
           another_value++) {
        if (another_value != current_value &&
            content_setting_registry->Get(info->type())
                ->IsSettingValid(static_cast<ContentSetting>(another_value))) {
          new_value = std::make_unique<base::Value>(another_value);
          break;
        }
      }
      ASSERT_TRUE(new_value)
          << "Every content setting should have at least two values.";
    } else {
      new_value = std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
      static_cast<base::DictionaryValue*>(new_value.get())
          ->SetPath({"foo", "bar"}, base::Value(0));
    }
    // Ensure a different value is received (|original_value| can be null for
    // website settings).
    DCHECK(!original_value || *original_value != *new_value);

    // Set the different value in incognito mode.
    base::Value incognito_value = new_value->Clone();
    incognito_map->SetWebsiteSettingCustomScope(
        pattern, pattern, info->type(), std::string(), std::move(new_value));

    // Ensure incognito mode value is changed.
    EXPECT_EQ(incognito_value,
              *incognito_map->GetWebsiteSetting(url, url, info->type(),
                                                std::string(), &setting_info));

    // Ensure regular mode value is not changed.
    std::unique_ptr<base::Value> regular_mode_value =
        regular_map->GetWebsiteSetting(url, url, info->type(), std::string(),
                                       &setting_info);
    if (regular_mode_value) {
      ASSERT_TRUE(original_value);
      EXPECT_EQ(*original_value, *regular_mode_value);
    } else {
      EXPECT_FALSE(original_value);
    }
  }
}
