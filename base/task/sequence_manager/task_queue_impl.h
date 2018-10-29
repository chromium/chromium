// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SEQUENCE_MANAGER_TASK_QUEUE_IMPL_H_
#define BASE_TASK_SEQUENCE_MANAGER_TASK_QUEUE_IMPL_H_

#include <stddef.h>

#include <memory>
#include <queue>
#include <set>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/pending_task.h"
#include "base/task/common/intrusive_heap.h"
#include "base/task/sequence_manager/associated_thread_id.h"
#include "base/task/sequence_manager/enqueue_order.h"
#include "base/task/sequence_manager/lazily_deallocated_deque.h"
#include "base/task/sequence_manager/sequenced_task_source.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"

namespace base {
namespace sequence_manager {

class LazyNow;
class TimeDomain;

namespace internal {

class SequenceManagerImpl;
class TaskQueueProxy;
class WorkQueue;
class WorkQueueSets;

struct IncomingImmediateWorkList {
  IncomingImmediateWorkList* next = nullptr;
  TaskQueueImpl* queue = nullptr;
  internal::EnqueueOrder order;
};

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

  ~TaskQueueImpl();

  // Types of queues TaskQueueImpl is maintaining internally.
  enum class WorkQueueType { kImmediate, kDelayed };

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
      RepeatingCallback<void(const Task&, const TaskQueue::TaskTiming&)>;

  // May be called from any thread.
  scoped_refptr<SingleThreadTaskRunner> CreateTaskRunner(int task_type) const;

  // TaskQueue implementation.
  const char* GetName() const;
  bool RunsTasksInCurrentSequence() const;
  void PostTask(PostedTask task);
  // Require a reference to enclosing task queue for lifetime control.
  std::unique_ptr<TaskQueue::QueueEnabledVoter> CreateQueueEnabledVoter(
      scoped_refptr<TaskQueue> owning_task_queue);
  bool IsQueueEnabled() const;
  bool IsEmpty() const;
  size_t GetNumberOfPendingTasks() const;
  bool HasTaskToRunImmediately() const;
  Optional<TimeTicks> GetNextScheduledWakeUp();
  Optional<DelayedWakeUp> GetNextScheduledWakeUpImpl();
  void SetQueuePriority(TaskQueue::QueuePriority priority);
  TaskQueue::QueuePriority GetQueuePriority() const;
  void AddTaskObserver(MessageLoop::TaskObserver* task_observer);
  void RemoveTaskObserver(MessageLoop::TaskObserver* task_observer);
  void SetTimeDomain(TimeDomain* time_domain);
  TimeDomain* GetTimeDomain() const;
  void SetBlameContext(trace_event::BlameContext* blame_context);
  void InsertFence(TaskQueue::InsertFencePosition position);
  void InsertFenceAt(TimeTicks time);
  void RemoveFence();
  bool HasActiveFence();
  bool BlockedByFence() const;

  // Implementation of TaskQueue::SetObserver.
  void SetOnNextWakeUpChangedCallback(OnNextWakeUpChangedCallback callback);

  void UnregisterTaskQueue();

  // Returns true if a (potentially hypothetical) task with the specified
  // |enqueue_order| could run on the queue. Must be called from the main
  // thread.
  bool CouldTaskRun(EnqueueOrder enqueue_order) const;

  // Must only be called from the thread this task queue was created on.
  void ReloadImmediateWorkQueueIfEmpty();

  void AsValueInto(TimeTicks now, trace_event::TracedValue* state) const;

  bool GetQuiescenceMonitored() const { return should_monitor_quiescence_; }
  bool GetShouldNotifyObservers() const { return should_notify_observers_; }

  void NotifyWillProcessTask(const PendingTask& pending_task);
  void NotifyDidProcessTask(const PendingTask& pending_task);

  // Check for available tasks in immediate work queues.
  // Used to check if we need to generate notifications about delayed work.
  bool HasPendingImmediateWork();

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

  // Protected by SequenceManagerImpl's AnyThread lock.
  IncomingImmediateWorkList* immediate_work_list_storage() {
    return &immediate_work_list_storage_;
  }

  // Enqueues any delayed tasks which should be run now on the
  // |delayed_work_queue|.
  // Must be called from the main thread.
  void WakeUpForDelayedWork(LazyNow* lazy_now);

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

