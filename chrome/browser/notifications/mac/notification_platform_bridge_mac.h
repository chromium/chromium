// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_MAC_NOTIFICATION_PLATFORM_BRIDGE_MAC_H_
#define CHROME_BROWSER_NOTIFICATIONS_MAC_NOTIFICATION_PLATFORM_BRIDGE_MAC_H_

#include <memory>
#include <string>

#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"
#include "components/webapps/common/web_app_id.h"

class NotificationDispatcherMac;

namespace message_center {
class Notification;
}  // namespace message_center

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
  // Closes the notification with the given `notification_id` and `profile` in
  // every dispatcher except `excluded_dispatcher`. Pass nullptr to
  // `excluded_dispatcher` to close the notification everywhere.
  void CloseImpl(Profile* profile,
                 const std::string& notification_id,
                 NotificationDispatcherMac* excluded_dispatcher = nullptr);

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
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_MAC_NOTIFICATION_PLATFORM_BRIDGE_MAC_H_
