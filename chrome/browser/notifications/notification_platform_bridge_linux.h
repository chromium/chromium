// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_LINUX_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_LINUX_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"

class NotificationPlatformBridgeLinuxImpl;

namespace dbus {
class Bus;
}

class NotificationPlatformBridgeLinux : public NotificationPlatformBridge {
 public:
  NotificationPlatformBridgeLinux();
  NotificationPlatformBridgeLinux(const NotificationPlatformBridgeLinux&) =
      delete;
  NotificationPlatformBridgeLinux& operator=(
      const NotificationPlatformBridgeLinux&) = delete;
  ~NotificationPlatformBridgeLinux() override;

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

 private:
  friend class NotificationPlatformBridgeLinuxTest;

  // Constructor only used in unit testing.
  explicit NotificationPlatformBridgeLinux(scoped_refptr<dbus::Bus> bus);

  void CleanUp();

  scoped_refptr<NotificationPlatformBridgeLinuxImpl> impl_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_LINUX_H_
