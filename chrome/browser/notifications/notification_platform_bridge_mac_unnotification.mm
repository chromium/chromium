// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_mac_unnotification.h"

#include "base/notreached.h"

NotificationPlatformBridgeMacUNNotification::
    NotificationPlatformBridgeMacUNNotification() = default;

NotificationPlatformBridgeMacUNNotification::
    ~NotificationPlatformBridgeMacUNNotification() = default;

void NotificationPlatformBridgeMacUNNotification::Display(
    NotificationHandler::Type notification_type,
    Profile* profile,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> metadata) {
  NOTIMPLEMENTED();
}

void NotificationPlatformBridgeMacUNNotification::Close(
    Profile* profile,
    const std::string& notification_id) {
  NOTIMPLEMENTED();
}

void NotificationPlatformBridgeMacUNNotification::GetDisplayed(
    Profile* profile,
    GetDisplayedNotificationsCallback callback) const {
  NOTIMPLEMENTED();
  std::move(callback).Run(/*notification_ids=*/{}, /*supports_sync=*/false);
}

void NotificationPlatformBridgeMacUNNotification::SetReadyCallback(
    NotificationBridgeReadyCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(/*success=*/true);
}

void NotificationPlatformBridgeMacUNNotification::DisplayServiceShutDown(
    Profile* profile) {
  NOTIMPLEMENTED();
}