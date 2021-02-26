// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_ALERT_DISPATCHER_MAC_H_
#define CHROME_BROWSER_NOTIFICATIONS_ALERT_DISPATCHER_MAC_H_

#include <string>
#include <tuple>

#import <Foundation/Foundation.h>

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "chrome/browser/notifications/displayed_notifications_dispatch_callback.h"

// Uniquely identifies a notification from any profile on a device.
struct MacNotificationIdentifier {
  std::string notification_id;
  std::string profile_id;
  bool incognito;
  // Comparator so we can use this class in a base::flat_set.
  bool operator<(const MacNotificationIdentifier& rhs) const {
    return std::tie(notification_id, profile_id, incognito) <
           std::tie(rhs.notification_id, rhs.profile_id, rhs.incognito);
  }
};

// Callback to get all alerts shown on the system for all profiles.
using GetAllDisplayedNotificationsCallback =
    base::OnceCallback<void(base::flat_set<MacNotificationIdentifier>)>;

// Interface to communicate with the Alert Notification service.
@protocol AlertDispatcher<NSObject>

// Deliver a notification to be displayed as an alert.
- (void)dispatchNotification:(NSDictionary*)data;

// Close a notification for a given |notificationId|, |profileId| and
// |incognito|.
- (void)closeNotificationWithId:(NSString*)notificationId
                      profileId:(NSString*)profileId
                      incognito:(BOOL)incognito;

// Close all notifications for a given |profileId| and |incognito|.
- (void)closeNotificationsWithProfileId:(NSString*)profileId
                              incognito:(BOOL)incognito;

// Close all notifications.
- (void)closeAllNotifications;

// Get currently displayed notifications for |profileId| and |incognito|. The
// returned ids are scoped to the passed profile and are not globally unique.
- (void)
getDisplayedAlertsForProfileId:(NSString*)profileId
                     incognito:(BOOL)incognito
                      callback:(GetDisplayedNotificationsCallback)callback;

// Get all currently displayed notifications.
- (void)getAllDisplayedAlertsWithCallback:
    (GetAllDisplayedNotificationsCallback)callback;

@end

#endif  // CHROME_BROWSER_NOTIFICATIONS_ALERT_DISPATCHER_MAC_H_
