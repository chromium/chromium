// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_mac.h"
#include "chrome/browser/notifications/notification_platform_bridge_mac_unnotification.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_mach_port.h"
#include "base/mac/scoped_nsobject.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/notification_platform_bridge_mac_utils.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/cocoa/notifications/notification_builder_mac.h"
#include "chrome/browser/ui/cocoa/notifications/notification_constants_mac.h"
#import "chrome/browser/ui/cocoa/notifications/notification_delivery.h"
#import "chrome/browser/ui/cocoa/notifications/notification_response_builder_mac.h"
#include "chrome/common/buildflags.h"
#include "chrome/grit/generated_resources.h"
#include "components/crash/core/app/crashpad.h"
#include "content/public/browser/browser_task_traits.h"
#include "third_party/blink/public/common/notifications/notification_constants.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"
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

namespace {

// This enum backs an UMA histogram, so it should be treated as append-only.
enum XPCConnectionEvent {
  INTERRUPTED = 0,
  INVALIDATED,
  XPC_CONNECTION_EVENT_COUNT
};

void RecordXPCEvent(XPCConnectionEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Notifications.XPCConnectionEvent", event,
                            XPC_CONNECTION_EVENT_COUNT);
}

bool IsPersistentNotification(
    const message_center::Notification& notification) {
  if (!NotificationPlatformBridgeMac::SupportsAlerts())
    return false;

  return notification.never_timeout() ||
         notification.type() == message_center::NOTIFICATION_TYPE_PROGRESS;
}

// Implements the version check to determine if alerts are supported. Do not
// call this method directly as SysInfo::OperatingSystemVersionNumbers might be
// an expensive call. Instead use NotificationPlatformBridgeMac::SupportsAlerts
// which caches this value.
bool SupportsAlertsImpl() {
  int32_t major, minor, bugfix;
  base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);
  // Allow alerts on all versions except 10.15.0, 10.15.1 & 10.15.2.
  // See crbug.com/1007418 for details.
  return major != 10 || minor != 15 || bugfix > 2;
}

}  // namespace

// A Cocoa class that represents the delegate of NSUserNotificationCenter and
// can forward commands to C++.
@interface NotificationCenterDelegate
    : NSObject<NSUserNotificationCenterDelegate> {
}
@end

