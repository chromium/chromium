// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/thread_controller.h"

#include "base/check.h"
#include "base/trace_event/base_tracing.h"

namespace base {
namespace sequence_manager {
namespace internal {

ThreadController::RunLevelTracker::RunLevelTracker() = default;
ThreadController::RunLevelTracker::~RunLevelTracker() {
  // There shouldn't be any remaining |run_levels_| by the time this unwinds.
  DCHECK(run_levels_.empty());
}

void ThreadController::RunLevelTracker::OnRunLoopStarted(State initial_state) {
  run_levels_.emplace(initial_state);
}

void ThreadController::RunLevelTracker::OnRunLoopEnded() {
  // Normally this will occur while kIdle or kSelectingNextTask but it can also
  // occur while kRunningTask in rare situations where the owning
  // ThreadController is deleted from within a task. Ref.
  // SequenceManagerWithTaskRunnerTest.DeleteSequenceManagerInsideATask. Thus we
  // can't assert anything about the current state other than that it must be
  // exiting an existing RunLevel.
  DCHECK(!run_levels_.empty());
  run_levels_.pop();
}

void ThreadController::RunLevelTracker::OnTaskStarted() {
  // Ignore tasks outside the main run loop.
  // The only practical case where this would happen is if a native loop is spun
  // outside the main runloop (e.g. system dialog during startup). We cannot
  // support this because we are not guaranteed to be able to observe its exit
  // (like we would inside an application task which is at least guaranteed to
  // itself notify us when it ends).
  if (run_levels_.empty())
    return;

  // Already running a task?
  if (run_levels_.top().state() == kRunningTask) {
    // #task-in-task-implies-nested
    run_levels_.emplace(kRunningTask);
  } else {
    // Simply going from kIdle or kSelectingNextTask to kRunningTask.
    run_levels_.top().UpdateState(kRunningTask);
  }
}

void ThreadController::RunLevelTracker::OnTaskEnded() {
  if (run_levels_.empty())
    return;

  // #done-task-while-not-running-implies-done-nested
  if (run_levels_.top().state() != kRunningTask)
    run_levels_.pop();

  // Whether we exited a nested run-level or not: the current run-level is now
  // transitioning from kRunningTask to kSelectingNextTask.
  DCHECK_EQ(run_levels_.top().state(), kRunningTask);
  run_levels_.top().UpdateState(kSelectingNextTask);
}

void ThreadController::RunLevelTracker::OnIdle() {
  if (run_levels_.empty())
    return;

  // This is similar to the logic in OnTaskStarted().
  if (run_levels_.top().state() == kRunningTask) {
    // #task-in-task-implies-nested
    // While OnIdle() isn't typically thought of as a "task" it is a way to "do
    // work" and, on platforms like Mac which uses an |idle_work_source_|,
    // DoIdleWork() can be invoked without DoWork() being first invoked at this
    // run-level. We need to create a nested kIdle RunLevel or we break
    // #done-task-while-not-running-implies-done-nested.
    run_levels_.emplace(kIdle);
  } else {
    // Simply going kIdle at the current run-level.
    run_levels_.top().UpdateState(kIdle);
  }
}

// static
void ThreadController::RunLevelTracker::SetTraceObserverForTesting(
    TraceObserverForTesting* trace_observer_for_testing) {
  DCHECK_NE(!!trace_observer_for_testing_, !!trace_observer_for_testing);
  trace_observer_for_testing_ = trace_observer_for_testing;
}

// static
ThreadController::RunLevelTracker::TraceObserverForTesting*
    ThreadController::RunLevelTracker::trace_observer_for_testing_ = nullptr;

ThreadController::RunLevelTracker::RunLevel::RunLevel(State initial_state) {
  UpdateState(initial_state);
}

ThreadController::RunLevelTracker::RunLevel::~RunLevel() {
  UpdateState(kIdle);
}

ThreadController::RunLevelTracker::RunLevel::RunLevel(RunLevel&& other)
    : state_(std::exchange(other.state_, kIdle)) {}
ThreadController::RunLevelTracker::RunLevel&
ThreadController::RunLevelTracker::RunLevel::operator=(RunLevel&& other) {
  state_ = std::exchange(other.state_, kIdle);
  return *this;
}

void ThreadController::RunLevelTracker::RunLevel::UpdateState(State new_state) {
  // The only state that can be redeclared is idle, anything else should be a
  // transition.
  DCHECK(state_ != new_state || new_state == kIdle)
      << state_ << "," << new_state;

  const bool was_active = state_ != kIdle;
  const bool is_active = new_state != kIdle;

  state_ = new_state;
  if (was_active == is_active)
    return;

  // Change of state.
  if (is_active)
    TRACE_EVENT_BEGIN0("base", "ThreadController active");
  else
    TRACE_EVENT_END0("base", "ThreadController active");

  if (trace_observer_for_testing_) {
    if (is_active)
      trace_observer_for_testing_->OnThreadControllerActiveBegin();
    else
      trace_observer_for_testing_->OnThreadControllerActiveEnd();
  }
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
