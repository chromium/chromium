// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/scheduled_task_handler/scheduled_task_executor_impl.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chromeos/settings/timezone_settings.h"
#include "third_party/icu/source/i18n/unicode/gregocal.h"

namespace policy {

namespace {
// Returns true iff a >= b.
bool IsCalGreaterThanEqual(const icu::Calendar& a, const icu::Calendar& b) {
  UErrorCode status = U_ZERO_ERROR;
  if (a.after(b, status)) {
    DCHECK(U_SUCCESS(status));
    return true;
  }

  if (a.equals(b, status)) {
    DCHECK(U_SUCCESS(status));
    return true;
  }

  return false;
}

// Advances |time| based on the policy represented by
// |scheduled_task_data|.
//
// For daily policy - Advances |time| by 1 day.
// For weekly policy - Advances |time| by 1 week.
// For monthly policy - Advances |time| by 1 month.
//
// Returns true on success and false if it failed to set a valid time.
bool AdvanceTimeBasedOnPolicy(
    const ScheduledTaskExecutor::ScheduledTaskData& scheduled_task_data,
    icu::Calendar* time) {
  UCalendarDateFields field = UCAL_MONTH;
  switch (scheduled_task_data.frequency) {
    case ScheduledTaskExecutor::Frequency::kDaily:
      field = UCAL_DAY_OF_MONTH;
      break;
    case ScheduledTaskExecutor::Frequency::kWeekly:
      field = UCAL_WEEK_OF_YEAR;
      break;
    case ScheduledTaskExecutor::Frequency::kMonthly:
      break;
  }
  UErrorCode status = U_ZERO_ERROR;
  time->add(field, 1, status);
  return U_SUCCESS(status);
}

// Sets |time| based on the policy represented by |scheduled_task_data|.
// Returns true on success and false if it failed to set a valid time.
bool SetTimeBasedOnPolicy(
    const ScheduledTaskExecutor::ScheduledTaskData& scheduled_task_data,
    icu::Calendar* time) {
  // Set the daily fields first as they will be common across different policy
  // types.
  time->set(UCAL_HOUR_OF_DAY, scheduled_task_data.hour);
  time->set(UCAL_MINUTE, scheduled_task_data.minute);
  time->set(UCAL_SECOND, 0);
  time->set(UCAL_MILLISECOND, 0);

  switch (scheduled_task_data.frequency) {
    case ScheduledTaskExecutor::Frequency::kDaily:
      return true;

    case ScheduledTaskExecutor::Frequency::kWeekly:
      DCHECK(scheduled_task_data.day_of_week);
      time->set(UCAL_DAY_OF_WEEK, scheduled_task_data.day_of_week.value());
      return true;

    case ScheduledTaskExecutor::Frequency::kMonthly: {
      DCHECK(scheduled_task_data.day_of_month);
      UErrorCode status = U_ZERO_ERROR;
      // If policy's |day_of_month| is greater than the maximum days in |time|'s
      // current month then it's set to the last day in the month.
      int cur_max_days_in_month =
          time->getActualMaximum(UCAL_DAY_OF_MONTH, status);
      if (U_FAILURE(status)) {
        LOG(ERROR) << "Failed to get max days in month";
        return false;
      }

      time->set(UCAL_DAY_OF_MONTH,
                std::min(scheduled_task_data.day_of_month.value(),
                         cur_max_days_in_month));
      return true;
    }
  }
}

}  // namespace

namespace scheduled_task_internal {

base::TimeDelta GetDiff(const icu::Calendar& a, const icu::Calendar& b) {
  UErrorCode status = U_ZERO_ERROR;
  UDate a_ms = a.getTime(status);
  DCHECK(U_SUCCESS(status));
  UDate b_ms = b.getTime(status);
  DCHECK(U_SUCCESS(status));
  DCHECK(a_ms >= b_ms);
  return base::TimeDelta::FromMilliseconds(a_ms - b_ms);
}

std::unique_ptr<icu::Calendar> ConvertUtcToTzIcuTime(base::Time cur_time,
                                                     const icu::TimeZone& tz) {
  // Get ms from epoch for |cur_time| and use it to get the new time in |tz|.
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::Calendar> cal_tz =
      std::make_unique<icu::GregorianCalendar>(tz, status);
  if (U_FAILURE(status)) {
    LOG(ERROR) << "Couldn't create calendar";
    return nullptr;
  }
  // Erase current time from the calendar.
  cal_tz->clear();
  time_t ms_from_epoch = cur_time.ToTimeT() * 1000;
  cal_tz->setTime(ms_from_epoch, status);
  if (U_FAILURE(status)) {
    LOG(ERROR) << "Couldn't create calendar";
    return nullptr;
  }

  return cal_tz;
}

}  // namespace scheduled_task_internal

ScheduledTaskExecutorImpl::ScheduledTaskExecutorImpl(const char* timer_tag)
    : timer_tag_(timer_tag) {}

ScheduledTaskExecutorImpl::~ScheduledTaskExecutorImpl() = default;

void ScheduledTaskExecutorImpl::Start(
    ScheduledTaskData* scheduled_task_data,
    chromeos::OnStartNativeTimerCallback result_cb,
    TimerCallback timer_expired_cb) {
  // Only one |ScheduledTaskTimer| can be outstanding
  scheduled_task_timer_.reset();

  DCHECK(scheduled_task_data);

  // For accuracy of the next scheduled task, capture current time as close to
  // the start of this function as possible.
  const base::TimeTicks cur_ticks = GetTicksSinceBoot();
  const base::Time cur_time = GetCurrentTime();

  // If this is a retry then |cur_ticks| could be >=
  // |next_scheduled_task_time_ticks| i.e. the next timer schedule has already
  // passed, recalculate it. Else respect the calculated time.
  if (cur_ticks >= scheduled_task_data->next_scheduled_task_time_ticks) {
    // Calculate the next scheduled task time. In case there is an error while
    // calculating, due to concurrent DST or Time Zone changes, then reschedule
    // this function and try to schedule the task again. There should
    // only be one outstanding task to start the timer. If there is a failure
    // the wake lock is released and acquired again when this task runs.
    base::TimeDelta delay =
        CalculateNextScheduledTaskTimerDelay(cur_time, scheduled_task_data);
    if (delay <= scheduled_task_internal::kInvalidDelay) {
      LOG(ERROR) << "Failed to calculate next scheduled task time";
      std::move(result_cb).Run(false);
      return;
    }
    scheduled_task_data->next_scheduled_task_time_ticks = cur_ticks + delay;
  }

  scheduled_task_timer_ = std::make_unique<chromeos::NativeTimer>(timer_tag_);
  scheduled_task_timer_->Start(
      scheduled_task_data->next_scheduled_task_time_ticks,
      std::move(timer_expired_cb), std::move(result_cb));
}

void ScheduledTaskExecutorImpl::Reset() {
  scheduled_task_timer_.reset();
}

base::TimeDelta ScheduledTaskExecutorImpl::CalculateNextScheduledTaskTimerDelay(
    base::Time cur_time,
    ScheduledTaskData* scheduled_task_data) {
  const auto cur_cal =
      scheduled_task_internal::ConvertUtcToTzIcuTime(cur_time, GetTimeZone());
  if (!cur_cal) {
    LOG(ERROR) << "Failed to get current ICU time";
    return scheduled_task_internal::kInvalidDelay;
  }

  auto scheduled_task_time = base::WrapUnique(cur_cal->clone());
  DCHECK(scheduled_task_time);

  // Set scheduled task time based on the policy in |scheduled_task_data|.
  if (!SetTimeBasedOnPolicy(*scheduled_task_data, scheduled_task_time.get())) {
    LOG(ERROR) << "Failed to set time based on policy";
    return scheduled_task_internal::kInvalidDelay;
  }

  // If the time has already passed it means that the scheduled task needs to be
  // advanced based on the policy i.e. by a day, week or month. The equal to
  // case happens when the timer_expired_cb runs and sets the next
  // |scheduled_task_timer_|. In this case |scheduled_task_time| definitely
  // needs to advance as per the policy. The |SetTimeBasedOnPolicy| is needed
  // for the monthly frequency, it won't change the time after advancing for
  // daily or weekly frequencies. For monthly, if the current time is Feb 28,
  // 1970, 8PM and an update check needs to happen on 7PM every 31st, then
  // setting time above and advancing time below gets us a time of Mar 28, 1970,
  // 7PM. An extra call to |SetTimeBasedOnPolicy| is required to finally get Mar
  // 31, 1970 7PM.
  if (IsCalGreaterThanEqual(*cur_cal, *scheduled_task_time)) {
    if (!AdvanceTimeBasedOnPolicy(*scheduled_task_data,
                                  scheduled_task_time.get())) {
      LOG(ERROR) << "Failed to advance time";
      return scheduled_task_internal::kInvalidDelay;
    }

    if (!SetTimeBasedOnPolicy(*scheduled_task_data,
                              scheduled_task_time.get())) {
      LOG(ERROR) << "Failed to set time based on policy";
      return scheduled_task_internal::kInvalidDelay;
    }
  }
  DCHECK(!IsCalGreaterThanEqual(*cur_cal, *scheduled_task_time));

  return scheduled_task_internal::GetDiff(*scheduled_task_time, *cur_cal);
}

base::Time ScheduledTaskExecutorImpl::GetCurrentTime() {
  return base::Time::Now();
}

base::TimeTicks ScheduledTaskExecutorImpl::GetTicksSinceBoot() {
  struct timespec ts = {};
  int ret = clock_gettime(CLOCK_BOOTTIME, &ts);
  DCHECK_EQ(ret, 0);
  return base::TimeTicks() + base::TimeDelta::FromTimeSpec(ts);
}

const icu::TimeZone& ScheduledTaskExecutorImpl::GetTimeZone() {
  return chromeos::system::TimezoneSettings::GetInstance()->GetTimezone();
}

}  // namespace policy