// Interface to communicate with the Alert XPC service.
@interface AlertDispatcherImpl : NSObject<AlertDispatcher>

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
  base::scoped_nsobject<AlertDispatcherImpl> alert_dispatcher(
      [[AlertDispatcherImpl alloc] init]);
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

  bool is_persistent = IsPersistentNotification(notification);

  [builder setSubTitle:base::SysUTF16ToNSString(CreateMacNotificationContext(
                           is_persistent, notification, requires_attribution))];

  if (!notification.icon().IsEmpty()) {
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

  [builder setTag:base::SysUTF8ToNSString(notification.id())];
  // If renotify is needed, delete the notification with the same id
  // from the notification center before displaying this one.
  // TODO(miguelg): This will need to work for alerts as well via XPC
  // once supported.
  if (notification.renotify()) {
    NSUserNotificationCenter* notification_center =
        [NSUserNotificationCenter defaultUserNotificationCenter];
    for (NSUserNotification* existing_notification in
         [notification_center deliveredNotifications]) {
      NSString* identifier = [existing_notification valueForKey:@"identifier"];
      if ([identifier
              isEqualToString:base::SysUTF8ToNSString(notification.id())]) {
        [notification_center removeDeliveredNotification:existing_notification];
        break;
      }
    }
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

  // Send persistent notifications to the XPC service so they
  // can be displayed as alerts. Chrome itself can only display
  // banners.
  if (IsPersistentNotification(notification)) {
    NSDictionary* dict = [builder buildDictionary];
    [alert_dispatcher_ dispatchNotification:dict];
  } else {
    NSUserNotification* toast = [builder buildUserNotification];
    [notification_center_ deliverNotification:toast];
  }
}

void NotificationPlatformBridgeMac::Close(Profile* profile,
                                          const std::string& notification_id) {
  NSString* candidate_id = base::SysUTF8ToNSString(notification_id);
  NSString* current_profile_id = base::SysUTF8ToNSString(GetProfileId(profile));

  bool notification_removed = false;
  for (NSUserNotification* toast in
       [notification_center_ deliveredNotifications]) {
    NSString* toast_id =
        [toast.userInfo objectForKey:notification_constants::kNotificationId];

    NSString* persistent_profile_id = [toast.userInfo
        objectForKey:notification_constants::kNotificationProfileId];

    if ([toast_id isEqualToString:candidate_id] &&
        [persistent_profile_id isEqualToString:current_profile_id]) {
      [notification_center_ removeDeliveredNotification:toast];
      notification_removed = true;
      break;
    }
  }

  // If no banner existed with that ID try to see if there is an alert
  // in the xpc server.
  if (!notification_removed) {
    [alert_dispatcher_ closeNotificationWithId:candidate_id
                                 withProfileId:current_profile_id];
  }
}

void NotificationPlatformBridgeMac::GetDisplayed(
    Profile* profile,
    GetDisplayedNotificationsCallback callback) const {
  [alert_dispatcher_ getDisplayedAlertsForProfileId:base::SysUTF8ToNSString(
                                                        GetProfileId(profile))
                                          incognito:profile->IsOffTheRecord()
                                 notificationCenter:notification_center_
                                           callback:std::move(callback)];
}

void NotificationPlatformBridgeMac::SetReadyCallback(
    NotificationBridgeReadyCallback callback) {
  std::move(callback).Run(true);
}

void NotificationPlatformBridgeMac::DisplayServiceShutDown(Profile* profile) {}

// static
bool NotificationPlatformBridgeMac::SupportsAlerts() {
  // Cache result as SysInfo::OperatingSystemVersionNumbers might be expensive.
  static bool supports_alerts = SupportsAlertsImpl();
  return supports_alerts;
}

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

@implementation AlertDispatcherImpl {
  // The connection to the XPC server in charge of delivering alerts.
  base::scoped_nsobject<NSXPCConnection> _xpcConnection;

  // YES if the remote object has had |-setMachExceptionPort:| called
  // since the service was last started, interrupted, or invalidated.
  // If NO, then -serviceProxy will set the exception port.
  BOOL _setExceptionPort;
}

- (instancetype)init {
  if ((self = [super init])) {
    _xpcConnection.reset([[NSXPCConnection alloc]
        initWithServiceName:
            [NSString
                stringWithFormat:notification_constants::kAlertXPCServiceName,
                                 [base::mac::OuterBundle() bundleIdentifier]]]);
    _xpcConnection.get().remoteObjectInterface =
        [NSXPCInterface interfaceWithProtocol:@protocol(NotificationDelivery)];

    _xpcConnection.get().interruptionHandler = ^{
      // We will be getting this handler both when the XPC server crashes or
      // when it decides to close the connection.
      LOG(WARNING) << "AlertNotificationService: XPC connection interrupted.";
      RecordXPCEvent(INTERRUPTED);
      _setExceptionPort = NO;
    };

    _xpcConnection.get().invalidationHandler = ^{
      // This means that the connection should be recreated if it needs
      // to be used again.
      LOG(WARNING) << "AlertNotificationService: XPC connection invalidated.";
      RecordXPCEvent(INVALIDATED);
      _setExceptionPort = NO;
    };

    _xpcConnection.get().exportedInterface =
        [NSXPCInterface interfaceWithProtocol:@protocol(NotificationReply)];
    _xpcConnection.get().exportedObject = self;
    [_xpcConnection resume];
  }

  return self;
}

// AlertDispatcher:
- (void)dispatchNotification:(NSDictionary*)data {
  [[self serviceProxy] deliverNotification:data];
}

- (void)closeNotificationWithId:(NSString*)notificationId
                  withProfileId:(NSString*)profileId {
  [[self serviceProxy] closeNotificationWithId:notificationId
                                 withProfileId:profileId];
}

- (void)closeAllNotifications {
  [[self serviceProxy] closeAllNotifications];
}

- (void)
getDisplayedAlertsForProfileId:(NSString*)profileId
                     incognito:(BOOL)incognito
            notificationCenter:(NSUserNotificationCenter*)notificationCenter
                      callback:(GetDisplayedNotificationsCallback)callback {
  // Create a copyable version of the OnceCallback because ObjectiveC blocks
  // copy all referenced variables via copy constructor.
  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  auto reply = ^(NSArray* alerts) {
    std::set<std::string> displayedNotifications;

    for (NSUserNotification* toast in
         [notificationCenter deliveredNotifications]) {
      NSString* toastProfileId = [toast.userInfo
          objectForKey:notification_constants::kNotificationProfileId];
      BOOL incognitoNotification = [[toast.userInfo
          objectForKey:notification_constants::kNotificationIncognito]
          boolValue];
      if ([toastProfileId isEqualToString:profileId] &&
          incognito == incognitoNotification) {
        displayedNotifications.insert(base::SysNSStringToUTF8([toast.userInfo
            objectForKey:notification_constants::kNotificationId]));
      }
    }

    for (NSString* alert in alerts)
      displayedNotifications.insert(base::SysNSStringToUTF8(alert));

    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(copyable_callback, std::move(displayedNotifications),
                       true /* supports_synchronization */));
  };

  [[self serviceProxy] getDisplayedAlertsForProfileId:profileId
                                         andIncognito:incognito
                                            withReply:reply];
}

// NotificationReply:
- (void)notificationClick:(NSDictionary*)notificationResponseData {
  ProcessMacNotificationResponse(notificationResponseData);
}

// Private methods:

// Retrieves the connection's remoteObjectProxy. Always use this as opposed
// to going directly through the connection, since this will ensure that the
// service has its exception port configured for crash reporting.
- (id<NotificationDelivery>)serviceProxy {
  id<NotificationDelivery> proxy = [_xpcConnection remoteObjectProxy];

  if (!_setExceptionPort) {
    base::mac::ScopedMachSendRight exceptionPort(
        crash_reporter::GetCrashpadClient().GetHandlerMachPort());
    base::scoped_nsobject<CrXPCMachPort> xpcPort(
        [[CrXPCMachPort alloc] initWithMachSendRight:std::move(exceptionPort)]);
    [proxy setMachExceptionPort:xpcPort];
    _setExceptionPort = YES;
  }

  return proxy;
}

@end
