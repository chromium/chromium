// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_mac.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/i18n/number_formatting.h"
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
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/cocoa/notifications/notification_builder_mac.h"
#include "chrome/browser/ui/cocoa/notifications/notification_constants_mac.h"
#import "chrome/browser/ui/cocoa/notifications/notification_delivery.h"
#import "chrome/browser/ui/cocoa/notifications/notification_response_builder_mac.h"
#include "chrome/common/buildflags.h"
#include "chrome/grit/generated_resources.h"
#include "components/crash/content/app/crashpad.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/platform/modules/notifications/web_notification_constants.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "url/gurl.h"
#include "url/origin.h"

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

// Loads the profile and process the Notification response
void DoProcessNotificationResponse(NotificationCommon::Operation operation,
                                   NotificationHandler::Type type,
                                   const std::string& profile_id,
                                   bool incognito,
                                   const GURL& origin,
                                   const std::string& notification_id,
                                   const base::Optional<int>& action_index,
                                   const base::Optional<base::string16>& reply,
                                   const base::Optional<bool>& by_user) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ProfileManager* profileManager = g_browser_process->profile_manager();
  DCHECK(profileManager);

  profileManager->LoadProfile(
      profile_id, incognito,
      base::Bind(&NotificationDisplayServiceImpl::ProfileLoadedCallback,
                 operation, type, origin, notification_id, action_index, reply,
                 by_user));
}

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

base::string16 CreateNotificationTitle(
    const message_center::Notification& notification) {
  base::string16 title;
  if (notification.type() == message_center::NOTIFICATION_TYPE_PROGRESS) {
    title += base::FormatPercent(notification.progress());
    title += base::UTF8ToUTF16(" - ");
  }
  title += notification.title();
  return title;
}

bool IsPersistentNotification(
    const message_center::Notification& notification) {
  return notification.never_timeout() ||
         notification.type() == message_center::NOTIFICATION_TYPE_PROGRESS;
}

