// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SEQUENCE_MANAGER_SEQUENCE_MANAGER_IMPL_H_
#define BASE_TASK_SEQUENCE_MANAGER_SEQUENCE_MANAGER_IMPL_H_

#include <list>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/cancelable_callback.h"
#include "base/containers/circular_deque.h"
#include "base/debug/task_annotator.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/pending_task.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/task/sequence_manager/associated_thread_id.h"
#include "base/task/sequence_manager/enqueue_order.h"
#include "base/task/sequence_manager/moveable_auto_lock.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/task_queue_impl.h"
#include "base/task/sequence_manager/task_queue_selector.h"
#include "base/task/sequence_manager/thread_controller.h"
#include "base/threading/thread_checker.h"

namespace base {

namespace debug {
struct CrashKeyString;
}  // namespace debug

namespace trace_event {
class ConvertableToTraceFormat;
}  // namespace trace_event

namespace sequence_manager {

class SequenceManagerForTest;
class TaskQueue;
class TaskTimeObserver;
class TimeDomain;

namespace internal {

class RealTimeDomain;
class TaskQueueImpl;

// The task queue manager provides N task queues and a selector interface for
// choosing which task queue to service next. Each task queue consists of two
// sub queues:
//
// 1. Incoming task queue. Tasks that are posted get immediately appended here.
//    When a task is appended into an empty incoming queue, the task manager
//    work function (DoWork()) is scheduled to run on the main task runner.
//
// 2. Work queue. If a work queue is empty when DoWork() is entered, tasks from
//    the incoming task queue (if any) are moved here. The work queues are
//    registered with the selector as input to the scheduling decision.
//
class BASE_EXPORT SequenceManagerImpl
    : public SequenceManager,
      public internal::SequencedTaskSource,
      public internal::TaskQueueSelector::Observer,
      public RunLoop::NestingObserver {
 public:
  using Observer = SequenceManager::Observer;

  ~SequenceManagerImpl() override;

  // Assume direct control over current thread and create a SequenceManager.
  // This function should be called only once per thread.
  // This function assumes that a MessageLoop is initialized for
  // the current thread.
  static std::unique_ptr<SequenceManagerImpl> CreateOnCurrentThread();

  // Create a SequenceManager for a future thread that will run the provided
  // MessageLoop. The SequenceManager can be initialized on the current thread
  // and then needs to be bound and initialized on the target thread by calling
  // BindToCurrentThread() and CompleteInitializationOnBoundThread() during the
  // thread's startup. If |message_loop| is null then BindToMessageLoop() must
  // be called instead of CompleteInitializationOnBoundThread.
  //
  // This function should be called only once per MessageLoop.
  static std::unique_ptr<SequenceManagerImpl> CreateUnbound(
      MessageLoop* message_loop);

  // SequenceManager implementation:
  void BindToCurrentThread() override;
  void BindToMessageLoop(MessageLoop* message_loop) override;
  void CompleteInitializationOnBoundThread() override;
  void SetObserver(Observer* observer) override;
  void AddTaskObserver(MessageLoop::TaskObserver* task_observer) override;
  void RemoveTaskObserver(MessageLoop::TaskObserver* task_observer) override;
  void AddTaskTimeObserver(TaskTimeObserver* task_time_observer) override;
  void RemoveTaskTimeObserver(TaskTimeObserver* task_time_observer) override;
  void RegisterTimeDomain(TimeDomain* time_domain) override;
  void UnregisterTimeDomain(TimeDomain* time_domain) override;
  TimeDomain* GetRealTimeDomain() const override;
  const TickClock* GetTickClock() const override;
  TimeTicks NowTicks() const override;
  void SetDefaultTaskRunner(
      scoped_refptr<SingleThreadTaskRunner> task_runner) override;
  void SweepCanceledDelayedTasks() override;
  bool GetAndClearSystemIsQuiescentBit() override;
  void SetWorkBatchSize(int work_batch_size) override;
  void SetTimerSlack(TimerSlack timer_slack) override;
  void EnableCrashKeys(const char* file_name_crash_key,
                       const char* function_name_crash_key) override;
  const MetricRecordingSettings& GetMetricRecordingSettings() const override;

  // Implementation of SequencedTaskSource:
  Optional<PendingTask> TakeTask() override;
  void DidRunTask() override;
  TimeDelta DelayTillNextTask(LazyNow* lazy_now) override;
  bool HasPendingHighResolutionTasks() override;

  // Requests that a task to process work is posted on the main task runner.
  // These tasks are de-duplicated in two buckets: main-thread and all other
  // threads. This distinction is done to reduce the overhead from locks, we
  // assume the main-thread path will be hot.
  void MaybeScheduleImmediateWork(const Location& from_here);

  // Requests that a delayed task to process work is posted on the main task
  // runner. These delayed tasks are de-duplicated. Must be called on the thread
  // this class was created on.

  // Schedules next wake-up at the given time, cancels any previous requests.
  // Use TimeTicks::Max() to cancel a wake-up.
  // Must be called from a TimeDomain only.
  void SetNextDelayedDoWork(LazyNow* lazy_now, TimeTicks run_time);

  // Returns the currently executing TaskQueue if any. Must be called on the
  // thread this class was created on.
  internal::TaskQueueImpl* currently_executing_task_queue() const;

  // Unregisters a TaskQueue previously created by |NewTaskQueue()|.
  // No tasks will run on this queue after this call.
  void UnregisterTaskQueueImpl(
      std::unique_ptr<internal::TaskQueueImpl> task_queue);

  // Schedule a call to UnregisterTaskQueueImpl as soon as it's safe to do so.
  void ShutdownTaskQueueGracefully(
      std::unique_ptr<internal::TaskQueueImpl> task_queue);

  const scoped_refptr<AssociatedThreadId>& associated_thread() const {
    return associated_thread_;
  }

  WeakPtr<SequenceManagerImpl> GetWeakPtr();

 protected:
  // Create a task queue manager where |controller| controls the thread
  // on which the tasks are eventually run.
  explicit SequenceManagerImpl(
      std::unique_ptr<internal::ThreadController> controller);

  friend class internal::TaskQueueImpl;
  friend class ::base::sequence_manager::SequenceManagerForTest;

 private:
  enum class ProcessTaskResult {
    kDeferred,
    kExecuted,
    kSequenceManagerDeleted,
  };

  struct AnyThread {
    AnyThread();
    ~AnyThread();

    // Task queues with newly available work on the incoming queue.
    internal::IncomingImmediateWorkList* incoming_immediate_work_list = nullptr;
  };

  // SequenceManager maintains a queue of non-nestable tasks since they're
  // uncommon and allocating an extra deque per TaskQueue will waste the memory.
  using NonNestableTaskDeque =
      circular_deque<internal::TaskQueueImpl::DeferredNonNestableTask>;

  // We have to track rentrancy because we support nested runloops but the
  // selector interface is unaware of those.  This struct keeps track off all
  // task related state needed to make pairs of TakeTask() / DidRunTask() work.
  struct ExecutingTask {
    ExecutingTask(Task&& task,
                  internal::TaskQueueImpl* task_queue,
                  TaskQueue::TaskTiming task_timing)
        : pending_task(std::move(task)),
          task_queue(task_queue),
          task_timing(task_timing),
          task_type(pending_task.task_type) {}

    Task pending_task;
    internal::TaskQueueImpl* task_queue = nullptr;
    TaskQueue::TaskTiming task_timing;
    // Save task metadata to use in after running a task as |pending_task|
    // won't be available then.
    int task_type;
  };

  struct MainThreadOnly {
    explicit MainThreadOnly(
        const scoped_refptr<AssociatedThreadId>& associated_thread);
    ~MainThreadOnly();

    int nesting_depth = 0;
    NonNestableTaskDeque non_nestable_task_queue;
    // TODO(altimin): Switch to instruction pointer crash key when it's
    // available.
    debug::CrashKeyString* file_name_crash_key = nullptr;
    debug::CrashKeyString* function_name_crash_key = nullptr;

    std::mt19937_64 random_generator;
    std::uniform_real_distribution<double> uniform_distribution;

    internal::TaskQueueSelector selector;
    ObserverList<MessageLoop::TaskObserver>::Unchecked task_observers;
    ObserverList<TaskTimeObserver>::Unchecked task_time_observers;
    std::set<TimeDomain*> time_domains;
    std::unique_ptr<internal::RealTimeDomain> real_time_domain;

    // List of task queues managed by this SequenceManager.
    // - active_queues contains queues that are still running tasks.
    //   Most often they are owned by relevant TaskQueues, but
    //   queues_to_gracefully_shutdown_ are included here too.
    // - queues_to_gracefully_shutdown contains queues which should be deleted
    //   when they become empty.
    // - queues_to_delete contains soon-to-be-deleted queues, because some
    //   internal scheduling code does not expect queues to be pulled
    //   from underneath.

    std::set<internal::TaskQueueImpl*> active_queues;
    std::map<internal::TaskQueueImpl*, std::unique_ptr<internal::TaskQueueImpl>>
        queues_to_gracefully_shutdown;
    std::map<internal::TaskQueueImpl*, std::unique_ptr<internal::TaskQueueImpl>>
        queues_to_delete;

    // Scratch space used to store the contents of
    // any_thread().incoming_immediate_work_list for use by
    // ReloadEmptyWorkQueues.  We keep hold of this vector to avoid unnecessary
    // memory allocations.
    std::vector<internal::TaskQueueImpl*> queues_to_reload;

    bool task_was_run_on_quiescence_monitored_queue = false;
    bool nesting_observer_registered_ = false;

    // Due to nested runloops more than one task can be executing concurrently.
    std::list<ExecutingTask> task_execution_stack;

    Observer* observer = nullptr;  // NOT OWNED
  };

  // TaskQueueSelector::Observer:
  void OnTaskQueueEnabled(internal::TaskQueueImpl* queue) override;

  // RunLoop::NestingObserver:
  void OnBeginNestedRunLoop() override;
  void OnExitNestedRunLoop() override;

  // Called by the task queue to inform this SequenceManager of a task that's
  // about to be queued. This SequenceManager may use this opportunity to add
  // metadata to |pending_task| before it is moved into the queue.
  void WillQueueTask(Task* pending_task);

  // Delayed Tasks with run_times <= Now() are enqueued onto the work queue and
  // reloads any empty work queues.
  void WakeUpReadyDelayedQueues(LazyNow* lazy_now);

  void NotifyWillProcessTask(ExecutingTask* task, LazyNow* time_before_task);
  void NotifyDidProcessTask(ExecutingTask* task, LazyNow* time_after_task);

  internal::EnqueueOrder GetNextSequenceNumber();

  std::unique_ptr<trace_event::ConvertableToTraceFormat>
  AsValueWithSelectorResult(bool should_run,
                            internal::WorkQueue* selected_work_queue) const;

  // Adds |queue| to |any_thread().has_incoming_immediate_work_| and if
  // |queue_is_blocked| is false it makes sure a DoWork is posted.
  // Can be called from any thread.
  void OnQueueHasIncomingImmediateWork(internal::TaskQueueImpl* queue,
                                       internal::EnqueueOrder enqueue_order,
                                       bool queue_is_blocked);

  // Returns true if |task_queue| was added to the list, or false if it was
  // already in the list.  If |task_queue| was inserted, the |order| is set
  // with |enqueue_order|.
  bool AddToIncomingImmediateWorkList(internal::TaskQueueImpl* task_queue,
                                      internal::EnqueueOrder enqueue_order);
  void RemoveFromIncomingImmediateWorkList(internal::TaskQueueImpl* task_queue);

  // Calls |ReloadImmediateWorkQueueIfEmpty| on all queues in
  // |main_thread_only().queues_to_reload|.
  void ReloadEmptyWorkQueues();

  std::unique_ptr<internal::TaskQueueImpl> CreateTaskQueueImpl(
      const TaskQueue::Spec& spec) override;

  // Deletes queues marked for deletion and empty queues marked for shutdown.
  void CleanUpQueues();

  bool ShouldRecordTaskTiming(const internal::TaskQueueImpl* task_queue);
  bool ShouldRecordCPUTimeForTask();

  // Helper to terminate all scoped trace events to allow starting new ones
  // in TakeTask().
  Optional<PendingTask> TakeTaskImpl();

  // Determines if wall time or thread time should be recorded for the next
  // task.
  TaskQueue::TaskTiming InitializeTaskTiming(
      internal::TaskQueueImpl* task_queue);

  scoped_refptr<AssociatedThreadId> associated_thread_;

  internal::EnqueueOrder::Generator enqueue_order_generator_;

  std::unique_ptr<internal::ThreadController> controller_;

  mutable Lock any_thread_lock_;
  AnyThread any_thread_;

  struct AnyThread& any_thread() {
    any_thread_lock_.AssertAcquired();
    return any_thread_;
  }
  const struct AnyThread& any_thread() const {
    any_thread_lock_.AssertAcquired();
    return any_thread_;
  }

  const MetricRecordingSettings metric_recording_settings_;

  // A check to bail out early during memory corruption.
  // https://crbug.com/757940
  bool Validate();

  int32_t memory_corruption_sentinel_;

  MainThreadOnly main_thread_only_;
  MainThreadOnly& main_thread_only() {
    DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
    return main_thread_only_;
  }
  const MainThreadOnly& main_thread_only() const {
    DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
    return main_thread_only_;
  }

  WeakPtrFactory<SequenceManagerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SequenceManagerImpl);
};

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base

#endif  // BASE_TASK_SEQUENCE_MANAGER_SEQUENCE_MANAGER_IMPL_H_
