// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/mac/notification_utils.h"

#include <optional>
#include <string>

#include "base/path_service.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/notifications/notification_operation.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/public/cpp/notification.h"

using message_center::Notification;

class NotificationUtilsMacTest : public testing::Test {
 public:
  void SetUp() override { response_ = BuildDefaultNotificationResponse(); }

 protected:
  mac_notifications::mojom::NotificationMetadataPtr
  CreateNotificationMetadata() {
    auto profile_identifier = mac_notifications::mojom::ProfileIdentifier::New(
        "profile", /*incognito=*/false);
    auto notification_identifier =
        mac_notifications::mojom::NotificationIdentifier::New(
            "notification_id", std::move(profile_identifier));
    base::FilePath user_data_dir;
    EXPECT_TRUE(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
    return mac_notifications::mojom::NotificationMetadata::New(
        std::move(notification_identifier), /*notification_type=*/0,
        /*origin_url=*/GURL(), user_data_dir.value());
  }

  mac_notifications::mojom::NotificationActionInfoPtr
  BuildDefaultNotificationResponse() {
    auto meta = CreateNotificationMetadata();
    return mac_notifications::mojom::NotificationActionInfo::New(
        std::move(meta), NotificationOperation::kClick,
        /*button_index=*/-1, /*reply=*/std::nullopt);
  }

  Notification CreateNotification(
      const std::u16string& title,
      const std::u16string& subtitle,
      const std::string& origin,
      message_center::NotificationType type,
      int progress,
      const std::optional<std::u16string>& contextMessage) {
    GURL url(origin);

    Notification notification(type, "test_id", title, subtitle,
                              ui::ImageModel(), u"Notifier's Name", url,
                              message_center::NotifierId(url),
                              message_center::RichNotificationData(),
                              /*delegate=*/nullptr);

    if (type == message_center::NOTIFICATION_TYPE_PROGRESS)
      notification.set_progress(progress);

    if (contextMessage)
      notification.set_context_message(*contextMessage);

    return notification;
  }

  mac_notifications::mojom::NotificationActionInfoPtr response_;
};

TEST_F(NotificationUtilsMacTest, TestCreateNotificationTitle) {
  Notification notification = CreateNotification(
      u"Title", u"Subtitle", "https://moe.example.com",
      message_center::NOTIFICATION_TYPE_SIMPLE, /*progress=*/0,
      /*contextMessage=*/std::nullopt);
  std::u16string createdTitle = CreateMacNotificationTitle(notification);
  EXPECT_EQ(u"Title", createdTitle);
}

TEST_F(NotificationUtilsMacTest,
       TestCreateNotificationTitleWithProgress) {
  Notification notification = CreateNotification(
      u"Title", u"Subtitle", "https://moe.example.com",
      message_center::NOTIFICATION_TYPE_PROGRESS, /*progress=*/50,
      /*contextMessage=*/std::nullopt);
  std::u16string createdTitle = CreateMacNotificationTitle(notification);
  EXPECT_EQ(u"50% - Title", createdTitle);
}

TEST_F(NotificationUtilsMacTest,
       TestCreateNotificationContextBanner) {
  Notification notification = CreateNotification(
      u"Title", u"Subtitle", "https://moe.example.com",
      message_center::NOTIFICATION_TYPE_SIMPLE, /*progress=*/0,
      /*contextMessage=*/std::nullopt);
  std::u16string createdContext = CreateMacNotificationContext(
      /*isPersistent=*/false, notification, /*requiresAttribution=*/true);
  EXPECT_EQ(u"moe.example.com", createdContext);
}

TEST_F(NotificationUtilsMacTest,
       TestCreateNotificationContextAlert) {
  Notification notification = CreateNotification(
      u"Title", u"Subtitle", "https://moe.example.com",
      message_center::NOTIFICATION_TYPE_SIMPLE, /*progress=*/0,
      /*contextMessage=*/std::nullopt);
  std::u16string createdContext = CreateMacNotificationContext(
      /*isPersistent=*/true, notification, /*requiresAttribution=*/true);
  EXPECT_EQ(u"moe.example.com", createdContext);
}

TEST_F(NotificationUtilsMacTest,
       TestCreateNotificationContextNoAttribution) {
  Notification notification =
      CreateNotification(u"Title", u"Subtitle", /*origin=*/std::string(),
                         message_center::NOTIFICATION_TYPE_SIMPLE,
                         /*progress=*/0,
                         /*contextMessage=*/u"moe");
  std::u16string createdContext = CreateMacNotificationContext(
      /*isPersistent=*/false, notification, /*requiresAttribution=*/false);
  EXPECT_EQ(u"moe", createdContext);
}

TEST_F(NotificationUtilsMacTest,
       TestCreateNotificationContexteTLDPlusOne) {
  Notification notification = CreateNotification(
      u"Title", u"Subtitle",
      "https://thisisareallyreallyreaaalllyyylongorigin.moe.example.com/",
      message_center::NOTIFICATION_TYPE_SIMPLE, /*progress=*/0,
      /*contextMessage=*/std::nullopt);
  std::u16string createdContext = CreateMacNotificationContext(
      /*isPersistent=*/false, notification, /*requiresAttribution=*/true);
  EXPECT_EQ(u"example.com", createdContext);

  // Should also work if the eTLD is in the format of '/+.+/'
  notification.set_origin_url(GURL(
      "https://thisisareallyreallyreaaalllyyylongorigin.moe.example.co.uk/"));
  createdContext = CreateMacNotificationContext(
      /*isPersistent=*/false, notification, /*requiresAttribution=*/true);
  EXPECT_EQ(u"example.co.uk", createdContext);
}

TEST_F(NotificationUtilsMacTest,
       TestCreateNotificationContextAlertLongOrigin) {
  Notification notification = CreateNotification(
      u"Title", u"Subtitle", "https://thisisalongorigin.moe.co.uk",
      message_center::NOTIFICATION_TYPE_SIMPLE, /*progress=*/0,
      /*contextMessage=*/std::nullopt);
  std::u16string createdContext = CreateMacNotificationContext(
      /*isPersistent=*/true, notification, /*requiresAttribution=*/true);
  EXPECT_EQ(u"moe.co.uk", createdContext);

  // For banners this should pass
  createdContext = CreateMacNotificationContext(
      /*isPersistent=*/false, notification, /*requiresAttribution=*/true);
  EXPECT_EQ(u"thisisalongorigin.moe.co.uk", createdContext);
}

TEST_F(NotificationUtilsMacTest,
       TestCreateNotificationContextLongOrigin) {
  Notification notification = CreateNotification(
      u"Title", u"Subtitle", "https://thisisareallylongorigin.moe.co.uk",
      message_center::NOTIFICATION_TYPE_SIMPLE, /*progress=*/0,
      /*contextMessage=*/std::nullopt);
  std::u16string createdContext = CreateMacNotificationContext(
      /*isPersistent=*/true, notification, /*requiresAttribution=*/true);
  EXPECT_EQ(u"moe.co.uk", createdContext);

  // It should get the eTLD+1 for banners too
  createdContext = CreateMacNotificationContext(
      /*isPersistent=*/false, notification, /*requiresAttribution=*/true);
  EXPECT_EQ(u"moe.co.uk", createdContext);
}

TEST_F(NotificationUtilsMacTest,
       TestNotificationVerifyValidResponse) {
  EXPECT_TRUE(VerifyMacNotificationData(response_));
}

TEST_F(NotificationUtilsMacTest, TestNotificationUnknownType) {
  response_->meta->type = 210581;
  EXPECT_FALSE(VerifyMacNotificationData(response_));
}

TEST_F(NotificationUtilsMacTest,
       TestNotificationVerifyNoProfileId) {
  response_->meta->id->profile = nullptr;
  EXPECT_FALSE(VerifyMacNotificationData(response_));
}

TEST_F(NotificationUtilsMacTest,
       TestNotificationVerifyNoNotificationId) {
  response_->meta->id = nullptr;
  EXPECT_FALSE(VerifyMacNotificationData(response_));
}

TEST_F(NotificationUtilsMacTest,
       TestNotificationVerifyInvalidButton) {
  response_->button_index = -5;
  EXPECT_FALSE(VerifyMacNotificationData(response_));
}

TEST_F(NotificationUtilsMacTest, TestNotificationVerifyOrigin) {
  response_->meta->origin_url = GURL("http://?");
  EXPECT_FALSE(VerifyMacNotificationData(response_));

  // If however the origin is not present the response should be fine.
  response_->meta->origin_url = GURL();
  EXPECT_TRUE(VerifyMacNotificationData(response_));
}
