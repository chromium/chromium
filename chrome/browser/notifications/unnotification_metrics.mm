// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/unnotification_metrics.h"

#import <UserNotifications/UserNotifications.h>

#include "base/metrics/histogram_functions.h"

void LogUNNotificationBannerStyle(UNUserNotificationCenter* center) {
  [center getNotificationSettingsWithCompletionHandler:^(
              UNNotificationSettings* _Nonnull settings) {
    UNNotificationStyle notification_style;

    switch (settings.alertStyle) {
      case UNAlertStyleBanner:
        notification_style = UNNotificationStyle::kBanners;
        break;
      case UNAlertStyleAlert:
        notification_style = UNNotificationStyle::kAlerts;
        break;
      default:
        notification_style = UNNotificationStyle::kNone;
        break;
    }

    base::UmaHistogramEnumeration(
        "Notifications.Permissions.UNNotification.Banners.Style",
        notification_style);
  }];
}

void LogUNNotificationBannerPermissionStatus(UNUserNotificationCenter* center) {
  [center getNotificationSettingsWithCompletionHandler:^(
              UNNotificationSettings* _Nonnull settings) {
    UNNotificationPermissionStatus permission_status;

    switch (settings.authorizationStatus) {
      case UNAuthorizationStatusDenied:
        permission_status = UNNotificationPermissionStatus::kPermissionDenied;
        break;
      case UNAuthorizationStatusAuthorized:
        permission_status = UNNotificationPermissionStatus::kPermissionGranted;
        break;
      default:
        permission_status = UNNotificationPermissionStatus::kNotRequestedYet;
        break;
    }

    base::UmaHistogramEnumeration(
        "Notifications.Permissions.UNNotification.Banners.PermissionStatus",
        permission_status);
  }];
}
