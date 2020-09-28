// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/AppKit.h>

#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"
#include "chrome/browser/notifications/notification_platform_bridge_mac_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/cocoa/notifications/notification_builder_mac.h"
#include "chrome/browser/ui/cocoa/notifications/notification_constants_mac.h"
#include "chrome/browser/ui/cocoa/notifications/notification_response_builder_mac.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/public/cpp/notification.h"

class NotificationPlatformBridgeMacUtilsTest
    : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    response_ = BuildDefaultNotificationResponse();
  }

 protected:
  NSMutableDictionary* BuildDefaultNotificationResponse() {
    return [NSMutableDictionary
        dictionaryWithDictionary:
            [NotificationResponseBuilder
                buildActivatedDictionary:BuildNotification()]];
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
    [builder setTag:@"tag1"];
    [builder setIcon:[NSImage imageNamed:@"NSApplicationIcon"]];
    [builder setNotificationId:@"notification_id"];
    [builder
        setProfileId:base::SysUTF8ToNSString(
                         NotificationPlatformBridge::GetProfileId(profile()))];
    [builder setIncognito:profile()->IsOffTheRecord()];
    [builder setCreatorPid:@(getpid())];
    [builder setNotificationType:
                 [NSNumber numberWithInteger:
                               static_cast<int>(
                                   NotificationHandler::Type::WEB_PERSISTENT)]];
    [builder setShowSettingsButton:true];

    return [builder buildUserNotification];
  }
};

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
