// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TEST_UTIL_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TEST_UTIL_H_

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/notifications/profile_notification.h"
#include "ui/message_center/public/cpp/notification.h"

class Profile;

class StubNotificationUIManager : public NotificationUIManager {
 public:
  StubNotificationUIManager();
  StubNotificationUIManager(const StubNotificationUIManager&) = delete;
  StubNotificationUIManager& operator=(const StubNotificationUIManager&) =
      delete;
  ~StubNotificationUIManager() override;

  // Returns the number of currently active notifications.
  unsigned int GetNotificationCount() const;

  // Returns a reference to the notification at index |index|.
  const message_center::Notification& GetNotificationAt(
      unsigned int index) const;

  // Emulates clearing a notification from the notification center
  // without running any of the delegates. This may happen when native
  // notification centers don't inform us about closed notifications,
  // for example as a result of a system reboot.
  bool SilentDismissById(const std::string& delegate_id,
                         ProfileNotification::ProfileID profile_id);

  // NotificationUIManager implementation.
  void Add(const message_center::Notification& notification,
           Profile* profile) override;
  bool Update(const message_center::Notification& notification,
              Profile* profile) override;
  const message_center::Notification* FindById(
      const std::string& delegate_id,
      ProfileNotification::ProfileID profile_id) const override;
  bool CancelById(const std::string& delegate_id,
                  ProfileNotification::ProfileID profile_id) override;
  std::set<std::string> GetAllIdsByProfile(
      ProfileNotification::ProfileID profile_id) override;
  std::set<std::string> GetAllIdsByProfileAndOrigin(
      ProfileNotification::ProfileID profile_id,
      const GURL& origin) override;
  bool CancelAllBySourceOrigin(const GURL& source_origin) override;
  void CancelAll() override;
  void StartShutdown() override;

  GURL& last_canceled_source() { return last_canceled_source_; }

 private:
  using NotificationPair =
      std::pair<message_center::Notification, ProfileNotification::ProfileID>;
  std::vector<NotificationPair> notifications_;

  bool is_shutdown_started_ = false;
  GURL last_canceled_source_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TEST_UTIL_H_
