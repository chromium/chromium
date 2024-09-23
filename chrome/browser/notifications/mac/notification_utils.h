// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_MAC_NOTIFICATION_UTILS_H_
#define CHROME_BROWSER_NOTIFICATIONS_MAC_NOTIFICATION_UTILS_H_

#include <optional>
#include <string>

#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/services/mac_notifications/public/cpp/notification_style.h"
#include "chrome/services/mac_notifications/public/mojom/mac_notifications.mojom.h"
#include "components/webapps/common/web_app_id.h"
#include "ui/message_center/public/cpp/notification.h"

class Profile;

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

// Validates contents of the |info| dictionary as received via mojo when a
// notification gets activated.
[[nodiscard]] bool VerifyMacNotificationData(
    const mac_notifications::mojom::NotificationActionInfoPtr& info);

// Processes a notification response generated from a user action
// (click close, etc.).
void ProcessMacNotificationResponse(
    mac_notifications::NotificationStyle notification_style,
    mac_notifications::mojom::NotificationActionInfoPtr info,
    std::optional<webapps::AppId> web_app_id = std::nullopt);

// Returns if the given |notification| should be shown as an alert.
bool IsAlertNotificationMac(const message_center::Notification& notification);

mac_notifications::mojom::NotificationPtr CreateMacNotification(
    NotificationHandler::Type notification_type,
    Profile* profile,
    const message_center::Notification& notification);

#endif  // CHROME_BROWSER_NOTIFICATIONS_MAC_NOTIFICATION_UTILS_H_
