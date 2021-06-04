// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SEQUENCE_MANAGER_THREAD_CONTROLLER_H_
#define BASE_TASK_SEQUENCE_MANAGER_THREAD_CONTROLLER_H_

#include <stack>
#include <vector>

#include "base/base_export.h"
#include "base/message_loop/message_pump.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task/sequence_manager/lazy_now.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace base {

class MessageLoopBase;
class TickClock;
struct PendingTask;

namespace sequence_manager {
namespace internal {

class AssociatedThreadId;
class SequencedTaskSource;

// Implementation of this interface is used by SequenceManager to schedule
// actual work to be run. Hopefully we can stop using MessageLoop and this
// interface will become more concise.
class ThreadController {
 public:
  virtual ~ThreadController() = default;

  // Sets the number of tasks executed in a single invocation of DoWork.
  // Increasing the batch size can reduce the overhead of yielding back to the
  // main message loop.
  virtual void SetWorkBatchSize(int work_batch_size = 1) = 0;

  // Notifies that |pending_task| is about to be enqueued. Needed for tracing
  // purposes. The impl may use this opportunity add metadata to |pending_task|
  // before it is moved into the queue.
  virtual void WillQueueTask(PendingTask* pending_task,
                             const char* task_queue_name) = 0;

  // Notify the controller that its associated sequence has immediate work
  // to run. Shortly after this is called, the thread associated with this
  // controller will run a task returned by sequence->TakeTask(). Can be called
  // from any sequence.
  //
  // TODO(altimin): Change this to "the thread associated with this
  // controller will run tasks returned by sequence->TakeTask() until it
  // returns null or sequence->DidRunTask() returns false" once the
  // code is changed to work that way.
  virtual void ScheduleWork() = 0;

  // Notify the controller that SequencedTaskSource will have a delayed work
  // ready to be run at |run_time|. This call cancels any previously
  // scheduled delayed work. Can only be called from the main sequence.
  // NOTE: DelayTillNextTask might return a different value as it also takes
  // immediate work into account.
  // TODO(kraynov): Remove |lazy_now| parameter.
  virtual void SetNextDelayedDoWork(LazyNow* lazy_now, TimeTicks run_time) = 0;

  // Sets the sequenced task source from which to take tasks after
  // a Schedule*Work() call is made.
  // Must be called before the first call to Schedule*Work().
  virtual void SetSequencedTaskSource(SequencedTaskSource*) = 0;

  // Requests desired timer precision from the OS.
  // Has no effect on some platforms.
  virtual void SetTimerSlack(TimerSlack timer_slack) = 0;

  // Completes delayed initialization of unbound ThreadControllers.
  // BindToCurrentThread(MessageLoopBase*) or BindToCurrentThread(MessagePump*)
  // may only be called once.
  virtual void BindToCurrentThread(
      std::unique_ptr<MessagePump> message_pump) = 0;

  // Explicitly allow or disallow task execution. Implicitly disallowed when
  // entering a nested runloop.
  virtual void SetTaskExecutionAllowed(bool allowed) = 0;

  // Whether task execution is allowed or not.
  virtual bool IsTaskExecutionAllowed() const = 0;

  // Returns the MessagePump we're bound to if any.
  virtual MessagePump* GetBoundMessagePump() const = 0;

  // Returns true if the current run loop should quit when idle.
  virtual bool ShouldQuitRunLoopWhenIdle() = 0;

#if defined(OS_IOS) || defined(OS_ANDROID)
  // On iOS, the main message loop cannot be Run().  Instead call
  // AttachToMessagePump(), which connects this ThreadController to the
  // UI thread's CFRunLoop and allows PostTask() to work.
  virtual void AttachToMessagePump() = 0;
#endif

#if defined(OS_IOS)
  // Detaches this ThreadController from the message pump, allowing the
  // controller to be shut down cleanly.
  virtual void DetachFromMessagePump() = 0;
#endif

  // Currently only overridden on ThreadControllerWithMessagePumpImpl.
  //
  // While Now() is less than |prioritize_until| we will alternate between
  // |work_batch_size| tasks before setting |yield_to_native| on the
  // NextWorkInfo and yielding to the underlying sequence (e.g. the message
  // pump).
  virtual void PrioritizeYieldingToNative(base::TimeTicks prioritize_until) = 0;

  // Sets the SingleThreadTaskRunner that will be returned by
  // ThreadTaskRunnerHandle::Get on the thread controlled by this
  // ThreadController.
  virtual void SetDefaultTaskRunner(scoped_refptr<SingleThreadTaskRunner>) = 0;

  // TODO(altimin): Get rid of the methods below.
  // These methods exist due to current integration of SequenceManager
  // with MessageLoop.

