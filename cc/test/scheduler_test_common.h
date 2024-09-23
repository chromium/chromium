// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_SCHEDULER_TEST_COMMON_H_
#define CC_TEST_SCHEDULER_TEST_COMMON_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "cc/metrics/compositor_timing_history.h"
#include "cc/metrics/dropped_frame_counter.h"
#include "cc/scheduler/scheduler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class TickClock;
}

namespace cc {

class RenderingStatsInstrumentation;

class FakeCompositorTimingHistory : public CompositorTimingHistory {
 public:
  static std::unique_ptr<FakeCompositorTimingHistory> Create(
      bool using_synchronous_renderer_compositor);
  FakeCompositorTimingHistory(const FakeCompositorTimingHistory&) = delete;
  ~FakeCompositorTimingHistory() override;

  FakeCompositorTimingHistory& operator=(const FakeCompositorTimingHistory&) =
      delete;

  void SetAllEstimatesTo(base::TimeDelta duration);

  void SetBeginMainFrameQueueDurationCriticalEstimate(base::TimeDelta duration);
  void SetBeginMainFrameQueueDurationNotCriticalEstimate(
      base::TimeDelta duration);
  void SetBeginMainFrameStartToReadyToCommitDurationEstimate(
      base::TimeDelta duration);
  void SetCommitToReadyToActivateDurationEstimate(base::TimeDelta duration);
  void SetCommitDurationEstimate(base::TimeDelta duration);
  void SetActivateDurationEstimate(base::TimeDelta duration);
  void SetDrawDurationEstimate(base::TimeDelta duration);
  void SetBeginMainFrameSentTime(base::TimeTicks time);

  base::TimeDelta BeginMainFrameQueueDurationCriticalEstimate() const override;
  base::TimeDelta BeginMainFrameQueueDurationNotCriticalEstimate()
      const override;
  base::TimeDelta BeginMainFrameStartToReadyToCommitDurationEstimate()
      const override;
  base::TimeDelta CommitDurationEstimate() const override;
  base::TimeDelta CommitToReadyToActivateDurationEstimate() const override;
  base::TimeDelta ActivateDurationEstimate() const override;
  base::TimeDelta DrawDurationEstimate() const override;

 protected:
  FakeCompositorTimingHistory(bool using_synchronous_renderer_compositor,
                              std::unique_ptr<RenderingStatsInstrumentation>
                                  rendering_stats_instrumentation_owned);

  std::unique_ptr<RenderingStatsInstrumentation>
      rendering_stats_instrumentation_owned_;

  base::TimeDelta begin_main_frame_queue_duration_critical_;
  base::TimeDelta begin_main_frame_queue_duration_not_critical_;
  base::TimeDelta begin_main_frame_start_to_ready_to_commit_duration_;
  base::TimeDelta commit_duration_;
  base::TimeDelta commit_to_ready_to_activate_duration_;
  base::TimeDelta activate_duration_;
  base::TimeDelta draw_duration_;
};

class TestScheduler : public Scheduler {
 public:
  TestScheduler(
      const base::TickClock* now_src,
      SchedulerClient* client,
      const SchedulerSettings& scheduler_settings,
      int layer_tree_host_id,
      base::SingleThreadTaskRunner* task_runner,
      std::unique_ptr<CompositorTimingHistory> compositor_timing_history,
      CompositorFrameReportingController*
          compositor_frame_reporting_controller);
  TestScheduler(const TestScheduler&) = delete;

  TestScheduler& operator=(const TestScheduler&) = delete;

  bool NeedsBeginMainFrame() const {
    return state_machine_.needs_begin_main_frame();
  }

  viz::BeginFrameSource& frame_source() { return *begin_frame_source_; }

  bool MainThreadMissedLastDeadline() const {
    return state_machine_.main_thread_missed_last_deadline();
  }

  bool begin_frames_expected() const {
    return begin_frame_source_ && observing_begin_frame_source_;
  }

  bool BeginFrameNeeded() const { return state_machine_.BeginFrameNeeded(); }

  int current_frame_number() const {
    return state_machine_.current_frame_number();
  }

  bool needs_impl_side_invalidation() const {
    return state_machine_.needs_impl_side_invalidation();
  }

  ~TestScheduler() override;

  base::TimeDelta BeginImplFrameInterval() {
    return begin_impl_frame_tracker_.Interval();
  }

  // Note: This setting will be overriden on the next BeginFrame in the
  // scheduler. To control the value it gets on the next BeginFrame
  // Pass in a fake CompositorTimingHistory that indicates BeginMainFrame
  // to Activation is fast.
  void SetCriticalBeginMainFrameToActivateIsFast(bool is_fast) {
    state_machine_.SetCriticalBeginMainFrameToActivateIsFast(is_fast);
  }

  bool ImplLatencyTakesPriority() const {
    return state_machine_.ImplLatencyTakesPriority();
  }

  const SchedulerStateMachine& state_machine() const { return state_machine_; }

 protected:
  // Overridden from Scheduler.
  base::TimeTicks Now() const override;

 private:
  raw_ptr<const base::TickClock> now_src_;
};

}  // namespace cc

#endif  // CC_TEST_SCHEDULER_TEST_COMMON_H_
