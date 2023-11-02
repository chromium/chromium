// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_MAC_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_MAC_H_

#include <memory>
#include <string>

#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"

class NotificationDispatcherMac;

namespace message_center {
class Notification;
}  // namespace message_center

// This class is an implementation of NotificationPlatformBridge that will
// send platform notifications to the MacOS notification center.
class NotificationPlatformBridgeMac : public NotificationPlatformBridge {
 public:
  NotificationPlatformBridgeMac(
      std::unique_ptr<NotificationDispatcherMac> banner_dispatcher,
      std::unique_ptr<NotificationDispatcherMac> alert_dispatcher);
  NotificationPlatformBridgeMac(const NotificationPlatformBridgeMac&) = delete;
  NotificationPlatformBridgeMac& operator=(
      const NotificationPlatformBridgeMac&) = delete;
  ~NotificationPlatformBridgeMac() override;

  // NotificationPlatformBridge implementation.
  void Display(NotificationHandler::Type notification_type,
               Profile* profile,
               const message_center::Notification& notification,
               std::unique_ptr<NotificationCommon::Metadata> metadata) override;

  void Close(Profile* profile, const std::string& notification_id) override;
  void GetDisplayed(Profile* profile,
                    GetDisplayedNotificationsCallback callback) const override;
  void SetReadyCallback(NotificationBridgeReadyCallback callback) override;
  void DisplayServiceShutDown(Profile* profile) override;

 private:
  // Closes all notifications for the given |profile|.
  void CloseAllNotificationsForProfile(Profile* profile);

  // The object in charge of dispatching banner notifications.
  std::unique_ptr<NotificationDispatcherMac> banner_dispatcher_;

  // The object in charge of dispatching remote notifications.
  std::unique_ptr<NotificationDispatcherMac> alert_dispatcher_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_MAC_H_
