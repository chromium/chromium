// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_NOTIFICATION_METRICS_H_
#define CHROME_APP_NOTIFICATION_METRICS_H_

// Describes from which app the notification action came for. This enum is used
// in UMA. Do not delete or re-order entries. New entries should only be added
// at the end.
enum class NotificationActionSource {
  // Action for the browser app, usually from a banner style notification.
  kBrowser = 0,
  // Action for the helper app, usually from an alert style notification.
  kHelperApp = 1,
  kMaxValue = kHelperApp,
};

// Logs to UMA that we got launched via the OS to handle a notification action.
void LogLaunchedViaNotificationAction(NotificationActionSource source);

#endif  // CHROME_APP_NOTIFICATION_METRICS_H_
