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

  // Record initial display date to enable comparing dictionaries.
  std::string displayedDate =
      service()->GetBucketLabelForLastMonday(base::Time::Now());

  ContentSettingsForOneType notifications_engagement_setting;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());

  // Testing the dictionary of URLS
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::NOTIFICATION_INTERACTIONS,
      &notifications_engagement_setting);
  ASSERT_EQ(0U, notifications_engagement_setting.size());

  // Test that a new Dict entry is added when no entry existed for the given
  // URL.
  service()->RecordNotificationDisplayed(hosts[0]);
  service()->RecordNotificationDisplayed(hosts[0]);
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::NOTIFICATION_INTERACTIONS,
      &notifications_engagement_setting);
  ASSERT_EQ(1U, notifications_engagement_setting.size());

  // Advance time to set same URL entries in different dates.
  task_environment_.FastForwardBy(base::Days(8));
  std::string displayedDateLater =
      service()->GetBucketLabelForLastMonday(base::Time::Now());

  // Test that the same URL entry is not duplicated.
  service()->RecordNotificationDisplayed(hosts[0]);
  service()->RecordNotificationInteraction(hosts[0]);
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::NOTIFICATION_INTERACTIONS,
      &notifications_engagement_setting);
  ASSERT_EQ(1U, notifications_engagement_setting.size());

  // Test that different entries are created for different URL.
  service()->RecordNotificationDisplayed(hosts[1]);
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::NOTIFICATION_INTERACTIONS,
      &notifications_engagement_setting);
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
  // Test notifications sent within the same week constitute one date entry.

  service()->RecordNotificationDisplayed(url1);

  task_environment_.FastForwardBy(base::Days(1));
  service()->RecordNotificationDisplayed(url1);

  base::Value website_engagement_value1 =
      host_content_settings_map->GetWebsiteSetting(
          url1, GURL(), ContentSettingsType::NOTIFICATION_INTERACTIONS,
          nullptr);
  ASSERT_TRUE(website_engagement_value1.is_dict());
  base::Value::Dict& website_engagement_dict1 =
      website_engagement_value1.GetDict();

  ASSERT_EQ(1U, website_engagement_dict1.size());

  // Test notifications sent in different weeks constitute different entries.
  service()->RecordNotificationDisplayed(url2);

  task_environment_.FastForwardBy(base::Days(14));
  service()->RecordNotificationDisplayed(url2);

  task_environment_.FastForwardBy(base::Days(28));
  service()->RecordNotificationDisplayed(url2);

  base::Value website_engagement_value2 =
      host_content_settings_map->GetWebsiteSetting(
          url2, GURL(), ContentSettingsType::NOTIFICATION_INTERACTIONS,
          nullptr);
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
          url3, GURL(), ContentSettingsType::NOTIFICATION_INTERACTIONS,
          nullptr);
  ASSERT_TRUE(website_engagement_value3.is_dict());
  base::Value::Dict& website_engagement_dict3 =
      website_engagement_value3.GetDict();

  std::string displayedDate =
      service()->GetBucketLabelForLastMonday(base::Time::Now());
  base::Value* entryForDate = website_engagement_dict3.Find(displayedDate);

  ASSERT_EQ(2, entryForDate->FindIntKey(kDisplayedKey).value());
  ASSERT_EQ(3, entryForDate->FindIntKey(kEngagementKey).value());
}

TEST_F(NotificationsEngagementServiceTest, EraseStaleEntries) {
  GURL url("https://www.google.com/");

  // Test that only entries older than 90 days are deleted.
  task_environment_.FastForwardBy(base::Days(10950));
  service()->RecordNotificationDisplayed(url);
  task_environment_.FastForwardBy(base::Days(91));
  service()->RecordNotificationDisplayed(url);

  task_environment_.FastForwardBy(base::Days(8));
  service()->RecordNotificationDisplayed(url);

  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());

  base::Value::Dict website_engagement =
      host_content_settings_map
          ->GetWebsiteSetting(url, GURL(),
                              ContentSettingsType::NOTIFICATION_INTERACTIONS,
                              nullptr)
          .GetDict()
          .Clone();

  // Stale entries should be erased, when a new display/interaction is
  // recorded.
  ASSERT_EQ(2u, website_engagement.size());
}

