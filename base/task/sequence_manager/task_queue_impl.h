// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SEQUENCE_MANAGER_TASK_QUEUE_IMPL_H_
#define BASE_TASK_SEQUENCE_MANAGER_TASK_QUEUE_IMPL_H_

#include <stddef.h>

#include <memory>
#include <queue>
#include <set>
#include <utility>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/pending_task.h"
#include "base/task/common/checked_lock.h"
#include "base/task/common/intrusive_heap.h"
#include "base/task/common/operations_controller.h"
#include "base/task/sequence_manager/associated_thread_id.h"
#include "base/task/sequence_manager/atomic_flag_set.h"
#include "base/task/sequence_manager/enqueue_order.h"
#include "base/task/sequence_manager/lazily_deallocated_deque.h"
#include "base/task/sequence_manager/sequenced_task_source.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/threading/thread_checker.h"
#include "base/time/time_override.h"
#include "base/trace_event/base_tracing_forward.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
namespace sequence_manager {

class LazyNow;
class TimeDomain;

namespace internal {

class SequenceManagerImpl;
class WorkQueue;
class WorkQueueSets;

// TaskQueueImpl has four main queues:
//
// Immediate (non-delayed) tasks:
//    |immediate_incoming_queue| - PostTask enqueues tasks here.
//    |immediate_work_queue| - SequenceManager takes immediate tasks here.
//
// Delayed tasks
//    |delayed_incoming_queue| - PostDelayedTask enqueues tasks here.
//    |delayed_work_queue| - SequenceManager takes delayed tasks here.
//
// The |immediate_incoming_queue| can be accessed from any thread, the other
// queues are main-thread only. To reduce the overhead of locking,
// |immediate_work_queue| is swapped with |immediate_incoming_queue| when
// |immediate_work_queue| becomes empty.
//
// Delayed tasks are initially posted to |delayed_incoming_queue| and a wake-up
// is scheduled with the TimeDomain.  When the delay has elapsed, the TimeDomain
// calls UpdateDelayedWorkQueue and ready delayed tasks are moved into the
// |delayed_work_queue|. Note the EnqueueOrder (used for ordering) for a delayed
// task is not set until it's moved into the |delayed_work_queue|.
//
// TaskQueueImpl uses the WorkQueueSets and the TaskQueueSelector to implement
// prioritization. Task selection is done by the TaskQueueSelector and when a
// queue is selected, it round-robins between the |immediate_work_queue| and
// |delayed_work_queue|.  The reason for this is we want to make sure delayed
// tasks (normally the most common type) don't starve out immediate work.
class BASE_EXPORT TaskQueueImpl {
 public:
  TaskQueueImpl(SequenceManagerImpl* sequence_manager,
                TimeDomain* time_domain,
                const TaskQueue::Spec& spec);

  TaskQueueImpl(const TaskQueueImpl&) = delete;
  TaskQueueImpl& operator=(const TaskQueueImpl&) = delete;
  ~TaskQueueImpl();

  // Types of queues TaskQueueImpl is maintaining internally.
  enum class WorkQueueType { kImmediate, kDelayed };

  // Some methods have fast paths when on the main thread.
  enum class CurrentThread { kMainThread, kNotMainThread };

  // Non-nestable tasks may get deferred but such queue is being maintained on
  // SequenceManager side, so we need to keep information how to requeue it.
  struct DeferredNonNestableTask {
    Task task;
    internal::TaskQueueImpl* task_queue;
    WorkQueueType work_queue_type;
  };

  using OnNextWakeUpChangedCallback = RepeatingCallback<void(TimeTicks)>;
  using OnTaskStartedHandler =
      RepeatingCallback<void(const Task&, const TaskQueue::TaskTiming&)>;
  using OnTaskCompletedHandler =
      RepeatingCallback<void(const Task&, TaskQueue::TaskTiming*, LazyNow*)>;
  using OnTaskPostedHandler = RepeatingCallback<void(const Task&)>;

  // May be called from any thread.
  scoped_refptr<SingleThreadTaskRunner> CreateTaskRunner(
      TaskType task_type) const;

