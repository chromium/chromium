// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_TEST_FAKE_SCHEDULED_TASK_EXECUTOR_H_
#define CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_TEST_FAKE_SCHEDULED_TASK_EXECUTOR_H_

#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scheduled_task_executor_impl.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace policy {
// TODO(crbug/1227641): Make FakeScheduledTaskExecutor independendt of the
// implementation.
class FakeScheduledTaskExecutor : public ScheduledTaskExecutorImpl {
 public:
  FakeScheduledTaskExecutor(const base::Clock* clock,
                            const base::TickClock* tick_clock);

  FakeScheduledTaskExecutor(const FakeScheduledTaskExecutor&) = delete;
  FakeScheduledTaskExecutor& operator=(const FakeScheduledTaskExecutor&) =
      delete;

  ~FakeScheduledTaskExecutor() override;

  void SetTimeZone(std::unique_ptr<icu::TimeZone> time_zone);

  base::Time GetCurrentTime() override;

  const icu::TimeZone& GetTimeZone() override;

  void SimulateCalculateNextScheduledTaskFailure(bool simulate);

  base::TimeDelta CalculateNextScheduledTaskTimerDelay(
      base::Time cur_time,
      ScheduledTaskData* scheduled_task_data) override;

 private:
  base::TimeTicks GetTicksSinceBoot() override;
  // Clock to use to get current time.
  const base::Clock* const clock_;

  // Clock to use to calculate time ticks.
  const base::TickClock* const tick_clock_;

  // The current time zone.
  std::unique_ptr<icu::TimeZone> time_zone_;

  // If set then |CalculateNextUpdateCheckTimerDelay| returns zero delay.
  bool simulate_calculate_next_update_check_failure_ = false;
};
}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_TEST_FAKE_SCHEDULED_TASK_EXECUTOR_H_
