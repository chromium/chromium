// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_TASK_SCHEDULER_TASK_TRACKER_H_
#define BASE_TASK_TASK_SCHEDULER_TASK_TRACKER_H_

#include <atomic>
#include <functional>
#include <limits>
#include <memory>
#include <queue>

#include "base/atomicops.h"
#include "base/base_export.h"
#include "base/callback_forward.h"
#include "base/debug/task_annotator.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_base.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/task_scheduler/can_schedule_sequence_observer.h"
#include "base/task/task_scheduler/scheduler_lock.h"
#include "base/task/task_scheduler/sequence.h"
#include "base/task/task_scheduler/task.h"
#include "base/task/task_scheduler/tracked_ref.h"
#include "base/task/task_traits.h"

namespace base {

class ConditionVariable;
class HistogramBase;

namespace internal {

// TaskTracker enforces policies that determines whether:
// - A task can be added to a sequence (WillPostTask).
// - A sequence can be scheduled (WillScheduleSequence).
// - The next task in a scheduled sequence can run (RunAndPopNextTask).
// TaskTracker also sets up the environment to run a task (RunAndPopNextTask)
// and records metrics and trace events. This class is thread-safe.
//
// Life of a sequence:
// (possible states: IDLE, PREEMPTED, SCHEDULED, RUNNING)
//
//                            Create a sequence
//                                   |
//  ------------------------> Sequence is IDLE
//  |                                |
//  |                     Add a task to the sequence
//  |            (allowed by TaskTracker::WillPostTask)
//  |                                |
//  |               TaskTracker:WillScheduleSequence
//  |           _____________________|_____________________
//  |           |                                          |
//  |    Returns true                                Returns false
//  |           |                                          |
//  |           |                                Sequence is PREEMPTED <----
//  |           |                                          |               |
//  |           |                            Eventually,                   |
//  |           |                            CanScheduleSequenceObserver   |
//  |           |                            is notified that the          |
//  |           |                            sequence can be scheduled.    |
//  |           |__________________________________________|               |
//  |                               |                                      |
//  |                   (*) Sequence is SCHEDULED                          |
//  |                               |                                      |
//  |                A thread is ready to run the next                     |
//  |                      task in the sequence                            |
//  |                               |                                      |
//  |                TaskTracker::RunAndPopNextTask                        |
//  |                A task from the sequence is run                       |
//  |                      Sequence is RUNNING                             |
//  |                               |                                      |
//  |         ______________________|____                                  |
//  |         |                          |                                 |
//  |   Sequence is empty      Sequence has more tasks                     |
//  |_________|             _____________|_______________                  |
//                          |                            |                 |
//                   Sequence can be            Sequence cannot be         |
//                   scheduled                  scheduled at this          |
//                          |                   moment                     |
//                   Go back to (*)                      |_________________|
//
//
// Note: A best-effort task is a task posted with TaskPriority::BEST_EFFORT. A
// foreground task is a task posted with TaskPriority::USER_VISIBLE or
// TaskPriority::USER_BLOCKING.
//
// TODO(fdoray): We want to allow disabling TaskPriority::BEST_EFFORT tasks in a
// scope (e.g. during startup or page load), but we don't need a dynamic maximum
// number of best-effort tasks. The code could probably be simplified if it
// didn't support that. https://crbug.com/831835
class BASE_EXPORT TaskTracker {
 public:
  // |histogram_label| is used as a suffix for histograms, it must not be empty.
  // The first constructor sets the maximum number of TaskPriority::BEST_EFFORT
  // sequences that can be scheduled concurrently to 0 if the
  // --disable-background-tasks flag is specified, max() otherwise. The second
  // constructor sets it to |max_num_scheduled_best_effort_sequences|.
  TaskTracker(StringPiece histogram_label);
  TaskTracker(StringPiece histogram_label,
              int max_num_scheduled_best_effort_sequences);

  virtual ~TaskTracker();

  // Synchronously shuts down the scheduler. Once this is called, only tasks
  // posted with the BLOCK_SHUTDOWN behavior will be run. Returns when:
  // - All SKIP_ON_SHUTDOWN tasks that were already running have completed their
  //   execution.
  // - All posted BLOCK_SHUTDOWN tasks have completed their execution.
  // CONTINUE_ON_SHUTDOWN tasks still may be running after Shutdown returns.
  // This can only be called once.
  void Shutdown();

  // Waits until there are no incomplete undelayed tasks. May be called in tests
  // to validate that a condition is met after all undelayed tasks have run.
  //
  // Does not wait for delayed tasks. Waits for undelayed tasks posted from
  // other threads during the call. Returns immediately when shutdown completes.
  void FlushForTesting();

