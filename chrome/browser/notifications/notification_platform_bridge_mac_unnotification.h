// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_MAC_UNNOTIFICATION_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_MAC_UNNOTIFICATION_H_

#include <memory>
#include <string>

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"

@class UNNotificationCenterDelegate;
@class UNUserNotificationCenter;

namespace message_center {
class Notification;
}  // namespace message_center

// This class is an implementation of NotificationPlatformBridge that will
// send platform notifications to the MacOS notification center for devices
// running on macOS 10.14+.
class API_AVAILABLE(macosx(10.14)) NotificationPlatformBridgeMacUNNotification
    : public NotificationPlatformBridge {
 public:
  NotificationPlatformBridgeMacUNNotification();

  explicit NotificationPlatformBridgeMacUNNotification(
      UNUserNotificationCenter* notification_center);

  NotificationPlatformBridgeMacUNNotification(
      const NotificationPlatformBridgeMacUNNotification&) = delete;
  NotificationPlatformBridgeMacUNNotification& operator=(
      const NotificationPlatformBridgeMacUNNotification&) = delete;
  ~NotificationPlatformBridgeMacUNNotification() override;

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

  // Request permission to send notifications
  void RequestPermission();

 private:
  // Cocoa class that receives callbacks from the UNUserNotificationCenter.
  base::scoped_nsobject<UNNotificationCenterDelegate> delegate_;

  // The notification center to use for local banner notifications,
  // this can be overridden in tests.
  base::scoped_nsobject<UNUserNotificationCenter> notification_center_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_MAC_UNNOTIFICATION_H_