  // TaskQueue implementation.
  const char* GetName() const;
  bool IsQueueEnabled() const;
  void SetQueueEnabled(bool enabled);
  void SetShouldReportPostedTasksWhenDisabled(bool should_report);
  bool IsEmpty() const;
  size_t GetNumberOfPendingTasks() const;
  bool HasTaskToRunImmediatelyOrReadyDelayedTask() const;
  absl::optional<DelayedWakeUp> GetNextDesiredWakeUp();
  void SetQueuePriority(TaskQueue::QueuePriority priority);
  TaskQueue::QueuePriority GetQueuePriority() const;
  void AddTaskObserver(TaskObserver* task_observer);
  void RemoveTaskObserver(TaskObserver* task_observer);
  void SetTimeDomain(TimeDomain* time_domain);
  TimeDomain* GetTimeDomain() const;
  void SetBlameContext(trace_event::BlameContext* blame_context);
  void InsertFence(TaskQueue::InsertFencePosition position);
  void InsertFenceAt(TimeTicks time);
  void RemoveFence();
  bool HasActiveFence();
  bool BlockedByFence() const;
  void SetThrottler(TaskQueue::Throttler* throttler);
  void ResetThrottler();

  void UnregisterTaskQueue();

  // Returns true if a (potentially hypothetical) task with the specified
  // |enqueue_order| could run on the queue. Must be called from the main
  // thread.
  bool CouldTaskRun(EnqueueOrder enqueue_order) const;

  // Returns true if a task with |enqueue_order| obtained from this queue was
  // ever in the queue while it was disabled, blocked by a fence, or less
  // important than kNormalPriority.
  bool WasBlockedOrLowPriority(EnqueueOrder enqueue_order) const;

  // Must only be called from the thread this task queue was created on.
  void ReloadEmptyImmediateWorkQueue();

  Value AsValue(TimeTicks now, bool force_verbose) const;

  bool GetQuiescenceMonitored() const { return should_monitor_quiescence_; }
  bool GetShouldNotifyObservers() const { return should_notify_observers_; }

  void NotifyWillProcessTask(const Task& task,
                             bool was_blocked_or_low_priority);
  void NotifyDidProcessTask(const Task& task);

  // Returns true iff this queue has work that can execute now, i.e. immediate
  // tasks or delayed tasks that have been transferred to the work queue by
  // MoveReadyDelayedTasksToWorkQueue(). Delayed tasks that are still in the
  // incoming queue are not taken into account. Ignores the queue's enabled
  // state and fences.
  bool HasTaskToRunImmediately() const;
  bool HasTaskToRunImmediatelyLocked() const
      EXCLUSIVE_LOCKS_REQUIRED(any_thread_lock_);

  bool has_pending_high_resolution_tasks() const {
    return main_thread_only()
        .delayed_incoming_queue.has_pending_high_resolution_tasks();
  }

  WorkQueue* delayed_work_queue() {
    return main_thread_only().delayed_work_queue.get();
  }

  const WorkQueue* delayed_work_queue() const {
    return main_thread_only().delayed_work_queue.get();
  }

  WorkQueue* immediate_work_queue() {
    return main_thread_only().immediate_work_queue.get();
  }

  const WorkQueue* immediate_work_queue() const {
    return main_thread_only().immediate_work_queue.get();
  }

  // Enqueues any delayed tasks which should be run now on the
  // |delayed_work_queue|. Must be called from the main thread.
  void MoveReadyDelayedTasksToWorkQueue(LazyNow* lazy_now);

  void OnWakeUp(LazyNow* lazy_now);

  base::internal::HeapHandle heap_handle() const {
    return main_thread_only().heap_handle;
  }

  void set_heap_handle(base::internal::HeapHandle heap_handle) {
    main_thread_only().heap_handle = heap_handle;
  }

  // Pushes |task| onto the front of the specified work queue. Caution must be
  // taken with this API because you could easily starve out other work.
  // TODO(kraynov): Simplify non-nestable task logic https://crbug.com/845437.
  void RequeueDeferredNonNestableTask(DeferredNonNestableTask task);

  void PushImmediateIncomingTaskForTest(Task&& task);

  // Iterates over |delayed_incoming_queue| removing canceled tasks. In
  // addition MaybeShrinkQueue is called on all internal queues.
  void ReclaimMemory(TimeTicks now);

  // Allows wrapping TaskQueue to set a handler to subscribe for notifications
  // about started and completed tasks.
  void SetOnTaskStartedHandler(OnTaskStartedHandler handler);
  void OnTaskStarted(const Task& task,
                     const TaskQueue::TaskTiming& task_timing);

  // |task_timing| may be passed in Running state and may not have the end time,
  // so that the handler can run an additional task that is counted as a part of
  // the main task.
  // The handler can call TaskTiming::RecordTaskEnd, which is optional, to
  // finalize the task, and use the resulting timing.
  void SetOnTaskCompletedHandler(OnTaskCompletedHandler handler);
  void OnTaskCompleted(const Task& task,
                       TaskQueue::TaskTiming* task_timing,
                       LazyNow* lazy_now);
  bool RequiresTaskTiming() const;