  // Returns and calls |flush_callback| when there are no incomplete undelayed
  // tasks. |flush_callback| may be called back on any thread and should not
  // perform a lot of work. May be used when additional work on the current
  // thread needs to be performed during a flush. Only one
  // FlushAsyncForTesting() may be pending at any given time.
  void FlushAsyncForTesting(OnceClosure flush_callback);

  // Informs this TaskTracker that |task| from a |shutdown_behavior| sequence
  // is about to be posted. Returns true if this operation is allowed (|task|
  // should be posted if-and-only-if it is). This method may also modify
  // metadata on |task| if desired.
  bool WillPostTask(Task* task, TaskShutdownBehavior shutdown_behavior);

  // Informs this TaskTracker that |sequence| is about to be scheduled. If this
  // returns |sequence|, it is expected that RunAndPopNextTask() will soon be
  // called with |sequence| as argument. Otherwise, RunAndPopNextTask() must not
  // be called with |sequence| as argument until |observer| is notified that
  // |sequence| can be scheduled (the caller doesn't need to keep a pointer to
  // |sequence|; it will be included in the notification to |observer|).
  // WillPostTask() must have allowed the task in front of |sequence| to be
  // posted before this is called. |observer| is only required if the priority
  // of |sequence| is TaskPriority::BEST_EFFORT
  scoped_refptr<Sequence> WillScheduleSequence(
      scoped_refptr<Sequence> sequence,
      CanScheduleSequenceObserver* observer);

  // Runs the next task in |sequence| unless the current shutdown state prevents
  // that. Then, pops the task from |sequence| (even if it didn't run). Returns
  // |sequence| if it can be rescheduled immediately. If |sequence| is non-empty
  // after popping a task from it but it can't be rescheduled immediately, it
  // will be handed back to |observer| when it can be rescheduled.
  // WillPostTask() must have allowed the task in front of |sequence| to be
  // posted before this is called. Also, WillScheduleSequence(),
  // RunAndPopNextTask() or CanScheduleSequenceObserver::OnCanScheduleSequence()
  // must have allowed |sequence| to be (re)scheduled.
  scoped_refptr<Sequence> RunAndPopNextTask(
      scoped_refptr<Sequence> sequence,
      CanScheduleSequenceObserver* observer);

  // Returns true once shutdown has started (Shutdown() has been called but
  // might not have returned). Note: sequential consistency with the thread
  // calling Shutdown() (or SetHasShutdownStartedForTesting()) isn't guaranteed
  // by this call.
  bool HasShutdownStarted() const;

  // Returns true if shutdown has completed (Shutdown() has returned).
  bool IsShutdownComplete() const;

  enum class LatencyHistogramType {
    // Records the latency of each individual task posted through TaskTracker.
    TASK_LATENCY,
    // Records the latency of heartbeat tasks which are independent of current
    // workload. These avoid a bias towards TASK_LATENCY reporting that high-
    // priority tasks are "slower" than regular tasks because high-priority
    // tasks tend to be correlated with heavy workloads.
    HEARTBEAT_LATENCY,
  };

  // Causes HasShutdownStarted() to return true. Unlike when Shutdown() returns,
  // IsShutdownComplete() won't return true after this returns. Shutdown()
  // cannot be called after this.
  void SetHasShutdownStartedForTesting();

  // Records two histograms
  // 1. TaskScheduler.[label].HeartbeatLatencyMicroseconds.[suffix]:
  //    Now() - posted_time
  // 2. TaskScheduler.[label].NumTasksRunWhileQueuing.[suffix]:
  //    GetNumTasksRun() - num_tasks_run_when_posted.
  // [label] is the histogram label provided to the constructor.
  // [suffix] is derived from |task_priority| and |may_block|.
  void RecordHeartbeatLatencyAndTasksRunWhileQueuingHistograms(
      TaskPriority task_priority,
      bool may_block,
      TimeTicks posted_time,
      int num_tasks_run_when_posted) const;

  // Returns the number of tasks run so far
  int GetNumTasksRun() const;

  TrackedRef<TaskTracker> GetTrackedRef() {
    return tracked_ref_factory_.GetTrackedRef();
  }

  // Enables/disables an execution fence. When the fence is released,
  // reschedules the sequences that were preempted by the fence.
  void SetExecutionFenceEnabled(bool execution_fence_enabled);

  // Returns the number of preempted sequences of a given priority.
  int GetPreemptedSequenceCountForTesting(TaskPriority priority);