  class QueueEnabledVoterImpl : public TaskQueue::QueueEnabledVoter {
   public:
    explicit QueueEnabledVoterImpl(scoped_refptr<TaskQueue> task_queue);
    ~QueueEnabledVoterImpl() override;

    // QueueEnabledVoter implementation.
    void SetQueueEnabled(bool enabled) override;

    TaskQueueImpl* GetTaskQueueForTest() const {
      return task_queue_->GetTaskQueueImpl();
    }

   private:
    friend class TaskQueueImpl;

    scoped_refptr<TaskQueue> task_queue_;
    bool enabled_;
  };

  // Iterates over |delayed_incoming_queue| removing canceled tasks.
  void SweepCanceledDelayedTasks(TimeTicks now);

  // Allows wrapping TaskQueue to set a handler to subscribe for notifications
  // about started and completed tasks.
  void SetOnTaskStartedHandler(OnTaskStartedHandler handler);
  void OnTaskStarted(const Task& task,
                     const TaskQueue::TaskTiming& task_timing);
  void SetOnTaskCompletedHandler(OnTaskCompletedHandler handler);
  void OnTaskCompleted(const Task& task,
                       const TaskQueue::TaskTiming& task_timing);
  bool RequiresTaskTiming() const;

  WeakPtr<SequenceManagerImpl> GetSequenceManagerWeakPtr();
  SequenceManagerImpl* sequence_manager() {
    return main_thread_only().sequence_manager;
  }

  // Returns true if this queue is unregistered or task queue manager is deleted
  // and this queue can be safely deleted on any thread.
  bool IsUnregistered() const;

  // Disables queue for testing purposes, when a QueueEnabledVoter can't be
  // constructed due to not having TaskQueue.
  void SetQueueEnabledForTest(bool enabled);

  // TODO(alexclarke): Remove when possible.
  void ClearSequenceManagerForTesting();

 protected:
  void SetDelayedWakeUpForTesting(Optional<DelayedWakeUp> wake_up);

 private:
  friend class WorkQueue;
  friend class WorkQueueTest;

  struct AnyThread {
    AnyThread(SequenceManagerImpl* sequence_manager, TimeDomain* time_domain);
    ~AnyThread();

    // SequenceManagerImpl, TimeDomain and Observer are maintained in two
    // copies: inside AnyThread and inside MainThreadOnly. They can be changed
    // only from main thread, so it should be locked before accessing from other
    // threads.
    SequenceManagerImpl* sequence_manager;
    TimeDomain* time_domain;
    // Callback corresponding to TaskQueue::Observer::OnQueueNextChanged.
    OnNextWakeUpChangedCallback on_next_wake_up_changed_callback;
  };

  // A queue for holding delayed tasks before their delay has expired.
  struct DelayedIncomingQueue {
   public:
    DelayedIncomingQueue();
    ~DelayedIncomingQueue();

    void push(Task&& task);
    void pop();
    bool empty() const { return queue_.empty(); }
    size_t size() const { return queue_.size(); }
    const Task& top() const { return queue_.top(); }

    bool has_pending_high_resolution_tasks() const {
      return pending_high_res_tasks_;
    }

    void SweepCancelledTasks(const SequenceManagerImpl*);
    std::priority_queue<Task> TakeTasks() { return std::move(queue_); }
    void AsValueInto(TimeTicks now, trace_event::TracedValue* state) const;

   private:
    std::priority_queue<Task> queue_;
    // Number of pending tasks in the queue that need high resolution timing.
    int pending_high_res_tasks_ = 0;

    DISALLOW_COPY_AND_ASSIGN(DelayedIncomingQueue);
  };

  struct MainThreadOnly {
    MainThreadOnly(SequenceManagerImpl* sequence_manager,
                   TaskQueueImpl* task_queue,
                   TimeDomain* time_domain);
    ~MainThreadOnly();

    // Another copy of SequenceManagerImpl, TimeDomain and Observer
    // for lock-free access from the main thread.
    // See description inside struct AnyThread for details.
    SequenceManagerImpl* sequence_manager;
    TimeDomain* time_domain;
    // Callback corresponding to TaskQueue::Observer::OnQueueNextChanged.
    OnNextWakeUpChangedCallback on_next_wake_up_changed_callback;

