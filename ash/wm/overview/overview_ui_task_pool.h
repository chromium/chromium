// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_UI_TASK_POOL_H_
#define ASH_WM_OVERVIEW_OVERVIEW_UI_TASK_POOL_H_

#include <deque>

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/host_begin_frame_observer.h"

namespace ui {
class Compositor;
}  // namespace ui

namespace ash {

// Runs `base::OnceClosure`s (tasks) provided by the caller on the UI thread at
// times that are least likely to disrupt an ongoing animation. The tasks are
// lower priority than the animation itself that's running in the foreground,
// but should be completed in the immediate future (typically before the
// animation ends). They also are not candidates for the `base::`ThreadPool`
// due to threading restrictions.
class ASH_EXPORT OverviewUiTaskPool
    : public ui::HostBeginFrameObserver::SimpleBeginFrameObserver,
      public ui::CompositorObserver {
 public:
  // `initial_blackout_period` is a duration during which no tasks will be
  // run. It starts from the time the task pool is constructed. It exists
  // because in practice, animations tend to have the heaviest load on the CPU
  // and the UI thread when they start. For lower priority tasks in the pool,
  // it's better to wait until mid-animation when the compositor has reached a
  // more regular frame cadence before they're run.
  OverviewUiTaskPool(ui::Compositor* compositor,
                     base::TimeDelta initial_blackout_period);
  OverviewUiTaskPool(const OverviewUiTaskPool&) = delete;
  OverviewUiTaskPool& operator=(const OverviewUiTaskPool&) = delete;
  // Any remaining tasks in the pool get dropped on destruction. Use `Flush()`
  // if required.
  ~OverviewUiTaskPool() override;

  // Adds a `task` to the pool that will be run as soon as a good time is found.
  // Tasks are always run on the UI thread, and there is no guaranteed order in
  // which they run. It is generally recommended that each individual `task`
  // takes no more than 4 milliseconds. It's also better to add multiple small
  // tasks than one large one.
  void AddTask(base::OnceClosure task);

  // Synchronously runs all pending tasks in the pool. `AddTask()` may still
  // be called afterwards.
  void Flush();

 private:
  // ui::HostBeginFrameObserver::SimpleBeginFrameObserver:
  void OnBeginFrame(base::TimeTicks frame_begin_time,
                    base::TimeDelta frame_interval) override;
  void OnBeginFrameSourceShuttingDown() override {}

  // ui::CompositorObserver:
  void OnCompositingStarted(ui::Compositor* compositor,
                            base::TimeTicks start_time) override;
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

  void RunNextTask(bool force_task_to_run);
  void StartObservingBeginFrames();
  void StopObservingBeginFrames();
  bool HasCompositingShutDown() const;
  void CancelScheduledTask();
  // Whether the currently active compositor frame is a candidate for running
  // tasks from the pool.
  bool IsCurrentCompositorFrameATaskCandidate() const;

  raw_ptr<ui::Compositor> compositor_ = nullptr;
  const base::TimeDelta initial_blackout_period_;
  std::deque<base::OnceClosure> pending_tasks_;
  // A "begin frame" a message signals the start of a new animation frame
  // (typically spaced 1/60th of a second apart). This holds the time at which
  // the next animation frame is expected to start.
  base::TimeTicks next_expected_begin_frame_time_;
  // Whether the begin frames are being observed from the gpu process. This is
  // true if there are any pending tasks in the pool; false if there are none or
  // if compositing has shut down.
  bool is_observing_begin_frames_ = false;
  // Time at which this class was constructed. Used to implement the blackout.
  const base::TimeTicks construction_time_;
  base::ScopedObservation<ui::Compositor, ui::CompositorObserver>
      compositor_observation_{this};
  base::WeakPtrFactory<OverviewUiTaskPool> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_UI_TASK_POOL_H_
