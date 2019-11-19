// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_channels_provider_android.h"

#include <map>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "chrome/browser/content_settings/content_settings_mock_observer.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/content_settings_pref.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;

namespace {
const char kTestOrigin[] = "https://example.com";
}  // namespace

class FakeNotificationChannelsBridge
    : public NotificationChannelsProviderAndroid::NotificationChannelsBridge {
 public:
  explicit FakeNotificationChannelsBridge(bool should_use_channels) {
    should_use_channels_ = should_use_channels;
  }

  ~FakeNotificationChannelsBridge() override = default;

  void SetChannelStatus(const std::string& origin,
                        NotificationChannelStatus status) {
    DCHECK_NE(NotificationChannelStatus::UNAVAILABLE, status);
    auto it = std::find_if(
        channels_.begin(), channels_.end(),
        [&origin](const std::pair<std::string, NotificationChannel>& pair) {
          return pair.second.origin == origin;
        });
    DCHECK(it != channels_.end())
        << "Must call bridge.CreateChannel before SetChannelStatus.";
    it->second.status = status;
  }

  // NotificationChannelsBridge methods.

  bool ShouldUseChannelSettings() override { return should_use_channels_; }

  NotificationChannel CreateChannel(const std::string& origin,
                                    const base::Time& timestamp,
                                    bool enabled) override {
    std::string channel_id =
        origin + base::NumberToString(timestamp.ToInternalValue());
    // Note if a channel with this channel ID was already created, this is a
    // no-op. This is intentional, to match the Android Channels API.
    NotificationChannel channel =
        NotificationChannel(channel_id, origin, timestamp,
                            enabled ? NotificationChannelStatus::ENABLED
                                    : NotificationChannelStatus::BLOCKED);
    channels_.emplace(channel_id, channel);
    return channel;
  }

  NotificationChannelStatus GetChannelStatus(
      const std::string& channel_id) override {
    auto entry = channels_.find(channel_id);
    if (entry != channels_.end())
      return entry->second.status;
    return NotificationChannelStatus::UNAVAILABLE;
  }

  void DeleteChannel(const std::string& channel_id) override {
    channels_.erase(channel_id);
  }

  std::vector<NotificationChannel> GetChannels() override {
    std::vector<NotificationChannel> channels;
    for (auto it = channels_.begin(); it != channels_.end(); it++)
      channels.push_back(it->second);
    return channels;
  }

 private:
  bool should_use_channels_;

  // Map from channel_id - channel.
  std::map<std::string, NotificationChannel> channels_;

  DISALLOW_COPY_AND_ASSIGN(FakeNotificationChannelsBridge);
};

class NotificationChannelsProviderAndroidTest : public testing::Test {
 public:
  NotificationChannelsProviderAndroidTest() {
    profile_ = std::make_unique<TestingProfile>();
    // Creating a test profile creates an (inaccessible) NCPA and migrates
    // (zero) channels, setting the 'migrated' pref to true in the process, so
    // we must first reset it to false before we reuse prefs for the instance
    // under test, in the MigrateToChannels* tests.
    // The same goes for the 'cleared blocked' pref and the ClearBlocked* tests.
    // TODO(crbug.com/700377): This shouldn't be necessary once NCPA is split
    // into a BrowserKeyedService and a class just containing the logic.
    profile_->GetPrefs()->SetBoolean(prefs::kMigratedToSiteNotificationChannels,
                                     false);
    profile_->GetPrefs()->SetBoolean(
        prefs::kClearedBlockedSiteNotificationChannels, false);
  }
  ~NotificationChannelsProviderAndroidTest() override {
    channels_provider_->ShutdownOnUIThread();
  }

 protected:
  void InitChannelsProvider(bool should_use_channels) {
    InitChannelsProviderWithClock(should_use_channels,
                                  std::make_unique<base::DefaultClock>());
  }

