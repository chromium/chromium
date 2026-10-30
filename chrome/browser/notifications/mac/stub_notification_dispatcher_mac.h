// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_MAC_STUB_NOTIFICATION_DISPATCHER_MAC_H_
#define CHROME_BROWSER_NOTIFICATIONS_MAC_STUB_NOTIFICATION_DISPATCHER_MAC_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/notifications/mac/notification_dispatcher_mac.h"
#include "chrome/services/mac_notifications/public/mojom/mac_notifications.mojom.h"

class StubNotificationDispatcherMac : public NotificationDispatcherMac {
 public:
  using OnDisplayCallback =
      base::RepeatingCallback<void(DisplayNotificationCallback)>;

  StubNotificationDispatcherMac();
  StubNotificationDispatcherMac(const StubNotificationDispatcherMac&) = delete;
  StubNotificationDispatcherMac& operator=(
      const StubNotificationDispatcherMac&) = delete;
  ~StubNotificationDispatcherMac() override;

  // NotificationDispatcherMac:
  void DisplayNotification(NotificationHandler::Type notification_type,
                           Profile* profile,
                           const message_center::Notification& notification,
                           DisplayNotificationCallback callback) override;
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

  const std::vector<mac_notifications::mojom::NotificationPtr>& notifications()
      const {
    return notifications_;
  }

  void set_supports_synchronization(bool supports_synchronization) {
    supports_synchronization_ = supports_synchronization;
  }

  // Configures `DisplayNotification()` to synchronously invoke its callback
  // with `display_result`. Overrides any callback previously set via
  // `set_on_display_callback()`.
  void set_display_result(
      mac_notifications::mojom::DisplayNotificationResult display_result);

  // Configures `DisplayNotification()` to pass its completion callback to
  // `callback` instead of completing synchronously, allowing tests to defer
  // completion and choose the `DisplayNotificationResult`. Note that closing a
  // notification while its callback is deferred will not suppress a subsequent
  // `kSuccess` completion from recording the notification.
  void set_on_display_callback(OnDisplayCallback callback);

  base::WeakPtr<StubNotificationDispatcherMac> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  void OnDisplayFinished(
      MacNotificationIdentifier identifier,
      mac_notifications::mojom::NotificationPtr notification,
      DisplayNotificationCallback callback,
      mac_notifications::mojom::DisplayNotificationResult result);
  void EraseNotificationWithId(const MacNotificationIdentifier& identifier);

  std::vector<mac_notifications::mojom::NotificationPtr> notifications_;
  bool supports_synchronization_ = true;
  OnDisplayCallback on_display_callback_;

  base::WeakPtrFactory<StubNotificationDispatcherMac> weak_factory_{this};
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_MAC_STUB_NOTIFICATION_DISPATCHER_MAC_H_
