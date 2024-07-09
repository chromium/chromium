// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/scheduler_test_common.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/tick_clock.h"
#include "cc/debug/rendering_stats_instrumentation.h"

namespace cc {

std::unique_ptr<FakeCompositorTimingHistory>
FakeCompositorTimingHistory::Create(
    bool using_synchronous_renderer_compositor) {
  std::unique_ptr<RenderingStatsInstrumentation>
      rendering_stats_instrumentation = RenderingStatsInstrumentation::Create();
  return base::WrapUnique(new FakeCompositorTimingHistory(
      using_synchronous_renderer_compositor,
      std::move(rendering_stats_instrumentation)));
}

FakeCompositorTimingHistory::FakeCompositorTimingHistory(
    bool using_synchronous_renderer_compositor,
    std::unique_ptr<RenderingStatsInstrumentation>
        rendering_stats_instrumentation)
    : CompositorTimingHistory(CompositorTimingHistory::NULL_UMA,
                              rendering_stats_instrumentation.get()),
      rendering_stats_instrumentation_owned_(
          std::move(rendering_stats_instrumentation)) {}

FakeCompositorTimingHistory::~FakeCompositorTimingHistory() = default;

void FakeCompositorTimingHistory::SetAllEstimatesTo(base::TimeDelta duration) {
  begin_main_frame_queue_duration_critical_ = duration;
  begin_main_frame_queue_duration_not_critical_ = duration;
  begin_main_frame_start_to_ready_to_commit_duration_ = duration;
  commit_duration_ = duration;
  commit_to_ready_to_activate_duration_ = duration;
  activate_duration_ = duration;
  draw_duration_ = duration;
}

void FakeCompositorTimingHistory::
    SetBeginMainFrameQueueDurationCriticalEstimate(base::TimeDelta duration) {
  begin_main_frame_queue_duration_critical_ = duration;
}

void FakeCompositorTimingHistory::
    SetBeginMainFrameQueueDurationNotCriticalEstimate(
        base::TimeDelta duration) {
  begin_main_frame_queue_duration_not_critical_ = duration;
}

void FakeCompositorTimingHistory::
    SetBeginMainFrameStartToReadyToCommitDurationEstimate(
        base::TimeDelta duration) {
  begin_main_frame_start_to_ready_to_commit_duration_ = duration;
}

void FakeCompositorTimingHistory::SetCommitDurationEstimate(
    base::TimeDelta duration) {
  commit_duration_ = duration;
}

void FakeCompositorTimingHistory::SetCommitToReadyToActivateDurationEstimate(
    base::TimeDelta duration) {
  commit_to_ready_to_activate_duration_ = duration;
}

void FakeCompositorTimingHistory::SetActivateDurationEstimate(
    base::TimeDelta duration) {
  activate_duration_ = duration;
}

void FakeCompositorTimingHistory::SetDrawDurationEstimate(
    base::TimeDelta duration) {
  draw_duration_ = duration;
}

void FakeCompositorTimingHistory::SetBeginMainFrameSentTime(
    base::TimeTicks time) {
  begin_main_frame_sent_time_ = time;
}

base::TimeDelta
FakeCompositorTimingHistory::BeginMainFrameQueueDurationCriticalEstimate()
    const {
  return begin_main_frame_queue_duration_critical_;
}

base::TimeDelta
FakeCompositorTimingHistory::BeginMainFrameQueueDurationNotCriticalEstimate()
    const {
  return begin_main_frame_queue_duration_not_critical_;
}

base::TimeDelta FakeCompositorTimingHistory::
    BeginMainFrameStartToReadyToCommitDurationEstimate() const {
  return begin_main_frame_start_to_ready_to_commit_duration_;
}

base::TimeDelta FakeCompositorTimingHistory::CommitDurationEstimate() const {
  return commit_duration_;
}

base::TimeDelta
FakeCompositorTimingHistory::CommitToReadyToActivateDurationEstimate() const {
  return commit_to_ready_to_activate_duration_;
}

base::TimeDelta FakeCompositorTimingHistory::ActivateDurationEstimate() const {
  return activate_duration_;
}

base::TimeDelta FakeCompositorTimingHistory::DrawDurationEstimate() const {
  return draw_duration_;
}

TestScheduler::TestScheduler(
    const base::TickClock* now_src,
    SchedulerClient* client,
    const SchedulerSettings& scheduler_settings,
    int layer_tree_host_id,
    base::SingleThreadTaskRunner* task_runner,
    std::unique_ptr<CompositorTimingHistory> compositor_timing_history,
    CompositorFrameReportingController* compositor_frame_reporting_controller)
    : Scheduler(client,
                scheduler_settings,
                layer_tree_host_id,
                task_runner,
                std::move(compositor_timing_history),
                compositor_frame_reporting_controller),
      now_src_(now_src) {}

base::TimeTicks TestScheduler::Now() const {
  return now_src_->NowTicks();
}

TestScheduler::~TestScheduler() = default;

}  // namespace cc