  void InitChannelsProviderWithClock(bool should_use_channels,
                                     std::unique_ptr<base::Clock> clock) {
    fake_bridge_ = new FakeNotificationChannelsBridge(should_use_channels);

    // Can't use std::make_unique because the provider's constructor is private.
    channels_provider_ =
        base::WrapUnique(new NotificationChannelsProviderAndroid(
            base::WrapUnique(fake_bridge_), std::move(clock)));
  }

  ContentSettingsPattern GetTestPattern() {
    return ContentSettingsPattern::FromURLNoWildcard(GURL(kTestOrigin));
  }

  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingProfile> profile_;

  std::unique_ptr<NotificationChannelsProviderAndroid> channels_provider_;

  // No leak because ownership is passed to channels_provider_ in constructor.
  FakeNotificationChannelsBridge* fake_bridge_;
};

TEST_F(NotificationChannelsProviderAndroidTest,
       SetWebsiteSettingWhenChannelsShouldNotBeUsed_ReturnsFalse) {
  this->InitChannelsProvider(false /* should_use_channels */);
  bool result = channels_provider_->SetWebsiteSetting(
      GetTestPattern(), ContentSettingsPattern(),
      ContentSettingsType::NOTIFICATIONS, std::string(),
      std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));

  EXPECT_FALSE(result);
}

TEST_F(NotificationChannelsProviderAndroidTest,
       SetWebsiteSettingAllowedCreatesOneAllowedRule) {
  InitChannelsProvider(true /* should_use_channels */);

  bool result = channels_provider_->SetWebsiteSetting(
      GetTestPattern(), ContentSettingsPattern(),
      ContentSettingsType::NOTIFICATIONS, std::string(),
      std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));
  EXPECT_TRUE(result);

  std::unique_ptr<content_settings::RuleIterator> rule_iterator =
      channels_provider_->GetRuleIterator(ContentSettingsType::NOTIFICATIONS,
                                          std::string(), false /* incognito */);
  EXPECT_TRUE(rule_iterator->HasNext());
  content_settings::Rule rule = rule_iterator->Next();
  EXPECT_EQ(GetTestPattern(), rule.primary_pattern);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            content_settings::ValueToContentSetting(&rule.value));
  EXPECT_FALSE(rule_iterator->HasNext());
}

TEST_F(NotificationChannelsProviderAndroidTest,
       SetWebsiteSettingBlockedCreatesOneBlockedRule) {
  InitChannelsProvider(true /* should_use_channels */);

  bool result = channels_provider_->SetWebsiteSetting(
      GetTestPattern(), ContentSettingsPattern(),
      ContentSettingsType::NOTIFICATIONS, std::string(),
      std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));

  EXPECT_TRUE(result);
  std::unique_ptr<content_settings::RuleIterator> rule_iterator =
      channels_provider_->GetRuleIterator(ContentSettingsType::NOTIFICATIONS,
                                          std::string(), false /* incognito */);
  EXPECT_TRUE(rule_iterator->HasNext());
  content_settings::Rule rule = rule_iterator->Next();
  EXPECT_EQ(GetTestPattern(), rule.primary_pattern);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            content_settings::ValueToContentSetting(&rule.value));
  EXPECT_FALSE(rule_iterator->HasNext());
}

TEST_F(NotificationChannelsProviderAndroidTest,
       SetWebsiteSettingAllowedTwiceForSameOriginCreatesOneAllowedRule) {
  InitChannelsProvider(true /* should_use_channels */);

  channels_provider_->SetWebsiteSetting(
      GetTestPattern(), ContentSettingsPattern(),
      ContentSettingsType::NOTIFICATIONS, std::string(),
      std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));
  bool result = channels_provider_->SetWebsiteSetting(
      GetTestPattern(), ContentSettingsPattern(),
      ContentSettingsType::NOTIFICATIONS, std::string(),
      std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));

  EXPECT_TRUE(result);
  std::unique_ptr<content_settings::RuleIterator> rule_iterator =
      channels_provider_->GetRuleIterator(ContentSettingsType::NOTIFICATIONS,
                                          std::string(), false /* incognito */);
  EXPECT_TRUE(rule_iterator->HasNext());
  content_settings::Rule rule = rule_iterator->Next();
  EXPECT_EQ(GetTestPattern(), rule.primary_pattern);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            content_settings::ValueToContentSetting(&rule.value));
  EXPECT_FALSE(rule_iterator->HasNext());
}

