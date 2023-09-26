// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_STUB_NOTIFICATION_PLATFORM_BRIDGE_H_
#define CHROME_BROWSER_NOTIFICATIONS_STUB_NOTIFICATION_PLATFORM_BRIDGE_H_

#include <memory>
#include <string>

#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"

class Profile;

namespace message_center {
class Notification;
}  // namespace message_center

// Stub implementation of the NotificationPlatformBridge interface that just
// implements the interface, and doesn't exercise behaviour of its own.
class StubNotificationPlatformBridge : public NotificationPlatformBridge {
 public:
  StubNotificationPlatformBridge();
  StubNotificationPlatformBridge(const StubNotificationPlatformBridge&) =
      delete;
  StubNotificationPlatformBridge& operator=(
      const StubNotificationPlatformBridge&) = delete;
  ~StubNotificationPlatformBridge() override;

  // NotificationPlatformBridge:
  void Display(NotificationHandler::Type notification_type,
               Profile* profile,
               const message_center::Notification& notification,
               std::unique_ptr<NotificationCommon::Metadata> metadata) override;
  void Close(Profile* profile, const std::string& notification_id) override;
  void GetDisplayed(Profile* profile,
                    GetDisplayedNotificationsCallback callback) const override;
  void GetDisplayedForOrigin(
      Profile* profile,
      const GURL& origin,
      GetDisplayedNotificationsCallback callback) const override;
  void SetReadyCallback(NotificationBridgeReadyCallback callback) override;
  void DisplayServiceShutDown(Profile* profile) override;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_STUB_NOTIFICATION_PLATFORM_BRIDGE_H_
