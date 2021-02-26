// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/notifications/alert_dispatcher_xpc.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/scoped_mach_port.h"
#include "base/mac/scoped_nsobject.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/notifications/notification_platform_bridge_mac_utils.h"
#import "chrome/browser/ui/cocoa/notifications/notification_delivery.h"
#include "chrome/browser/ui/cocoa/notifications/xpc_mach_port.h"
#include "chrome/services/mac_notifications/public/cpp/notification_constants_mac.h"
#include "components/crash/core/app/crashpad.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"

namespace {

// This enum backs an UMA histogram, so it should be treated as append-only.
enum class XPCConnectionEvent {
  kInterrupted = 0,
  kInvalidated = 1,
  kMaxValue = kInvalidated,
};

void RecordXPCEvent(XPCConnectionEvent event) {
  base::UmaHistogramEnumeration("Notifications.XPCConnectionEvent", event);
}

}  // namespace

@implementation AlertDispatcherXPC {
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
      RecordXPCEvent(XPCConnectionEvent::kInterrupted);
      _setExceptionPort = NO;
    };

    _xpcConnection.get().invalidationHandler = ^{
      // This means that the connection should be recreated if it needs
      // to be used again.
      LOG(WARNING) << "AlertNotificationService: XPC connection invalidated.";
      RecordXPCEvent(XPCConnectionEvent::kInvalidated);
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
                      profileId:(NSString*)profileId
                      incognito:(BOOL)incognito {
  [[self serviceProxy] closeNotificationWithId:notificationId
                                     profileId:profileId
                                     incognito:incognito];
}

- (void)closeNotificationsWithProfileId:(NSString*)profileId
                              incognito:(BOOL)incognito {
  [[self serviceProxy] closeNotificationsWithProfileId:profileId
                                             incognito:incognito];
}

- (void)closeAllNotifications {
  [[self serviceProxy] closeAllNotifications];
}

- (void)getDisplayedAlertsForProfileId:(NSString*)profileId
                             incognito:(BOOL)incognito
                              callback:
                                  (GetDisplayedNotificationsCallback)callback {
  // Move |callback| into block storage so we can use it from the block below.
  __block GetDisplayedNotificationsCallback blockCallback = std::move(callback);
  auto reply = ^(NSArray* alerts) {
    std::set<std::string> displayedNotifications;

    for (NSString* alert in alerts)
      displayedNotifications.insert(base::SysNSStringToUTF8(alert));

    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(blockCallback),
                                  std::move(displayedNotifications),
                                  /*supports_synchronization=*/true));
  };

  [[self serviceProxy] getDisplayedAlertsForProfileId:profileId
                                            incognito:incognito
                                                reply:reply];
}

- (void)getAllDisplayedAlertsWithCallback:
    (GetAllDisplayedNotificationsCallback)callback {
  // Move |callback| into block storage so we can use it from the block below.
  __block GetAllDisplayedNotificationsCallback blockCallback =
      std::move(callback);
  auto reply = ^(NSArray* alerts) {
    std::vector<MacNotificationIdentifier> alertIds;
    alertIds.reserve([alerts count]);

    for (NSDictionary* toast in alerts) {
      std::string notificationId = base::SysNSStringToUTF8(
          [toast objectForKey:notification_constants::kNotificationId]);
      std::string profileId = base::SysNSStringToUTF8(
          [toast objectForKey:notification_constants::kNotificationProfileId]);
      bool incognito =
          [[toast objectForKey:notification_constants::kNotificationIncognito]
              boolValue];

      alertIds.push_back(
          {std::move(notificationId), std::move(profileId), incognito});
    }

    // Create set from std::vector to avoid N^2 insertion runtime.
    base::flat_set<MacNotificationIdentifier> alertSet(std::move(alertIds));

    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(blockCallback), std::move(alertSet)));
  };

  [[self serviceProxy] getAllDisplayedAlertsWithReply:reply];
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
    [proxy setUseUNNotification:NO machExceptionPort:xpcPort];
    _setExceptionPort = YES;
  }

  return proxy;
}

@end