TEST_F(NotificationChannelsProviderAndroidTest,
       SetWebsiteSettingBlockedTwiceForSameOriginCreatesOneBlockedRule) {
  InitChannelsProvider(true /* should_use_channels */);

  channels_provider_->SetWebsiteSetting(
      GetTestPattern(), ContentSettingsPattern(),
      ContentSettingsType::NOTIFICATIONS, std::string(),
      std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  bool result = channels_provider_->SetWebsiteSetting(
      GetTestPattern(), ContentSettingsPattern(),
      ContentSettingsType::NOTIFICATIONS, std::string(),
      std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));

  EXPECT_TRUE(result);
  std::unique_ptr<content_settings::RuleIterator> rule_iterator =
      channels_provider_->GetRuleIterator(ContentSettingsType::NOTIFICATIONS,
                                          std::string(), false /* incognito */);
  EXPECT_TRUE(rule_iterator->HasNext());
  content_settings::Rule rule = rule_iterator->Next();
  EXPECT_EQ(GetTestPattern(), rule.primary_pattern);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            content_settings::ValueToContentSetting(&rule.value));
  EXPECT_FALSE(rule_iterator->HasNext());
}

TEST_F(NotificationChannelsProviderAndroidTest,
       SetWebsiteSettingDefault_DeletesRule) {
  InitChannelsProvider(true /* should_use_channels */);
  channels_provider_->SetWebsiteSetting(
      GetTestPattern(), ContentSettingsPattern(),
      ContentSettingsType::NOTIFICATIONS, std::string(),
      std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));

  bool result = channels_provider_->SetWebsiteSetting(
      GetTestPattern(), ContentSettingsPattern(),
      ContentSettingsType::NOTIFICATIONS, std::string(), nullptr);

  EXPECT_FALSE(result)
      << "SetWebsiteSetting should return false when passed a null value.";
  EXPECT_FALSE(channels_provider_->GetRuleIterator(
      ContentSettingsType::NOTIFICATIONS, std::string(),
      false /* incognito */));
}

TEST_F(NotificationChannelsProviderAndroidTest,
       NoRulesWhenChannelsShouldNotBeUsed) {
  InitChannelsProvider(false /* should_use_channels */);
  EXPECT_FALSE(channels_provider_->GetRuleIterator(
      ContentSettingsType::NOTIFICATIONS, std::string(),
      false /* incognito */));
}

TEST_F(NotificationChannelsProviderAndroidTest, NoRulesInIncognito) {
  InitChannelsProvider(true /* should_use_channels */);
  channels_provider_->SetWebsiteSetting(
      GetTestPattern(), ContentSettingsPattern(),
      ContentSettingsType::NOTIFICATIONS, std::string(),
      std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));
  EXPECT_FALSE(channels_provider_->GetRuleIterator(
      ContentSettingsType::NOTIFICATIONS, std::string(), true /* incognito */));
}

TEST_F(NotificationChannelsProviderAndroidTest,
       NoRulesWhenNoWebsiteSettingsSet) {
  InitChannelsProvider(true /* should_use_channels */);
  EXPECT_FALSE(channels_provider_->GetRuleIterator(
      ContentSettingsType::NOTIFICATIONS, std::string(),
      false /* incognito */));
}

