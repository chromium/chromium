// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_WIN_NOTIFICATION_UTIL_H_
#define CHROME_BROWSER_NOTIFICATIONS_WIN_NOTIFICATION_UTIL_H_

#include <windows.ui.notifications.h>

class NotificationLaunchId;

// Extracts a NotificationLaunchId from a Toast |notification|. Outside tests,
// this function should be called on a non-UI thread.
NotificationLaunchId GetNotificationLaunchId(
    ABI::Windows::UI::Notifications::IToastNotification* notification);

#endif  // CHROME_BROWSER_NOTIFICATIONS_WIN_NOTIFICATION_UTIL_H_
