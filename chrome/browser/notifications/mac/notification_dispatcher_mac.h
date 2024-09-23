// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_MAC_NOTIFICATION_DISPATCHER_MAC_H_
#define CHROME_BROWSER_NOTIFICATIONS_MAC_NOTIFICATION_DISPATCHER_MAC_H_

#include <string>
#include <tuple>

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/notifications/displayed_notifications_dispatch_callback.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/services/mac_notifications/public/mojom/mac_notifications.mojom.h"
#include "ui/message_center/public/cpp/notification.h"

class Profile;

// Uniquely identifies a notification from any profile on a device.
struct MacNotificationIdentifier {
  std::string notification_id;
  std::string profile_id;
  bool incognito;
  // Comparator so we can use this class in a base::flat_set.
  bool operator<(const MacNotificationIdentifier& rhs) const {
    return std::tie(notification_id, profile_id, incognito) <
           std::tie(rhs.notification_id, rhs.profile_id, rhs.incognito);
  }
};

// Interface to interact with notifications on macOS. It is responsible for one
// style of notifications, either banners or alerts, across multiple profiles.
class NotificationDispatcherMac {
 public:
  // Callback to get all notifications shown on the system for all profiles.
  using GetAllDisplayedNotificationsCallback =
      base::OnceCallback<void(base::flat_set<MacNotificationIdentifier>)>;

  NotificationDispatcherMac() = default;
  NotificationDispatcherMac(const NotificationDispatcherMac&) = delete;
  NotificationDispatcherMac& operator=(const NotificationDispatcherMac&) =
      delete;
  virtual ~NotificationDispatcherMac() = default;

  // Display the given |notification| for |profile|.
  virtual void DisplayNotification(
      NotificationHandler::Type notification_type,
      Profile* profile,
      const message_center::Notification& notification) = 0;

  // Close a notification with the given |identifier|.
  virtual void CloseNotificationWithId(
      const MacNotificationIdentifier& identifier) = 0;

  // Close all notifications for a given |profile_id| and |incognito|.
  virtual void CloseNotificationsWithProfileId(const std::string& profile_id,
                                               bool incognito) = 0;

  // Close all notifications for all profiles.
  virtual void CloseAllNotifications() = 0;

  // Get the currently displayed notifications for |profile_id| and |incognito|.
  // The ids are scoped to the passed in profile and are not globally unique.
  virtual void GetDisplayedNotificationsForProfileId(
      const std::string& profile_id,
      bool incognito,
      GetDisplayedNotificationsCallback callback) = 0;

  // Same as `GetDisplayedNotificationsForProfileId`, but additionally filters
  // the returned ids to only those associated with `origin`.
  virtual void GetDisplayedNotificationsForProfileIdAndOrigin(
      const std::string& profile_id,
      bool incognito,
      const GURL& origin,
      GetDisplayedNotificationsCallback callback) = 0;

  // Get all currently displayed notifications for all profiles.
  virtual void GetAllDisplayedNotifications(
      GetAllDisplayedNotificationsCallback callback) = 0;

  // Informs the dispatcher that the user has initiated a (clean) shutdown of
  // this notification service.
  virtual void UserInitiatedShutdown() = 0;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_MAC_NOTIFICATION_DISPATCHER_MAC_H_
