// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_mac_unnotification.h"

#include <vector>

#import <UserNotifications/UserNotifications.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/timer/timer.h"
#include "chrome/browser/notifications/notification_platform_bridge_mac_metrics.h"
#include "chrome/browser/notifications/notification_platform_bridge_mac_utils.h"
#include "chrome/browser/notifications/unnotification_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/cocoa/notifications/unnotification_builder_mac.h"
#import "chrome/browser/ui/cocoa/notifications/unnotification_response_builder_mac.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/services/mac_notifications/public/cpp/notification_operation.h"
#include "chrome/services/mac_notifications/public/cpp/notification_utils_mac.h"
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
    : NSObject <UNUserNotificationCenterDelegate>
- (instancetype)initWithNotificationClosedHandler:
    (base::RepeatingCallback<void(std::string)>)onNotificationClosed;
@end

NotificationPlatformBridgeMacUNNotification::
    NotificationPlatformBridgeMacUNNotification(
        UNUserNotificationCenter* notification_center,
        id<AlertDispatcher> alert_dispatcher)
    : notification_center_([notification_center retain]),
      alert_dispatcher_([alert_dispatcher retain]),
      delivered_notifications_([[NSMutableDictionary alloc] init]),
      category_manager_(notification_center) {
  delegate_.reset([[UNNotificationCenterDelegate alloc]
      initWithNotificationClosedHandler:
          base::BindRepeating(&NotificationPlatformBridgeMacUNNotification::
                                  OnNotificationClosed,
                              weak_factory_.GetWeakPtr())]);
  [notification_center_ setDelegate:delegate_.get()];
  LogUNNotificationBannerPermissionStatus(notification_center_.get());
  LogUNNotificationBannerStyle(notification_center_.get());

  // TODO(crbug/1129366): Determine when to request permission.
  RequestPermission();
}

NotificationPlatformBridgeMacUNNotification::
    ~NotificationPlatformBridgeMacUNNotification() {
  [notification_center_ setDelegate:nil];
  [notification_center_ removeAllDeliveredNotifications];
  [alert_dispatcher_ closeAllNotifications];
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

  std::u16string context_message =
      notification.items().empty()
          ? notification.message()
          : (notification.items().at(0).title + u" - " +
             notification.items().at(0).message);

  bool is_alert = IsAlertNotificationMac(notification);

  bool requires_attribution =
      notification.context_message().empty() &&
      notification_type != NotificationHandler::Type::EXTENSION;

  [builder setTitle:base::SysUTF16ToNSString(
                        CreateMacNotificationTitle(notification))];
  [builder setContextMessage:base::SysUTF16ToNSString(context_message)];
  [builder setSubTitle:base::SysUTF16ToNSString(CreateMacNotificationContext(
                           is_alert, notification, requires_attribution))];

  if (!notification.icon().IsEmpty()) {
    // TODO(crbug/1138176): Resize images by adding a transparent border so that
    // its dimensions are uniform and do not get resized once sent to the
    // notification center.
    if (is_alert) {
      [builder setIcon:notification.icon().ToNSImage()];
    } else {
      base::FilePath path =
          image_retainer_.RegisterTemporaryImage(notification.icon());
      [builder setIconPath:base::SysUTF8ToNSString(path.value())];
    }
  }

  [builder setRenotify:notification.renotify()];
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

  [builder setOrigin:base::SysUTF8ToNSString(notification.origin_url().spec())];
  [builder setNotificationId:base::SysUTF8ToNSString(notification.id())];
  [builder setProfileId:base::SysUTF8ToNSString(GetProfileId(profile))];
  [builder setIncognito:profile->IsOffTheRecord()];
  [builder setCreatorPid:[NSNumber numberWithInteger:static_cast<NSInteger>(
                                                         getpid())]];
  [builder
      setNotificationType:[NSNumber numberWithInteger:static_cast<NSInteger>(
                                                          notification_type)]];

  std::string system_notification_id = DeriveMacNotificationId(
      profile->IsOffTheRecord(), GetProfileId(profile), notification.id());
  NSString* notification_id = base::SysUTF8ToNSString(system_notification_id);
  [builder setIdentifier:notification_id];

  if (is_alert) {
    LogMacNotificationDelivered(is_alert, /*success=*/true);
    NSDictionary* dict = [builder buildDictionary];
    [alert_dispatcher_ dispatchNotification:dict];
    [builder setClosedFromAlert:YES];
    DeliveredSuccessfully(system_notification_id, std::move(builder));
    return;
  }

  // Create a new category from the desired action buttons.
  std::vector<std::u16string> button_titles;
  for (const message_center::ButtonInfo& button : notification.buttons())
    button_titles.push_back(button.title);
  NSString* category = category_manager_.GetOrCreateCategory(
      system_notification_id, button_titles,
      notification.should_show_settings_button());

  UNMutableNotificationContent* content = [builder buildUserNotification];
  [content setCategoryIdentifier:category];

  base::WeakPtr<NotificationPlatformBridgeMacUNNotification> weak_ptr =
      weak_factory_.GetWeakPtr();

  void (^notification_delivered_block)(NSError* _Nullable) = ^(
      NSError* _Nullable error) {
    LogMacNotificationDelivered(is_alert, /*success=*/!error);
    if (error != nil) {
      DVLOG(1) << "Notification request did not succeed";
      return;
    }
    [builder setClosedFromAlert:NO];
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &NotificationPlatformBridgeMacUNNotification::DeliveredSuccessfully,
            weak_ptr, system_notification_id, std::move(builder)));
  };

  // If the renotify is not set try to replace the notification silently.
  bool should_replace = !notification.renotify();
  bool can_replace = [notification_center_
      respondsToSelector:@selector
      (replaceContentForRequestWithIdentifier:
                           replacementContent:completionHandler:)];
  if (should_replace && can_replace) {
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
  NSString* original_notification_id = base::SysUTF8ToNSString(notification_id);
  std::string system_notification_id = DeriveMacNotificationId(
      profile->IsOffTheRecord(), GetProfileId(profile), notification_id);
  NSString* system_notification_id_ns =
      base::SysUTF8ToNSString(system_notification_id);
  base::WeakPtr<NotificationPlatformBridgeMacUNNotification> weak_ptr =
      weak_factory_.GetWeakPtr();

  [notification_center_ getDeliveredNotificationsWithCompletionHandler:^(
                            NSArray<UNNotification*>* _Nonnull notifications) {
    for (UNNotification* notification in notifications) {
      NSString* toast_notification_id = [[notification request] identifier];
      if ([system_notification_id_ns isEqualToString:toast_notification_id]) {
        [notification_center_ removeDeliveredNotificationsWithIdentifiers:@[
          toast_notification_id
        ]];
        return;
      }
    }

    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &NotificationPlatformBridgeMacUNNotification::DoCloseAlert,
            weak_ptr, profile,
            base::SysNSStringToUTF8(original_notification_id)));
  }];

  OnNotificationClosed(std::move(system_notification_id));
  [delivered_notifications_ removeObjectForKey:system_notification_id_ns];
}