TEST_F(NotificationChannelsProviderAndroidTest,
       SetWebsiteSettingForMultipleOriginsCreatesMultipleRules) {
  InitChannelsProvider(true /* should_use_channels */);
  ContentSettingsPattern abc_pattern =
      ContentSettingsPattern::FromURLNoWildcard(GURL("https://abc.com"));
  ContentSettingsPattern xyz_pattern =
      ContentSettingsPattern::FromURLNoWildcard(GURL("https://xyz.com"));
  channels_provider_->SetWebsiteSetting(
      abc_pattern, ContentSettingsPattern(), ContentSettingsType::NOTIFICATIONS,
      std::string(), std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));
  channels_provider_->SetWebsiteSetting(
      xyz_pattern, ContentSettingsPattern(), ContentSettingsType::NOTIFICATIONS,
      std::string(), std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));

  std::unique_ptr<content_settings::RuleIterator> rule_iterator =
      channels_provider_->GetRuleIterator(ContentSettingsType::NOTIFICATIONS,
                                          std::string(), false /* incognito */);
  EXPECT_TRUE(rule_iterator->HasNext());
  content_settings::Rule first_rule = rule_iterator->Next();
  EXPECT_EQ(abc_pattern, first_rule.primary_pattern);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            content_settings::ValueToContentSetting(&first_rule.value));
  EXPECT_TRUE(rule_iterator->HasNext());
  content_settings::Rule second_rule = rule_iterator->Next();
  EXPECT_EQ(xyz_pattern, second_rule.primary_pattern);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            content_settings::ValueToContentSetting(&second_rule.value));
  EXPECT_FALSE(rule_iterator->HasNext());
}

TEST_F(NotificationChannelsProviderAndroidTest,
       NotifiesObserversOnChannelStatusChanges) {
  InitChannelsProvider(true /* should_use_channels */);
  content_settings::MockObserver mock_observer;
  channels_provider_->AddObserver(&mock_observer);

  // Create channel as enabled initially - this should notify the mock observer.
  EXPECT_CALL(mock_observer, OnContentSettingChanged(
                                 _, _, ContentSettingsType::NOTIFICATIONS, ""));
  channels_provider_->SetWebsiteSetting(
      ContentSettingsPattern::FromString("https://example.com"),
      ContentSettingsPattern(), ContentSettingsType::NOTIFICATIONS,
      std::string(), std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));
  content::RunAllTasksUntilIdle();

  // Emulate user blocking the channel.
  fake_bridge_->SetChannelStatus("https://example.com",
                                 NotificationChannelStatus::BLOCKED);

  // Observer should be notified on first invocation of GetRuleIterator.
  EXPECT_CALL(mock_observer, OnContentSettingChanged(
                                 _, _, ContentSettingsType::NOTIFICATIONS, ""));
  channels_provider_->GetRuleIterator(ContentSettingsType::NOTIFICATIONS,
                                      std::string(), false /* incognito */);
  content::RunAllTasksUntilIdle();

  // Observer should not be notified the second time.
  channels_provider_->GetRuleIterator(ContentSettingsType::NOTIFICATIONS,
                                      std::string(), false /* incognito */);
  content::RunAllTasksUntilIdle();
}

