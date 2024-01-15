// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_CHROMEOS_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_CHROMEOS_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"
#include "chrome/browser/notifications/notification_platform_bridge_delegate.h"
#include "chrome/browser/notifications/profile_notification.h"

// A platform bridge that uses Ash's message center to display notifications.
// Forwards requests to a helper implementation class, which either makes
// in-process C++ calls (pre-lacros) or mojo calls (post-lacros).
class NotificationPlatformBridgeChromeOs
    : public NotificationPlatformBridge,
      public NotificationPlatformBridgeDelegate {
 public:
  NotificationPlatformBridgeChromeOs();
  NotificationPlatformBridgeChromeOs(
      const NotificationPlatformBridgeChromeOs&) = delete;
  NotificationPlatformBridgeChromeOs& operator=(
      const NotificationPlatformBridgeChromeOs&) = delete;
  ~NotificationPlatformBridgeChromeOs() override;

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

  // NotificationPlatformBridgeDelegate:
  void HandleNotificationClosed(const std::string& id, bool by_user) override;
  void HandleNotificationClicked(const std::string& id) override;
  void HandleNotificationButtonClicked(
      const std::string& id,
      int button_index,
      const std::optional<std::u16string>& reply) override;
  void HandleNotificationSettingsButtonClicked(const std::string& id) override;
  void DisableNotification(const std::string& id) override;

 private:
  // Gets the ProfileNotification for the given identifier which has been
  // mutated to uniquely identify the profile. This may return null if the
  // notification has already been closed due to profile shutdown. Ash may
  // asynchronously inform |this| of actions on notifications after their
  // associated profile has already been destroyed.
  ProfileNotification* GetProfileNotification(
      const std::string& profile_notification_id);

  // Callback after getting displayed notifications from the |impl_| to convert
  // profile IDs back to the original notification IDs.
  void OnGetDisplayed(GetDisplayedNotificationsCallback callback,
                      std::set<std::string> notification_ids,
                      bool supports_synchronization) const;

  // Helper implementation.
  std::unique_ptr<NotificationPlatformBridge> impl_;

  // A container for all active notifications, where IDs are permuted to
  // uniquely identify both the notification and its source profile. The key is
  // the permuted ID.
  std::map<std::string, std::unique_ptr<ProfileNotification>>
      active_notifications_;

  base::WeakPtrFactory<NotificationPlatformBridgeChromeOs> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_CHROMEOS_H_