void NotificationPlatformBridgeMacUNNotification::GetDisplayed(
    Profile* profile,
    GetDisplayedNotificationsCallback callback) const {
  GetDisplayedNotificationsCallback alerts_callback = base::BindOnce(
      &NotificationPlatformBridgeMacUNNotification::DidGetDisplayedAlerts,
      weak_factory_.GetWeakPtr(), profile, std::move(callback));
  [alert_dispatcher_
      getDisplayedAlertsForProfileId:base::SysUTF8ToNSString(
                                         GetProfileId(profile))
                           incognito:profile && profile->IsOffTheRecord()
                            callback:std::move(alerts_callback)];
}

void NotificationPlatformBridgeMacUNNotification::SetReadyCallback(
    NotificationBridgeReadyCallback callback) {
  std::move(callback).Run(/*success=*/true);
}

void NotificationPlatformBridgeMacUNNotification::DisplayServiceShutDown(
    Profile* profile) {
  // Close all alerts and banners for |profile| on shutdown. We have to clean up
  // here instead of the destructor as mojo messages won't be delivered from
  // there as it's too late in the shutdown process. If the profile is null it
  // was the SystemNotificationHelper instance but we never show notifications
  // without a profile (Type::TRANSIENT) on macOS, so nothing to do here.
  if (profile)
    CloseAllNotificationsForProfile(profile);
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
                        DVLOG(1) << "Requesting permission did not succeed";
                      }
                    }];
}

void NotificationPlatformBridgeMacUNNotification::DoCloseAlert(
    Profile* profile,
    const std::string& notification_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  NSString* notificationId = base::SysUTF8ToNSString(notification_id);
  NSString* profileId = base::SysUTF8ToNSString(GetProfileId(profile));
  bool incognito = profile->IsOffTheRecord();

  [alert_dispatcher_ closeNotificationWithId:notificationId
                                   profileId:profileId
                                   incognito:incognito];
}

void NotificationPlatformBridgeMacUNNotification::OnNotificationClosed(
    std::string notification_id) {
  category_manager_.ReleaseCategory(notification_id);
}

void NotificationPlatformBridgeMacUNNotification::DeliveredSuccessfully(
    const std::string& notification_id,
    base::scoped_nsobject<UNNotificationBuilder> builder) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  NSDictionary* dict = [builder buildDictionary];

  [delivered_notifications_ setObject:dict
                               forKey:base::SysUTF8ToNSString(notification_id)];

  MaybeStartSynchronization();
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

  // TODO(crbug.com/1127306): Skip the |alert_dispatcher_| if it is using the
  // NSUserNotification API as it can handle close events.
  [alert_dispatcher_
      getAllDisplayedAlertsWithCallback:
          base::BindOnce(&NotificationPlatformBridgeMacUNNotification::
                             DidGetAllDisplayedAlerts,
                         weak_factory_.GetWeakPtr())];
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
    OnNotificationClosed(base::SysNSStringToUTF8(notification_id));
    ProcessMacNotificationResponse(
        [delivered_notifications_ objectForKey:notification_id]);
  }

  delivered_notifications_.reset(remaining_notifications);

  if (notification_ids.empty())
    synchronize_displayed_notifications_timer_.Stop();
}

