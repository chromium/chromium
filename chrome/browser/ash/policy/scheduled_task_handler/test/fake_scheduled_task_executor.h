// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_TEST_FAKE_SCHEDULED_TASK_EXECUTOR_H_
#define CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_TEST_FAKE_SCHEDULED_TASK_EXECUTOR_H_

#include "base/memory/raw_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scheduled_task_executor.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace policy {

class FakeScheduledTaskExecutor : public ScheduledTaskExecutor {
 public:
  explicit FakeScheduledTaskExecutor(const base::Clock* clock);

  FakeScheduledTaskExecutor(const FakeScheduledTaskExecutor&) = delete;
  FakeScheduledTaskExecutor& operator=(const FakeScheduledTaskExecutor&) =
      delete;

  ~FakeScheduledTaskExecutor() override;

  // ScheduledTaskExecutor implementation:
  void Start(ScheduledTaskData* scheduled_task_data,
             chromeos::OnStartNativeTimerCallback result_cb,
             TimerCallback timer_expired_cb,
             base::TimeDelta small_delay = base::TimeDelta()) override;
  void Reset() override;
  const base::Time GetScheduledTaskTime() const override;

  void SetTimeZone(std::unique_ptr<icu::TimeZone> time_zone);

  base::Time GetCurrentTime() const;

  const icu::TimeZone& GetTimeZone() const;

  void SimulateCalculateNextScheduledTaskFailure(bool simulate);

 private:
  // Clock to use to get current time.
  const raw_ptr<const base::Clock> clock_;

  // The current time zone.
  std::unique_ptr<icu::TimeZone> time_zone_;

  // If set then |CalculateNextUpdateCheckTimerDelay| returns zero delay.
  bool simulate_calculate_next_update_check_failure_ = false;

  // Timer that is scheduled to execute the task.
  base::OneShotTimer timer_;

  // Scheduled task time.
  base::Time scheduled_task_time_;
};
}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_TEST_FAKE_SCHEDULED_TASK_EXECUTOR_H_