 protected:
  // Runs and deletes |task| if |can_run_task| is true. Otherwise, just deletes
  // |task|. |task| is always deleted in the environment where it runs or would
  // have run. |sequence| is the sequence from which |task| was extracted. An
  // override is expected to call its parent's implementation but is free to
  // perform extra work before and after doing so.
  virtual void RunOrSkipTask(Task task, Sequence* sequence, bool can_run_task);

  // Returns true if there are undelayed tasks that haven't completed their
  // execution (still queued or in progress). If it returns false: the side-
  // effects of all completed tasks are guaranteed to be visible to the caller.
  bool HasIncompleteUndelayedTasksForTesting() const;

 private:
  class State;
  struct PreemptedSequence;

  struct PreemptionState {
    PreemptionState();
    ~PreemptionState();

    // A priority queue of sequences that are waiting to be scheduled. Use
    // std::greater so that the sequence which contains the task that has been
    // posted the earliest is on top of the priority queue.
    std::priority_queue<PreemptedSequence,
                        std::vector<PreemptedSequence>,
                        std::greater<PreemptedSequence>>
        preempted_sequences;

    // Maximum number of sequences that can that be scheduled concurrently.
    int max_scheduled_sequences = std::numeric_limits<int>::max();

    // Caches the |max_scheduled_sequences| before enabling the execution fence.
    int max_scheduled_sequences_before_fence = 0;

    // Number of currently scheduled sequences.
    int current_scheduled_sequences = 0;

    // Synchronizes accesses to other members.
    // |max_scheduled_sequences| and |max_scheduled_sequences_before_fence| are
    // only written from the main sequence within the scope of |lock|. Reads can
    // happen on the main sequence without holding |lock|, or on any other
    // sequence while holding |lock|.
    SchedulerLock lock;

   private:
    DISALLOW_COPY_AND_ASSIGN(PreemptionState);
  };

  void PerformShutdown();

  // Sets the maximum number of sequences of priority |priority| that can be
  // scheduled concurrently to |max_scheduled_sequences|.
  void SetMaxNumScheduledSequences(int max_scheduled_sequences,
                                   TaskPriority priority);

  // Pops the next sequence in |preemption_state_[priority].preempted_sequences|
  // and increments |preemption_state_[priority].current_scheduled_sequences|.
  // Must only be called in the scope of |preemption_state_[priority].lock|,
  // with |preemption_state_[priority].preempted_sequences| non-empty. The
  // caller must forward the returned sequence to the associated
  // CanScheduleSequenceObserver as soon as |preemption_state_[priority].lock|
  // is released.
  PreemptedSequence GetPreemptedSequenceToScheduleLockRequired(
      TaskPriority priority);

  // Schedules |sequence_to_schedule.sequence| using
  // |sequence_to_schedule.observer|. Does not verify that the sequence is
  // allowed to be scheduled.
  void SchedulePreemptedSequence(PreemptedSequence sequence_to_schedule);

  // Called before WillPostTask() informs the tracing system that a task has
  // been posted. Updates |num_tasks_blocking_shutdown_| if necessary and
  // returns true if the current shutdown state allows the task to be posted.
  bool BeforePostTask(TaskShutdownBehavior effective_shutdown_behavior);

  // Called before a task with |effective_shutdown_behavior| is run by
  // RunTask(). Updates |num_tasks_blocking_shutdown_| if necessary and returns
  // true if the current shutdown state allows the task to be run.
  bool BeforeRunTask(TaskShutdownBehavior effective_shutdown_behavior);

  // Called after a task with |effective_shutdown_behavior| has been run by
  // RunTask(). Updates |num_tasks_blocking_shutdown_| and signals
  // |shutdown_cv_| if necessary.
  void AfterRunTask(TaskShutdownBehavior effective_shutdown_behavior);

  // Called when the number of tasks blocking shutdown becomes zero after
  // shutdown has started.
  void OnBlockingShutdownTasksComplete();

  // Decrements the number of incomplete undelayed tasks and signals |flush_cv_|
  // if it reaches zero.
  void DecrementNumIncompleteUndelayedTasks();

