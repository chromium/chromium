// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/test/mock_time_message_pump.h"

#include <algorithm>
#include <ostream>

#include "base/auto_reset.h"
#include "base/notreached.h"
#include "base/test/simple_test_tick_clock.h"

namespace base {
namespace sequence_manager {

MockTimeMessagePump::MockTimeMessagePump(SimpleTestTickClock* clock)
    : clock_(clock) {}

MockTimeMessagePump::~MockTimeMessagePump() = default;

bool MockTimeMessagePump::MaybeAdvanceTime(TimeTicks target_time) {
  auto now = clock_->NowTicks();

  if (target_time <= now)
    return true;

  TimeTicks next_now;

  if (!target_time.is_max()) {
    next_now = std::min(allow_advance_until_, target_time);
  } else if (allow_advance_until_ == TimeTicks::Max()) {
    next_now = now;
  } else {
    next_now = allow_advance_until_;
  }

  if (now < next_now) {
    clock_->SetNowTicks(next_now);
    return true;
  }
  return false;
}

void MockTimeMessagePump::Run(Delegate* delegate) {
  AutoReset<bool> auto_reset_keep_running(&keep_running_, true);

  for (;;) {
    Delegate::NextWorkInfo info = delegate->DoWork();

    if (!keep_running_ || quit_after_do_some_work_)
      break;

    if (info.is_immediate())
      continue;

    delegate->DoIdleWork();
    if (!keep_running_)
      break;

    if (MaybeAdvanceTime(info.delayed_run_time))
      continue;

    next_wake_up_time_ = info.delayed_run_time;

    if (stop_when_message_pump_is_idle_)
      return;

    NOTREACHED() << "Pump would go to sleep. Probably not what you wanted, "
                    "consider rewriting your test.";
  }
}

void MockTimeMessagePump::Quit() {
  keep_running_ = false;
}

void MockTimeMessagePump::ScheduleWork() {}

void MockTimeMessagePump::ScheduleDelayedWork(
    const Delegate::NextWorkInfo& next_work_info) {
  next_wake_up_time_ = next_work_info.delayed_run_time;
}

}  // namespace sequence_manager
}  // namespace base
