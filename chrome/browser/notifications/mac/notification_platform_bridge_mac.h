// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_MAC_NOTIFICATION_PLATFORM_BRIDGE_MAC_H_
#define CHROME_BROWSER_NOTIFICATIONS_MAC_NOTIFICATION_PLATFORM_BRIDGE_MAC_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/notifications/mac/notification_dispatcher_mac.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"
#include "chrome/services/mac_notifications/public/mojom/mac_notifications.mojom.h"
#include "components/webapps/common/web_app_id.h"
#include "url/gurl.h"

// This class is an implementation of NotificationPlatformBridge that will
// send platform notifications to the MacOS notification center.
class NotificationPlatformBridgeMac : public NotificationPlatformBridge {
 public:
  using WebAppDispatcherFactory =
      base::RepeatingCallback<std::unique_ptr<NotificationDispatcherMac>(
          const webapps::AppId& web_app_id)>;

  NotificationPlatformBridgeMac(
      std::unique_ptr<NotificationDispatcherMac> banner_dispatcher,
      std::unique_ptr<NotificationDispatcherMac> alert_dispatcher,
      WebAppDispatcherFactory web_app_dispatcher_factory);
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
  void GetDisplayedForOrigin(
      Profile* profile,
      const GURL& origin,
      GetDisplayedNotificationsCallback callback) const override;
  void SetReadyCallback(NotificationBridgeReadyCallback callback) override;
  void DisplayServiceShutDown(Profile* profile) override;

  // Called when an app shim for `web_app_id` is terminating gracefully. This
  // is used by NotificationDispatcherMac to correctly distinguish graceful from
  // ungraceful shutdowns in metrics.
  void AppShimWillTerminate(const webapps::AppId& web_app_id);

 private:
  void OnAttributedNotificationDisplayed(
      const webapps::AppId& web_app_id,
      NotificationHandler::Type notification_type,
      base::WeakPtr<Profile> profile,
      const message_center::Notification& notification,
      const MacNotificationIdentifier& identifier,
      uint64_t display_request_id,
      mac_notifications::mojom::DisplayNotificationResult result);

  // Closes the notification with the given `notification_id` and `profile` in
  // every dispatcher except those in `excluded_dispatchers`.
  void CloseImpl(Profile* profile,
                 const std::string& notification_id,
                 base::span<const NotificationDispatcherMac* const>
                     excluded_dispatchers = {});

  // Closes all notifications for the given |profile|.
  void CloseAllNotificationsForProfile(Profile* profile);

  NotificationDispatcherMac* GetOrCreateDispatcherForWebApp(
      const webapps::AppId& web_app_id) const;

  // The object in charge of dispatching banner notifications.
  std::unique_ptr<NotificationDispatcherMac> banner_dispatcher_;

  // The object in charge of dispatching remote notifications.
  std::unique_ptr<NotificationDispatcherMac> alert_dispatcher_;

  // The objects in charge of dispatching per-app notifications.
  // TODO(crbug.com/40616749): Implement some logic for cleaning up no
  // longer needed dispatchers.
  mutable std::map<webapps::AppId, std::unique_ptr<NotificationDispatcherMac>>
      app_specific_dispatchers_;
  WebAppDispatcherFactory web_app_dispatcher_factory_;

  // Tracks in-flight attributed DisplayNotification requests so they are
  // included in `GetDisplayedForOrigin()` and can be cancelled if the
  // notification is closed or re-displayed before the app shim replies.
  struct PendingDisplay {
    uint64_t request_id;
    GURL origin_url;
  };
  uint64_t next_display_request_id_ = 0;
  base::flat_map<MacNotificationIdentifier, PendingDisplay> pending_displays_;

  base::WeakPtrFactory<NotificationPlatformBridgeMac> weak_factory_{this};
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_MAC_NOTIFICATION_PLATFORM_BRIDGE_MAC_H_
