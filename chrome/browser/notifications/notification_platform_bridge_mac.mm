// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_mac.h"
#include "chrome/browser/notifications/notification_platform_bridge_mac_unnotification.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_mach_port.h"
#include "base/mac/scoped_nsobject.h"
#include "base/optional.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/notifications/alert_dispatcher_xpc.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/notification_platform_bridge_mac_utils.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/cocoa/notifications/notification_builder_mac.h"
#include "chrome/browser/ui/cocoa/notifications/notification_constants_mac.h"
#import "chrome/browser/ui/cocoa/notifications/notification_response_builder_mac.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/blink/public/common/notifications/notification_constants.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "url/gurl.h"

@class NSUserNotification;
@class NSUserNotificationCenter;

// The mapping from web notifications to NsUserNotification works as follows

// notification#title in NSUserNotification.title
// notification#message in NSUserNotification.informativeText
// notification#context_message in NSUserNotification.subtitle
// notification#id in NSUserNotification.identifier (10.9)
// notification#icon in NSUserNotification.contentImage (10.9)
// Site settings button is implemented as NSUserNotification's action button
// Not easy to implement:
// -notification.requireInteraction

// TODO(miguelg) implement the following features
// - Sound names can be implemented by setting soundName in NSUserNotification
//   NSUserNotificationDefaultSoundName gives you the platform default.

// A Cocoa class that represents the delegate of NSUserNotificationCenter and
// can forward commands to C++.
@interface NotificationCenterDelegate
    : NSObject<NSUserNotificationCenterDelegate> {
}
@end

// /////////////////////////////////////////////////////////////////////////////
NotificationPlatformBridgeMac::NotificationPlatformBridgeMac(
    NSUserNotificationCenter* notification_center,
    id<AlertDispatcher> alert_dispatcher)
    : delegate_([NotificationCenterDelegate alloc]),
      notification_center_([notification_center retain]),
      alert_dispatcher_([alert_dispatcher retain]) {
  [notification_center_ setDelegate:delegate_.get()];
}

NotificationPlatformBridgeMac::~NotificationPlatformBridgeMac() {
  [notification_center_ setDelegate:nil];

  // TODO(miguelg) do not remove banners if possible.
  [notification_center_ removeAllDeliveredNotifications];
  [alert_dispatcher_ closeAllNotifications];
}

// static
std::unique_ptr<NotificationPlatformBridge>
NotificationPlatformBridge::Create() {
  if (@available(macOS 10.14, *)) {
    if (base::FeatureList::IsEnabled(features::kNewMacNotificationAPI)) {
      return std::make_unique<NotificationPlatformBridgeMacUNNotification>();
    }
  }
  // TODO(crbug.com/1170731): Use a helper app to display alerts.
  base::scoped_nsobject<NSObject<AlertDispatcher>> alert_dispatcher(
      [[AlertDispatcherXPC alloc] init]);
  return std::make_unique<NotificationPlatformBridgeMac>(
      [NSUserNotificationCenter defaultUserNotificationCenter],
      alert_dispatcher.get());
}

// static
bool NotificationPlatformBridge::CanHandleType(
    NotificationHandler::Type notification_type) {
  return notification_type != NotificationHandler::Type::TRANSIENT;
}

void NotificationPlatformBridgeMac::Display(
    NotificationHandler::Type notification_type,
    Profile* profile,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> metadata) {
  base::scoped_nsobject<NotificationBuilder> builder(
      [[NotificationBuilder alloc]
      initWithCloseLabel:l10n_util::GetNSString(IDS_NOTIFICATION_BUTTON_CLOSE)
            optionsLabel:l10n_util::GetNSString(IDS_NOTIFICATION_BUTTON_MORE)
           settingsLabel:l10n_util::GetNSString(
                             IDS_NOTIFICATION_BUTTON_SETTINGS)]);

  [builder setTitle:base::SysUTF16ToNSString(
                        CreateMacNotificationTitle(notification))];

  base::string16 context_message =
      notification.items().empty()
          ? notification.message()
          : (notification.items().at(0).title + base::UTF8ToUTF16(" - ") +
             notification.items().at(0).message);

  [builder setContextMessage:base::SysUTF16ToNSString(context_message)];

  bool requires_attribution =
      notification.context_message().empty() &&
      notification_type != NotificationHandler::Type::EXTENSION;

  bool is_alert = IsAlertNotificationMac(notification);

  [builder setSubTitle:base::SysUTF16ToNSString(CreateMacNotificationContext(
                           is_alert, notification, requires_attribution))];

  if (!notification.icon().IsEmpty()) {
    // TODO(crbug/1138176): Resize images by adding a transparent border so that
    // its dimensions are uniform and do not get resized once sent to the
    // notification center
    [builder setIcon:notification.icon().ToNSImage()];
  }

  [builder setShowSettingsButton:(notification.should_show_settings_button())];
  std::vector<message_center::ButtonInfo> buttons = notification.buttons();
  if (!buttons.empty()) {
    DCHECK_LE(buttons.size(), blink::kNotificationMaxActions);
    NSString* buttonOne = base::SysUTF16ToNSString(buttons[0].title);
    NSString* buttonTwo = nullptr;
    if (buttons.size() > 1)
      buttonTwo = base::SysUTF16ToNSString(buttons[1].title);
    [builder setButtons:buttonOne secondaryButton:buttonTwo];
  }

  std::string identifier = DeriveMacNotificationId(
      profile->IsOffTheRecord(), GetProfileId(profile), notification.id());
  [builder setIdentifier:base::SysUTF8ToNSString(identifier)];

  // If renotify is needed, delete the notification with the same id
  // from the notification center before displaying this one.
  if (notification.renotify())
    Close(profile, notification.id());

  [builder setOrigin:base::SysUTF8ToNSString(notification.origin_url().spec())];
  [builder setNotificationId:base::SysUTF8ToNSString(notification.id())];
  [builder setProfileId:base::SysUTF8ToNSString(GetProfileId(profile))];
  [builder setIncognito:profile->IsOffTheRecord()];
  [builder setCreatorPid:[NSNumber numberWithInteger:static_cast<NSInteger>(
                                                         getpid())]];
  [builder
      setNotificationType:[NSNumber numberWithInteger:static_cast<NSInteger>(
                                                          notification_type)]];

  // Send alert notifications to the alert dispatcher. Chrome itself can only
  // display banners.
  if (is_alert) {
    NSDictionary* dict = [builder buildDictionary];
    [alert_dispatcher_ dispatchNotification:dict];
  } else {
    NSUserNotification* toast = [builder buildUserNotification];
    [notification_center_ deliverNotification:toast];
  }
}