  // Set a callback for adding custom functionality for processing posted task.
  // Callback will be dispatched while holding a scheduler lock. As a result,
  // callback should not call scheduler APIs directly, as this can lead to
  // deadlocks. For example, PostTask should not be called directly and
  // ScopedDeferTaskPosting::PostOrDefer should be used instead.
  void SetOnTaskPostedHandler(OnTaskPostedHandler handler);

  WeakPtr<SequenceManagerImpl> GetSequenceManagerWeakPtr();

  SequenceManagerImpl* sequence_manager() const { return sequence_manager_; }

  // Returns true if this queue is unregistered or task queue manager is deleted
  // and this queue can be safely deleted on any thread.
  bool IsUnregistered() const;

  // Updates this queue's next wake up time in the time domain,
  // taking into account the desired run time of queued tasks and
  // policies enforced by the Throttler.
  void UpdateDelayedWakeUp(LazyNow* lazy_now);

 protected:
  // Sets this queue's next wake up time to |wake_up| in the time domain.
  void SetNextDelayedWakeUp(LazyNow* lazy_now,
                            absl::optional<DelayedWakeUp> wake_up);

 private:
  friend class WorkQueue;
  friend class WorkQueueTest;

  // A TaskQueueImpl instance can be destroyed or unregistered before all its
  // associated TaskRunner instances are (they are refcounted). Thus we need a
  // way to prevent TaskRunner instances from posting further tasks. This class
  // guards PostTask calls using an OperationsController.
  // This class is ref-counted as both the TaskQueueImpl instance and all
  // associated TaskRunner instances share the same GuardedTaskPoster instance.
  // When TaskQueueImpl shuts down it calls ShutdownAndWaitForZeroOperations(),
  // preventing further PostTask calls being made to the underlying
  // TaskQueueImpl.
  class GuardedTaskPoster : public RefCountedThreadSafe<GuardedTaskPoster> {
   public:
    explicit GuardedTaskPoster(TaskQueueImpl* outer);

    bool PostTask(PostedTask task);

    void StartAcceptingOperations() {
      operations_controller_.StartAcceptingOperations();
    }

    void ShutdownAndWaitForZeroOperations() {
      operations_controller_.ShutdownAndWaitForZeroOperations();
    }

   private:
    friend class RefCountedThreadSafe<GuardedTaskPoster>;

    ~GuardedTaskPoster();

    base::internal::OperationsController operations_controller_;
    // Pointer might be stale, access guarded by |operations_controller_|
    TaskQueueImpl* const outer_;
  };

  class TaskRunner final : public SingleThreadTaskRunner {
   public:
    explicit TaskRunner(scoped_refptr<GuardedTaskPoster> task_poster,
                        scoped_refptr<AssociatedThreadId> associated_thread,
                        TaskType task_type);

    bool PostDelayedTask(const Location& location,
                         OnceClosure callback,
                         TimeDelta delay) final;
    bool PostNonNestableDelayedTask(const Location& location,
                                    OnceClosure callback,
                                    TimeDelta delay) final;
    bool RunsTasksInCurrentSequence() const final;

   private:
    ~TaskRunner() final;

    const scoped_refptr<GuardedTaskPoster> task_poster_;
    const scoped_refptr<AssociatedThreadId> associated_thread_;
    const TaskType task_type_;
  };

  // A queue for holding delayed tasks before their delay has expired.
  struct DelayedIncomingQueue {
   public:
    DelayedIncomingQueue();
    DelayedIncomingQueue(const DelayedIncomingQueue&) = delete;
    DelayedIncomingQueue& operator=(const DelayedIncomingQueue&) = delete;
    ~DelayedIncomingQueue();

    void push(Task&& task);
    void pop();
    bool empty() const { return queue_.empty(); }
    size_t size() const { return queue_.size(); }
    const Task& top() const { return queue_.top(); }
    void swap(DelayedIncomingQueue* other);

    bool has_pending_high_resolution_tasks() const {
      return pending_high_res_tasks_;
    }

    // TODO(crbug.com/1155905): we pass SequenceManager to be able to record
    // crash keys. Remove this parameter after chasing down this crash.
    void SweepCancelledTasks(SequenceManagerImpl* sequence_manager);
    std::priority_queue<Task> TakeTasks() { return std::move(queue_); }
    Value AsValue(TimeTicks now) const;

