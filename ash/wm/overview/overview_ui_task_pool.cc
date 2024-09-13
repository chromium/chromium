// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_ui_task_pool.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

// IMPLEMENTATION details:
// One animation frame typically looks like this:
// 1) OverviewUiTaskPool::OnBeginFrame() - Marks start of a new compositor
//    frame.
// 2) For the next 4-8 ms, the UI thread is occupied generating a compositor
//    frame for the next frame of the animation.
// 3) OverviewUiTaskPool::OnCompositingStarted() - Called when the step above is
//    done, and UI thread's work is complete.
// 4) This leaves several milliseconds of empty space on the UI thread to run
//    miscellaneous tasks without interrupting the compositor's schedule. We can
//    call OverviewUiTaskPool::RunNextTask() during this period. For simplicity,
//    the task pool runs at most one task at this stage.
// 5) OverviewUiTaskPool::OnBeginFrame() - Marks start of next compositor frame.
//    Typically spaced 1/60th of a second apart (~17 ms).
//
// Caveats:
// * If step 2) takes longer than expected, then by the time we arrive at step
//   4), there may not be enough time to run the the next task in the pool. To
//   handle this, the task pool estimates how much time is left before the next
//   begin frame, and if it's less than some value (`kMinTimeRequiredPerTask`),
//   then it skips this compositor frame and waits for another one to do work.
// * This is just a best effort to avoid disrupting the animation. There are
//   still corner cases where it may not work as intended. Mainly, if the UI
//   thread is very congested, the tasks in the pool may not get a chance to
//   run until the caller calls `Flush()`. Or, if the caller provides tasks that
//   take longer than `kMinTimeRequiredPerTask`, it may disrupt the compositor's
//   timing and degrade animation smoothness.

namespace ash {
namespace {

// Rough estimate for the maximum amount of time each individual task should
// take. If tasks are not getting run frequently enough, this value may be too
// large. If tasks are interrupting the animation's smoothness, this value may
// be too small.
constexpr base::TimeDelta kMinTimeRequiredPerTask = base::Milliseconds(4);

// In practice, `OverviewUiTaskPool::OnBeginFrame()` is called shortly after
// the true start of a new frame. This delay is due to the time it takes to
// propagate the "begin frame" message from the gpu process to the browser
// process's UI thread. That gives a little extra time for a task in the pool to
// run. Factoring this into the equation increases the odds of tasks being run.
constexpr base::TimeDelta kExpectedBeginFramePropagationDelay =
    base::Milliseconds(1);

}  // namespace

OverviewUiTaskPool::OverviewUiTaskPool(ui::Compositor* compositor,
                                       base::TimeDelta initial_blackout_period)
    : compositor_(compositor),
      initial_blackout_period_(initial_blackout_period),
      construction_time_(base::TimeTicks::Now()) {
  CHECK(compositor_);
  compositor_observation_.Observe(compositor_);
}

OverviewUiTaskPool::~OverviewUiTaskPool() {
  StopObservingBeginFrames();
}

void OverviewUiTaskPool::AddTask(base::OnceClosure task) {
  pending_tasks_.push_back(std::move(task));
  StartObservingBeginFrames();
}

void OverviewUiTaskPool::Flush() {
  CancelScheduledTask();
  while (!pending_tasks_.empty()) {
    RunNextTask(/*force_task_to_run=*/true);
  }
}

void OverviewUiTaskPool::OnBeginFrame(base::TimeTicks frame_begin_time,
                                      base::TimeDelta frame_interval) {
  if (base::TimeTicks::Now() - construction_time_ <= initial_blackout_period_) {
    return;
  }
  CancelScheduledTask();
  next_expected_begin_frame_time_ = frame_begin_time + frame_interval;
}

void OverviewUiTaskPool::OnCompositingStarted(ui::Compositor* compositor,
                                              base::TimeTicks start_time) {
  if (!IsCurrentCompositorFrameATaskCandidate()) {
    // In the blackout period still, or there are no tasks yet.
    return;
  }
  // Do not call `RunNextTask()` synchronously or it risks disrupting the
  // internal timing of the compositor. Make sure the tasks are run in a
  // separate call stack.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&OverviewUiTaskPool::RunNextTask,
                                weak_ptr_factory_.GetWeakPtr(),
                                /*force_task_to_run=*/false));
}

void OverviewUiTaskPool::OnCompositingShuttingDown(ui::Compositor* compositor) {
  compositor_observation_.Reset();
  StopObservingBeginFrames();
  compositor_ = nullptr;
  CancelScheduledTask();
}

void OverviewUiTaskPool::RunNextTask(bool force_task_to_run) {
  if (pending_tasks_.empty()) {
    return;
  }
  if (!force_task_to_run) {
    const base::TimeDelta time_available_for_task_to_run =
        (next_expected_begin_frame_time_ - base::TimeTicks::Now()) +
        kExpectedBeginFramePropagationDelay;
    DVLOG(4) << __func__ << " time_available_for_task_to_run="
             << time_available_for_task_to_run;
    if (time_available_for_task_to_run < kMinTimeRequiredPerTask) {
      return;
    }
  }

  auto next_task = std::move(pending_tasks_.front());
  pending_tasks_.pop_front();
  if (pending_tasks_.empty()) {
    StopObservingBeginFrames();
  }
  std::move(next_task).Run();
}

void OverviewUiTaskPool::StartObservingBeginFrames() {
  if (is_observing_begin_frames_ || HasCompositingShutDown()) {
    return;
  }
  compositor_->AddSimpleBeginFrameObserver(this);
  is_observing_begin_frames_ = true;
}

void OverviewUiTaskPool::StopObservingBeginFrames() {
  if (!is_observing_begin_frames_) {
    return;
  }
  CHECK(!HasCompositingShutDown());
  compositor_->RemoveSimpleBeginFrameObserver(this);
  is_observing_begin_frames_ = false;
}

bool OverviewUiTaskPool::HasCompositingShutDown() const {
  return !compositor_;
}

void OverviewUiTaskPool::CancelScheduledTask() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

bool OverviewUiTaskPool::IsCurrentCompositorFrameATaskCandidate() const {
  return !next_expected_begin_frame_time_.is_null();
}

}  // namespace ash
