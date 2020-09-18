// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_mac_unnotification.h"

#import <UserNotifications/UserNotifications.h>

#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/message_center/public/cpp/notification.h"

@class NSNotification;
@class UNMutableNotificationContent;
@class UNUserNotificationCenter;

// A Cocoa class that represents the delegate of UNUserNotificationCenter and
// can forward commands to C++.
API_AVAILABLE(macosx(10.14))
@interface UNNotificationCenterDelegate
    : NSObject <UNUserNotificationCenterDelegate> {
}
@end

NotificationPlatformBridgeMacUNNotification::
    NotificationPlatformBridgeMacUNNotification()
    : NotificationPlatformBridgeMacUNNotification(
          [UNUserNotificationCenter currentNotificationCenter]) {}

NotificationPlatformBridgeMacUNNotification::
    NotificationPlatformBridgeMacUNNotification(
        UNUserNotificationCenter* notification_center)
    : delegate_([UNNotificationCenterDelegate alloc]),
      notification_center_([notification_center retain]) {
  [notification_center_ setDelegate:delegate_.get()];

  // TODO(crbug/1129366): Determine when to request permission
  NotificationPlatformBridgeMacUNNotification::RequestPermission();
}

NotificationPlatformBridgeMacUNNotification::
    ~NotificationPlatformBridgeMacUNNotification() {
  [notification_center_ setDelegate:nil];
  [notification_center_ removeAllDeliveredNotifications];
}

void NotificationPlatformBridgeMacUNNotification::Display(
    NotificationHandler::Type notification_type,
    Profile* profile,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> metadata) {
  base::scoped_nsobject<UNMutableNotificationContent> content(
      [[UNMutableNotificationContent alloc] init]);

  [content setTitle:base::SysUTF16ToNSString(notification.title())];

  base::string16 context_message = notification.message();
  [content setBody:base::SysUTF16ToNSString(context_message)];

  [content
      setSubtitle:base::SysUTF8ToNSString(notification.origin_url().spec())];

  // TODO(crbug/1129398): Add close button to banners
  UNNotificationRequest* request = [UNNotificationRequest
      requestWithIdentifier:base::SysUTF8ToNSString(notification.id())
                    content:content
                    trigger:nil];

  [notification_center_ addNotificationRequest:request
                         withCompletionHandler:^(NSError* _Nullable error) {
                           if (error != nil) {
                             LOG(WARNING)
                                 << "Notification request did not succeed";
                           }
                         }];
}

void NotificationPlatformBridgeMacUNNotification::Close(
    Profile* profile,
    const std::string& notification_id) {
  NOTIMPLEMENTED();
}

void NotificationPlatformBridgeMacUNNotification::GetDisplayed(
    Profile* profile,
    GetDisplayedNotificationsCallback callback) const {
  NOTIMPLEMENTED();
  std::move(callback).Run(/*notification_ids=*/{}, /*supports_sync=*/false);
}

void NotificationPlatformBridgeMacUNNotification::SetReadyCallback(
    NotificationBridgeReadyCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(/*success=*/true);
}

void NotificationPlatformBridgeMacUNNotification::DisplayServiceShutDown(
    Profile* profile) {
  NOTIMPLEMENTED();
}

void NotificationPlatformBridgeMacUNNotification::RequestPermission() {
  UNAuthorizationOptions authOptions = UNAuthorizationOptionAlert |
                                       UNAuthorizationOptionSound |
                                       UNAuthorizationOptionBadge;

  [notification_center_
      requestAuthorizationWithOptions:authOptions
                    completionHandler:^(BOOL granted,
                                        NSError* _Nullable error) {
                      if (error != nil) {
                        LOG(WARNING) << "Requesting permission did not succeed";
                      }
                    }];
}

// /////////////////////////////////////////////////////////////////////////////
@implementation UNNotificationCenterDelegate

- (void)userNotificationCenter:(UNUserNotificationCenter*)center
       willPresentNotification:(UNNotification*)notification
         withCompletionHandler:
             (void (^)(UNNotificationPresentationOptions options))
                 completionHandler {
  // receiving a notification when the app is in the foreground
  UNNotificationPresentationOptions presentationOptions =
      UNNotificationPresentationOptionSound |
      UNNotificationPresentationOptionAlert |
      UNNotificationPresentationOptionBadge;

  completionHandler(presentationOptions);
}

- (void)userNotificationCenter:(UNUserNotificationCenter*)center
    didReceiveNotificationResponse:(UNNotificationResponse*)response
             withCompletionHandler:(void (^)(void))completionHandler {
  completionHandler();
}

@end