  virtual bool RunsTasksInCurrentSequence() = 0;
  virtual const TickClock* GetClock() = 0;
  virtual scoped_refptr<SingleThreadTaskRunner> GetDefaultTaskRunner() = 0;
  virtual void RestoreDefaultTaskRunner() = 0;
  virtual void AddNestingObserver(RunLoop::NestingObserver* observer) = 0;
  virtual void RemoveNestingObserver(RunLoop::NestingObserver* observer) = 0;
  virtual const scoped_refptr<AssociatedThreadId>& GetAssociatedThread()
      const = 0;

 protected:
  // Tracks the state of each run-level (main and nested ones) in its associated
  // ThreadController. It does so using two high-level principles:
  //  1) #task-in-task-implies-nested :
  //     If the |state_| is kRunningTask and another task starts
  //     (OnTaskStarted()), it implies this inner-task is running from a nested
  //     loop and another RunLevel is pushed onto |run_levels_|.
  //  2) #done-task-while-not-running-implies-done-nested
  //     If the current task completes (OnTaskEnded()) and |state_| is not
  //     kRunningTask, the top RunLevel was an (already exited) nested loop and
  //     will be popped off |run_levels_|.
  // We need this logic because native nested loops can run from any task
  // without a RunLoop being involved, see
  // ThreadControllerWithMessagePumpTest.ThreadControllerActive* tests for
  // examples. Using these two heuristics is the simplest way, trying to capture
  // all the ways in which native+application tasks can nest is harder than
  // reacting as it happens.
  //
  // Note 1: "native tasks" are only captured if the MessagePump is
  // instrumented to see them and shares them with ThreadController (via
  // MessagePump::Delegate::OnBeginWorkItem). As such it is still possible to
  // view trace events emanating from native tasks without "ThreadController
  // active" being active.
  // Note 2: Non-instrumented native tasks do not break the two high-level
  // principles above because:
  //  A) If a non-instrumented task enters a nested loop, either:
  //     i) No instrumented tasks run within the loop so it's invisible.
  //     ii) Instrumented tasks run *and* current state is kRunningTask ((A) is
  //         a task within an instrumented task):
  //         #task-in-task-implies-nested triggers and the nested loop is
  //         visible.
  //     iii) Instrumented tasks run *and* current state is kIdle or
  //          kSelectingNextTask ((A) is a task run by a native loop):
  //          #task-in-task-implies-nested doesn't trigger and tasks (iii) look
  //          like a non-nested continuation of tasks at the current RunLevel.
  //  B) When task (A) exits its nested loop and completes, either:
  //     i) The loop was invisible so no RunLevel was created for it and
  //        #done-task-while-not-running-implies-done-nested doesn't trigger so
  //        it balances out.
  //     ii) Instrumented tasks did run in which case |state_| is
  //         kSelectingNextTask or kIdle. When the task in which (A) runs
  //         completes #done-task-while-not-running-implies-done-nested triggers
  //         and everything balances out.
  //     iii) Same as (ii) but we're back to kSelectingNextTask or kIdle as
  //          before and (A) was a no-op on the RunLevels.
  class BASE_EXPORT RunLevelTracker {
   public:
    enum State {
      // Waiting for work.
      kIdle,
      // Between two tasks but not idle.
      kSelectingNextTask,
      // Running and currently processing a unit of work.
      kRunningTask,
    };

    RunLevelTracker();
    ~RunLevelTracker();

    void OnRunLoopStarted(State initial_state);
    void OnRunLoopEnded();
    void OnTaskStarted();
    void OnTaskEnded();
    void OnIdle();

    size_t num_run_levels() const { return run_levels_.size(); }

    // Observers changes of state sent as trace-events so they can be tested.
    class TraceObserverForTesting {
     public:
      virtual ~TraceObserverForTesting() = default;

      virtual void OnThreadControllerActiveBegin() = 0;
      virtual void OnThreadControllerActiveEnd() = 0;
    };

    static void SetTraceObserverForTesting(
        TraceObserverForTesting* trace_observer_for_testing);

   private:
    class RunLevel {
     public:
      explicit RunLevel(State initial_state);
      ~RunLevel();

      // Moveable for STL compat. Marks |other| as idle so it noops on
      // destruction after handing off its responsibility.
      RunLevel(RunLevel&& other);
      RunLevel& operator=(RunLevel&& other);

      void UpdateState(State new_state);

      State state() const { return state_; }

     private:
      State state_ = kIdle;
    };

    std::stack<RunLevel, std::vector<RunLevel>> run_levels_;

    static TraceObserverForTesting* trace_observer_for_testing_;
  };
};

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base

#endif  // BASE_TASK_SEQUENCE_MANAGER_THREAD_CONTROLLER_H_
