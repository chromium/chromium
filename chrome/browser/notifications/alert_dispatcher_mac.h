// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_ALERT_DISPATCHER_MAC_H_
#define CHROME_BROWSER_NOTIFICATIONS_ALERT_DISPATCHER_MAC_H_

#import <Foundation/Foundation.h>

#include "chrome/browser/notifications/displayed_notifications_dispatch_callback.h"

// Interface to communicate with the Alert XPC service.
@protocol AlertDispatcher<NSObject>

// Deliver a notification to the XPC service to be displayed as an alert.
- (void)dispatchNotification:(NSDictionary*)data;

// Close a notification for a given |notificationId|, |profileId| and
// |incognito|.
- (void)closeNotificationWithId:(NSString*)notificationId
                      profileId:(NSString*)profileId
                      incognito:(BOOL)incognito;

// Close all notifications.
- (void)closeAllNotifications;

// Get currently displayed notifications for |profileId| and |incognito|. The
// returned ids are scoped to the passed profile and are not globally unique.
- (void)
getDisplayedAlertsForProfileId:(NSString*)profileId
                     incognito:(BOOL)incognito
                      callback:(GetDisplayedNotificationsCallback)callback;
@end

#endif  // CHROME_BROWSER_NOTIFICATIONS_ALERT_DISPATCHER_MAC_H_
