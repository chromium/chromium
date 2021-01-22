// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_MAC_UTILS_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_MAC_UTILS_H_

#include <string>

#include "base/strings/string16.h"
#include "chrome/browser/notifications/notification_common.h"
#include "ui/message_center/public/cpp/notification.h"

namespace message_center {
class Notification;
}  // namespace message_center

// This file is a combination of methods that are shared between the macOS
// notification bridges.

base::string16 CreateMacNotificationTitle(
    const message_center::Notification& notification);

base::string16 CreateMacNotificationContext(
    bool is_persistent,
    const message_center::Notification& notification,
    bool requires_attribution);

// Derives a unique notification identifier to be used by the macOS system
// notification center to uniquely identify a notification.
std::string DeriveMacNotificationId(const std::string& profile_id,
                                    const std::string& notification_id);

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
