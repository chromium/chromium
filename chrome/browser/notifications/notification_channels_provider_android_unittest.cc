// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_channels_provider_android.h"

#include <map>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_future.h"
#include "base/time/clock.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/content_settings_mock_observer.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/content_settings_provider.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_thread.h"
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
  FakeNotificationChannelsBridge() = default;

  FakeNotificationChannelsBridge(const FakeNotificationChannelsBridge&) =
      delete;
  FakeNotificationChannelsBridge& operator=(
      const FakeNotificationChannelsBridge&) = delete;
  ~FakeNotificationChannelsBridge() override = default;

  void SetChannelStatus(const std::string& origin,
                        NotificationChannelStatus status) {
    DCHECK_NE(NotificationChannelStatus::UNAVAILABLE, status);
    auto it = base::ranges::find(
        channels_, origin,
        [](const Channels::value_type& pair) { return pair.second.origin; });
    CHECK(it != channels_.end(), base::NotFatalUntil::M130)
        << "Must call bridge.CreateChannel before SetChannelStatus.";
    it->second.status = status;
  }

  // NotificationChannelsBridge methods.
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

  void DeleteChannel(const std::string& channel_id) override {
    channels_.erase(channel_id);
  }

  void GetChannels(NotificationChannelsProviderAndroid::GetChannelsCallback
                       callback) override {
    std::vector<NotificationChannel> channels;
    for (auto it = channels_.begin(); it != channels_.end(); it++)
      channels.push_back(it->second);
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), channels));
  }

 private:
  using Channels = std::map<std::string, NotificationChannel>;

  // Map from channel_id - channel.
  Channels channels_;
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
    // TODO(crbug.com/40510194): This shouldn't be necessary once NCPA is split
    // into a BrowserKeyedService and a class just containing the logic.
    profile_->GetPrefs()->SetBoolean(prefs::kMigratedToSiteNotificationChannels,
                                     false);
    profile_->GetPrefs()->SetBoolean(
        prefs::kClearedBlockedSiteNotificationChannels, false);
  }
  ~NotificationChannelsProviderAndroidTest() override {
    channels_provider_->ShutdownOnUIThread();
  }

  void VerifyNumberOfChannels(int count) {
    base::test::TestFuture<const std::vector<NotificationChannel>&> future;
    fake_bridge_->GetChannels(future.GetCallback());
    const std::vector<NotificationChannel>& channels = future.Get();
    EXPECT_EQ(static_cast<size_t>(count), channels.size());
  }

  void MigrateToChannelsIfNecessary(
      content_settings::ProviderInterface* pref_provider) {
    channels_provider_->MigrateToChannelsIfNecessary(pref_provider);
  }

  void ClearBlockedChannelsIfNecessary(
      TemplateURLService* template_url_service) {
    channels_provider_->ClearBlockedChannelsIfNecessary(template_url_service);
  }

 protected:
  void InitChannelsProvider() {
    fake_bridge_ = new FakeNotificationChannelsBridge();

    // Can't use std::make_unique because the provider's constructor is private.
    channels_provider_ =
        base::WrapUnique(new NotificationChannelsProviderAndroid(
            profile_->GetPrefs(), base::WrapUnique(fake_bridge_.get())));
  }

  ContentSettingsPattern GetTestPattern() {
    return ContentSettingsPattern::FromURLNoWildcard(GURL(kTestOrigin));
  }

  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<NotificationChannelsProviderAndroid> channels_provider_;

  // No leak because ownership is passed to channels_provider_ in constructor.
  raw_ptr<FakeNotificationChannelsBridge> fake_bridge_;
};

