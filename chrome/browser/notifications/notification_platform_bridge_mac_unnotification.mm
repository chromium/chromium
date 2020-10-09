// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_mac_unnotification.h"

#import <UserNotifications/UserNotifications.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification_platform_bridge_mac_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/cocoa/notifications/unnotification_builder_mac.h"
#import "chrome/browser/ui/cocoa/notifications/unnotification_response_builder_mac.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_task_traits.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/message_center/public/cpp/notification.h"

@class UNMutableNotificationContent;
@class UNUserNotificationCenter;

namespace {

NSString* const kCloseAndSettingsCategory = @"CLOSE_AND_SETTINGS";
NSString* const kCloseCategory = @"CLOSE";

}  // namespace

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
  NotificationPlatformBridgeMacUNNotification::CreateDefaultCategories();
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

  // TODO(crbug/1136061): Add support for complex categories and move setting
  // the categories to the place that will be building the complex categories
  if (notification.should_show_settings_button())
    [content setCategoryIdentifier:kCloseAndSettingsCategory];
  else
    [content setCategoryIdentifier:kCloseCategory];

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
  NSString* candidateId = base::SysUTF8ToNSString(notification_id);
  NSString* currentProfileId = base::SysUTF8ToNSString(GetProfileId(profile));

  [notification_center_ getDeliveredNotificationsWithCompletionHandler:^(
                            NSArray<UNNotification*>* _Nonnull notifications) {
    for (UNNotification* notification in notifications) {
      NSString* toastId = [[[[notification request] content] userInfo]
          objectForKey:notification_constants::kNotificationId];
      NSString* persistentProfileId =
          [[[[notification request] content] userInfo]
              objectForKey:notification_constants::kNotificationProfileId];

      if ([toastId isEqualToString:candidateId] &&
          [persistentProfileId isEqualToString:currentProfileId]) {
        [notification_center_
            removeDeliveredNotificationsWithIdentifiers:@[ toastId ]];
        break;
      }
    }
  }];
  // TODO(crbug/1134539): If the notification was not present as a banner, check
  // alerts
}

void NotificationPlatformBridgeMacUNNotification::GetDisplayed(
    Profile* profile,
    GetDisplayedNotificationsCallback callback) const {
  bool incognito = profile->IsOffTheRecord();
  NSString* profileId = base::SysUTF8ToNSString(GetProfileId(profile));
  // Create a copyable version of the OnceCallback because ObjectiveC blocks
  // copy all referenced variables via copy constructor.
  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));

  [notification_center_ getDeliveredNotificationsWithCompletionHandler:^(
                            NSArray<UNNotification*>* _Nonnull notifications) {
    std::set<std::string> displayedNotifications;

    for (UNNotification* notification in notifications) {
      NSString* toastProfileId = [[[[notification request] content] userInfo]
          objectForKey:notification_constants::kNotificationProfileId];
      bool incognitoNotification = [[[[[notification request] content] userInfo]
          objectForKey:notification_constants::kNotificationIncognito]
          boolValue];

      if ([toastProfileId isEqualToString:profileId] &&
          incognito == incognitoNotification) {
        displayedNotifications.insert(
            base::SysNSStringToUTF8([[[[notification request] content] userInfo]
                objectForKey:notification_constants::kNotificationId]));
      }
    }
    // TODO(crbug/1134570): Query for displayed alerts as well

    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(copyable_callback, std::move(displayedNotifications),
                       true /* supports_synchronization */));
  }];
}

void NotificationPlatformBridgeMacUNNotification::SetReadyCallback(
    NotificationBridgeReadyCallback callback) {
  std::move(callback).Run(/*success=*/true);
}

void NotificationPlatformBridgeMacUNNotification::DisplayServiceShutDown(
    Profile* profile) {}

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

void NotificationPlatformBridgeMacUNNotification::CreateDefaultCategories() {
  UNNotificationAction* closeButton = [UNNotificationAction
      actionWithIdentifier:notification_constants::kNotificationCloseButtonTag
                     title:l10n_util::GetNSString(IDS_NOTIFICATION_BUTTON_CLOSE)
                   options:UNNotificationActionOptionNone];

  UNNotificationAction* settingsButton = [UNNotificationAction
      actionWithIdentifier:notification_constants::
                               kNotificationSettingsButtonTag
                     title:l10n_util::GetNSString(
                               IDS_NOTIFICATION_BUTTON_SETTINGS)
                   options:UNNotificationActionOptionForeground];

  // The actions in categories are ordered by LIFO. So having closeButton at the
  // end ensures that it is always the button on top.
  UNNotificationCategory* closeAndSettingsCategory = [UNNotificationCategory
      categoryWithIdentifier:kCloseAndSettingsCategory
                     actions:@[ settingsButton, closeButton ]
           intentIdentifiers:@[]
                     options:UNNotificationCategoryOptionCustomDismissAction];

  UNNotificationCategory* closeCategory = [UNNotificationCategory
      categoryWithIdentifier:kCloseCategory
                     actions:@[ closeButton ]
           intentIdentifiers:@[]
                     options:UNNotificationCategoryOptionCustomDismissAction];

  [notification_center_
      setNotificationCategories:[NSSet setWithObjects:closeAndSettingsCategory,
                                                      closeCategory, nil]];
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