TEST_F(NotificationsEngagementServiceTest, GetBucketLabelForLastMonday) {
  base::Time date1, date2, date3, date4;
  base::Time expected_monday1, expected_monday2, expected_monday3,
      expected_monday4;

  ASSERT_TRUE(base::Time::FromString("23 Oct 2009 11:30 GMT", &date1));
  ASSERT_TRUE(
      base::Time::FromString("2009-10-19 00:00:00.000 GMT", &expected_monday1));
  ASSERT_EQ(NotificationsEngagementService::GetBucketLabelForLastMonday(date1),
            base::NumberToString(expected_monday1.base::Time::ToTimeT()));
  std::string label1 =
      NotificationsEngagementService::GetBucketLabelForLastMonday(date1);
  ASSERT_EQ(label1,
            base::NumberToString(expected_monday1.base::Time::ToTimeT()));
  absl::optional<base::Time> begin1 =
      NotificationsEngagementService::ParsePeriodBeginFromBucketLabel(label1);
  ASSERT_TRUE(begin1.has_value());
  EXPECT_EQ(label1, NotificationsEngagementService::GetBucketLabelForLastMonday(
                        begin1.value()));

  ASSERT_TRUE(base::Time::FromString("Mon Oct 15 12:45 PDT 2007", &date2));
  ASSERT_TRUE(
      base::Time::FromString("2007-10-15 00:00:00.000 GMT", &expected_monday2));
  ASSERT_EQ(NotificationsEngagementService::GetBucketLabelForLastMonday(date2),
            base::NumberToString(expected_monday2.base::Time::ToTimeT()));
  std::string label2 =
      NotificationsEngagementService::GetBucketLabelForLastMonday(date2);
  ASSERT_EQ(label2,
            base::NumberToString(expected_monday2.base::Time::ToTimeT()));
  absl::optional<base::Time> begin2 =
      NotificationsEngagementService::ParsePeriodBeginFromBucketLabel(label2);
  ASSERT_TRUE(begin2.has_value());
  EXPECT_EQ(label2, NotificationsEngagementService::GetBucketLabelForLastMonday(
                        begin2.value()));

  ASSERT_TRUE(base::Time::FromString("Thu Jan 01 00:59:58 +0100 1970", &date3));
  ASSERT_TRUE(
      base::Time::FromString("1969-12-29 00:00:00.000 GMT", &expected_monday3));
  ASSERT_EQ(NotificationsEngagementService::GetBucketLabelForLastMonday(date3),
            base::NumberToString(expected_monday3.base::Time::ToTimeT()));
  std::string label3 =
      NotificationsEngagementService::GetBucketLabelForLastMonday(date3);
  ASSERT_EQ(label3,
            base::NumberToString(expected_monday3.base::Time::ToTimeT()));
  absl::optional<base::Time> begin3 =
      NotificationsEngagementService::ParsePeriodBeginFromBucketLabel(label3);
  ASSERT_TRUE(begin3.has_value());
  EXPECT_EQ(label3, NotificationsEngagementService::GetBucketLabelForLastMonday(
                        begin3.value()));

  ASSERT_TRUE(base::Time::FromString("Wed Mar 23 04:38:58 -1000 2022", &date4));
  ASSERT_TRUE(
      base::Time::FromString("2022-03-21 00:00:00.000 GMT", &expected_monday4));
  std::string label4 =
      NotificationsEngagementService::GetBucketLabelForLastMonday(date4);
  ASSERT_EQ(label4,
            base::NumberToString(expected_monday4.base::Time::ToTimeT()));
  absl::optional<base::Time> begin4 =
      NotificationsEngagementService::ParsePeriodBeginFromBucketLabel(label4);
  ASSERT_TRUE(begin4.has_value());
  EXPECT_EQ(label4, NotificationsEngagementService::GetBucketLabelForLastMonday(
                        begin4.value()));
}
}  // namespace permissions
