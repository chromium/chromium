// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/stub_notification_platform_bridge.h"

#include <set>

StubNotificationPlatformBridge::StubNotificationPlatformBridge() = default;

StubNotificationPlatformBridge::~StubNotificationPlatformBridge() = default;

void StubNotificationPlatformBridge::Display(
    NotificationHandler::Type notification_type,
    Profile* profile,
    const message_center::Notification& notification,
    std::unique_ptr<NotificationCommon::Metadata> metadata) {}

void StubNotificationPlatformBridge::Close(Profile* profile,
                                           const std::string& notification_id) {
}

void StubNotificationPlatformBridge::GetDisplayed(
    Profile* profile,
    GetDisplayedNotificationsCallback callback) const {
  std::set<std::string> displayed_notifications;
  std::move(callback).Run(std::move(displayed_notifications),
                          false /* supports_synchronization */);
}

void StubNotificationPlatformBridge::GetDisplayedForOrigin(
    Profile* profile,
    const GURL& origin,
    GetDisplayedNotificationsCallback callback) const {
  std::set<std::string> displayed_notifications;
  std::move(callback).Run(std::move(displayed_notifications),
                          false /* supports_synchronization */);
}

void StubNotificationPlatformBridge::SetReadyCallback(
    NotificationBridgeReadyCallback callback) {
  std::move(callback).Run(true /* ready */);
}

void StubNotificationPlatformBridge::DisplayServiceShutDown(Profile* profile) {}
