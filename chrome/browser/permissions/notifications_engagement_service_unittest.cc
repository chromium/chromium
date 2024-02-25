// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/notifications_engagement_service_factory.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace permissions {
constexpr char kEngagementKey[] = "click_count";
constexpr char kDisplayedKey[] = "display_count";

class NotificationsEngagementServiceTest : public testing::Test {
 public:
  NotificationsEngagementServiceTest()
      : profile_(std::make_unique<TestingProfile>()) {}

  void SetUp() override;

  NotificationsEngagementServiceTest(
      const NotificationsEngagementServiceTest&) = delete;
  NotificationsEngagementServiceTest& operator=(
      const NotificationsEngagementServiceTest&) = delete;

 protected:
  NotificationsEngagementService* service() {
    return NotificationsEngagementServiceFactory::GetForProfile(profile());
  }

  TestingProfile* profile() { return profile_.get(); }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  std::unique_ptr<TestingProfile> profile_;
};

void NotificationsEngagementServiceTest::SetUp() {
  testing::Test::SetUp();
}

TEST_F(NotificationsEngagementServiceTest,
       NotificationsEngagementContentSetting) {
  GURL hosts[] = {GURL("https://google.com/"),
                  GURL("https://www.youtube.com/")};

  // Otherwise the test time and the service time will be out of sync and cause
  // the tests to fail.
  task_environment_.FastForwardBy(base::Days(2));

  // Record initial display date to enable comparing dictionaries.
  std::string displayedDate = service()->GetBucketLabel(base::Time::Now());

  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());

  // Testing the dictionary of URLS
  ContentSettingsForOneType notifications_engagement_setting =
      host_content_settings_map->GetSettingsForOneType(
          ContentSettingsType::NOTIFICATION_INTERACTIONS);
  ASSERT_EQ(0U, notifications_engagement_setting.size());

  // Test that a new Dict entry is added when no entry existed for the given
  // URL.
  service()->RecordNotificationDisplayed(hosts[0]);
  service()->RecordNotificationDisplayed(hosts[0]);
  notifications_engagement_setting =
      host_content_settings_map->GetSettingsForOneType(
          ContentSettingsType::NOTIFICATION_INTERACTIONS);
  ASSERT_EQ(1U, notifications_engagement_setting.size());

  // Advance time to set same URL entries in different dates.
  task_environment_.FastForwardBy(base::Days(8));
  std::string displayedDateLater = service()->GetBucketLabel(base::Time::Now());

  // Test that the same URL entry is not duplicated.
  service()->RecordNotificationDisplayed(hosts[0]);
  service()->RecordNotificationInteraction(hosts[0]);
  notifications_engagement_setting =
      host_content_settings_map->GetSettingsForOneType(
          ContentSettingsType::NOTIFICATION_INTERACTIONS);
  ASSERT_EQ(1U, notifications_engagement_setting.size());

  // Test that different entries are created for different URL.
  service()->RecordNotificationDisplayed(hosts[1]);
  notifications_engagement_setting =
      host_content_settings_map->GetSettingsForOneType(
          ContentSettingsType::NOTIFICATION_INTERACTIONS);
  ASSERT_EQ(2U, notifications_engagement_setting.size());

  // Verify the contents of the |notifications_engagement_setting| for
  // hosts[0].
  base::Value::Dict engagement_dict1;
  base::Value::Dict dict1_entry1;
  dict1_entry1.Set(kDisplayedKey, 2);
  base::Value::Dict dict1_entry2;
  dict1_entry2.Set(kDisplayedKey, 1);
  dict1_entry2.Set(kEngagementKey, 1);
  engagement_dict1.Set(displayedDate, std::move(dict1_entry1));
  engagement_dict1.Set(displayedDateLater, std::move(dict1_entry2));
  base::Value engagement_dict_value1 = base::Value(std::move(engagement_dict1));

  ASSERT_EQ(ContentSettingsPattern::FromURLNoWildcard(hosts[0]),
            notifications_engagement_setting.at(0).primary_pattern);
  ASSERT_EQ(ContentSettingsPattern::Wildcard(),
            notifications_engagement_setting.at(0).secondary_pattern);
  ASSERT_EQ(engagement_dict_value1,
            notifications_engagement_setting.at(0).setting_value);

  // Verify the contents of the |notifications_engagement_setting| for
  // hosts[1].
  base::Value::Dict engagement_dict2;
  base::Value::Dict dict2_entry1;
  dict2_entry1.Set(kDisplayedKey, 1);
  engagement_dict2.Set(displayedDateLater, std::move(dict2_entry1));
  base::Value engagement_dict_value2 = base::Value(std::move(engagement_dict2));

  ASSERT_EQ(ContentSettingsPattern::FromURLNoWildcard(hosts[1]),
            notifications_engagement_setting.at(1).primary_pattern);
  ASSERT_EQ(ContentSettingsPattern::Wildcard(),
            notifications_engagement_setting.at(1).secondary_pattern);
  ASSERT_EQ(engagement_dict_value2,
            notifications_engagement_setting.at(1).setting_value);
}

