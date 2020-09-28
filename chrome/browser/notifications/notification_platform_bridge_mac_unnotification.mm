// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_mac_unnotification.h"

#import <UserNotifications/UserNotifications.h>

#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification_platform_bridge_mac_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/cocoa/notifications/unnotification_builder_mac.h"
#import "chrome/browser/ui/cocoa/notifications/unnotification_response_builder_mac.h"
#include "ui/message_center/public/cpp/notification.h"

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
  base::scoped_nsobject<UNNotificationBuilder> builder(
      [[UNNotificationBuilder alloc] init]);

  base::string16 context_message =
      notification.items().empty()
          ? notification.message()
          : (notification.items().at(0).title + base::UTF8ToUTF16(" - ") +
             notification.items().at(0).message);

  bool requires_attribution =
      notification.context_message().empty() &&
      notification_type != NotificationHandler::Type::EXTENSION;

  [builder setTitle:base::SysUTF16ToNSString(
                        CreateMacNotificationTitle(notification))];
  [builder setContextMessage:base::SysUTF16ToNSString(context_message)];
  [builder setSubTitle:base::SysUTF16ToNSString(CreateMacNotificationContext(
                           /*is_persistent=*/false, notification,
                           requires_attribution))];

  [builder setOrigin:base::SysUTF8ToNSString(notification.origin_url().spec())];
  [builder setNotificationId:base::SysUTF8ToNSString(notification.id())];
  [builder setProfileId:base::SysUTF8ToNSString(GetProfileId(profile))];
  [builder setIncognito:profile->IsOffTheRecord()];
  [builder setCreatorPid:[NSNumber numberWithInteger:static_cast<NSInteger>(
                                                         getpid())]];

  [builder
      setNotificationType:[NSNumber numberWithInteger:static_cast<NSInteger>(
                                                          notification_type)]];

  UNMutableNotificationContent* content = [builder buildUserNotification];

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
  NSDictionary* notificationResponse =
      [UNNotificationResponseBuilder buildDictionary:response];
  ProcessMacNotificationResponse(notificationResponse);
  completionHandler();
}

@end
