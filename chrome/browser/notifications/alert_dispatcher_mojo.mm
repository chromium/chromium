// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/notifications/alert_dispatcher_mojo.h"

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/notifications/notification_alert_service_bridge.h"

@implementation AlertDispatcherMojo {
  base::scoped_nsobject<NotificationAlertServiceBridge> _mojoService;
}

- (void)dispatchNotification:(NSDictionary*)data {
  // TODO(knollr): Implement.
}

- (void)closeNotificationWithId:(NSString*)notificationId
                      profileId:(NSString*)profileId
                      incognito:(BOOL)incognito {
  [[self serviceProxy] closeNotificationWithId:notificationId
                                     profileId:profileId
                                     incognito:incognito];
}

- (void)closeAllNotifications {
  [[self serviceProxy] closeAllNotifications];
}

- (void)getDisplayedAlertsForProfileId:(NSString*)profileId
                             incognito:(BOOL)incognito
                              callback:
                                  (GetDisplayedNotificationsCallback)callback {
  // TODO(knollr): Implement.
  std::move(callback).Run(/*alerts=*/{}, /*supports_synchronization=*/false);
}

- (void)getAllDisplayedAlertsWithCallback:
    (GetAllDisplayedNotificationsCallback)callback {
  // TODO(knollr): Implement.
  std::move(callback).Run(/*alerts=*/{});
}

- (id<NotificationDelivery>)serviceProxy {
  if (!_mojoService) {
    auto onDisconnect = base::BindOnce(
        [](base::scoped_nsobject<NotificationAlertServiceBridge>* ptr) {
          // Reset the bridge when the mojo connection disconnects.
          ptr->reset();
        },
        base::Unretained(&_mojoService));
    _mojoService.reset([[NotificationAlertServiceBridge alloc]
        initWithDisconnectHandler:std::move(onDisconnect)]);
  }
  return _mojoService.get();
}

@end