TEST_F(NotificationChannelsProviderAndroidTest,
       ClearAllContentSettingsRulesClearsRulesAndNotifiesObservers) {
  InitChannelsProvider(true /* should_use_channels */);
  content_settings::MockObserver mock_observer;
  channels_provider_->AddObserver(&mock_observer);

  // Set up some channels.
  ContentSettingsPattern abc_pattern =
      ContentSettingsPattern::FromURLNoWildcard(GURL("https://abc.com"));
  ContentSettingsPattern xyz_pattern =
      ContentSettingsPattern::FromURLNoWildcard(GURL("https://xyz.com"));
  channels_provider_->SetWebsiteSetting(
      abc_pattern, ContentSettingsPattern(), ContentSettingsType::NOTIFICATIONS,
      std::string(), std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));
  channels_provider_->SetWebsiteSetting(
      xyz_pattern, ContentSettingsPattern(), ContentSettingsType::NOTIFICATIONS,
      std::string(), std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));

  EXPECT_NE(base::Time(),
            channels_provider_->GetWebsiteSettingLastModified(
                abc_pattern, ContentSettingsPattern(),
                ContentSettingsType::NOTIFICATIONS, std::string()));

  EXPECT_CALL(mock_observer,
              OnContentSettingChanged(
                  ContentSettingsPattern(), ContentSettingsPattern(),
                  ContentSettingsType::NOTIFICATIONS, std::string()));

  channels_provider_->ClearAllContentSettingsRules(
      ContentSettingsType::NOTIFICATIONS);

  // Ensure cached data is erased.
  EXPECT_EQ(base::Time(),
            channels_provider_->GetWebsiteSettingLastModified(
                abc_pattern, ContentSettingsPattern(),
                ContentSettingsType::NOTIFICATIONS, std::string()));

  // Check no rules are returned.
  EXPECT_FALSE(channels_provider_->GetRuleIterator(
      ContentSettingsType::NOTIFICATIONS, std::string(),
      false /* incognito */));
}

TEST_F(NotificationChannelsProviderAndroidTest,
       ClearAllContentSettingsRulesNoopsForUnrelatedContentSettings) {
  InitChannelsProvider(true /* should_use_channels */);

  // Set up some channels.
  ContentSettingsPattern abc_pattern =
      ContentSettingsPattern::FromURLNoWildcard(GURL("https://abc.com"));
  ContentSettingsPattern xyz_pattern =
      ContentSettingsPattern::FromURLNoWildcard(GURL("https://xyz.com"));
  channels_provider_->SetWebsiteSetting(
      abc_pattern, ContentSettingsPattern(), ContentSettingsType::NOTIFICATIONS,
      std::string(), std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));
  channels_provider_->SetWebsiteSetting(
      xyz_pattern, ContentSettingsPattern(), ContentSettingsType::NOTIFICATIONS,
      std::string(), std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));

  channels_provider_->ClearAllContentSettingsRules(
      ContentSettingsType::COOKIES);
  channels_provider_->ClearAllContentSettingsRules(
      ContentSettingsType::JAVASCRIPT);
  channels_provider_->ClearAllContentSettingsRules(
      ContentSettingsType::GEOLOCATION);

  // Check two rules are still returned.
  std::unique_ptr<content_settings::RuleIterator> rule_iterator =
      channels_provider_->GetRuleIterator(ContentSettingsType::NOTIFICATIONS,
                                          std::string(), false /* incognito */);
  EXPECT_TRUE(rule_iterator->HasNext());
  rule_iterator->Next();
  EXPECT_TRUE(rule_iterator->HasNext());
  rule_iterator->Next();
  EXPECT_FALSE(rule_iterator->HasNext());
}

TEST_F(NotificationChannelsProviderAndroidTest,
       GetWebsiteSettingLastModifiedReturnsNullIfNoModifications) {
  InitChannelsProvider(true /* should_use_channels */);

  auto result = channels_provider_->GetWebsiteSettingLastModified(
      GetTestPattern(), ContentSettingsPattern(),
      ContentSettingsType::NOTIFICATIONS, std::string());

  EXPECT_TRUE(result.is_null());
}

TEST_F(NotificationChannelsProviderAndroidTest,
       GetWebsiteSettingLastModifiedForOtherSettingsReturnsNull) {
  InitChannelsProvider(true /* should_use_channels */);

  channels_provider_->SetWebsiteSetting(
      GetTestPattern(), ContentSettingsPattern(),
      ContentSettingsType::NOTIFICATIONS, std::string(),
      std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));

  auto result = channels_provider_->GetWebsiteSettingLastModified(
      GetTestPattern(), ContentSettingsPattern(),
      ContentSettingsType::GEOLOCATION, std::string());

  EXPECT_TRUE(result.is_null());

  result = channels_provider_->GetWebsiteSettingLastModified(
      GetTestPattern(), ContentSettingsPattern(), ContentSettingsType::COOKIES,
      std::string());

  EXPECT_TRUE(result.is_null());
}

