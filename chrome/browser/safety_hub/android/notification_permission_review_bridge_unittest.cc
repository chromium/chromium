// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safety_hub/android/notification_permission_review_bridge.h"

#include <jni.h>

#include <vector>

#include "base/android/jni_android.h"
#include "base/run_loop.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/permissions/notifications_engagement_service_factory.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::android::AttachCurrentThread;

namespace {

constexpr char kOrigin1[] = "https://example1.com:443";
const int kNotificationCount1 = 5;

constexpr char kOrigin2[] = "https://example2.com:443";
const int kNotificationCount2 = 15;

}  // namespace

class NotificationPermissionReviewBridgeTest : public testing::Test {
 public:
  NotificationPermissionReviewBridgeTest() : env_(AttachCurrentThread()) {}

  void TearDown() override { ClearNotificationsChannels(); }

  void AddNotificationPermissionForReview(const std::string& origin,
                                          int notification_count) {
    // Set a host to have large number of notifications and keep engagement as
    // LOW.
    GURL url = GURL(origin);
    hcsm()->SetContentSettingDefaultScope(
        url, GURL(), ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ALLOW);

    auto* notifications_engagement_service =
        NotificationsEngagementServiceFactory::GetForProfile(profile());
    notifications_engagement_service->RecordNotificationDisplayed(
        url, notification_count * 7);

    auto* site_engagement_service =
        site_engagement::SiteEngagementServiceFactory::GetForProfile(profile());
    site_engagement_service->AddPointsForTesting(url, 1.0);

    EXPECT_EQ(blink::mojom::EngagementLevel::LOW,
              site_engagement_service->GetEngagementLevel(url));

    // Trigger the update for changes to be seen.
    service()->UpdateAsync();
    RunUntilIdle();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  TestingProfile* profile() { return &testing_profile_; }

  HostContentSettingsMap* hcsm() {
    return HostContentSettingsMapFactory::GetForProfile(profile());
  }

  NotificationPermissionsReviewService* service() {
    return NotificationPermissionsReviewServiceFactory::GetForProfile(
        profile());
  }

  raw_ptr<JNIEnv> env() { return env_; }

 private:
  raw_ptr<JNIEnv> env_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile testing_profile_;

  void ClearNotificationsChannels() {
    // Because notification channel settings aren't tied to the profile, they
    // will persist across tests. We need to make sure they're reset here.
    for (auto& setting :
         hcsm()->GetSettingsForOneType(ContentSettingsType::NOTIFICATIONS)) {
      if (!setting.primary_pattern.MatchesAllHosts() ||
          !setting.secondary_pattern.MatchesAllHosts()) {
        hcsm()->SetContentSettingCustomScope(
            setting.primary_pattern, setting.secondary_pattern,
            ContentSettingsType ::NOTIFICATIONS,
            ContentSetting::CONTENT_SETTING_DEFAULT);
      }
    }
  }
};

TEST_F(NotificationPermissionReviewBridgeTest, TestJavaRoundTrip) {
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString(kOrigin1);
  ContentSettingsPattern secondary_pattern = ContentSettingsPattern::Wildcard();
  NotificationPermissions expected(primary_pattern, secondary_pattern,
                                   kNotificationCount1);

  const auto jobject = ToJavaNotificationPermissions(env(), expected);
  NotificationPermissions converted =
      FromJavaNotificationPermissions(env(), jobject);

  EXPECT_EQ(expected.primary_pattern, converted.primary_pattern);
  EXPECT_EQ(expected.secondary_pattern, converted.secondary_pattern);
  EXPECT_EQ(expected.notification_count, converted.notification_count);
}

TEST_F(NotificationPermissionReviewBridgeTest, GetNotificationPermissions) {
  AddNotificationPermissionForReview(kOrigin1, kNotificationCount1);
  AddNotificationPermissionForReview(kOrigin2, kNotificationCount2);
  std::vector<NotificationPermissions> notification_permissions =
      GetNotificationPermissions(profile());
  EXPECT_EQ(notification_permissions.size(), 2UL);

  // Expect notification permissions sorted in descending notification
  // count order.
  EXPECT_EQ(notification_permissions[0].primary_pattern,
            ContentSettingsPattern::FromString(kOrigin2));
  EXPECT_EQ(notification_permissions[0].secondary_pattern,
            ContentSettingsPattern::Wildcard());
  EXPECT_EQ(notification_permissions[0].notification_count,
            kNotificationCount2);

  EXPECT_EQ(notification_permissions[1].primary_pattern,
            ContentSettingsPattern::FromString(kOrigin1));
  EXPECT_EQ(notification_permissions[1].secondary_pattern,
            ContentSettingsPattern::Wildcard());
  EXPECT_EQ(notification_permissions[1].notification_count,
            kNotificationCount1);
}

TEST_F(NotificationPermissionReviewBridgeTest,
       IgnoreOriginForNotificationPermissionReview) {
  AddNotificationPermissionForReview(kOrigin1, kNotificationCount1);
  AddNotificationPermissionForReview(kOrigin2, kNotificationCount2);

  std::vector<NotificationPermissions> notification_permissions =
      GetNotificationPermissions(profile());
  EXPECT_EQ(notification_permissions.size(), 2UL);

  IgnoreOriginForNotificationPermissionReview(profile(), kOrigin2);
  service()->UpdateAsync();
  RunUntilIdle();

  notification_permissions = GetNotificationPermissions(profile());
  EXPECT_EQ(notification_permissions.size(), 1UL);
  EXPECT_EQ(notification_permissions[0].primary_pattern,
            ContentSettingsPattern::FromString(kOrigin1));
}

TEST_F(NotificationPermissionReviewBridgeTest,
       UndoIgnoreOriginForNotificationPermissionReview) {
  AddNotificationPermissionForReview(kOrigin1, kNotificationCount1);
  AddNotificationPermissionForReview(kOrigin2, kNotificationCount2);
  IgnoreOriginForNotificationPermissionReview(profile(), kOrigin2);
  service()->UpdateAsync();
  RunUntilIdle();

  std::vector<NotificationPermissions> notification_permissions =
      GetNotificationPermissions(profile());
  EXPECT_EQ(notification_permissions.size(), 1UL);

  UndoIgnoreOriginForNotificationPermissionReview(profile(), kOrigin2);
  service()->UpdateAsync();
  RunUntilIdle();

  notification_permissions = GetNotificationPermissions(profile());
  EXPECT_EQ(notification_permissions.size(), 2UL);
}

TEST_F(NotificationPermissionReviewBridgeTest,
       AllowNotificationPermissionForOrigin) {
  auto pattern = ContentSettingsPattern::FromString(kOrigin1);
  hcsm()->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString(kOrigin1),
      ContentSettingsPattern::Wildcard(), ContentSettingsType::NOTIFICATIONS,
      CONTENT_SETTING_DEFAULT);
  auto type = hcsm()->GetContentSetting(GURL(kOrigin1), GURL(),
                                        ContentSettingsType::NOTIFICATIONS);
  ASSERT_EQ(CONTENT_SETTING_ASK, type);

  AllowNotificationPermissionForOrigin(profile(), kOrigin1);

  type = hcsm()->GetContentSetting(GURL(kOrigin1), GURL(),
                                   ContentSettingsType::NOTIFICATIONS);
  ASSERT_EQ(CONTENT_SETTING_ALLOW, type);
}

TEST_F(NotificationPermissionReviewBridgeTest,
       ResetNotificationPermissionForOrigin) {
  hcsm()->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString(kOrigin1),
      ContentSettingsPattern::Wildcard(), ContentSettingsType::NOTIFICATIONS,
      CONTENT_SETTING_ALLOW);
  auto type = hcsm()->GetContentSetting(GURL(kOrigin1), GURL(),
                                        ContentSettingsType::NOTIFICATIONS);
  ASSERT_EQ(CONTENT_SETTING_ALLOW, type);

  ResetNotificationPermissionForOrigin(profile(), kOrigin1);

  type = hcsm()->GetContentSetting(GURL(kOrigin1), GURL(),
                                   ContentSettingsType::NOTIFICATIONS);
  ASSERT_EQ(CONTENT_SETTING_ASK, type);
}