   private:
    struct PQueue : public std::priority_queue<Task> {
      // Removes all cancelled tasks from the queue. Returns the number of
      // removed high resolution tasks (which could be lower than the total
      // number of removed tasks).
      //
      // TODO(crbug.com/1155905): we pass SequenceManager to be able to record
      // crash keys. Remove this parameter after chasing down this crash.
      size_t SweepCancelledTasks(SequenceManagerImpl* sequence_manager);
      Value AsValue(TimeTicks now) const;
    };

    PQueue queue_;

    // Number of pending tasks in the queue that need high resolution timing.
    int pending_high_res_tasks_ = 0;
  };

  struct MainThreadOnly {
    MainThreadOnly(TaskQueueImpl* task_queue, TimeDomain* time_domain);
    ~MainThreadOnly();

    // Another copy of TimeDomain for lock-free access from the main thread.
    // See description inside struct AnyThread for details.
    TimeDomain* time_domain;

    TaskQueue::Throttler* throttler = nullptr;

    std::unique_ptr<WorkQueue> delayed_work_queue;
    std::unique_ptr<WorkQueue> immediate_work_queue;
    DelayedIncomingQueue delayed_incoming_queue;
    ObserverList<TaskObserver>::Unchecked task_observers;
    base::internal::HeapHandle heap_handle;
    bool is_enabled = true;
    trace_event::BlameContext* blame_context = nullptr;  // Not owned.
    EnqueueOrder current_fence;
    absl::optional<TimeTicks> delayed_fence;
    // Snapshots the next sequence number when the queue is unblocked, otherwise
    // it contains EnqueueOrder::none(). If the EnqueueOrder of a task just
    // popped from this queue is greater than this, it means that the queue was
    // never disabled or blocked by a fence while the task was queued.
    EnqueueOrder enqueue_order_at_which_we_became_unblocked;
    // If the EnqueueOrder of a task just popped from this queue is greater than
    // this, it means that the queue was never disabled, blocked by a fence or
    // less important than kNormalPriority while the task was queued.
    //
    // Implementation details:
    // 1) When the queue is made less important than kNormalPriority, this is
    //    set to EnqueueOrder::max(). The EnqueueOrder of any task will compare
    //    less than this.
    // 2) When the queue is made at least as important as kNormalPriority, this
    //    snapshots the next sequence number. If the queue is blocked, the value
    //    is irrelevant because no task should be popped. If the queue is not
    //    blocked, the EnqueueOrder of any already queued task will compare less
    //    than this.
    // 3) When the queue is unblocked while at least as important as
    //    kNormalPriority, this snapshots the next sequence number. The
    //    EnqueueOrder of any already queued task will compare less than this.
    EnqueueOrder
        enqueue_order_at_which_we_became_unblocked_with_normal_priority;
    OnTaskStartedHandler on_task_started_handler;
    OnTaskCompletedHandler on_task_completed_handler;
    // Last reported wake up, used only in UpdateWakeUp to avoid
    // excessive calls.
    absl::optional<DelayedWakeUp> scheduled_wake_up;
    // If false, queue will be disabled. Used only for tests.
    bool is_enabled_for_test = true;
    // The time at which the task queue was disabled, if it is currently
    // disabled.
    absl::optional<TimeTicks> disabled_time;
    // Whether or not the task queue should emit tracing events for tasks
    // posted to this queue when it is disabled.
    bool should_report_posted_tasks_when_disabled = false;
  };

  void PostTask(PostedTask task);

  void PostImmediateTaskImpl(PostedTask task, CurrentThread current_thread);
  void PostDelayedTaskImpl(PostedTask task, CurrentThread current_thread);

  // Push the task onto the |delayed_incoming_queue|. Lock-free main thread
  // only fast path.
  void PushOntoDelayedIncomingQueueFromMainThread(Task pending_task,
                                                  TimeTicks now,
                                                  bool notify_task_annotator);

  // Push the task onto the |delayed_incoming_queue|.  Slow path from other
  // threads.
  void PushOntoDelayedIncomingQueue(Task pending_task);

  void ScheduleDelayedWorkTask(Task pending_task);

  void MoveReadyImmediateTasksToImmediateWorkQueueLocked()
      EXCLUSIVE_LOCKS_REQUIRED(any_thread_lock_);

  // LazilyDeallocatedDeque use TimeTicks to figure out when to resize.  We
  // should use real time here always.
  using TaskDeque =
      LazilyDeallocatedDeque<Task, subtle::TimeTicksNowIgnoringOverride>;

