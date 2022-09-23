// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/notification_permission_review_service_factory.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/permissions/notification_permission_review_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace permissions {

#if !BUILDFLAG(IS_ANDROID)
class NotificationPermissionReviewServiceTest : public testing::Test {
 protected:
  TestingProfile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(NotificationPermissionReviewServiceTest,
       ignoreOriginForNotificationPermissionReview) {
  // Add a couple of notification permission and check they appear in all sites.
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  auto url_1 = GURL("http://example1.com");
  auto url_2 = GURL("http://example2.com");
  map->SetContentSettingDefaultScope(
      url_1, GURL(), ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ALLOW);
  map->SetContentSettingDefaultScope(
      url_2, GURL(), ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ALLOW);

  auto* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile());
  auto notification_permissions = service->GetNotificationSiteListForReview();
  EXPECT_EQ(2UL, notification_permissions.size());

  // Add notification permission to block list and check if it will be not be
  // shown on the list.
  service->AddOriginToNotificationPermissionReviewBlocklist(
      url::Origin::Create(url_1));
  notification_permissions = service->GetNotificationSiteListForReview();
  EXPECT_EQ(1UL, notification_permissions.size());
  EXPECT_EQ(notification_permissions[0].origin.GetURL(), url_2);
}

// TODO(crbug.com/1363714): Move this test to ContentSettingsPatternTest.
TEST_F(NotificationPermissionReviewServiceTest, singleOriginTest) {
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  const std::string url_1 = "http://[*].example1.com";
  const std::string url_2 = "http://example2.com";
  map->SetContentSettingDefaultScope(GURL(url_1), GURL(),
                                     ContentSettingsType::NOTIFICATIONS,
                                     CONTENT_SETTING_ALLOW);
  map->SetContentSettingDefaultScope(GURL(url_2), GURL(),
                                     ContentSettingsType::NOTIFICATIONS,
                                     CONTENT_SETTING_ALLOW);

  auto pattern_1 = ContentSettingsPattern::FromString(url_1);
  auto pattern_2 = ContentSettingsPattern::FromString(url_2);
  // Assert wildcard in primary pattern returns false on single origin check.
  EXPECT_EQ(false, content_settings::PatternAppliesToSingleOrigin(
                       pattern_1, ContentSettingsPattern::Wildcard()));
  EXPECT_EQ(true, content_settings::PatternAppliesToSingleOrigin(
                      pattern_2, ContentSettingsPattern::Wildcard()));
  EXPECT_EQ(false, content_settings::PatternAppliesToSingleOrigin(pattern_1,
                                                                  pattern_2));

  // Assert the review list only has the URL with single origin.
  auto* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile());
  auto notification_permissions = service->GetNotificationSiteListForReview();
  EXPECT_EQ(1UL, notification_permissions.size());
  EXPECT_EQ(GURL(url_2), notification_permissions[0].origin.GetURL());
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace permissions
