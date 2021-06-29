// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_SCHEDULED_TASK_HANDLER_SCHEDULED_TASK_EXECUTOR_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_SCHEDULED_TASK_HANDLER_SCHEDULED_TASK_EXECUTOR_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/native_timer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/icu/source/i18n/unicode/calendar.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace policy {

using TimerCallback = base::OnceClosure;

// This class is used to start the native timer for a scheduled task.
class ScheduledTaskExecutor {
 public:
  // Frequency at which the task should occur.
  enum class Frequency {
    kDaily,
    kWeekly,
    kMonthly,
  };

  // Holds the data associated with the current scheduled task policy.
  struct ScheduledTaskData {
    ScheduledTaskData();
    ScheduledTaskData(const ScheduledTaskData&);
    ~ScheduledTaskData();

    // Corresponds to UCAL_HOUR_OF_DAY in icu::Calendar.
    int hour;

    // Corresponds to UCAL_MINUTE in icu::Calendar.
    int minute;

    Frequency frequency;

    // Only set when frequency is |kWeekly|. Corresponds to UCAL_DAY_OF_WEEK in
    // icu::Calendar. Values between 1 (SUNDAY) to 7 (SATURDAY).
    absl::optional<UCalendarDaysOfWeek> day_of_week;

    // Only set when frequency is |kMonthly|. Corresponds to UCAL_DAY_OF_MONTH
    // in icu::Calendar i.e. values between 1 to 31.
    absl::optional<int> day_of_month;

    // Absolute time ticks when the next scheduled task (i.e. |UpdateCheck|)
    // will happen.
    base::TimeTicks next_scheduled_task_time_ticks;
  };

  explicit ScheduledTaskExecutor(const char* timer_tag);
  ScheduledTaskExecutor(const ScheduledTaskExecutor&) = delete;
  ScheduledTaskExecutor& operator=(const ScheduledTaskExecutor&) = delete;
  virtual ~ScheduledTaskExecutor();

  // Starts the scheduled_task_timer_. Runs result_cb with false result if there
  // was an error while calculating the next_scheduled_task_time_ticks,
  // otherwise starts NativeTimer.
  void Start(ScheduledTaskData* scheduled_task_data,
             chromeos::OnStartNativeTimerCallback result_cb,
             TimerCallback timer_expired_cb);

  // Resets the scheduled_task_timer_.
  void Reset();

 protected:
  // Calculates the delay from |cur_time| at which |scheduled_task_timer_|
  // should run next. Returns 0 delay if the calculation failed due to a
  // concurrent DST or Time Zone change.
  virtual base::TimeDelta CalculateNextScheduledTaskTimerDelay(
      base::Time cur_time,
      ScheduledTaskData* scheduled_task_data);

 private:
  // Returns current time.
  virtual base::Time GetCurrentTime();

  // Returns time ticks from boot including time ticks spent during sleeping.
  virtual base::TimeTicks GetTicksSinceBoot();

  // Returns the current time zone.
  virtual const icu::TimeZone& GetTimeZone();

  // Tag associated with native timer on timer instantiation.
  std::string timer_tag_;

  // Timer that is scheduled to execute the task.
  std::unique_ptr<chromeos::NativeTimer> scheduled_task_timer_;
};

namespace scheduled_task_internal {
// Used as canonical value for timer delay calculations.
constexpr base::TimeDelta kInvalidDelay = base::TimeDelta();

// Calculates the difference in milliseconds of |a| - |b|. Caller has to ensure
// |a| >= |b|.
base::TimeDelta GetDiff(const icu::Calendar& a, const icu::Calendar& b);

// Converts |cur_time| to ICU time in the time zone |tz|.
std::unique_ptr<icu::Calendar> ConvertUtcToTzIcuTime(base::Time cur_time,
                                                     const icu::TimeZone& tz);
}  // namespace scheduled_task_internal

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_SCHEDULED_TASK_HANDLER_SCHEDULED_TASK_EXECUTOR_H_