  // Extracts all the tasks from the immediate incoming queue and swaps it with
  // |queue| which must be empty.
  // Can be called from any thread.
  void TakeImmediateIncomingQueueTasks(TaskDeque* queue);

  void TraceQueueSize() const;
  static Value QueueAsValue(const TaskDeque& queue, TimeTicks now);
  static Value TaskAsValue(const Task& task, TimeTicks now);

  // Activate a delayed fence if a time has come.
  void ActivateDelayedFenceIfNeeded(TimeTicks now);

  // Updates state protected by any_thread_lock_.
  void UpdateCrossThreadQueueStateLocked()
      EXCLUSIVE_LOCKS_REQUIRED(any_thread_lock_);

  void MaybeLogPostTask(PostedTask* task);
  void MaybeAdjustTaskDelay(PostedTask* task, CurrentThread current_thread);

  // Reports the task if it was due to IPC and was posted to a disabled queue.
  // This should be called after WillQueueTask has been called for the task.
  void MaybeReportIpcTaskQueuedFromMainThread(Task* pending_task,
                                              const char* task_queue_name);
  bool ShouldReportIpcTaskQueuedFromAnyThreadLocked(
      base::TimeDelta* time_since_disabled)
      EXCLUSIVE_LOCKS_REQUIRED(any_thread_lock_);
  void MaybeReportIpcTaskQueuedFromAnyThreadLocked(Task* pending_task,
                                                   const char* task_queue_name)
      EXCLUSIVE_LOCKS_REQUIRED(any_thread_lock_);
  void MaybeReportIpcTaskQueuedFromAnyThreadUnlocked(
      Task* pending_task,
      const char* task_queue_name);
  void ReportIpcTaskQueued(Task* pending_task,
                           const char* task_queue_name,
                           const base::TimeDelta& time_since_disabled);

  // Invoked when the queue becomes enabled and not blocked by a fence.
  void OnQueueUnblocked();

  const char* name_;
  SequenceManagerImpl* const sequence_manager_;

  scoped_refptr<AssociatedThreadId> associated_thread_;

  const scoped_refptr<GuardedTaskPoster> task_poster_;

  mutable base::internal::CheckedLock any_thread_lock_;

  struct AnyThread {
    // Mirrored from MainThreadOnly. These are only used for tracing.
    struct TracingOnly {
      TracingOnly();
      ~TracingOnly();

      bool is_enabled = true;
      absl::optional<TimeTicks> disabled_time;
      bool should_report_posted_tasks_when_disabled = false;
    };

    explicit AnyThread(TimeDomain* time_domain);
    ~AnyThread();

    // TimeDomain is maintained in two copies: inside AnyThread and inside
    // MainThreadOnly. It can be changed only from main thread, so it should be
    // locked before accessing from other threads.
    TimeDomain* time_domain;

    TaskDeque immediate_incoming_queue;

    // True if main_thread_only().immediate_work_queue is empty.
    bool immediate_work_queue_empty = true;

    bool post_immediate_task_should_schedule_work = true;

    bool unregistered = false;

    OnTaskPostedHandler on_task_posted_handler;

#if DCHECK_IS_ON()
    // A cache of |immediate_work_queue->work_queue_set_index()| which is used
    // to index into
    // SequenceManager::Settings::per_priority_cross_thread_task_delay to apply
    // a priority specific delay for debugging purposes.
    int queue_set_index = 0;
#endif

    TracingOnly tracing_only;
  };

  AnyThread any_thread_ GUARDED_BY(any_thread_lock_);

  MainThreadOnly main_thread_only_;
  MainThreadOnly& main_thread_only() {
    DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
    return main_thread_only_;
  }
  const MainThreadOnly& main_thread_only() const {
    DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
    return main_thread_only_;
  }

  // Handle to our entry within the SequenceManagers |empty_queues_to_reload_|
  // atomic flag set. Used to signal that this queue needs to be reloaded.
  // If you call SetActive(false) you should do so inside |any_thread_lock_|
  // because there is a danger a cross thread PostTask might reset it before we
  // make |immediate_work_queue| non-empty.
  AtomicFlagSet::AtomicFlag empty_queues_to_reload_handle_;

  const bool should_monitor_quiescence_;
  const bool should_notify_observers_;
  const bool delayed_fence_allowed_;
};

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base

#endif  // BASE_TASK_SEQUENCE_MANAGER_TASK_QUEUE_IMPL_H_