  // To be called after running a task from |just_ran_sequence|. Performs the
  // following actions:
  //  - If |just_ran_sequence| is non-null:
  //    - returns it if it should be rescheduled by the caller of
  //      RunAndPopNextTask(), i.e. its next task is set to run earlier than the
  //      earliest currently preempted sequence.
  //    - Otherwise |just_ran_sequence| is preempted and the next preempted
  //      sequence is scheduled (|observer| will be notified when
  //      |just_ran_sequence| should be scheduled again).
  //  - If |just_ran_sequence| is null (RunAndPopNextTask() just popped the last
  //    task from it):
  //    - the next preempeted sequence (if any) is scheduled.
  //  - In all cases: adjusts the number of scheduled sequences accordingly.
  scoped_refptr<Sequence> ManageSequencesAfterRunningTask(
      scoped_refptr<Sequence> just_ran_sequence,
      CanScheduleSequenceObserver* observer,
      TaskPriority task_priority);

  // Calls |flush_callback_for_testing_| if one is available in a lock-safe
  // manner.
  void CallFlushCallbackForTesting();

  // Records |Now() - posted_time| to the appropriate |latency_histogram_type|
  // based on |task_traits|.
  void RecordLatencyHistogram(LatencyHistogramType latency_histogram_type,
                              TaskTraits task_traits,
                              TimeTicks posted_time) const;

  void IncrementNumTasksRun();

  debug::TaskAnnotator task_annotator_;

  // Number of tasks blocking shutdown and boolean indicating whether shutdown
  // has started.
  const std::unique_ptr<State> state_;

  // Number of undelayed tasks that haven't completed their execution. Is
  // decremented with a memory barrier after a task runs. Is accessed with an
  // acquire memory barrier in FlushForTesting(). The memory barriers ensure
  // that the memory written by flushed tasks is visible when FlushForTesting()
  // returns.
  subtle::Atomic32 num_incomplete_undelayed_tasks_ = 0;

  // Lock associated with |flush_cv_|. Partially synchronizes access to
  // |num_incomplete_undelayed_tasks_|. Full synchronization isn't needed
  // because it's atomic, but synchronization is needed to coordinate waking and
  // sleeping at the right time. Fully synchronizes access to
  // |flush_callback_for_testing_|.
  mutable SchedulerLock flush_lock_;

  // Signaled when |num_incomplete_undelayed_tasks_| is or reaches zero or when
  // shutdown completes.
  const std::unique_ptr<ConditionVariable> flush_cv_;

  // Invoked if non-null when |num_incomplete_undelayed_tasks_| is zero or when
  // shutdown completes.
  OnceClosure flush_callback_for_testing_;

  // Synchronizes access to shutdown related members below.
  mutable SchedulerLock shutdown_lock_;

  // Event instantiated when shutdown starts and signaled when shutdown
  // completes.
  std::unique_ptr<WaitableEvent> shutdown_event_;

  // Counter for number of tasks run so far, used to record tasks run while
  // a task queued to histogram.
  std::atomic_int num_tasks_run_{0};

  // TaskScheduler.TaskLatencyMicroseconds.*,
  // TaskScheduler.HeartbeatLatencyMicroseconds.*, and
  // TaskScheduler.NumTasksRunWhileQueuing.* histograms. The first index is
  // a TaskPriority. The second index is 0 for non-blocking tasks, 1 for
  // blocking tasks. Intentionally leaked.
  // TODO(scheduler-dev): Consider using STATIC_HISTOGRAM_POINTER_GROUP for
  // these.
  static constexpr int kNumTaskPriorities =
      static_cast<int>(TaskPriority::HIGHEST) + 1;
  HistogramBase* const task_latency_histograms_[kNumTaskPriorities][2];
  HistogramBase* const heartbeat_latency_histograms_[kNumTaskPriorities][2];
  HistogramBase* const
      num_tasks_run_while_queuing_histograms_[kNumTaskPriorities][2];

  PreemptionState preemption_state_[kNumTaskPriorities];

#if DCHECK_IS_ON()
  // Indicates whether to prevent tasks running.
  bool execution_fence_enabled_ = false;
#endif

  // Number of BLOCK_SHUTDOWN tasks posted during shutdown.
  HistogramBase::Sample num_block_shutdown_tasks_posted_during_shutdown_ = 0;

  // Enforces that |max_scheduled_sequences| and
  // |max_scheduled_sequences_before_fence| in PreemptedState are only written
  // on the main sequence (determined by the first call to
  // SetMaxNumScheduledSequences or SetExecutionFenceEnabled).
  SEQUENCE_CHECKER(sequence_checker_);

  // Ensures all state (e.g. dangling cleaned up workers) is coalesced before
  // destroying the TaskTracker (e.g. in test environments).
  // Ref. https://crbug.com/827615.
  TrackedRefFactory<TaskTracker> tracked_ref_factory_;

  DISALLOW_COPY_AND_ASSIGN(TaskTracker);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_TASK_SCHEDULER_TASK_TRACKER_H_
