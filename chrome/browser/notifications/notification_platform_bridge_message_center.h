// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_MESSAGE_CENTER_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_MESSAGE_CENTER_H_

#include "chrome/browser/notifications/notification_platform_bridge.h"

class Profile;

// Implementation of the platform bridge that enables delivering notifications
// through Chrome's Message Center if there is no system specific notification
// platform bridge. There is only one instance of this class.
class NotificationPlatformBridgeMessageCenter
    : public NotificationPlatformBridge {
 public:
  static NotificationPlatformBridgeMessageCenter* Get();

  NotificationPlatformBridgeMessageCenter();
  NotificationPlatformBridgeMessageCenter(
      const NotificationPlatformBridgeMessageCenter&) = delete;
  NotificationPlatformBridgeMessageCenter& operator=(
      const NotificationPlatformBridgeMessageCenter&) = delete;
  ~NotificationPlatformBridgeMessageCenter() override;

  // NotificationPlatformBridge implementation:
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

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_MESSAGE_CENTER_H_