TEST_F(NotificationsEngagementServiceTest,
       RecordNotificationDisplayedAndInteraction) {
  GURL url1("https://www.google.com/");
  GURL url2("https://www.youtube.com/");
  GURL url3("https://www.permissions.site.com/");

  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  // Test notifications sent within the same day constitute one date entry.

  task_environment_.FastForwardBy(base::Days(2));
  service()->RecordNotificationDisplayed(url1);
  service()->RecordNotificationDisplayed(url1);

  base::Value website_engagement_value1 =
      host_content_settings_map->GetWebsiteSetting(
          url1, GURL(), ContentSettingsType::NOTIFICATION_INTERACTIONS);
  ASSERT_TRUE(website_engagement_value1.is_dict());
  base::Value::Dict& website_engagement_dict1 =
      website_engagement_value1.GetDict();

  ASSERT_EQ(1U, website_engagement_dict1.size());

  // Test notifications sent in different days constitute different entries.
  service()->RecordNotificationDisplayed(url2);

  task_environment_.FastForwardBy(base::Days(14));
  service()->RecordNotificationDisplayed(url2);

  task_environment_.FastForwardBy(base::Days(7));
  service()->RecordNotificationDisplayed(url2);

  base::Value website_engagement_value2 =
      host_content_settings_map->GetWebsiteSetting(
          url2, GURL(), ContentSettingsType::NOTIFICATION_INTERACTIONS);
  ASSERT_TRUE(website_engagement_value2.is_dict());
  base::Value::Dict& website_engagement_dict2 =
      website_engagement_value2.GetDict();

  ASSERT_EQ(3U, website_engagement_dict2.size());

  // Test that display_count and click_count are incremented correctly.
  service()->RecordNotificationDisplayed(url3);
  service()->RecordNotificationDisplayed(url3);

  service()->RecordNotificationInteraction(url3);
  service()->RecordNotificationInteraction(url3);
  service()->RecordNotificationInteraction(url3);

  base::Value website_engagement_value3 =
      host_content_settings_map->GetWebsiteSetting(
          url3, GURL(), ContentSettingsType::NOTIFICATION_INTERACTIONS);
  ASSERT_TRUE(website_engagement_value3.is_dict());
  base::Value::Dict& website_engagement_dict3 =
      website_engagement_value3.GetDict();

  std::string displayedDate = service()->GetBucketLabel(base::Time::Now());
  base::Value* entryForDate = website_engagement_dict3.Find(displayedDate);

  ASSERT_EQ(2, entryForDate->GetDict().FindInt(kDisplayedKey).value());
  ASSERT_EQ(3, entryForDate->GetDict().FindInt(kEngagementKey).value());
}

TEST_F(NotificationsEngagementServiceTest, EraseStaleEntries) {
  GURL url("https://www.google.com/");

  // Test that only entries older than 30 days are deleted.
  task_environment_.FastForwardBy(base::Days(10950));
  service()->RecordNotificationDisplayed(url);
  task_environment_.FastForwardBy(base::Days(31));
  service()->RecordNotificationDisplayed(url);

  task_environment_.FastForwardBy(base::Days(8));
  service()->RecordNotificationDisplayed(url);

  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());

  base::Value::Dict website_engagement =
      host_content_settings_map
          ->GetWebsiteSetting(url, GURL(),
                              ContentSettingsType::NOTIFICATION_INTERACTIONS)
          .GetDict()
          .Clone();

  // Stale entries should be erased, when a new display/interaction is
  // recorded.
  ASSERT_EQ(2u, website_engagement.size());
}

