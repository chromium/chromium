// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_LACROS_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_LACROS_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"
#include "chromeos/crosapi/mojom/message_center.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

class NotificationPlatformBridgeDelegate;

// Sends notifications to ash-chrome over mojo. Responds to user actions like
// clicks on notifications received over mojo. Works together with
// NotificationPlatformBridgeChromeOs because that class contains support for
// transient notifications and multiple profiles.
class NotificationPlatformBridgeLacros : public NotificationPlatformBridge {
 public:
  NotificationPlatformBridgeLacros(
      NotificationPlatformBridgeDelegate* delegate,
      mojo::Remote<crosapi::mojom::MessageCenter>* message_center_remote);
  NotificationPlatformBridgeLacros(const NotificationPlatformBridgeLacros&) =
      delete;
  NotificationPlatformBridgeLacros& operator=(
      const NotificationPlatformBridgeLacros&) = delete;
  ~NotificationPlatformBridgeLacros() override;

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
  class RemoteNotificationDelegate;

  // Cleans up after a remote notification is closed.
  void OnRemoteNotificationClosed(const std::string& id);

  const raw_ptr<NotificationPlatformBridgeDelegate> bridge_delegate_;

  // May be nullptr if the message center is unavailable.
  const raw_ptr<mojo::Remote<crosapi::mojom::MessageCenter>>
      message_center_remote_;

  // Map key is notification ID.
  std::map<std::string, std::unique_ptr<RemoteNotificationDelegate>>
      remote_notifications_;

  base::WeakPtrFactory<NotificationPlatformBridgeLacros> weak_factory_{this};
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_LACROS_H_
