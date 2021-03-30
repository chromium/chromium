// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/AppKit.h>

#include <string>

#include "base/mac/scoped_nsobject.h"
#include "base/optional.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"
#include "chrome/browser/notifications/notification_platform_bridge_mac_utils.h"
#include "chrome/browser/ui/cocoa/notifications/notification_builder_mac.h"
#include "chrome/browser/ui/cocoa/notifications/notification_response_builder_mac.h"
#include "chrome/services/mac_notifications/public/cpp/notification_constants_mac.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/public/cpp/notification.h"

using message_center::Notification;

class NotificationPlatformBridgeMacUtilsTest : public testing::Test {
 public:
  void SetUp() override {
    response_ = BuildDefaultNotificationResponse();
  }

 protected:
  NSMutableDictionary* BuildDefaultNotificationResponse() {
    return [NSMutableDictionary
        dictionaryWithDictionary:
            [NotificationResponseBuilder
                buildActivatedDictionary:BuildNotification()
                               fromAlert:NO]];
  }

  Notification CreateNotification(
      const std::string& title,
      const std::string& subtitle,
      const std::string& origin,
      message_center::NotificationType type,
      int progress,
      const base::Optional<std::string>& contextMessage) {
    GURL url(origin);

    Notification notification(
        type, "test_id", base::UTF8ToUTF16(title), base::UTF8ToUTF16(subtitle),
        gfx::Image(), u"Notifier's Name", url, message_center::NotifierId(url),
        message_center::RichNotificationData(),
        /*delegate=*/nullptr);

    if (type == message_center::NOTIFICATION_TYPE_PROGRESS)
      notification.set_progress(progress);

    if (contextMessage)
      notification.set_context_message(base::UTF8ToUTF16(*contextMessage));

    return notification;
  }

  NSMutableDictionary* response_;

 private:
  NSUserNotification* BuildNotification() {
    base::scoped_nsobject<NotificationBuilder> builder(
        [[NotificationBuilder alloc] initWithCloseLabel:@"Close"
                                           optionsLabel:@"More"
                                          settingsLabel:@"Settings"]);
    [builder setTitle:@"Title"];
    [builder setOrigin:@"https://www.moe.com/"];
    [builder setContextMessage:@""];
    [builder setButtons:@"Button1" secondaryButton:@"Button2"];
    [builder setIdentifier:@"identifier"];
    [builder setIcon:[NSImage imageNamed:@"NSApplicationIcon"]];
    [builder setNotificationId:@"notification_id"];
    [builder setProfileId:@"Default"];
    [builder setIncognito:false];
    [builder setCreatorPid:@(getpid())];
    [builder setNotificationType:
                 [NSNumber numberWithInteger:
                               static_cast<int>(
                                   NotificationHandler::Type::WEB_PERSISTENT)]];
    [builder setShowSettingsButton:true];

    return [builder buildUserNotification];
  }
};

TEST_F(NotificationPlatformBridgeMacUtilsTest, TestCreateNotificationTitle) {
  Notification notification = CreateNotification(
      "Title", "Subtitle", "https://moe.example.com",
      message_center::NOTIFICATION_TYPE_SIMPLE, /*progress=*/0,
      /*contextMessage=*/base::nullopt);
  std::u16string createdTitle = CreateMacNotificationTitle(notification);
  EXPECT_EQ(u"Title", createdTitle);
}

TEST_F(NotificationPlatformBridgeMacUtilsTest,
       TestCreateNotificationTitleWithProgress) {
  Notification notification = CreateNotification(
      "Title", "Subtitle", "https://moe.example.com",
      message_center::NOTIFICATION_TYPE_PROGRESS, /*progress=*/50,
      /*contextMessage=*/base::nullopt);
  std::u16string createdTitle = CreateMacNotificationTitle(notification);
  EXPECT_EQ(u"50% - Title", createdTitle);
}

TEST_F(NotificationPlatformBridgeMacUtilsTest,
       TestCreateNotificationContextBanner) {
  Notification notification = CreateNotification(
      "Title", "Subtitle", "https://moe.example.com",
      message_center::NOTIFICATION_TYPE_SIMPLE, /*progress=*/0,
      /*contextMessage=*/base::nullopt);
  std::u16string createdContext = CreateMacNotificationContext(
      /*isPersistent=*/false, notification, /*requiresAttribution=*/true);
  EXPECT_EQ(u"moe.example.com", createdContext);
}

TEST_F(NotificationPlatformBridgeMacUtilsTest,
       TestCreateNotificationContextAlert) {
  Notification notification = CreateNotification(
      "Title", "Subtitle", "https://moe.example.com",
      message_center::NOTIFICATION_TYPE_SIMPLE, /*progress=*/0,
      /*contextMessage=*/base::nullopt);
  std::u16string createdContext = CreateMacNotificationContext(
      /*isPersistent=*/true, notification, /*requiresAttribution=*/true);
  EXPECT_EQ(u"moe.example.com", createdContext);
}

TEST_F(NotificationPlatformBridgeMacUtilsTest,
       TestCreateNotificationContextNoAttribution) {
  Notification notification =
      CreateNotification("Title", "Subtitle", /*origin=*/std::string(),
                         message_center::NOTIFICATION_TYPE_SIMPLE,
                         /*progress=*/0,
                         /*contextMessage=*/"moe");
  std::u16string createdContext = CreateMacNotificationContext(
      /*isPersistent=*/false, notification, /*requiresAttribution=*/false);
  EXPECT_EQ(u"moe", createdContext);
}