TEST_F(NotificationChannelsProviderAndroidTest,
       GetWebsiteSettingLastModifiedReturnsMostRecentTimestamp) {
  auto test_clock = std::make_unique<base::SimpleTestClock>();
  base::Time t1 = base::Time::Now();
  test_clock->SetNow(t1);
  base::SimpleTestClock* clock = test_clock.get();
  InitChannelsProviderWithClock(true /* should_use_channels */,
                                std::move(test_clock));

  // Create channel and check last-modified time is the creation time.
  std::string first_origin = "https://example.com";
  channels_provider_->SetWebsiteSetting(
      ContentSettingsPattern::FromString(first_origin),
      ContentSettingsPattern(), ContentSettingsType::NOTIFICATIONS,
      std::string(), std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));
  clock->Advance(base::TimeDelta::FromSeconds(1));

  base::Time last_modified = channels_provider_->GetWebsiteSettingLastModified(
      ContentSettingsPattern::FromString(first_origin),
      ContentSettingsPattern(), ContentSettingsType::NOTIFICATIONS,
      std::string());
  EXPECT_EQ(last_modified, t1);

  // Delete and recreate the same channel after some time has passed.
  // This simulates the user clearing data and regranting permisison.
  clock->Advance(base::TimeDelta::FromSeconds(3));
  base::Time t2 = clock->Now();
  channels_provider_->SetWebsiteSetting(
      ContentSettingsPattern::FromString(first_origin),
      ContentSettingsPattern(), ContentSettingsType::NOTIFICATIONS,
      std::string(), nullptr);
  channels_provider_->SetWebsiteSetting(
      ContentSettingsPattern::FromString(first_origin),
      ContentSettingsPattern(), ContentSettingsType::NOTIFICATIONS,
      std::string(), std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));

  // Last modified time should be updated.
  last_modified = channels_provider_->GetWebsiteSettingLastModified(
      ContentSettingsPattern::FromString(first_origin),
      ContentSettingsPattern(), ContentSettingsType::NOTIFICATIONS,
      std::string());
  EXPECT_EQ(last_modified, t2);

  // Create an unrelated channel after some more time has passed.
  clock->Advance(base::TimeDelta::FromSeconds(5));
  std::string second_origin = "https://other.com";
  channels_provider_->SetWebsiteSetting(
      ContentSettingsPattern::FromString(second_origin),
      ContentSettingsPattern(), ContentSettingsType::NOTIFICATIONS,
      std::string(), std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));

  // Expect first origin's last-modified time to be unchanged.
  last_modified = channels_provider_->GetWebsiteSettingLastModified(
      ContentSettingsPattern::FromString(first_origin),
      ContentSettingsPattern(), ContentSettingsType::NOTIFICATIONS,
      std::string());
  EXPECT_EQ(last_modified, t2);
}

TEST_F(NotificationChannelsProviderAndroidTest,
       MigrateToChannels_NoopWhenNoNotificationSettingsToMigrate) {
  InitChannelsProvider(true /* should_use_channels */);
  auto old_provider = std::make_unique<content_settings::MockProvider>();
  old_provider->SetWebsiteSetting(
      ContentSettingsPattern::FromString("https://blocked.com"),
      ContentSettingsPattern::Wildcard(), ContentSettingsType::COOKIES,
      std::string(), std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));

  channels_provider_->MigrateToChannelsIfNecessary(profile_->GetPrefs(),
                                                   old_provider.get());
  EXPECT_EQ(fake_bridge_->GetChannels().size(), 0u);
}

