// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TRIGGER_SCHEDULER_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TRIGGER_SCHEDULER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace content {
class StoragePartition;
}  // namespace content

class NotificationTriggerScheduler {
 public:
  static std::unique_ptr<NotificationTriggerScheduler> Create();

  // Triggers pending notifications for all loaded profiles.
  static void TriggerNotifications();

  virtual ~NotificationTriggerScheduler();

  // Schedules a trigger at |timestamp| that calls TriggerNotifications on each
  // StoragePartition of profiles that have pending notifications at that time.
  // If there is an existing earlier trigger set, this is a nop. Otherwise this
  // overwrites the existing trigger so only the earliest is set at any time.
  virtual void ScheduleTrigger(base::Time timestamp);

  // Triggers pending notifications for |partition|.
  // TODO(knollr): Mock the actual storage partitions to observe this call in
  // tests and make this static in the implementation.
  virtual void TriggerNotificationsForStoragePartition(
      content::StoragePartition* partition);

 protected:
  NotificationTriggerScheduler();

 private:
  base::OneShotTimer trigger_timer_;

  DISALLOW_COPY_AND_ASSIGN(NotificationTriggerScheduler);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_TRIGGER_SCHEDULER_H_
