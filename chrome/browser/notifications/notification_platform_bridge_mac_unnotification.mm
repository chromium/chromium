// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_mac_unnotification.h"

#import <UserNotifications/UserNotifications.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/timer/timer.h"
#include "chrome/browser/notifications/notification_platform_bridge_mac_utils.h"
#include "chrome/browser/notifications/unnotification_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/cocoa/notifications/notification_operation.h"
#include "chrome/browser/ui/cocoa/notifications/unnotification_builder_mac.h"
#import "chrome/browser/ui/cocoa/notifications/unnotification_response_builder_mac.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_task_traits.h"
#include "third_party/blink/public/common/notifications/notification_constants.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/message_center/public/cpp/notification.h"

@class UNMutableNotificationContent;
@class UNUserNotificationCenter;

namespace {

// Defined timer used when synchronizing the notifications.
const base::TimeDelta kSynchronizationInterval =
    base::TimeDelta::FromMinutes(10);

}  // namespace

// This uses a private API so that updated banners do not keep reappearing on
// the screen, for example banners that are used to show progress would keep
// reappearing on the screen without the usage of this private API.
API_AVAILABLE(macosx(10.14))
@interface UNUserNotificationCenter (Private)
- (void)replaceContentForRequestWithIdentifier:(NSString*)arg1
                            replacementContent:
                                (UNMutableNotificationContent*)arg2
                             completionHandler:
                                 (void (^)(NSError* _Nullable error))arg3;
@end

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
      notification_center_([notification_center retain]),
      categories_([[NSMutableSet alloc] init]),
      delivered_categories_([[NSMutableDictionary alloc] init]),
      delivered_notifications_([[NSMutableDictionary alloc] init]) {
  [notification_center_ setDelegate:delegate_.get()];
  LogUNNotificationBannerPermissionStatus(notification_center_.get());
  LogUNNotificationBannerStyle(notification_center_.get());

  // TODO(crbug/1129366): Determine when to request permission.
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
  base::scoped_nsobject<UNNotificationBuilder> builder([[UNNotificationBuilder
      alloc]
      initWithCloseLabel:l10n_util::GetNSString(IDS_NOTIFICATION_BUTTON_CLOSE)
            optionsLabel:l10n_util::GetNSString(IDS_NOTIFICATION_BUTTON_MORE)
           settingsLabel:l10n_util::GetNSString(
                             IDS_NOTIFICATION_BUTTON_SETTINGS)]);

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

  if (!notification.icon().IsEmpty()) {
    // TODO(crbug/1138176): Resize images by adding a transparent border so that
    // its dimensions are uniform and do not get resized once sent to the
    // notification center.
    base::FilePath path =
        image_retainer_.RegisterTemporaryImage(notification.icon());
    [builder setIconPath:base::SysUTF8ToNSString(path.value())];
  }

  [builder setShowSettingsButton:notification.should_show_settings_button()];
  const std::vector<message_center::ButtonInfo>& buttons =
      notification.buttons();
  if (!buttons.empty()) {
    DCHECK_LE(buttons.size(), blink::kNotificationMaxActions);
    NSString* buttonOne = base::SysUTF16ToNSString(buttons[0].title);
    NSString* buttonTwo = nullptr;
    if (buttons.size() > 1)
      buttonTwo = base::SysUTF16ToNSString(buttons[1].title);
    [builder setButtons:buttonOne secondaryButton:buttonTwo];
  }

  NSString* notification_id = base::SysUTF8ToNSString(notification.id());

  [builder setOrigin:base::SysUTF8ToNSString(notification.origin_url().spec())];
  [builder setNotificationId:notification_id];
  [builder setProfileId:base::SysUTF8ToNSString(GetProfileId(profile))];
  [builder setIncognito:profile->IsOffTheRecord()];
  [builder setCreatorPid:[NSNumber numberWithInteger:static_cast<NSInteger>(
                                                         getpid())]];

  [builder
      setNotificationType:[NSNumber numberWithInteger:static_cast<NSInteger>(
                                                          notification_type)]];

  UNNotificationCategory* category = [builder buildCategory];

  // Check if this notification had an already existing category from a previous
  // call, if that is the case then remove it.
  if (UNNotificationCategory* existing =
          [delivered_categories_ objectForKey:notification_id]) {
    [categories_ removeObject:existing];
  }

  // This makes sure the map is always carrying the most recent category for
  // this notification.
  [delivered_categories_ setObject:category forKey:notification_id];
  [categories_ addObject:category];

  [notification_center_ setNotificationCategories:categories_];

  UNMutableNotificationContent* content = [builder buildUserNotification];

  base::WeakPtr<NotificationPlatformBridgeMacUNNotification> weak_ptr =
      weak_factory_.GetWeakPtr();

  void (^notification_delivered_block)(NSError* _Nullable) = ^(
      NSError* _Nullable error) {
    if (error != nil) {
      DVLOG(1) << "Notification request did not succeed";
      return;
    }
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &NotificationPlatformBridgeMacUNNotification::DeliveredSuccessfully,
            weak_ptr, std::move(builder)));
  };

  if ([notification_center_
          respondsToSelector:@selector
          (replaceContentForRequestWithIdentifier:
                               replacementContent:completionHandler:)]) {
    // If the notification has been delivered before, it will get updated in the
    // notification center. If it hasn't been delivered before it will deliver
    // it and show it on the screen.
    [notification_center_
        replaceContentForRequestWithIdentifier:notification_id
                            replacementContent:content
                             completionHandler:notification_delivered_block];
    return;
  }
  UNNotificationRequest* request =
      [UNNotificationRequest requestWithIdentifier:notification_id
                                           content:content
                                           trigger:nil];

  [notification_center_ addNotificationRequest:request
                         withCompletionHandler:notification_delivered_block];
}