TEST_F(NotificationChannelsProviderAndroidTest,
       MigrateToChannels_NoopWhenChannelsShouldNotBeUsed) {
  InitChannelsProvider(false /* should_use_channels */);
  auto old_provider = std::make_unique<content_settings::MockProvider>();

  // Give the old provider some notification settings to provide.
  old_provider->SetWebsiteSetting(
      ContentSettingsPattern::FromString("https://blocked.com"),
      ContentSettingsPattern::Wildcard(), ContentSettingsType::NOTIFICATIONS,
      std::string(), std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  old_provider->SetWebsiteSetting(
      ContentSettingsPattern::FromString("https://allowed.com"),
      ContentSettingsPattern::Wildcard(), ContentSettingsType::NOTIFICATIONS,
      std::string(), std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));

  channels_provider_->MigrateToChannelsIfNecessary(profile_->GetPrefs(),
                                                   old_provider.get());
  EXPECT_EQ(fake_bridge_->GetChannels().size(), 0u);
}

TEST_F(NotificationChannelsProviderAndroidTest,
       MigrateToChannels_CreatesChannelsForProvidedSettings) {
  InitChannelsProvider(true /* should_use_channels */);
  auto old_provider = std::make_unique<content_settings::MockProvider>();

  // Give the old provider some notification settings to provide.
  old_provider->SetWebsiteSetting(
      ContentSettingsPattern::FromString("https://blocked.com"),
      ContentSettingsPattern::Wildcard(), ContentSettingsType::NOTIFICATIONS,
      std::string(), std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  old_provider->SetWebsiteSetting(
      ContentSettingsPattern::FromString("https://allowed.com"),
      ContentSettingsPattern::Wildcard(), ContentSettingsType::NOTIFICATIONS,
      std::string(), std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));

  channels_provider_->MigrateToChannelsIfNecessary(profile_->GetPrefs(),
                                                   old_provider.get());

  auto channels = fake_bridge_->GetChannels();
  ASSERT_EQ(channels.size(), 2u);
  bool checked_allowed = false;
  bool checked_blocked = false;
  for (size_t i = 0; i < 2; ++i) {
    const NotificationChannel& channel = channels[i];
    if (channel.origin == "https://allowed.com") {
      ASSERT_FALSE(checked_allowed);
      EXPECT_EQ(channel.status, NotificationChannelStatus::ENABLED);
      checked_allowed = true;
    } else {
      ASSERT_FALSE(checked_blocked);
      ASSERT_EQ(channel.origin, "https://blocked.com");
      EXPECT_EQ(channel.status, NotificationChannelStatus::BLOCKED);
      checked_blocked = true;
    }
  }
  EXPECT_FALSE(old_provider->GetRuleIterator(ContentSettingsType::NOTIFICATIONS,
                                             std::string(),
                                             false /* incognito */));
}

TEST_F(NotificationChannelsProviderAndroidTest,
       MigrateToChannels_DoesNotMigrateIfAlreadyMigrated) {
  InitChannelsProvider(true /* should_use_channels */);
  auto old_provider = std::make_unique<content_settings::MockProvider>();
  profile_->GetPrefs()->SetBoolean(prefs::kMigratedToSiteNotificationChannels,
                                   true);
  old_provider->SetWebsiteSetting(
      ContentSettingsPattern::FromString("https://blocked.com"),
      ContentSettingsPattern::Wildcard(), ContentSettingsType::NOTIFICATIONS,
      std::string(), std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));

  channels_provider_->MigrateToChannelsIfNecessary(profile_->GetPrefs(),
                                                   old_provider.get());
  EXPECT_EQ(fake_bridge_->GetChannels().size(), 0u);
}