void NotificationPlatformBridgeMac::Close(Profile* profile,
                                          const std::string& notification_id) {
  NSString* notificationId = base::SysUTF8ToNSString(notification_id);
  NSString* profileId = base::SysUTF8ToNSString(GetProfileId(profile));
  bool incognito = profile->IsOffTheRecord();

  for (NSUserNotification* toast in
       [notification_center_ deliveredNotifications]) {
    NSString* toastId =
        [toast.userInfo objectForKey:notification_constants::kNotificationId];
    NSString* toastProfileId = [toast.userInfo
        objectForKey:notification_constants::kNotificationProfileId];
    BOOL toastIncognito = [[toast.userInfo
        objectForKey:notification_constants::kNotificationIncognito] boolValue];

    if ([notificationId isEqualToString:toastId] &&
        [profileId isEqualToString:toastProfileId] &&
        incognito == toastIncognito) {
      [notification_center_ removeDeliveredNotification:toast];
      return;
    }
  }

  // If no banner existed with that ID try to see if there is an alert
  // in the alert dispatcher.
  [alert_dispatcher_ closeNotificationWithId:notificationId
                                   profileId:profileId
                                   incognito:incognito];
}

void NotificationPlatformBridgeMac::GetDisplayed(
    Profile* profile,
    GetDisplayedNotificationsCallback callback) const {
  NSString* profileId = base::SysUTF8ToNSString(GetProfileId(profile));
  bool incognito = profile->IsOffTheRecord();
  std::set<std::string> banners;

  for (NSUserNotification* toast in
       [notification_center_ deliveredNotifications]) {
    NSString* toastProfileId = [toast.userInfo
        objectForKey:notification_constants::kNotificationProfileId];
    BOOL toastIncognito = [[toast.userInfo
        objectForKey:notification_constants::kNotificationIncognito] boolValue];

    if ([profileId isEqualToString:toastProfileId] &&
        incognito == toastIncognito) {
      banners.insert(base::SysNSStringToUTF8([toast.userInfo
          objectForKey:notification_constants::kNotificationId]));
    }
  }

  GetDisplayedNotificationsCallback alerts_callback = base::BindOnce(
      [](GetDisplayedNotificationsCallback callback,
         std::set<std::string> banners, std::set<std::string> alerts,
         bool supports_synchronization) {
        // Merge banner and alert notification ids.
        banners.insert(alerts.begin(), alerts.end());
        std::move(callback).Run(std::move(banners), supports_synchronization);
      },
      std::move(callback), std::move(banners));

  [alert_dispatcher_ getDisplayedAlertsForProfileId:profileId
                                          incognito:incognito
                                           callback:std::move(alerts_callback)];
}

void NotificationPlatformBridgeMac::SetReadyCallback(
    NotificationBridgeReadyCallback callback) {
  std::move(callback).Run(true);
}

void NotificationPlatformBridgeMac::DisplayServiceShutDown(Profile* profile) {}

// /////////////////////////////////////////////////////////////////////////////
@implementation NotificationCenterDelegate
- (void)userNotificationCenter:(NSUserNotificationCenter*)center
       didActivateNotification:(NSUserNotification*)notification {
  NSDictionary* notificationResponse =
      [NotificationResponseBuilder buildActivatedDictionary:notification];
  ProcessMacNotificationResponse(notificationResponse);
}

// Overriden from _NSUserNotificationCenterDelegatePrivate.
// Emitted when a user clicks the "Close" button in the notification.
// It not is emitted if the notification is closed from the notification
// center or if the app is not running at the time the Close button is
// pressed so it's essentially just a best effort way to detect
// notifications closed by the user.
- (void)userNotificationCenter:(NSUserNotificationCenter*)center
               didDismissAlert:(NSUserNotification*)notification {
  NSDictionary* notificationResponse =
      [NotificationResponseBuilder buildDismissedDictionary:notification];
  ProcessMacNotificationResponse(notificationResponse);
}

// Overriden from _NSUserNotificationCenterDelegatePrivate.
// Emitted when a user closes a notification from the notification center.
// This is an undocumented method introduced in 10.8 according to
// https://bugzilla.mozilla.org/show_bug.cgi?id=852648#c21
- (void)userNotificationCenter:(NSUserNotificationCenter*)center
    didRemoveDeliveredNotifications:(NSArray*)notifications {
  for (NSUserNotification* notification in notifications) {
    NSDictionary* notificationResponse =
        [NotificationResponseBuilder buildDismissedDictionary:notification];
    ProcessMacNotificationResponse(notificationResponse);
  }
}

- (BOOL)userNotificationCenter:(NSUserNotificationCenter*)center
     shouldPresentNotification:(NSUserNotification*)nsNotification {
  // Always display notifications, regardless of whether the app is foreground.
  return YES;
}

@end
