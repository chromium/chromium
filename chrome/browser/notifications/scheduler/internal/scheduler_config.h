// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_SCHEDULER_CONFIG_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_SCHEDULER_CONFIG_H_

#include <memory>

#include "base/time/time.h"

namespace notifications {

// Configure the maxmium number of notifications daily shown for all types.
constexpr char kMaxDailyShownAllTypeConfig[] = "max_daily_shown_all_type";

// Configure the maxmium number of notifications daily shown per type.
constexpr char kMaxDailyShownPerTypeConfig[] = "max_daily_shown_per_type";

// Configure the initial number of notifications daily shown per type.
constexpr char kInitialDailyShownPerTypeConfig[] =
    "initial_daily_shown_per_type";

// Configure the expiration duration for notifications.
constexpr char kNotificationExpirationConfig[] =
    "notification_expiration_in_days";

// Configure the expiration duration for impressions.
constexpr char kImpressionExpirationConfig[] = "impression_expiration_in_days";

// Configure the expiration duration for suppression.
constexpr char kSuppressionDurationConfig[] = "suppression_duration_in_days";

// Configure the number of dismiss count.
constexpr char kDismissCountConfig[] = "dismiss_count";

// Configure the duration of a dismiss.
constexpr char kDismissDurationConfig[] = "dismiss_duration_in_days";

// Configure the duration of background task window.
constexpr char kBackgroundTaskWindowDurationConfig[] =
    "background_task_window_duration_in_hours";

// Configuration of notification scheduler system.
struct SchedulerConfig {
  // Creates a default scheduler config.
  static std::unique_ptr<SchedulerConfig> Create();

  static std::unique_ptr<SchedulerConfig> CreateFromFinch();

  SchedulerConfig();
  SchedulerConfig(const SchedulerConfig&) = delete;
  SchedulerConfig& operator=(const SchedulerConfig&) = delete;
  ~SchedulerConfig();

  // Maximum number of all types of notifications shown to the user per day.
  int max_daily_shown_all_type;

  // Maximum number of notifications shown to the user per day for each type.
  int max_daily_shown_per_type;

  // The initial number of notifications shown to the user per day for each
  // type.
  int initial_daily_shown_per_type;

  // The time for a notification entry to expire. The
  // notification entry will be deleted then.
  base::TimeDelta notification_expiration;

  // The time for a notification impression history data to expire. The
  // impression history will be deleted then.
  base::TimeDelta impression_expiration;

  // Duration of suppression when negative impression is applied.
  base::TimeDelta suppression_duration;

  // The number of consecutive notification dismisses to generate a dismiss
  // event.
  int dismiss_count;

  // Used to check whether |dismiss_count| consecutive notification dimisses are
  // in this duration, to generate a dismiss event.
  base::TimeDelta dismiss_duration;

  // The time window to launch the background task.
  base::TimeDelta background_task_window_duration;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_SCHEDULER_CONFIG_H_
