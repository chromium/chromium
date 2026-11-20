// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_LINUX_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_LINUX_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"

class NotificationPlatformBridgeLinuxImpl;

using GSettings = struct _GSettings;

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

  // Constructor only used in unit testing. Does not watch system GSettings.
  explicit NotificationPlatformBridgeLinux(scoped_refptr<dbus::Bus> bus);

  // Constructor only used in unit testing. Watches `notification_settings` (if
  // non-null) when the server is GNOME Shell instead of reading system
  // GSettings.
  NotificationPlatformBridgeLinux(scoped_refptr<dbus::Bus> bus,
                                  GSettings* notification_settings);

  // Overrides the value of org.gnome.desktop.notifications show-banners.
  void SetGnomeShowBannersForTesting(std::optional<bool> show_banners);

  void CleanUp();

  std::unique_ptr<NotificationPlatformBridgeLinuxImpl> impl_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_LINUX_H_
