// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SYSTEM_NOTIFICATION_HELPER_H_
#define CHROME_BROWSER_NOTIFICATIONS_SYSTEM_NOTIFICATION_HELPER_H_

#include <memory>
#include <string>

#include "ui/message_center/public/cpp/notification.h"

class NotificationDisplayService;

// This class assists in displaying notifications that do not have an associated
// Profile.
class SystemNotificationHelper {
 public:
  // Returns the global instance.
  static SystemNotificationHelper* GetInstance();

  // Note that only single instance of this class should be created. The
  // instance is retrievable by SystemNotificationHelper::GetInstance().
  SystemNotificationHelper();
  SystemNotificationHelper(const SystemNotificationHelper&) = delete;
  SystemNotificationHelper& operator=(const SystemNotificationHelper&) = delete;
  ~SystemNotificationHelper();

  // Displays a notification which isn't tied to a normal user profile. The
  // notification will be displayed asynchronously if the generic profile has
  // not yet been loaded.
  void Display(const message_center::Notification& notification);

  // Closes a notification which isn't tied to a normal user profile.
  void Close(const std::string& notification_id);

  void SetSystemServiceForTesting(
      std::unique_ptr<NotificationDisplayService> service);

 private:
  // Gets or creates a NotificationDisplayService for system notifications.
  NotificationDisplayService* GetSystemService();

  // The global system NotificationDisaplyService, not bound to any profile.
  std::unique_ptr<NotificationDisplayService> system_service_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_SYSTEM_NOTIFICATION_HELPER_H_