TEST_F(NotificationPlatformBridgeMacUtilsTest,
       TestCreateNotificationContexteTLDPlusOne) {
  Notification notification = CreateNotification(
      "Title", "Subtitle",
      "https://thisisareallyreallyreaaalllyyylongorigin.moe.example.com/",
      message_center::NOTIFICATION_TYPE_SIMPLE, /*progress=*/0,
      /*contextMessage=*/base::nullopt);
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

TEST_F(NotificationPlatformBridgeMacUtilsTest,
       TestCreateNotificationContextAlertLongOrigin) {
  Notification notification = CreateNotification(
      "Title", "Subtitle", "https://thisisalongorigin.moe.co.uk",
      message_center::NOTIFICATION_TYPE_SIMPLE, /*progress=*/0,
      /*contextMessage=*/base::nullopt);
  std::u16string createdContext = CreateMacNotificationContext(
      /*isPersistent=*/true, notification, /*requiresAttribution=*/true);
  EXPECT_EQ(u"moe.co.uk", createdContext);

  // For banners this should pass
  createdContext = CreateMacNotificationContext(
      /*isPersistent=*/false, notification, /*requiresAttribution=*/true);
  EXPECT_EQ(u"thisisalongorigin.moe.co.uk", createdContext);
}

TEST_F(NotificationPlatformBridgeMacUtilsTest,
       TestCreateNotificationContextLongOrigin) {
  Notification notification = CreateNotification(
      "Title", "Subtitle", "https://thisisareallylongorigin.moe.co.uk",
      message_center::NOTIFICATION_TYPE_SIMPLE, /*progress=*/0,
      /*contextMessage=*/base::nullopt);
  std::u16string createdContext = CreateMacNotificationContext(
      /*isPersistent=*/true, notification, /*requiresAttribution=*/true);
  EXPECT_EQ(u"moe.co.uk", createdContext);

  // It should get the eTLD+1 for banners too
  createdContext = CreateMacNotificationContext(
      /*isPersistent=*/false, notification, /*requiresAttribution=*/true);
  EXPECT_EQ(u"moe.co.uk", createdContext);
}

TEST_F(NotificationPlatformBridgeMacUtilsTest,
       TestNotificationVerifyValidResponse) {
  EXPECT_TRUE(VerifyMacNotificationData(response_));
}

TEST_F(NotificationPlatformBridgeMacUtilsTest, TestNotificationUnknownType) {
  [response_ setValue:[NSNumber numberWithInt:210581]
               forKey:notification_constants::kNotificationType];
  EXPECT_FALSE(VerifyMacNotificationData(response_));
}

TEST_F(NotificationPlatformBridgeMacUtilsTest,
       TestNotificationVerifyUnknownOperation) {
  [response_ setValue:[NSNumber numberWithInt:40782]
               forKey:notification_constants::kNotificationOperation];
  EXPECT_FALSE(VerifyMacNotificationData(response_));
}

TEST_F(NotificationPlatformBridgeMacUtilsTest,
       TestNotificationVerifyMissingOperation) {
  [response_ removeObjectForKey:notification_constants::kNotificationOperation];
  EXPECT_FALSE(VerifyMacNotificationData(response_));
}

TEST_F(NotificationPlatformBridgeMacUtilsTest,
       TestNotificationVerifyNoProfileId) {
  [response_ removeObjectForKey:notification_constants::kNotificationProfileId];
  EXPECT_FALSE(VerifyMacNotificationData(response_));
}

TEST_F(NotificationPlatformBridgeMacUtilsTest,
       TestNotificationVerifyNoNotificationId) {
  [response_ setValue:@"" forKey:notification_constants::kNotificationId];
  EXPECT_FALSE(VerifyMacNotificationData(response_));
}

TEST_F(NotificationPlatformBridgeMacUtilsTest,
       TestNotificationVerifyInvalidButton) {
  [response_ setValue:[NSNumber numberWithInt:-5]
               forKey:notification_constants::kNotificationButtonIndex];
  EXPECT_FALSE(VerifyMacNotificationData(response_));
}

TEST_F(NotificationPlatformBridgeMacUtilsTest,
       TestNotificationVerifyMissingButtonIndex) {
  [response_
      removeObjectForKey:notification_constants::kNotificationButtonIndex];
  EXPECT_FALSE(VerifyMacNotificationData(response_));
}

TEST_F(NotificationPlatformBridgeMacUtilsTest, TestNotificationVerifyOrigin) {
  [response_ setValue:@"invalidorigin"
               forKey:notification_constants::kNotificationOrigin];
  EXPECT_FALSE(VerifyMacNotificationData(response_));

  // If however the origin is not present the response should be fine.
  [response_ removeObjectForKey:notification_constants::kNotificationOrigin];
  EXPECT_TRUE(VerifyMacNotificationData(response_));

  // Empty origin should be fine.
  [response_ setValue:@"" forKey:notification_constants::kNotificationOrigin];
  EXPECT_TRUE(VerifyMacNotificationData(response_));
}

TEST_F(NotificationPlatformBridgeMacUtilsTest,
       TestNotificationVerifyMissingIsAlert) {
  [response_ removeObjectForKey:notification_constants::kNotificationIsAlert];
  EXPECT_FALSE(VerifyMacNotificationData(response_));
}