void NotificationPlatformBridgeMacUNNotification::Close(
    Profile* profile,
    const std::string& notification_id) {
  NSString* candidateId = base::SysUTF8ToNSString(notification_id);
  NSString* currentProfileId = base::SysUTF8ToNSString(GetProfileId(profile));
  base::WeakPtr<NotificationPlatformBridgeMacUNNotification> weak_ptr =
      weak_factory_.GetWeakPtr();

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
        content::GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE,
            base::BindOnce(
                &NotificationPlatformBridgeMacUNNotification::DoClose, weak_ptr,
                base::SysNSStringToUTF8(toastId)));
        break;
      }
    }
  }];
  // TODO(crbug/1134539): If the notification was not present as a banner, check
  // alerts.
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
    // TODO(crbug/1134570): Query for displayed alerts as well.

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
                        DVLOG(1) << "Requesting permission did not succeed";
                      }
                    }];
}

void NotificationPlatformBridgeMacUNNotification::DoClose(
    const std::string& notification_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  NSString* toast_id = base::SysUTF8ToNSString(notification_id);
  [notification_center_
      removeDeliveredNotificationsWithIdentifiers:@[ toast_id ]];

  // Remove the category of the closed notification, and remove it from
  // |delivered_notifications_|.
  [categories_ removeObject:[delivered_categories_ objectForKey:toast_id]];
  [delivered_categories_ removeObjectForKey:toast_id];
  [delivered_notifications_ removeObjectForKey:toast_id];
}

void NotificationPlatformBridgeMacUNNotification::DeliveredSuccessfully(
    base::scoped_nsobject<UNNotificationBuilder> builder) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  NSDictionary* dict = [builder buildDictionary];

  [delivered_notifications_
      setObject:dict
         forKey:[dict objectForKey:notification_constants::kNotificationId]];

  NotificationPlatformBridgeMacUNNotification::MaybeStartSynchronization();
}

void NotificationPlatformBridgeMacUNNotification::MaybeStartSynchronization() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (synchronize_displayed_notifications_timer_.IsRunning())
    return;

  // Using base::Unretained is safe here as the timer is a member of the class,
  // so destroying the bridge destroys the timer as well.
  synchronize_displayed_notifications_timer_.Start(
      FROM_HERE, kSynchronizationInterval,
      base::BindRepeating(&NotificationPlatformBridgeMacUNNotification::
                              SynchronizeNotifications,
                          base::Unretained(this)));
}

void NotificationPlatformBridgeMacUNNotification::SynchronizeNotifications() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::WeakPtr<NotificationPlatformBridgeMacUNNotification> weak_ptr =
      weak_factory_.GetWeakPtr();

  [notification_center_ getDeliveredNotificationsWithCompletionHandler:^(
                            NSArray<UNNotification*>* _Nonnull notifications) {
    base::flat_set<std::string> notification_ids;

    for (UNNotification* notification in notifications) {
      std::string notification_id =
          base::SysNSStringToUTF8([[[[notification request] content] userInfo]
              objectForKey:notification_constants::kNotificationId]);
      notification_ids.insert(std::move(notification_id));
    }

    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&NotificationPlatformBridgeMacUNNotification::
                                      DoSynchronizeNotifications,
                                  weak_ptr, std::move(notification_ids)));
  }];
}

void NotificationPlatformBridgeMacUNNotification::DoSynchronizeNotifications(
    base::flat_set<std::string> notification_ids) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::scoped_nsobject<NSMutableDictionary> remaining_notifications(
      [[NSMutableDictionary alloc] init]);

  for (const std::string& notification_id_utf8 : notification_ids) {
    NSString* notification_id_ns =
        base::SysUTF8ToNSString(notification_id_utf8);
    NSDictionary* notification_dictionary =
        [delivered_notifications_ objectForKey:notification_id_ns];

    if (!notification_dictionary)
      continue;

    [remaining_notifications setObject:notification_dictionary
                                forKey:notification_id_ns];
    [delivered_notifications_ removeObjectForKey:notification_id_ns];
  }

  for (NSString* notification_id in delivered_notifications_.get()) {
    base::scoped_nsobject<NSMutableDictionary> dict(
        [[delivered_notifications_ objectForKey:notification_id] mutableCopy]);

    // Remove the category of the dismissed notification.
    [categories_
        removeObject:[delivered_categories_ objectForKey:notification_id]];
    [delivered_categories_ removeObjectForKey:notification_id];

    // Closed notifications need to carry
    // NotificationOperation::NOTIFICATION_CLOSE and an invalid button index.
    // TODO(crbug/1141869): Modify the builder so that it sets these values by
    // default.
    [dict
        setObject:@(static_cast<int>(NotificationOperation::NOTIFICATION_CLOSE))
           forKey:notification_constants::kNotificationOperation];
    [dict setObject:@(notification_constants::kNotificationInvalidButtonIndex)
             forKey:notification_constants::kNotificationButtonIndex];

    ProcessMacNotificationResponse(dict.autorelease());
  }

  delivered_notifications_.reset(remaining_notifications);

  if (notification_ids.empty())
    synchronize_displayed_notifications_timer_.Stop();
}

// /////////////////////////////////////////////////////////////////////////////
@implementation UNNotificationCenterDelegate

- (void)userNotificationCenter:(UNUserNotificationCenter*)center
       willPresentNotification:(UNNotification*)notification
         withCompletionHandler:
             (void (^)(UNNotificationPresentationOptions options))
                 completionHandler {
  // receiving a notification when the app is in the foreground.
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
