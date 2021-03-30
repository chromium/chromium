// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_MAC_UTILS_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_MAC_UTILS_H_

#include <string>

#include "chrome/browser/notifications/notification_common.h"
#include "ui/message_center/public/cpp/notification.h"

namespace message_center {
class Notification;
}  // namespace message_center

// This file is a combination of methods that are shared between the macOS
// notification bridges.

std::u16string CreateMacNotificationTitle(
    const message_center::Notification& notification);

std::u16string CreateMacNotificationContext(
    bool is_persistent,
    const message_center::Notification& notification,
    bool requires_attribution);

// Validates contents of the |response| dictionary as received from the system
// when a notification gets activated.
bool VerifyMacNotificationData(NSDictionary* response) WARN_UNUSED_RESULT;

// Processes a notification response generated from a user action
// (click close, etc.).
void ProcessMacNotificationResponse(NSDictionary* response);

// Returns if alerts via XPC are supported on this machine.
bool MacOSSupportsXPCAlerts();

// Returns if the given |notification| should be shown as an alert.
bool IsAlertNotificationMac(const message_center::Notification& notification);

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_MAC_UTILS_H_
