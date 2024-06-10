// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_MAC_STUB_NOTIFICATION_DISPATCHER_MAC_H_
#define CHROME_BROWSER_NOTIFICATIONS_MAC_STUB_NOTIFICATION_DISPATCHER_MAC_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/notifications/mac/notification_dispatcher_mac.h"
#include "chrome/services/mac_notifications/public/mojom/mac_notifications.mojom.h"

class StubNotificationDispatcherMac : public NotificationDispatcherMac {
 public:
  StubNotificationDispatcherMac();
  StubNotificationDispatcherMac(const StubNotificationDispatcherMac&) = delete;
  StubNotificationDispatcherMac& operator=(
      const StubNotificationDispatcherMac&) = delete;
  ~StubNotificationDispatcherMac() override;

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

  const std::vector<mac_notifications::mojom::NotificationPtr>& notifications()
      const {
    return notifications_;
  }

  base::WeakPtr<StubNotificationDispatcherMac> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  std::vector<mac_notifications::mojom::NotificationPtr> notifications_;

  base::WeakPtrFactory<StubNotificationDispatcherMac> weak_factory_{this};
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_MAC_STUB_NOTIFICATION_DISPATCHER_MAC_H_