base::string16 CreateNotificationContext(
    const message_center::Notification& notification,
    bool requires_attribution) {
  if (!requires_attribution)
    return notification.context_message();

  // Mac OS notifications don't provide a good way to elide the domain (or tell
  // you the maximum width of the subtitle field). We have experimentally
  // determined the maximum number of characters that fit using the widest
  // possible character (m). If the domain fits in those character we show it
  // completely. Otherwise we use eTLD + 1.

  // These numbers have been obtained through experimentation on various
  // Mac OS platforms.

  constexpr size_t kMaxDomainLengthAlert = 19;
  constexpr size_t kMaxDomainLengthBanner = 28;

  size_t max_characters = IsPersistentNotification(notification)
                              ? kMaxDomainLengthAlert
                              : kMaxDomainLengthBanner;

  base::string16 origin = url_formatter::FormatOriginForSecurityDisplay(
      url::Origin::Create(notification.origin_url()),
      url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);

  if (origin.size() <= max_characters)
    return origin;

  // Too long, use etld+1
  base::string16 etldplusone =
      base::UTF8ToUTF16(net::registry_controlled_domains::GetDomainAndRegistry(
          notification.origin_url(),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));

  // localhost, raw IPs etc. are not handled by GetDomainAndRegistry.
  if (etldplusone.empty())
    return origin;

  return etldplusone;
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
NotificationPlatformBridge* NotificationPlatformBridge::Create() {
  base::scoped_nsobject<AlertDispatcherImpl> alert_dispatcher(
      [[AlertDispatcherImpl alloc] init]);
  return new NotificationPlatformBridgeMac(
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

  [builder
      setTitle:base::SysUTF16ToNSString(CreateNotificationTitle(notification))];

  base::string16 context_message =
      notification.items().empty()
          ? notification.message()
          : (notification.items().at(0).title + base::UTF8ToUTF16(" - ") +
             notification.items().at(0).message);

  [builder setContextMessage:base::SysUTF16ToNSString(context_message)];

  bool requires_attribution =
      notification.context_message().empty() &&
      notification_type != NotificationHandler::Type::EXTENSION;
  [builder setSubTitle:base::SysUTF16ToNSString(CreateNotificationContext(
                           notification, requires_attribution))];

  if (!notification.icon().IsEmpty()) {
    [builder setIcon:notification.icon().ToNSImage()];
  }

  [builder setShowSettingsButton:(notification_type !=
                                  NotificationHandler::Type::EXTENSION)];
  std::vector<message_center::ButtonInfo> buttons = notification.buttons();
  if (!buttons.empty()) {
    DCHECK_LE(buttons.size(), blink::kWebNotificationMaxActions);
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

// static
void NotificationPlatformBridgeMac::ProcessNotificationResponse(
    NSDictionary* response) {
  if (!NotificationPlatformBridgeMac::VerifyNotificationData(response))
    return;

  NSNumber* button_index =
      [response objectForKey:notification_constants::kNotificationButtonIndex];
  NSNumber* operation =
      [response objectForKey:notification_constants::kNotificationOperation];

  std::string notification_origin = base::SysNSStringToUTF8(
      [response objectForKey:notification_constants::kNotificationOrigin]);
  std::string notification_id = base::SysNSStringToUTF8(
      [response objectForKey:notification_constants::kNotificationId]);
  std::string profile_id = base::SysNSStringToUTF8(
      [response objectForKey:notification_constants::kNotificationProfileId]);
  NSNumber* is_incognito =
      [response objectForKey:notification_constants::kNotificationIncognito];
  NSNumber* notification_type =
      [response objectForKey:notification_constants::kNotificationType];

  base::Optional<int> action_index;
  if (button_index.intValue !=
      notification_constants::kNotificationInvalidButtonIndex) {
    action_index = button_index.intValue;
  }

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::Bind(DoProcessNotificationResponse,
                 static_cast<NotificationCommon::Operation>(
                     operation.unsignedIntValue),
                 static_cast<NotificationHandler::Type>(
                     notification_type.unsignedIntValue),
                 profile_id, [is_incognito boolValue],
                 GURL(notification_origin), notification_id, action_index,
                 base::nullopt /* reply */, true /* by_user */));
}

// static
bool NotificationPlatformBridgeMac::VerifyNotificationData(
    NSDictionary* response) {
  if (![response
          objectForKey:notification_constants::kNotificationButtonIndex] ||
      ![response objectForKey:notification_constants::kNotificationOperation] ||
      ![response objectForKey:notification_constants::kNotificationId] ||
      ![response objectForKey:notification_constants::kNotificationProfileId] ||
      ![response objectForKey:notification_constants::kNotificationIncognito] ||
      ![response objectForKey:notification_constants::kNotificationType]) {
    LOG(ERROR) << "Missing required key";
    return false;
  }

  NSNumber* button_index =
      [response objectForKey:notification_constants::kNotificationButtonIndex];
  NSNumber* operation =
      [response objectForKey:notification_constants::kNotificationOperation];
  NSString* notification_id =
      [response objectForKey:notification_constants::kNotificationId];
  NSString* profile_id =
      [response objectForKey:notification_constants::kNotificationProfileId];
  NSNumber* notification_type =
      [response objectForKey:notification_constants::kNotificationType];

  if (button_index.intValue <
          notification_constants::kNotificationInvalidButtonIndex ||
      button_index.intValue >=
          static_cast<int>(blink::kWebNotificationMaxActions)) {
    LOG(ERROR) << "Invalid number of buttons supplied "
               << button_index.intValue;
    return false;
  }

  if (operation.unsignedIntValue > NotificationCommon::OPERATION_MAX) {
    LOG(ERROR) << operation.unsignedIntValue
               << " does not correspond to a valid operation.";
    return false;
  }

  if (notification_id.length <= 0) {
    LOG(ERROR) << "Notification Id is empty";
    return false;
  }

  if (profile_id.length <= 0) {
    LOG(ERROR) << "ProfileId not provided";
    return false;
  }

  if (notification_type.unsignedIntValue >
      static_cast<unsigned int>(NotificationHandler::Type::MAX)) {
    LOG(ERROR) << notification_type.unsignedIntValue
               << " Does not correspond to a valid operation.";
    return false;
  }

  // Origin is not actually required but if it's there it should be a valid one.
  NSString* origin =
      [response objectForKey:notification_constants::kNotificationOrigin];
  if (origin) {
    std::string notificationOrigin = base::SysNSStringToUTF8(origin);
    GURL url(notificationOrigin);
    if (!url.is_valid())
      return false;
  }

  return true;
}

// /////////////////////////////////////////////////////////////////////////////
@implementation NotificationCenterDelegate
- (void)userNotificationCenter:(NSUserNotificationCenter*)center
       didActivateNotification:(NSUserNotification*)notification {
  NSDictionary* notificationResponse =
      [NotificationResponseBuilder buildDictionary:notification];
  NotificationPlatformBridgeMac::ProcessNotificationResponse(
      notificationResponse);
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
      [NotificationResponseBuilder buildDictionary:notification];
  NotificationPlatformBridgeMac::ProcessNotificationResponse(
      notificationResponse);
}

- (BOOL)userNotificationCenter:(NSUserNotificationCenter*)center
     shouldPresentNotification:(NSUserNotification*)nsNotification {
  // Always display notifications, regardless of whether the app is foreground.
  return YES;
}

@end

@implementation AlertDispatcherImpl {
  // The connection to the XPC server in charge of delivering alerts.
  base::scoped_nsobject<NSXPCConnection> xpcConnection_;

  // YES if the remote object has had |-setMachExceptionPort:| called
  // since the service was last started, interrupted, or invalidated.
  // If NO, then -serviceProxy will set the exception port.
  BOOL setExceptionPort_;
}

- (instancetype)init {
  if ((self = [super init])) {
    xpcConnection_.reset([[NSXPCConnection alloc]
        initWithServiceName:
            [NSString
                stringWithFormat:notification_constants::kAlertXPCServiceName,
                                 [base::mac::OuterBundle() bundleIdentifier]]]);
    xpcConnection_.get().remoteObjectInterface =
        [NSXPCInterface interfaceWithProtocol:@protocol(NotificationDelivery)];

    xpcConnection_.get().interruptionHandler = ^{
      // We will be getting this handler both when the XPC server crashes or
      // when it decides to close the connection.
      LOG(WARNING) << "AlertNotificationService: XPC connection interrupted.";
      RecordXPCEvent(INTERRUPTED);
      setExceptionPort_ = NO;
    };

    xpcConnection_.get().invalidationHandler = ^{
      // This means that the connection should be recreated if it needs
      // to be used again.
      LOG(WARNING) << "AlertNotificationService: XPC connection invalidated.";
      RecordXPCEvent(INVALIDATED);
      setExceptionPort_ = NO;
    };

    xpcConnection_.get().exportedInterface =
        [NSXPCInterface interfaceWithProtocol:@protocol(NotificationReply)];
    xpcConnection_.get().exportedObject = self;
    [xpcConnection_ resume];
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
  auto reply = ^(NSArray* alerts) {
    std::unique_ptr<std::set<std::string>> displayedNotifications =
        std::make_unique<std::set<std::string>>();

    for (NSUserNotification* toast in
         [notificationCenter deliveredNotifications]) {
      NSString* toastProfileId = [toast.userInfo
          objectForKey:notification_constants::kNotificationProfileId];
      BOOL incognitoNotification = [[toast.userInfo
          objectForKey:notification_constants::kNotificationIncognito]
          boolValue];
      if ([toastProfileId isEqualToString:profileId] &&
          incognito == incognitoNotification) {
        displayedNotifications->insert(base::SysNSStringToUTF8([toast.userInfo
            objectForKey:notification_constants::kNotificationId]));
      }
    }

    for (NSString* alert in alerts)
      displayedNotifications->insert(base::SysNSStringToUTF8(alert));

    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::Bind(callback, base::Passed(&displayedNotifications),
                   true /* supports_synchronization */));
  };

  [[self serviceProxy] getDisplayedAlertsForProfileId:profileId
                                         andIncognito:incognito
                                            withReply:reply];
}

// NotificationReply:
- (void)notificationClick:(NSDictionary*)notificationResponseData {
  NotificationPlatformBridgeMac::ProcessNotificationResponse(
      notificationResponseData);
}

// Private methods:

// Retrieves the connection's remoteObjectProxy. Always use this as opposed
// to going directly through the connection, since this will ensure that the
// service has its exception port configured for crash reporting.
- (id<NotificationDelivery>)serviceProxy {
  id<NotificationDelivery> proxy = [xpcConnection_ remoteObjectProxy];

  if (!setExceptionPort_) {
    base::mac::ScopedMachSendRight exceptionPort(
        crash_reporter::GetCrashpadClient().GetHandlerMachPort());
    base::scoped_nsobject<CrXPCMachPort> xpcPort(
        [[CrXPCMachPort alloc] initWithMachSendRight:std::move(exceptionPort)]);
    [proxy setMachExceptionPort:xpcPort];
    setExceptionPort_ = YES;
  }

  return proxy;
}

@end
