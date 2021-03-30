// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_MAC_METRICS_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_MAC_METRICS_H_

// Called when a user performed an action on a notification on macOS.
// |is_alert| determines if the notification was an alert or a banner.
// |is_valid| determines if the action data was valid and we passed it along.
void LogMacNotificationActionReceived(bool is_alert, bool is_valid);

// Called when we delivered a new notification to the macOS notification center.
// |is_alert| determines if the notification was an alert or a banner.
// |success| determines if there was an error while delivering the notification.
void LogMacNotificationDelivered(bool is_alert, bool success);

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_MAC_METRICS_H_
