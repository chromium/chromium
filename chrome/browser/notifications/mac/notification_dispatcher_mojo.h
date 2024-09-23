// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_MAC_NOTIFICATION_DISPATCHER_MOJO_H_
#define CHROME_BROWSER_NOTIFICATIONS_MAC_NOTIFICATION_DISPATCHER_MOJO_H_

#include <memory>
#include <string>
#include <tuple>

#include "base/cancelable_callback.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/displayed_notifications_dispatch_callback.h"
#include "chrome/browser/notifications/mac/notification_dispatcher_mac.h"
#include "chrome/services/mac_notifications/public/mojom/mac_notifications.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class MacNotificationProviderFactory;

// Connects to the macOS notification service via mojo to manage notifications.
class NotificationDispatcherMojo
    : public NotificationDispatcherMac,
      public mac_notifications::mojom::MacNotificationActionHandler {
 public:
  explicit NotificationDispatcherMojo(
      std::unique_ptr<MacNotificationProviderFactory> provider_factory);
  NotificationDispatcherMojo(const NotificationDispatcherMojo&) = delete;
  NotificationDispatcherMojo& operator=(const NotificationDispatcherMojo&) =
      delete;
  ~NotificationDispatcherMojo() override;

  // NotificationDispatcherMac:
  void DisplayNotification(
      NotificationHandler::Type notification_type,
      Profile* profile,
      const message_center::Notification& notification) override;
  void CloseNotificationWithId(
      const MacNotificationIdentifier& identifier) override;
  void CloseNotificationsWithProfileId(const std::string& profile_id,
                                       bool incognito) override;
  void CloseAllNotifications() override;
  void GetDisplayedNotificationsForProfileId(
      const std::string& profile_id,
      bool incognito,
      GetDisplayedNotificationsCallback callback) override;
  void GetDisplayedNotificationsForProfileIdAndOrigin(
      const std::string& profile_id,
      bool incognito,
      const GURL& origin,
      GetDisplayedNotificationsCallback callback) override;
  void GetAllDisplayedNotifications(
      GetAllDisplayedNotificationsCallback callback) override;
  void UserInitiatedShutdown() override;

  // mac_notifications::mojom::MacNotificationActionHandler:
  void OnNotificationAction(
      mac_notifications::mojom::NotificationActionInfoPtr info) override;

 private:
  enum ShutdownType {
    // The service was shutdown because Chrome no longer needed it to alive.
    kChromeInitiated,
    // The service was shutdown because the user initiated the shutdown; for
    // example when the service lives in an app shim process, the user closed
    // the application.
    kUserInitiated,
    // Connection to the service was lost unexpectedly.
    kUnexpected
  };

  void CheckIfServiceCanBeTerminated();
  void OnServiceDisconnectedGracefully(ShutdownType shutdown_type);
  bool HasNoDisplayedNotifications() const;

  mac_notifications::mojom::MacNotificationService* GetOrCreateService();

  void DispatchGetNotificationsReply(
      GetDisplayedNotificationsCallback callback,
      std::vector<mac_notifications::mojom::NotificationIdentifierPtr>
          notifications);
  void DispatchGetAllNotificationsReply(
      GetAllDisplayedNotificationsCallback callback,
      std::vector<mac_notifications::mojom::NotificationIdentifierPtr>
          notifications);

  std::unique_ptr<MacNotificationProviderFactory> provider_factory_;
  mojo::Remote<mac_notifications::mojom::MacNotificationProvider> provider_;
  mojo::Remote<mac_notifications::mojom::MacNotificationService> service_;
  mojo::Receiver<mac_notifications::mojom::MacNotificationActionHandler>
      handler_{this};
  base::CancelableOnceClosure no_notifications_checker_;
  base::TimeTicks service_start_time_;
  base::OneShotTimer service_restart_timer_;
  base::TimeDelta next_service_restart_timer_delay_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_MAC_NOTIFICATION_DISPATCHER_MOJO_H_
