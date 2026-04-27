// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_default.h"

#include <optional>

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_APPLE)
#include <mach/thread_policy.h>

#include "base/apple/mach_logging.h"
#include "base/apple/scoped_mach_port.h"
#include "base/apple/scoped_nsautorelease_pool.h"
#include "base/threading/threading_features.h"
#endif

namespace base {

namespace {
enum class BusyLoopPredictionAccuracy {
  // Heuristic predicted busy loop, but no task arrived within the max busy
  // loop duration.
  kFalsePositive,
  // Heuristic predicted busy loop, and a task arrived within the max busy loop
  // duration.
  kTruePositive,
  // Heuristic predicted no busy loop, but a task arrived within the max busy
  // loop duration.
  kFalseNegative,
  // Heuristic predicted no busy loop, and no task arrived within the max busy
  // loop duration.
  kTrueNegative,
  kMaxValue = kTrueNegative,
};
}  // namespace

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

    bool should_busy_loop = ShouldBusyLoop();
    bool signaled = false;
    if (should_busy_loop) {
      signaled = BusyWaitOnEvent(before, next_work_info.remaining_delay());
      if (!signaled) {
        next_work_info.recent_now = base::TimeTicks::Now();
        event_.TimedWait(next_work_info.remaining_delay());
      }
    } else {
      event_.TimedWait(next_work_info.remaining_delay());
    }

    if (may_busy_loop) {
      base::TimeDelta wait_time = base::TimeTicks::Now() - before;
      RecordWaitTime(wait_time);

      if (base::ShouldRecordSubsampledMetric(0.001)) {
        BusyLoopPredictionAccuracy heuristic_result =
            should_busy_loop
                ? (signaled ? BusyLoopPredictionAccuracy::kTruePositive
                            : BusyLoopPredictionAccuracy::kFalsePositive)
                : (wait_time > max_busy_loop_time_
                       ? BusyLoopPredictionAccuracy::kTrueNegative
                       : BusyLoopPredictionAccuracy::kFalseNegative);
        UMA_HISTOGRAM_ENUMERATION(
            "Scheduling.MessagePumpDefault.BusyLoop.PredictionAccuracy",
            heuristic_result);
      }
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

bool MessagePumpDefault::BusyWaitOnEvent(base::TimeTicks before,
                                         base::TimeDelta next_work_delay) {
  base::TimeDelta max_busy_loop_time =
      std::min(max_busy_loop_time_, next_work_delay);

  const bool should_sample = base::ShouldRecordSubsampledMetric(0.001);

  std::optional<base::ElapsedTimer> timer;
  if (should_sample) {
    timer.emplace();
  }

  bool signaled = false;
  {
    TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("base"), "BusyWait",
                "last_wait_time_ms", last_wait_time_.InMillisecondsF(),
                "wait_time_exponential_moving_average_ms",
                wait_time_exponential_moving_average_.InMillisecondsF());
    do {
      signaled = event_.TimedWait(base::TimeDelta());
    } while (!signaled &&
             (base::TimeTicks::Now() - before) < max_busy_loop_time);
  }

  if (should_sample) {
    base::TimeDelta busy_loop_duration = timer->Elapsed();
    // The maximum busy loop time is much lower than 100ms but set the
    // histogram's upper bound to 100ms to capture cases where the thread is
    // descheduled while busy looping.
    if (signaled) {
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          "Scheduling.MessagePumpDefault.BusyLoop.Duration.TaskArrived",
          busy_loop_duration, base::Microseconds(1), base::Milliseconds(100),
          50);
    } else {
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          "Scheduling.MessagePumpDefault.BusyLoop.Duration.TimedOut",
          busy_loop_duration, base::Microseconds(1), base::Milliseconds(100),
          50);
    }

    UMA_HISTOGRAM_BOOLEAN("Scheduling.MessagePumpDefault.BusyLoop.TaskArrived",
                          signaled);
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Scheduling.MessagePumpDefault.BusyLoop.TargetDuration",
        max_busy_loop_time, base::Microseconds(1), base::Milliseconds(10), 50);
  }

  return signaled;
}

}  // namespace base