TEST_F(NotificationChannelsProviderAndroidTest,
       ClearBlockedChannels_ZeroBlockedChannels) {
  InitChannelsProvider(true /* should_use_channels */);
  fake_bridge_->CreateChannel("https://example.com", base::Time::Now(),
                              true /* enabled */);

  ASSERT_FALSE(profile_->GetPrefs()->GetBoolean(
      prefs::kClearedBlockedSiteNotificationChannels));
  ASSERT_EQ(fake_bridge_->GetChannels().size(), 1u);

  channels_provider_->ClearBlockedChannelsIfNecessary(
      profile_->GetPrefs(), nullptr /* template_url_service */);

  EXPECT_EQ(fake_bridge_->GetChannels().size(), 1u);

  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(
      prefs::kClearedBlockedSiteNotificationChannels));
}

TEST_F(NotificationChannelsProviderAndroidTest,
       ClearBlockedChannels_MultipleBlockedChannels) {
  InitChannelsProvider(true /* should_use_channels */);

  fake_bridge_->CreateChannel("https://example.com", base::Time::Now(),
                              false /* enabled */);
  fake_bridge_->CreateChannel("https://chromium.org", base::Time::Now(),
                              false /* enabled */);
  fake_bridge_->CreateChannel("https://foo.com", base::Time::Now(),
                              true /* enabled */);

  ASSERT_FALSE(profile_->GetPrefs()->GetBoolean(
      prefs::kClearedBlockedSiteNotificationChannels));
  ASSERT_EQ(fake_bridge_->GetChannels().size(), 3u);

  channels_provider_->ClearBlockedChannelsIfNecessary(
      profile_->GetPrefs(), nullptr /* template_url_service */);

  EXPECT_EQ(fake_bridge_->GetChannels().size(), 1u);
  EXPECT_EQ(fake_bridge_->GetChannels()[0].origin, "https://foo.com");

  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(
      prefs::kClearedBlockedSiteNotificationChannels));

  // Create another blocked channel and check ClearBlocked is now a no-op.

  fake_bridge_->CreateChannel("https://example.com", base::Time::Now(),
                              false /* enabled */);

  ASSERT_EQ(fake_bridge_->GetChannels().size(), 2u);

  channels_provider_->ClearBlockedChannelsIfNecessary(
      profile_->GetPrefs(), nullptr /* template_url_service */);

  EXPECT_EQ(fake_bridge_->GetChannels().size(), 2u);
}

TEST_F(NotificationChannelsProviderAndroidTest,
       ClearBlockedChannels_DefaultSearchEngineIsNotCleared) {
  InitChannelsProvider(true /* should_use_channels */);

  // Set up TemplateURLService with a default search engine.
  TemplateURLService* template_url_service = new TemplateURLService(NULL, 0);
  TemplateURLData data;
  data.SetURL("https://default-search-engine.com/url?bar={searchTerms}");
  TemplateURL* template_url =
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  template_url_service->SetUserSelectedDefaultSearchProvider(template_url);

  // Block the DSE and another channel.
  fake_bridge_->CreateChannel("https://default-search-engine.com",
                              base::Time::Now(), false /* enabled */);
  fake_bridge_->CreateChannel("https://example.com", base::Time::Now(),
                              false /* enabled */);

  ASSERT_FALSE(profile_->GetPrefs()->GetBoolean(
      prefs::kClearedBlockedSiteNotificationChannels));
  ASSERT_EQ(fake_bridge_->GetChannels().size(), 2u);

  channels_provider_->ClearBlockedChannelsIfNecessary(profile_->GetPrefs(),
                                                      template_url_service);

  // DSE channel should still exist and be blocked..
  EXPECT_EQ(fake_bridge_->GetChannels().size(), 1u);
  EXPECT_EQ(fake_bridge_->GetChannels()[0].origin,
            "https://default-search-engine.com");
  EXPECT_EQ(fake_bridge_->GetChannels()[0].status,
            NotificationChannelStatus::BLOCKED);

  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(
      prefs::kClearedBlockedSiteNotificationChannels));
}
