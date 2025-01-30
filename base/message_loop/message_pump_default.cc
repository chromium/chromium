// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_default.h"

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_APPLE)
#include <mach/thread_policy.h>

#include "base/apple/mach_logging.h"
#include "base/apple/scoped_mach_port.h"
#include "base/apple/scoped_nsautorelease_pool.h"
#include "base/threading/threading_features.h"
#endif

namespace base {

MessagePumpDefault::MessagePumpDefault()
    : keep_running_(true),
      event_(WaitableEvent::ResetPolicy::AUTOMATIC,
             WaitableEvent::InitialState::NOT_SIGNALED) {
  event_.declare_only_used_while_idle();
}

MessagePumpDefault::~MessagePumpDefault() = default;

void MessagePumpDefault::Run(Delegate* delegate) {
  AutoReset<bool> auto_reset_keep_running(&keep_running_, true);

  for (;;) {
#if BUILDFLAG(IS_APPLE)
    apple::ScopedNSAutoreleasePool autorelease_pool;
#endif

    Delegate::NextWorkInfo next_work_info = delegate->DoWork();
    bool has_more_immediate_work = next_work_info.is_immediate();
    if (!keep_running_) {
      break;
    }

    if (has_more_immediate_work) {
      continue;
    }

    delegate->DoIdleWork();
    if (!keep_running_) {
      break;
    }

    base::TimeTicks before;
    bool may_busy_loop = max_busy_loop_time_.is_positive();
    if (may_busy_loop) {
      before = base::TimeTicks::Now();
    }

    if (next_work_info.delayed_run_time.is_max()) {
      if (ShouldBusyLoop()) {
        bool signaled = BusyWaitOnEvent(before);
        if (!signaled) {
          event_.Wait();
        }
      } else {
        event_.Wait();
      }
    } else {
      TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("base"), "TimeWait", "delay_ms",
                  next_work_info.remaining_delay().InMilliseconds());
      // Not handling shorter sleeps to keep the code as simple as possible.
      if (ShouldBusyLoop() &&
          next_work_info.remaining_delay() > max_busy_loop_time_) {
        bool signaled = BusyWaitOnEvent(before);
        if (!signaled) {
          next_work_info.recent_now = base::TimeTicks::Now();
          event_.TimedWait(next_work_info.remaining_delay());
        }
      } else {
        event_.TimedWait(next_work_info.remaining_delay());
      }
    }
    if (may_busy_loop) {
      RecordWaitTime(base::TimeTicks::Now() - before);
    }
    // Since event_ is auto-reset, we don't need to do anything special here
    // other than service each delegate method.
  }
}

void MessagePumpDefault::Quit() {
  keep_running_ = false;
}

void MessagePumpDefault::ScheduleWork() {
  // Since this can be called on any thread, we need to ensure that our Run
  // loop wakes up.
  event_.Signal();
}

void MessagePumpDefault::ScheduleDelayedWork(
    const Delegate::NextWorkInfo& next_work_info) {
  // Since this is always called from the same thread as Run(), there is nothing
  // to do as the loop is already running. It will wait in Run() with the
  // correct timeout when it's out of immediate tasks.
  // TODO(gab): Consider removing ScheduleDelayedWork() when all pumps function
  // this way (bit.ly/merge-message-pump-do-work).
}

void MessagePumpDefault::RecordWaitTime(base::TimeDelta wait_time) {
  last_wait_time_ = wait_time;
  constexpr float kAlpha = .9;
  wait_time_exponential_moving_average_ =
      kAlpha * wait_time_exponential_moving_average_ +
      (1. - kAlpha) * wait_time;
}

bool MessagePumpDefault::ShouldBusyLoop() const {
  // Should only busy loop when the expected wait time is short. Of course, we
  // don't know whether it will be, but we have two crude heuristics here:
  // - Last wait was short, maybe the next one will. Not that if this one is
  //   wrong, it only impacts a single wait.
  // - Recent waits were short (burst of small tasks with waiting in-between)
  //
  // The second one is laggy, both to start and to stop, which is why the first
  // one is there too, to start busy looping faster.
  //
  // One important part though is that to avoid wasting too much power, we
  // should not busy wait for regular sleeps, for instance animations updating
  // at 60Hz.
  return max_busy_loop_time_.is_positive() &&
         (last_wait_time_ < max_busy_loop_time_ ||
          wait_time_exponential_moving_average_ < max_busy_loop_time_);
}

bool MessagePumpDefault::BusyWaitOnEvent(base::TimeTicks before) {
  bool signaled = false;
  {
    TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("base"), "BusyWait",
                "last_wait_time_ms", last_wait_time_.InMillisecondsF(),
                "wait_time_exponential_moving_average_ms",
                wait_time_exponential_moving_average_.InMillisecondsF());
    do {
      signaled = event_.TimedWait(base::TimeDelta());
    } while (!signaled &&
             (base::TimeTicks::Now() - before) < max_busy_loop_time_);
  }
  return signaled;
}

}  // namespace base
