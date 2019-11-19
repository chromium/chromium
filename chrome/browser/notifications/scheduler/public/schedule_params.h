// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_SCHEDULE_PARAMS_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_SCHEDULE_PARAMS_H_

#include <map>

#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"

namespace notifications {

// Specifies when to show the scheduled notification, and throttling details.
struct ScheduleParams {
  enum class Priority {
    // Notification may be delivered if picked by display decision layer. Most
    // notification types should use this priority.
    kLow,
    // No notification throttling logic is applied, every notification scheduled
    // will be delivered.
    kNoThrottle,
  };

  ScheduleParams();
  ScheduleParams(const ScheduleParams& other);
  bool operator==(const ScheduleParams& other) const;
  ~ScheduleParams();

  Priority priority;

  // Override the default mapping from an user action to impression result. By
  // default, click on the notification and helpful button click are positive
  // impression and may increase feature exposure. Unhelp button click is
  // negative impression and may reduce feature exposure. Dimiss/close
  // notification is neutural. Only put value when need to change the default
  // mapping.
  std::map<UserFeedback, ImpressionResult> impression_mapping;

  // The start time of the deliver time window of the notification.
  base::Optional<base::Time> deliver_time_start;

  // The end time of the deliver time window of the notification. Use in pair
  // with |deliver_time_start|.
  base::Optional<base::Time> deliver_time_end;

  // Support a custom suppression duration(in days) for the notification.
  // If client sets this field, it will override |suppression_duration| in
  // config.
  base::Optional<base::TimeDelta> custom_suppression_duration;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_SCHEDULE_PARAMS_H_