    std::unique_ptr<WorkQueue> delayed_work_queue;
    std::unique_ptr<WorkQueue> immediate_work_queue;
    DelayedIncomingQueue delayed_incoming_queue;
    ObserverList<MessageLoop::TaskObserver>::Unchecked task_observers;
    size_t set_index;
    base::internal::HeapHandle heap_handle;
    int is_enabled_refcount;
    int voter_refcount;
    trace_event::BlameContext* blame_context;  // Not owned.
    EnqueueOrder current_fence;
    Optional<TimeTicks> delayed_fence;
    OnTaskStartedHandler on_task_started_handler;
    OnTaskCompletedHandler on_task_completed_handler;
    // Last reported wake up, used only in UpdateWakeUp to avoid
    // excessive calls.
    Optional<DelayedWakeUp> scheduled_wake_up;
    // If false, queue will be disabled. Used only for tests.
    bool is_enabled_for_test;
  };

  void PostImmediateTaskImpl(PostedTask task);
  void PostDelayedTaskImpl(PostedTask task);

  // Push the task onto the |delayed_incoming_queue|. Lock-free main thread
  // only fast path.
  void PushOntoDelayedIncomingQueueFromMainThread(Task pending_task,
                                                  TimeTicks now,
                                                  bool notify_task_annotator);

  // Push the task onto the |delayed_incoming_queue|.  Slow path from other
  // threads.
  void PushOntoDelayedIncomingQueueLocked(Task pending_task);

  void ScheduleDelayedWorkTask(Task pending_task);

  void MoveReadyImmediateTasksToImmediateWorkQueueLocked();

  // Push the task onto the |immediate_incoming_queue| and for auto pumped
  // queues it calls MaybePostDoWorkOnMainRunner if the Incoming queue was
  // empty.
  void PushOntoImmediateIncomingQueueLocked(Task task);

  using TaskDeque = LazilyDeallocatedDeque<Task>;

  // Extracts all the tasks from the immediate incoming queue and swaps it with
  // |queue| which must be empty.
  // Can be called from any thread.
  void ReloadEmptyImmediateQueue(TaskDeque* queue);

  void TraceQueueSize() const;
  static void QueueAsValueInto(const TaskDeque& queue,
                               TimeTicks now,
                               trace_event::TracedValue* state);
  static void QueueAsValueInto(const std::priority_queue<Task>& queue,
                               TimeTicks now,
                               trace_event::TracedValue* state);
  static void TaskAsValueInto(const Task& task,
                              TimeTicks now,
                              trace_event::TracedValue* state);

  void RemoveQueueEnabledVoter(const QueueEnabledVoterImpl* voter);
  void OnQueueEnabledVoteChanged(bool enabled);
  void EnableOrDisableWithSelector(bool enable);

  // Schedules delayed work on time domain and calls the observer.
  void UpdateDelayedWakeUp(LazyNow* lazy_now);
  void UpdateDelayedWakeUpImpl(LazyNow* lazy_now,
                               Optional<DelayedWakeUp> wake_up);

  // Activate a delayed fence if a time has come.
  void ActivateDelayedFenceIfNeeded(TimeTicks now);

  const char* name_;

  scoped_refptr<AssociatedThreadId> associated_thread_;

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

  MainThreadOnly main_thread_only_;
  MainThreadOnly& main_thread_only() {
    DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
    return main_thread_only_;
  }
  const MainThreadOnly& main_thread_only() const {
    DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
    return main_thread_only_;
  }

  // Proxy which allows TaskQueueTaskRunner to dispatch tasks and it can be
  // detached from TaskQueueImpl to leave dangling task runners behind sefely.
  const scoped_refptr<TaskQueueProxy> proxy_;

  mutable Lock immediate_incoming_queue_lock_;
  TaskDeque immediate_incoming_queue_;
  TaskDeque& immediate_incoming_queue() {
    immediate_incoming_queue_lock_.AssertAcquired();
    return immediate_incoming_queue_;
  }
  const TaskDeque& immediate_incoming_queue() const {
    immediate_incoming_queue_lock_.AssertAcquired();
    return immediate_incoming_queue_;
  }

  // Protected by SequenceManagerImpl's AnyThread lock.
  IncomingImmediateWorkList immediate_work_list_storage_;

  const bool should_monitor_quiescence_;
  const bool should_notify_observers_;

  DISALLOW_COPY_AND_ASSIGN(TaskQueueImpl);
};

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base

#endif  // BASE_TASK_SEQUENCE_MANAGER_TASK_QUEUE_IMPL_H_