TEST_F(NotificationChannelsProviderAndroidTest,
       SetWebsiteSettingAllowedCreatesOneAllowedRule) {
  InitChannelsProvider();

  bool result = channels_provider_->SetWebsiteSetting(
      GetTestPattern(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::NOTIFICATIONS, base::Value(CONTENT_SETTING_ALLOW),
      /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_TRUE(result);

  // Rule is not created immediately after SetWebsiteSetting().
  std::unique_ptr<content_settings::RuleIterator> rule_iterator =
      channels_provider_->GetRuleIterator(
          ContentSettingsType::NOTIFICATIONS, false /* incognito */,
          content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_FALSE(rule_iterator);

  // Wait for all async tasks to complete and check the new rules.
  content::RunAllTasksUntilIdle();
  rule_iterator = channels_provider_->GetRuleIterator(
      ContentSettingsType::NOTIFICATIONS, false /* incognito */,
      content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_TRUE(rule_iterator->HasNext());
  std::unique_ptr<content_settings::Rule> rule = rule_iterator->Next();
  EXPECT_EQ(GetTestPattern(), rule->primary_pattern);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            content_settings::ValueToContentSetting(rule->value));
  EXPECT_FALSE(rule_iterator->HasNext());
}

TEST_F(NotificationChannelsProviderAndroidTest,
       SetWebsiteSettingBlockedCreatesOneBlockedRule) {
  InitChannelsProvider();

  bool result = channels_provider_->SetWebsiteSetting(
      GetTestPattern(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::NOTIFICATIONS, base::Value(CONTENT_SETTING_BLOCK),
      /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_TRUE(result);
  // Rule is not created immediately after SetWebsiteSetting().
  std::unique_ptr<content_settings::RuleIterator> rule_iterator =
      channels_provider_->GetRuleIterator(
          ContentSettingsType::NOTIFICATIONS, false /* incognito */,
          content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_FALSE(rule_iterator);

  content::RunAllTasksUntilIdle();

  // Wait for all async tasks to complete and check the new rules.
  rule_iterator = channels_provider_->GetRuleIterator(
      ContentSettingsType::NOTIFICATIONS, false /* incognito */,
      content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_TRUE(rule_iterator->HasNext());
  std::unique_ptr<content_settings::Rule> rule = rule_iterator->Next();
  EXPECT_EQ(GetTestPattern(), rule->primary_pattern);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            content_settings::ValueToContentSetting(rule->value));
  EXPECT_FALSE(rule_iterator->HasNext());
}

TEST_F(NotificationChannelsProviderAndroidTest,
       SetWebsiteSettingAllowedTwiceForSameOriginCreatesOneAllowedRule) {
  InitChannelsProvider();

  channels_provider_->SetWebsiteSetting(
      GetTestPattern(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::NOTIFICATIONS, base::Value(CONTENT_SETTING_ALLOW),
      /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  bool result = channels_provider_->SetWebsiteSetting(
      GetTestPattern(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::NOTIFICATIONS, base::Value(CONTENT_SETTING_ALLOW),
      /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());

  EXPECT_TRUE(result);
  content::RunAllTasksUntilIdle();

  std::unique_ptr<content_settings::RuleIterator> rule_iterator =
      channels_provider_->GetRuleIterator(
          ContentSettingsType::NOTIFICATIONS, false /* incognito */,
          content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_TRUE(rule_iterator->HasNext());
  std::unique_ptr<content_settings::Rule> rule = rule_iterator->Next();
  EXPECT_EQ(GetTestPattern(), rule->primary_pattern);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            content_settings::ValueToContentSetting(rule->value));
  EXPECT_FALSE(rule_iterator->HasNext());
}

TEST_F(NotificationChannelsProviderAndroidTest,
       SetWebsiteSettingBlockedTwiceForSameOriginCreatesOneBlockedRule) {
  InitChannelsProvider();

  channels_provider_->SetWebsiteSetting(
      GetTestPattern(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::NOTIFICATIONS, base::Value(CONTENT_SETTING_BLOCK),
      /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  bool result = channels_provider_->SetWebsiteSetting(
      GetTestPattern(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::NOTIFICATIONS, base::Value(CONTENT_SETTING_BLOCK),
      /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());

  EXPECT_TRUE(result);
  content::RunAllTasksUntilIdle();

  std::unique_ptr<content_settings::RuleIterator> rule_iterator =
      channels_provider_->GetRuleIterator(
          ContentSettingsType::NOTIFICATIONS, false /* incognito */,
          content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_TRUE(rule_iterator->HasNext());
  std::unique_ptr<content_settings::Rule> rule = rule_iterator->Next();
  EXPECT_EQ(GetTestPattern(), rule->primary_pattern);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            content_settings::ValueToContentSetting(rule->value));
  EXPECT_FALSE(rule_iterator->HasNext());
}

TEST_F(NotificationChannelsProviderAndroidTest,
       SetWebsiteSettingDefault_DeletesRule) {
  InitChannelsProvider();
  channels_provider_->SetWebsiteSetting(
      GetTestPattern(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::NOTIFICATIONS, base::Value(CONTENT_SETTING_ALLOW),
      /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  content::RunAllTasksUntilIdle();
  std::unique_ptr<content_settings::RuleIterator> rule_iterator =
      channels_provider_->GetRuleIterator(
          ContentSettingsType::NOTIFICATIONS, false /* incognito */,
          content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_TRUE(rule_iterator->HasNext());
  std::unique_ptr<content_settings::Rule> rule = rule_iterator->Next();
  EXPECT_EQ(GetTestPattern(), rule->primary_pattern);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            content_settings::ValueToContentSetting(rule->value));
  EXPECT_FALSE(rule_iterator->HasNext());

  bool result = channels_provider_->SetWebsiteSetting(
      GetTestPattern(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::NOTIFICATIONS, base::Value(), /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_FALSE(result)
      << "SetWebsiteSetting should return false when passed a null value.";
  // Rule should still exist before the async task completes.
  rule_iterator = channels_provider_->GetRuleIterator(
      ContentSettingsType::NOTIFICATIONS, false /* incognito */,
      content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_TRUE(rule_iterator->HasNext());

  content::RunAllTasksUntilIdle();

  // Rule should now get deleted.
  EXPECT_FALSE(channels_provider_->GetRuleIterator(
      ContentSettingsType::NOTIFICATIONS, false /* incognito */,
      content_settings::PartitionKey::GetDefaultForTesting()));
}

TEST_F(NotificationChannelsProviderAndroidTest, NoRulesInIncognito) {
  InitChannelsProvider();
  channels_provider_->SetWebsiteSetting(
      GetTestPattern(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::NOTIFICATIONS, base::Value(CONTENT_SETTING_ALLOW),
      /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_FALSE(channels_provider_->GetRuleIterator(
      ContentSettingsType::NOTIFICATIONS, true /* incognito */,
      content_settings::PartitionKey::GetDefaultForTesting()));
}

TEST_F(NotificationChannelsProviderAndroidTest,
       NoRulesWhenNoWebsiteSettingsSet) {
  InitChannelsProvider();
  EXPECT_FALSE(channels_provider_->GetRuleIterator(
      ContentSettingsType::NOTIFICATIONS, false /* incognito */,
      content_settings::PartitionKey::GetDefaultForTesting()));
}

TEST_F(NotificationChannelsProviderAndroidTest,
       SetWebsiteSettingForMultipleOriginsCreatesMultipleRules) {
  InitChannelsProvider();
  ContentSettingsPattern abc_pattern =
      ContentSettingsPattern::FromURLNoWildcard(GURL("https://abc.com"));
  ContentSettingsPattern xyz_pattern =
      ContentSettingsPattern::FromURLNoWildcard(GURL("https://xyz.com"));
  channels_provider_->SetWebsiteSetting(
      abc_pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::NOTIFICATIONS, base::Value(CONTENT_SETTING_ALLOW),
      /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  channels_provider_->SetWebsiteSetting(
      xyz_pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::NOTIFICATIONS, base::Value(CONTENT_SETTING_BLOCK),
      /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  content::RunAllTasksUntilIdle();

  std::unique_ptr<content_settings::RuleIterator> rule_iterator =
      channels_provider_->GetRuleIterator(
          ContentSettingsType::NOTIFICATIONS, false /* incognito */,
          content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_TRUE(rule_iterator->HasNext());
  std::unique_ptr<content_settings::Rule> first_rule = rule_iterator->Next();
  EXPECT_EQ(abc_pattern, first_rule->primary_pattern);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            content_settings::ValueToContentSetting(first_rule->value));
  EXPECT_TRUE(rule_iterator->HasNext());
  std::unique_ptr<content_settings::Rule> second_rule = rule_iterator->Next();
  EXPECT_EQ(xyz_pattern, second_rule->primary_pattern);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            content_settings::ValueToContentSetting(second_rule->value));
  EXPECT_FALSE(rule_iterator->HasNext());
}

TEST_F(NotificationChannelsProviderAndroidTest,
       NotifiesObserversOnChannelStatusChanges) {
  InitChannelsProvider();
  content_settings::MockObserver mock_observer;
  channels_provider_->AddObserver(&mock_observer);

  // Create channel as enabled initially - this should notify the mock observer.
  EXPECT_CALL(mock_observer, OnContentSettingChanged(
                                 _, _, ContentSettingsType::NOTIFICATIONS));
  channels_provider_->SetWebsiteSetting(
      ContentSettingsPattern::FromString("https://example.com"),
      ContentSettingsPattern::Wildcard(), ContentSettingsType::NOTIFICATIONS,
      base::Value(CONTENT_SETTING_ALLOW), /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  content::RunAllTasksUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // Emulate user blocking the channel.
  fake_bridge_->SetChannelStatus("https://example.com",
                                 NotificationChannelStatus::BLOCKED);

  // Observer should be notified on invocation of GetRuleIterator.
  EXPECT_CALL(mock_observer,
              OnContentSettingChanged(_, _, ContentSettingsType::NOTIFICATIONS))
      .Times(1);
  channels_provider_->GetRuleIterator(
      ContentSettingsType::NOTIFICATIONS, false /* incognito */,
      content_settings::PartitionKey::GetDefaultForTesting());
  content::RunAllTasksUntilIdle();

  // Observer should be notified when a new website setting is added.
  EXPECT_CALL(mock_observer,
              OnContentSettingChanged(_, _, ContentSettingsType::NOTIFICATIONS))
      .Times(1);
  channels_provider_->SetWebsiteSetting(
      ContentSettingsPattern::FromString("https://abc.com"),
      ContentSettingsPattern::Wildcard(), ContentSettingsType::NOTIFICATIONS,
      base::Value(CONTENT_SETTING_ALLOW), /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  content::RunAllTasksUntilIdle();
}

TEST_F(NotificationChannelsProviderAndroidTest,
       ClearAllContentSettingsRulesClearsRulesAndNotifiesObservers) {
  InitChannelsProvider();
  content_settings::MockObserver mock_observer;
  channels_provider_->AddObserver(&mock_observer);

  // Set up some channels.
  GURL abc_url("https://abc.com");
  ContentSettingsPattern abc_pattern =
      ContentSettingsPattern::FromURLNoWildcard(abc_url);
  GURL xyz_url("https://xyz.com");
  ContentSettingsPattern xyz_pattern =
      ContentSettingsPattern::FromURLNoWildcard(xyz_url);
  channels_provider_->SetWebsiteSetting(
      abc_pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::NOTIFICATIONS, base::Value(CONTENT_SETTING_ALLOW),
      /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  channels_provider_->SetWebsiteSetting(
      xyz_pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::NOTIFICATIONS, base::Value(CONTENT_SETTING_BLOCK),
      /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  content::RunAllTasksUntilIdle();

  EXPECT_NE(base::Time(), content_settings::TestUtils::GetLastModified(
                              channels_provider_.get(), abc_url, abc_url,
                              ContentSettingsType::NOTIFICATIONS));

  EXPECT_CALL(mock_observer,
              OnContentSettingChanged(ContentSettingsPattern::Wildcard(),
                                      ContentSettingsPattern::Wildcard(),
                                      ContentSettingsType::NOTIFICATIONS));

  channels_provider_->ClearAllContentSettingsRules(
      ContentSettingsType::NOTIFICATIONS,
      content_settings::PartitionKey::GetDefaultForTesting());

  content::RunAllTasksUntilIdle();
  // Ensure cached data is erased.
  EXPECT_EQ(base::Time(), content_settings::TestUtils::GetLastModified(
                              channels_provider_.get(), abc_url, abc_url,
                              ContentSettingsType::NOTIFICATIONS));

  // Check no rules are returned.
  EXPECT_FALSE(channels_provider_->GetRuleIterator(
      ContentSettingsType::NOTIFICATIONS, false /* incognito */,
      content_settings::PartitionKey::GetDefaultForTesting()));
}

TEST_F(NotificationChannelsProviderAndroidTest,
       ClearAllContentSettingsRulesNoopsForUnrelatedContentSettings) {
  InitChannelsProvider();

  // Set up some channels.
  ContentSettingsPattern abc_pattern =
      ContentSettingsPattern::FromURLNoWildcard(GURL("https://abc.com"));
  ContentSettingsPattern xyz_pattern =
      ContentSettingsPattern::FromURLNoWildcard(GURL("https://xyz.com"));
  channels_provider_->SetWebsiteSetting(
      abc_pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::NOTIFICATIONS, base::Value(CONTENT_SETTING_ALLOW),
      /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  channels_provider_->SetWebsiteSetting(
      xyz_pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::NOTIFICATIONS, base::Value(CONTENT_SETTING_BLOCK),
      /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  content::RunAllTasksUntilIdle();

  channels_provider_->ClearAllContentSettingsRules(
      ContentSettingsType::COOKIES,
      content_settings::PartitionKey::GetDefaultForTesting());
  channels_provider_->ClearAllContentSettingsRules(
      ContentSettingsType::JAVASCRIPT,
      content_settings::PartitionKey::GetDefaultForTesting());
  channels_provider_->ClearAllContentSettingsRules(
      ContentSettingsType::GEOLOCATION,
      content_settings::PartitionKey::GetDefaultForTesting());
  content::RunAllTasksUntilIdle();

  // Check two rules are still returned.
  std::unique_ptr<content_settings::RuleIterator> rule_iterator =
      channels_provider_->GetRuleIterator(
          ContentSettingsType::NOTIFICATIONS, false /* incognito */,
          content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_TRUE(rule_iterator->HasNext());
  rule_iterator->Next();
  EXPECT_TRUE(rule_iterator->HasNext());
  rule_iterator->Next();
  EXPECT_FALSE(rule_iterator->HasNext());
}

TEST_F(NotificationChannelsProviderAndroidTest,
       GetWebsiteSettingLastModifiedReturnsNullIfNoModifications) {
  InitChannelsProvider();

  auto result = content_settings::TestUtils::GetLastModified(
      channels_provider_.get(), GURL(kTestOrigin), GURL(kTestOrigin),
      ContentSettingsType::NOTIFICATIONS);

  EXPECT_TRUE(result.is_null());
}

TEST_F(NotificationChannelsProviderAndroidTest,
       GetWebsiteSettingLastModifiedForOtherSettingsReturnsNull) {
  InitChannelsProvider();

  channels_provider_->SetWebsiteSetting(
      GetTestPattern(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::NOTIFICATIONS, base::Value(CONTENT_SETTING_ALLOW),
      /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());

  auto result = content_settings::TestUtils::GetLastModified(
      channels_provider_.get(), GURL(kTestOrigin), GURL(kTestOrigin),
      ContentSettingsType::GEOLOCATION);

  EXPECT_TRUE(result.is_null());

  result = content_settings::TestUtils::GetLastModified(
      channels_provider_.get(), GURL(kTestOrigin), GURL(kTestOrigin),
      ContentSettingsType::COOKIES);

  EXPECT_TRUE(result.is_null());
}

TEST_F(NotificationChannelsProviderAndroidTest,
       GetWebsiteSettingLastModifiedReturnsMostRecentTimestamp) {
  base::SimpleTestClock clock;
  base::Time t1 = base::Time::Now();
  clock.SetNow(t1);
  InitChannelsProvider();
  channels_provider_->SetClockForTesting(&clock);

  // Create channel and check last-modified time is the creation time.
  GURL first_origin("https://example.com");
  ContentSettingsPattern first_pattern =
      ContentSettingsPattern::FromString(first_origin.spec());
  channels_provider_->SetWebsiteSetting(
      first_pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::NOTIFICATIONS, base::Value(CONTENT_SETTING_ALLOW),
      /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  content::RunAllTasksUntilIdle();
  clock.Advance(base::Seconds(1));

  base::Time last_modified = content_settings::TestUtils::GetLastModified(
      channels_provider_.get(), first_origin, first_origin,
      ContentSettingsType::NOTIFICATIONS);
  EXPECT_EQ(last_modified, t1);

  // Delete and recreate the same channel after some time has passed.
  // This simulates the user clearing data and regranting permisison.
  clock.Advance(base::Seconds(3));
  base::Time t2 = clock.Now();
  channels_provider_->SetWebsiteSetting(
      first_pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::NOTIFICATIONS, base::Value(), /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  channels_provider_->SetWebsiteSetting(
      first_pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::NOTIFICATIONS, base::Value(CONTENT_SETTING_ALLOW),
      /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  content::RunAllTasksUntilIdle();

  // Last modified time should be updated.
  last_modified = content_settings::TestUtils::GetLastModified(
      channels_provider_.get(), first_origin, first_origin,
      ContentSettingsType::NOTIFICATIONS);
  EXPECT_EQ(last_modified, t2);

  // Create an unrelated channel after some more time has passed.
  clock.Advance(base::Seconds(5));
  std::string second_origin = "https://other.com";
  channels_provider_->SetWebsiteSetting(
      ContentSettingsPattern::FromString(second_origin),
      ContentSettingsPattern::Wildcard(), ContentSettingsType::NOTIFICATIONS,
      base::Value(CONTENT_SETTING_ALLOW), /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  content::RunAllTasksUntilIdle();

  // Expect first origin's last-modified time to be unchanged.
  last_modified = content_settings::TestUtils::GetLastModified(
      channels_provider_.get(), first_origin, first_origin,
      ContentSettingsType::NOTIFICATIONS);
  EXPECT_EQ(last_modified, t2);
}

TEST_F(NotificationChannelsProviderAndroidTest,
       MigrateToChannels_NoopWhenNoNotificationSettingsToMigrate) {
  InitChannelsProvider();
  auto old_provider = std::make_unique<content_settings::MockProvider>();
  old_provider->SetWebsiteSetting(
      ContentSettingsPattern::FromString("https://blocked.com"),
      ContentSettingsPattern::Wildcard(), ContentSettingsType::COOKIES,
      base::Value(CONTENT_SETTING_BLOCK), /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());

  MigrateToChannelsIfNecessary(old_provider.get());
  content::RunAllTasksUntilIdle();
}

TEST_F(NotificationChannelsProviderAndroidTest,
       MigrateToChannels_CreatesChannelsForProvidedSettings) {
  InitChannelsProvider();
  auto old_provider = std::make_unique<content_settings::MockProvider>();

  // Give the old provider some notification settings to provide.
  old_provider->SetWebsiteSetting(
      ContentSettingsPattern::FromString("https://blocked.com"),
      ContentSettingsPattern::Wildcard(), ContentSettingsType::NOTIFICATIONS,
      base::Value(CONTENT_SETTING_BLOCK), /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  old_provider->SetWebsiteSetting(
      ContentSettingsPattern::FromString("https://allowed.com"),
      ContentSettingsPattern::Wildcard(), ContentSettingsType::NOTIFICATIONS,
      base::Value(CONTENT_SETTING_ALLOW), /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());
  content::RunAllTasksUntilIdle();

  MigrateToChannelsIfNecessary(old_provider.get());
  content::RunAllTasksUntilIdle();
  base::RunLoop run_loop;
  fake_bridge_->GetChannels(base::BindOnce(
      [](base::RunLoop* run_loop,
         const std::vector<NotificationChannel>& channels) {
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
        run_loop->Quit();
      },
      &run_loop));
  run_loop.Run();
  EXPECT_FALSE(old_provider->GetRuleIterator(
      ContentSettingsType::NOTIFICATIONS,
      false /* incognito */,
      content_settings::PartitionKey::GetDefaultForTesting()));
}

TEST_F(NotificationChannelsProviderAndroidTest,
       MigrateToChannels_DoesNotMigrateIfAlreadyMigrated) {
  InitChannelsProvider();
  auto old_provider = std::make_unique<content_settings::MockProvider>();
  profile_->GetPrefs()->SetBoolean(prefs::kMigratedToSiteNotificationChannels,
                                   true);
  old_provider->SetWebsiteSetting(
      ContentSettingsPattern::FromString("https://blocked.com"),
      ContentSettingsPattern::Wildcard(), ContentSettingsType::NOTIFICATIONS,
      base::Value(CONTENT_SETTING_BLOCK), /*constraints=*/{},
      content_settings::PartitionKey::GetDefaultForTesting());

  MigrateToChannelsIfNecessary(old_provider.get());
  content::RunAllTasksUntilIdle();

  VerifyNumberOfChannels(0);
}

TEST_F(NotificationChannelsProviderAndroidTest,
       ClearBlockedChannels_ZeroBlockedChannels) {
  InitChannelsProvider();
  fake_bridge_->CreateChannel("https://example.com", base::Time::Now(),
                              true /* enabled */);

  ASSERT_FALSE(profile_->GetPrefs()->GetBoolean(
      prefs::kClearedBlockedSiteNotificationChannels));
  VerifyNumberOfChannels(1);

  ClearBlockedChannelsIfNecessary(nullptr /* template_url_service */);

  VerifyNumberOfChannels(1);

  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(
      prefs::kClearedBlockedSiteNotificationChannels));
}

TEST_F(NotificationChannelsProviderAndroidTest,
       ClearBlockedChannels_MultipleBlockedChannels) {
  InitChannelsProvider();

  fake_bridge_->CreateChannel("https://example.com", base::Time::Now(),
                              false /* enabled */);
  fake_bridge_->CreateChannel("https://chromium.org", base::Time::Now(),
                              false /* enabled */);
  fake_bridge_->CreateChannel("https://foo.com", base::Time::Now(),
                              true /* enabled */);

  ASSERT_FALSE(profile_->GetPrefs()->GetBoolean(
      prefs::kClearedBlockedSiteNotificationChannels));
  VerifyNumberOfChannels(3);

  ClearBlockedChannelsIfNecessary(nullptr /* template_url_service */);
  content::RunAllTasksUntilIdle();

  base::RunLoop run_loop;
  fake_bridge_->GetChannels(base::BindOnce(
      [](base::RunLoop* run_loop,
         const std::vector<NotificationChannel>& channels) {
        EXPECT_EQ(channels.size(), 1u);
        EXPECT_EQ(channels[0].origin, "https://foo.com");
        run_loop->Quit();
      },
      &run_loop));
  run_loop.Run();

  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(
      prefs::kClearedBlockedSiteNotificationChannels));

  // Create another blocked channel and check ClearBlocked is now a no-op.

  fake_bridge_->CreateChannel("https://example.com", base::Time::Now(),
                              false /* enabled */);
  VerifyNumberOfChannels(2);

  ClearBlockedChannelsIfNecessary(nullptr /* template_url_service */);
  content::RunAllTasksUntilIdle();
  VerifyNumberOfChannels(2);
}

TEST_F(NotificationChannelsProviderAndroidTest,
       ClearBlockedChannels_DefaultSearchEngineIsNotCleared) {
  InitChannelsProvider();

  // Set up TemplateURLService with a default search engine.
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment;
  TemplateURLService* template_url_service =
      search_engines_test_environment.template_url_service();
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
  VerifyNumberOfChannels(2);

  ClearBlockedChannelsIfNecessary(template_url_service);
  content::RunAllTasksUntilIdle();

  base::RunLoop run_loop;
  // DSE channel should still exist and be blocked..
  fake_bridge_->GetChannels(base::BindOnce(
      [](base::RunLoop* run_loop,
         const std::vector<NotificationChannel>& channels) {
        ASSERT_EQ(channels.size(), 1u);
        EXPECT_EQ(channels[0].origin, "https://default-search-engine.com");
        EXPECT_EQ(channels[0].status, NotificationChannelStatus::BLOCKED);
        run_loop->Quit();
      },
      &run_loop));
  run_loop.Run();

  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(
      prefs::kClearedBlockedSiteNotificationChannels));
}
