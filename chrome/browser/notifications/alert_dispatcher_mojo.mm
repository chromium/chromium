// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/notifications/alert_dispatcher_mojo.h"

#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/flat_set.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/notifications/notification_alert_service_bridge.h"
#include "chrome/services/mac_notifications/public/cpp/notification_constants_mac.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

@implementation AlertDispatcherMojo {
  std::unique_ptr<MacNotificationProviderFactory> _providerFactory;
  base::scoped_nsobject<NotificationAlertServiceBridge> _mojoService;
}

- (instancetype)initWithProviderFactory:
    (std::unique_ptr<MacNotificationProviderFactory>)providerFactory {
  if ((self = [super init])) {
    _providerFactory = std::move(providerFactory);
  }
  return self;
}

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
    std::set<std::string> alertIds;

    for (NSString* alert in alerts)
      alertIds.insert(base::SysNSStringToUTF8(alert));

    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(blockCallback), std::move(alertIds),
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
    std::vector<MacNotificationIdentifier> alertIdentifiers;
    alertIdentifiers.reserve([alerts count]);

    for (NSDictionary* alert in alerts) {
      NSString* notificationId =
          [alert objectForKey:notification_constants::kNotificationId];
      NSString* profileId =
          [alert objectForKey:notification_constants::kNotificationProfileId];
      bool incognito =
          [[alert objectForKey:notification_constants::kNotificationIncognito]
              boolValue];

      alertIdentifiers.push_back({base::SysNSStringToUTF8(notificationId),
                                  base::SysNSStringToUTF8(profileId),
                                  incognito});
    }

    // Initialize the base::flat_set via a std::vector to avoid N^2 runtime.
    base::flat_set<MacNotificationIdentifier> identifiers(
        std::move(alertIdentifiers));
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(blockCallback), std::move(identifiers)));
  };

  [[self serviceProxy] getAllDisplayedAlertsWithReply:reply];
}

- (id<NotificationDelivery>)serviceProxy {
  if (!_mojoService) {
    auto onDisconnect = base::BindOnce(base::RetainBlock(^{
      _mojoService.reset();
    }));
    auto onAction = base::BindRepeating(base::RetainBlock(^{
        // TODO(crbug.com/1170731): Check if we can disconnect.
    }));
    _mojoService.reset([[NotificationAlertServiceBridge alloc]
        initWithDisconnectHandler:std::move(onDisconnect)
                    actionHandler:std::move(onAction)
                         provider:_providerFactory->LaunchProvider()]);
  }
  return _mojoService.get();
}

@end