void NotificationPlatformBridgeMacUNNotification::DidGetDisplayedAlerts(
    Profile* profile,
    GetDisplayedNotificationsCallback callback,
    std::set<std::string> alert_ids,
    bool supports_synchronization) {
  // Move |callback| into block storage so we can use it from the block below.
  __block GetDisplayedNotificationsCallback block_callback =
      std::move(callback);
  bool incognito = profile->IsOffTheRecord();
  NSString* profileId = base::SysUTF8ToNSString(GetProfileId(profile));

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

    for (const std::string& alert_id : alert_ids)
      displayedNotifications.insert(alert_id);

    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(block_callback),
                                  std::move(displayedNotifications),
                                  supports_synchronization));
  }];
}

void NotificationPlatformBridgeMacUNNotification::DidGetAllDisplayedAlerts(
    base::flat_set<MacNotificationIdentifier> alert_ids) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::WeakPtr<NotificationPlatformBridgeMacUNNotification> weak_ptr =
      weak_factory_.GetWeakPtr();

  [notification_center_ getDeliveredNotificationsWithCompletionHandler:^(
                            NSArray<UNNotification*>* _Nonnull notifications) {
    std::vector<std::string> notification_ids;
    notification_ids.reserve([notifications count] + alert_ids.size());

    for (UNNotification* notification in notifications) {
      std::string notification_id =
          base::SysNSStringToUTF8([[notification request] identifier]);
      notification_ids.push_back(std::move(notification_id));
    }

    for (const MacNotificationIdentifier& alert_id : alert_ids) {
      notification_ids.push_back(DeriveMacNotificationId(
          alert_id.incognito, alert_id.profile_id, alert_id.notification_id));
    }

    base::flat_set<std::string> all_ids(std::move(notification_ids));
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&NotificationPlatformBridgeMacUNNotification::
                                      DoSynchronizeNotifications,
                                  weak_ptr, std::move(all_ids)));
  }];
}

void NotificationPlatformBridgeMacUNNotification::
    CloseAllNotificationsForProfile(Profile* profile) {
  DCHECK(profile);
  NSString* profile_id = base::SysUTF8ToNSString(GetProfileId(profile));
  bool incognito = profile->IsOffTheRecord();

  [alert_dispatcher_ closeNotificationsWithProfileId:profile_id
                                           incognito:incognito];

  // Filter and close banner notifications for the profile.
  [notification_center_ getDeliveredNotificationsWithCompletionHandler:^(
                            NSArray<UNNotification*>* _Nonnull notifications) {
    base::scoped_nsobject<NSMutableArray> identifiers_to_close(
        [[NSMutableArray alloc] init]);

    for (UNNotification* notification in notifications) {
      NSString* toast_profile_id = [[[[notification request] content] userInfo]
          objectForKey:notification_constants::kNotificationProfileId];
      bool toast_incognito = [[[[[notification request] content] userInfo]
          objectForKey:notification_constants::kNotificationIncognito]
          boolValue];

      if ([profile_id isEqualToString:toast_profile_id] &&
          incognito == toast_incognito) {
        [identifiers_to_close addObject:[[notification request] identifier]];
      }
    }

    [notification_center_
        removeDeliveredNotificationsWithIdentifiers:identifiers_to_close];
  }];

  // Clean up stored notifications and their categories.
  NSString* profile_prefix = base::SysUTF8ToNSString(
      DeriveMacNotificationId(incognito, GetProfileId(profile),
                              /*notification_id=*/std::string()));

  for (NSString* identifier in [delivered_notifications_ allKeys]) {
    if (![identifier hasPrefix:profile_prefix])
      continue;
    [delivered_notifications_ removeObjectForKey:identifier];
    OnNotificationClosed(base::SysNSStringToUTF8(identifier));
  }
}

// /////////////////////////////////////////////////////////////////////////////
@implementation UNNotificationCenterDelegate {
  base::RepeatingCallback<void(std::string)> _onNotificationClosed;
}

- (instancetype)initWithNotificationClosedHandler:
    (base::RepeatingCallback<void(std::string)>)onNotificationClosed {
  if ((self = [super init])) {
    _onNotificationClosed = std::move(onNotificationClosed);
  }
  return self;
}

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
      [UNNotificationResponseBuilder buildDictionary:response fromAlert:NO];

  // Notify platform bridge about closed notifications for cleanup tasks.
  int operation = [[notificationResponse
      objectForKey:notification_constants::kNotificationOperation] intValue];
  if (operation ==
      static_cast<int>(NotificationOperation::NOTIFICATION_CLOSE)) {
    std::string notificationId =
        base::SysNSStringToUTF8([[[response notification] request] identifier]);
    _onNotificationClosed.Run(std::move(notificationId));
  }

  ProcessMacNotificationResponse(notificationResponse);
  completionHandler();
}

@end
