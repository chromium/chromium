// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_SCHEDULED_TASK_EXECUTOR_IMPL_H_
#define CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_SCHEDULED_TASK_EXECUTOR_IMPL_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scheduled_task_executor.h"
#include "chromeos/dbus/power/native_timer.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

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
             TimerCallback timer_expired_cb,
             base::TimeDelta external_delay = base::TimeDelta()) override;
  void Reset() override;
  const base::Time GetScheduledTaskTime() const override;

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

  // Time when the task will be executed.
  base::Time scheduled_task_time_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_SCHEDULED_TASK_EXECUTOR_IMPL_H_
