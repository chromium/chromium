// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/notifications/stub_alert_dispatcher_mac.h"

#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/cocoa/notifications/notification_constants_mac.h"

@implementation StubAlertDispatcher {
  base::scoped_nsobject<NSMutableArray> _alerts;
}

- (instancetype)init {
  if ((self = [super init])) {
    _alerts.reset([[NSMutableArray alloc] init]);
  }
  return self;
}

- (void)dispatchNotification:(NSDictionary*)data {
  [_alerts addObject:data];
}

- (void)closeNotificationWithId:(NSString*)notificationId
                      profileId:(NSString*)profileId
                      incognito:(BOOL)incognito {
  DCHECK(profileId);
  DCHECK(notificationId);
  for (NSDictionary* toast in _alerts.get()) {
    NSString* toastId =
        [toast objectForKey:notification_constants::kNotificationId];
    NSString* toastProfileId =
        [toast objectForKey:notification_constants::kNotificationProfileId];
    BOOL toastIncognito = [[toast
        objectForKey:notification_constants::kNotificationIncognito] boolValue];

    if ([notificationId isEqualToString:toastId] &&
        [profileId isEqualToString:toastProfileId] &&
        incognito == toastIncognito) {
      [_alerts removeObject:toast];
      break;
    }
  }
}

- (void)closeAllNotifications {
  [_alerts removeAllObjects];
}

- (void)
getDisplayedAlertsForProfileId:(NSString*)profileId
                     incognito:(BOOL)incognito
                      callback:(GetDisplayedNotificationsCallback)callback {
  std::set<std::string> alerts;

  for (NSDictionary* toast in _alerts.get()) {
    NSString* toastProfileId =
        [toast objectForKey:notification_constants::kNotificationProfileId];
    BOOL toastIncognito = [[toast
        objectForKey:notification_constants::kNotificationIncognito] boolValue];

    if ([profileId isEqualToString:toastProfileId] &&
        incognito == toastIncognito) {
      alerts.insert(base::SysNSStringToUTF8(
          [toast objectForKey:notification_constants::kNotificationId]));
    }
  }

  std::move(callback).Run(std::move(alerts),
                          /*supports_synchronization=*/true);
}

- (NSArray*)alerts {
  return [[_alerts copy] autorelease];
}

@end
