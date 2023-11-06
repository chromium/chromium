// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TRIGGER_SCHEDULER_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TRIGGER_SCHEDULER_H_

#include <memory>

#include "base/time/time.h"
#include "base/timer/timer.h"

namespace content {
class StoragePartition;
}  // namespace content

class Profile;

class NotificationTriggerScheduler {
 public:
  static std::unique_ptr<NotificationTriggerScheduler> Create();

  // Triggers pending notifications for all loaded profiles.
  static void TriggerNotifications();

  NotificationTriggerScheduler(const NotificationTriggerScheduler&) = delete;
  NotificationTriggerScheduler& operator=(const NotificationTriggerScheduler&) =
      delete;
  virtual ~NotificationTriggerScheduler();

 protected:
  // Use NotificationTriggerScheduler::Create() to get an instance of this.
  NotificationTriggerScheduler();

  // Triggers pending notifications for |partition|. Virtual so we can observe
  // PlatformNotificationContextImpl::TriggerNotifications() calls in tests.
  virtual void TriggerNotificationsForStoragePartition(
      content::StoragePartition* partition);

 private:
  // Triggers pending notifications for |profile|.
  static void TriggerNotificationsForProfile(Profile* profile);

  base::OneShotTimer trigger_timer_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TRIGGER_SCHEDULER_H_
