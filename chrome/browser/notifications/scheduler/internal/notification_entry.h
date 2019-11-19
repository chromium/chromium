// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_NOTIFICATION_ENTRY_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_NOTIFICATION_ENTRY_H_

#include <map>
#include <string>

#include "base/time/time.h"
#include "chrome/browser/notifications/scheduler/public/notification_data.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"
#include "chrome/browser/notifications/scheduler/public/schedule_params.h"

namespace notifications {

// Represents the in-memory counterpart of scheduled notification database
// record.
struct NotificationEntry {
  NotificationEntry();
  NotificationEntry(SchedulerClientType type, const std::string& guid);
  NotificationEntry(const NotificationEntry& other);
  bool operator==(const NotificationEntry& other) const;
  bool operator!=(const NotificationEntry& other) const;
  ~NotificationEntry();

  // The type of the notification.
  SchedulerClientType type;

  // The unique id of the notification database entry.
  std::string guid;

  // Creation timestamp.
  base::Time create_time;

  // Contains information to construct the notification. The icon data is
  // persisted to disk and only loads into memory before the notification is
  // shown.
  NotificationData notification_data;

  // The map of icons uuid on notification, which must be
  // loaded asynchronously into memory.
  std::map<IconType, std::string> icons_uuid;

  // Scheduling details.
  ScheduleParams schedule_params;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_NOTIFICATION_ENTRY_H_
