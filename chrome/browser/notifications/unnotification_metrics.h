// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_UNNOTIFICATION_METRICS_H_
#define CHROME_BROWSER_NOTIFICATIONS_UNNOTIFICATION_METRICS_H_

#import <Foundation/Foundation.h>

@class UNUserNotificationCenter;

// This file is used to record metrics specific for UNNotifications.

// This enum is used in UMA. Do not delete or re-order entries. New entries
// should only be added at the end.
enum class UNNotificationStyle {
  kNone = 0,
  kBanners = 1,
  kAlerts = 2,
  kMaxValue = kAlerts,
};

// This enum is used in UMA. Do not delete or re-order entries. New entries
// should only be added at the end.
enum class UNNotificationPermissionStatus {
  kNotRequestedYet = 0,
  kPermissionDenied = 1,
  kPermissionGranted = 2,
  kMaxValue = kPermissionGranted,
};

// Log the type of notifications being used.
API_AVAILABLE(macosx(10.14))
void LogUNNotificationBannerStyle(UNUserNotificationCenter* center);

// Log the permission status for sending out notifications.
API_AVAILABLE(macosx(10.14))
void LogUNNotificationBannerPermissionStatus(UNUserNotificationCenter* center);

#endif  // CHROME_BROWSER_NOTIFICATIONS_UNNOTIFICATION_METRICS_H_