// Disabled, due to http://go/crb/1407635.
TEST_F(NotificationsEngagementServiceTest, DISABLED_GetBucketLabel) {
  base::Time date1, date2, date3, date4;
  base::Time expected_date1, expected_date2, expected_date3, expected_date4;

  ASSERT_TRUE(base::Time::FromString("23 Oct 2009 11:30 GMT", &date1));
  ASSERT_TRUE(
      base::Time::FromString("2009-10-23 00:00:00.000 GMT", &expected_date1));
  ASSERT_EQ(NotificationsEngagementService::GetBucketLabel(date1),
            base::NumberToString(expected_date1.base::Time::ToTimeT()));
  std::string label1 = NotificationsEngagementService::GetBucketLabel(date1);
  ASSERT_EQ(label1, base::NumberToString(expected_date1.base::Time::ToTimeT()));
  std::optional<base::Time> begin1 =
      NotificationsEngagementService::ParsePeriodBeginFromBucketLabel(label1);
  ASSERT_TRUE(begin1.has_value());
  EXPECT_EQ(label1,
            NotificationsEngagementService::GetBucketLabel(begin1.value()));

  ASSERT_TRUE(base::Time::FromString("Mon Oct 15 12:45 PDT 2007", &date2));
  ASSERT_TRUE(
      base::Time::FromString("2007-10-15 00:00:00.000 GMT", &expected_date2));
  ASSERT_EQ(NotificationsEngagementService::GetBucketLabel(date2),
            base::NumberToString(expected_date2.base::Time::ToTimeT()));
  std::string label2 = NotificationsEngagementService::GetBucketLabel(date2);
  ASSERT_EQ(label2, base::NumberToString(expected_date2.base::Time::ToTimeT()));
  std::optional<base::Time> begin2 =
      NotificationsEngagementService::ParsePeriodBeginFromBucketLabel(label2);
  ASSERT_TRUE(begin2.has_value());
  EXPECT_EQ(label2,
            NotificationsEngagementService::GetBucketLabel(begin2.value()));

  ASSERT_TRUE(base::Time::FromString("Mon Jan 01 13:59:58 +0100 1973", &date3));
  ASSERT_TRUE(
      base::Time::FromString("1973-01-01 00:00:00.000 GMT", &expected_date3));
  ASSERT_EQ(NotificationsEngagementService::GetBucketLabel(date3),
            base::NumberToString(expected_date3.base::Time::ToTimeT()));
  std::string label3 = NotificationsEngagementService::GetBucketLabel(date3);
  ASSERT_EQ(label3, base::NumberToString(expected_date3.base::Time::ToTimeT()));
  std::optional<base::Time> begin3 =
      NotificationsEngagementService::ParsePeriodBeginFromBucketLabel(label3);
  ASSERT_TRUE(begin3.has_value());
  EXPECT_EQ(label3,
            NotificationsEngagementService::GetBucketLabel(begin3.value()));

  ASSERT_TRUE(base::Time::FromString("Wed Mar 23 04:38:58 -1000 2022", &date4));
  ASSERT_TRUE(
      base::Time::FromString("2022-03-23 00:00:00.000 GMT", &expected_date4));
  std::string label4 = NotificationsEngagementService::GetBucketLabel(date4);
  ASSERT_EQ(label4, base::NumberToString(expected_date4.base::Time::ToTimeT()));
  std::optional<base::Time> begin4 =
      NotificationsEngagementService::ParsePeriodBeginFromBucketLabel(label4);
  ASSERT_TRUE(begin4.has_value());
  EXPECT_EQ(label4,
            NotificationsEngagementService::GetBucketLabel(begin4.value()));
}
}  // namespace permissions
