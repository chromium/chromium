// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_SCHEDULED_TASK_HANDLER_SCHEDULED_TASK_EXECUTOR_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_SCHEDULED_TASK_HANDLER_SCHEDULED_TASK_EXECUTOR_IMPL_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/scheduled_task_handler/scheduled_task_executor.h"
#include "chromeos/dbus/power/native_timer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/icu/source/i18n/unicode/calendar.h"

namespace policy {

// This class is used to start the native timer for a scheduled task.
class ScheduledTaskExecutorImpl : public ScheduledTaskExecutor {
 public:
  explicit ScheduledTaskExecutorImpl(const char* timer_tag);
  ScheduledTaskExecutorImpl(const ScheduledTaskExecutorImpl&) = delete;
  ScheduledTaskExecutorImpl& operator=(const ScheduledTaskExecutorImpl&) =
      delete;
  ~ScheduledTaskExecutorImpl() override;

  // ScheduledTaskExecutor:
  void Start(ScheduledTaskData* scheduled_task_data,
             chromeos::OnStartNativeTimerCallback result_cb,
             TimerCallback timer_expired_cb) override;

  // ScheduledTaskExecutor:
  void Reset() override;

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

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_SCHEDULED_TASK_HANDLER_SCHEDULED_TASK_EXECUTOR_IMPL_H_
