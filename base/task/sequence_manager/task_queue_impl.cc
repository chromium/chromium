// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/task_queue_impl.h"

#include <inttypes.h>

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_token.h"
#include "base/strings/stringprintf.h"
#include "base/task/common/scoped_defer_task_posting.h"
#include "base/task/default_delayed_task_handle_delegate.h"
#include "base/task/sequence_manager/associated_thread_id.h"
#include "base/task/sequence_manager/delayed_task_handle_delegate.h"
#include "base/task/sequence_manager/fence.h"
#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/task/sequence_manager/task_order.h"
#include "base/task/sequence_manager/wake_up_queue.h"
#include "base/task/sequence_manager/work_queue.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_features.h"
#include "base/task/task_observer.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"

namespace base {
namespace sequence_manager {

namespace internal {

// This class outside the anonymous namespace exists to allow being a friend of
// `SingleThreadTaskRunner::CurrentDefaultHandle` in order to access
// `SingleThreadTaskRunner::CurrentDefaultHandle::MayAlreadyExist`.
class CurrentDefaultHandleOverrideForRunOrPostTask {
 public:
  explicit CurrentDefaultHandleOverrideForRunOrPostTask(
      scoped_refptr<SequencedTaskRunner> task_runner)
      : sttr_override_(
            nullptr,
            SingleThreadTaskRunner::CurrentDefaultHandle::MayAlreadyExist{}),
        str_override_(std::move(task_runner)) {}

 private:
  SingleThreadTaskRunner::CurrentDefaultHandle sttr_override_;
  SequencedTaskRunner::CurrentDefaultHandle str_override_;
};

namespace {

// An atomic is used here because the value is queried from other threads when
// tasks are posted cross-thread, which can race with its initialization.
std::atomic<base::TimeDelta> g_max_precise_delay{kDefaultMaxPreciseDelay};
#if BUILDFLAG(IS_WIN)
// An atomic is used here because the flag is queried from other threads when
// tasks are posted cross-thread, which can race with its initialization.
std::atomic_bool g_explicit_high_resolution_timer_win{true};
#endif  // BUILDFLAG(IS_WIN)

void RunTaskSynchronously(AssociatedThreadId* associated_thread,
                          scoped_refptr<SingleThreadTaskRunner> task_runner,
                          OnceClosure closure) {
  base::internal::TaskScope sequence_scope(
      associated_thread->GetBoundSequenceToken(),
      /* is_thread_bound=*/false,
      /* is_running_synchronously=*/true);
  CurrentDefaultHandleOverrideForRunOrPostTask task_runner_override(
      std::move(task_runner));
  associated_thread->StartInSequenceWithCurrentThread();
  std::move(closure).Run();
  associated_thread->StopInSequenceWithCurrentThread();
}

}  // namespace

TaskQueueImpl::GuardedTaskPoster::GuardedTaskPoster(TaskQueueImpl* outer)
    : outer_(outer) {}

TaskQueueImpl::GuardedTaskPoster::~GuardedTaskPoster() {}

bool TaskQueueImpl::GuardedTaskPoster::PostTask(PostedTask task) {
  // Do not process new PostTasks while we are handling a PostTask (tracing
  // has to do this) as it can lead to a deadlock and defer it instead.
  ScopedDeferTaskPosting disallow_task_posting;

  auto token = operations_controller_.TryBeginOperation();
  if (!token)
    return false;

  outer_->PostTask(std::move(task));
  return true;
}

DelayedTaskHandle TaskQueueImpl::GuardedTaskPoster::PostCancelableTask(
    PostedTask task) {
  // Do not process new PostTasks while we are handling a PostTask (tracing
  // has to do this) as it can lead to a deadlock and defer it instead.
  ScopedDeferTaskPosting disallow_task_posting;

  auto token = operations_controller_.TryBeginOperation();
  if (!token)
    return DelayedTaskHandle();

  auto delayed_task_handle_delegate =
      std::make_unique<DelayedTaskHandleDelegate>(outer_);
  task.delayed_task_handle_delegate = delayed_task_handle_delegate->AsWeakPtr();

  outer_->PostTask(std::move(task));
  DCHECK(delayed_task_handle_delegate->IsValid());
  return DelayedTaskHandle(std::move(delayed_task_handle_delegate));
}

bool TaskQueueImpl::GuardedTaskPoster::RunOrPostTask(PostedTask task) {
  auto token = operations_controller_.TryBeginOperation();
  if (!token) {
    return false;
  }

  auto sync_work_auth =
      outer_->sequence_manager_->TryAcquireSyncWorkAuthorization();
  // The queue may be disabled immediately after checking
  // `IsQueueEnabledFromAnyThread()`. That won't prevent the task from running.
  if (sync_work_auth.IsValid() && outer_->IsQueueEnabledFromAnyThread()) {
    RunTaskSynchronously(outer_->associated_thread_.get(),
                         outer_->sequence_manager_->GetTaskRunner(),
                         std::move(task.callback));
    return true;
  }

  return PostTask(std::move(task));
}

TaskQueueImpl::TaskRunner::TaskRunner(
    scoped_refptr<GuardedTaskPoster> task_poster,
    scoped_refptr<AssociatedThreadId> associated_thread,
    TaskType task_type)
    : task_poster_(std::move(task_poster)),
      associated_thread_(std::move(associated_thread)),
      task_type_(task_type) {}

TaskQueueImpl::TaskRunner::~TaskRunner() {}

bool TaskQueueImpl::TaskRunner::PostDelayedTask(const Location& location,
                                                OnceClosure callback,
                                                TimeDelta delay) {
  return task_poster_->PostTask(PostedTask(this, std::move(callback), location,
                                           delay, Nestable::kNestable,
                                           task_type_));
}

bool TaskQueueImpl::TaskRunner::PostDelayedTaskAt(
    subtle::PostDelayedTaskPassKey,
    const Location& location,
    OnceClosure callback,
    TimeTicks delayed_run_time,
    base::subtle::DelayPolicy delay_policy) {
  return task_poster_->PostTask(PostedTask(this, std::move(callback), location,
                                           delayed_run_time, delay_policy,
                                           Nestable::kNestable, task_type_));
}

DelayedTaskHandle TaskQueueImpl::TaskRunner::PostCancelableDelayedTaskAt(
    subtle::PostDelayedTaskPassKey pass_key,
    const Location& location,
    OnceClosure callback,
    TimeTicks delayed_run_time,
    base::subtle::DelayPolicy delay_policy) {
  return task_poster_->PostCancelableTask(
      PostedTask(this, std::move(callback), location, delayed_run_time,
                 delay_policy, Nestable::kNestable, task_type_));
}

DelayedTaskHandle TaskQueueImpl::TaskRunner::PostCancelableDelayedTask(
    subtle::PostDelayedTaskPassKey pass_key,
    const Location& location,
    OnceClosure callback,
    TimeDelta delay) {
  return task_poster_->PostCancelableTask(
      PostedTask(this, std::move(callback), location, delay,
                 Nestable::kNestable, task_type_));
}

bool TaskQueueImpl::TaskRunner::PostNonNestableDelayedTask(
    const Location& location,
    OnceClosure callback,
    TimeDelta delay) {
  return task_poster_->PostTask(PostedTask(this, std::move(callback), location,
                                           delay, Nestable::kNonNestable,
                                           task_type_));
}

bool TaskQueueImpl::TaskRunner::RunOrPostTask(subtle::RunOrPostTaskPassKey,
                                              const Location& location,
                                              OnceClosure callback) {
  return task_poster_->RunOrPostTask(
      PostedTask(this, std::move(callback), location, TimeDelta(),
                 Nestable::kNestable, task_type_));
}

bool TaskQueueImpl::TaskRunner::BelongsToCurrentThread() const {
  return associated_thread_->IsBoundToCurrentThread();
}

bool TaskQueueImpl::TaskRunner::RunsTasksInCurrentSequence() const {
  // Return true on the bound thread. This works even after `thread_local`
  // destruction.
  if (BelongsToCurrentThread()) {
    return true;
  }

  // Return true in a `RunOrPostTask` callback running synchronously on a
  // different thread.
  if (associated_thread_->IsBound() &&
      associated_thread_->GetBoundSequenceToken() ==
          base::internal::SequenceToken::GetForCurrentThread()) {
    return true;
  }

  return false;
}

// static
void TaskQueueImpl::InitializeFeatures() {
  g_max_precise_delay = kMaxPreciseDelay.Get();
#if BUILDFLAG(IS_WIN)
  g_explicit_high_resolution_timer_win.store(
      FeatureList::IsEnabled(kExplicitHighResolutionTimerWin),
      std::memory_order_relaxed);
#endif  // BUILDFLAG(IS_WIN)
}

TaskQueueImpl::TaskQueueImpl(SequenceManagerImpl* sequence_manager,
                             WakeUpQueue* wake_up_queue,
                             const TaskQueue::Spec& spec)
    : name_(spec.name),
      sequence_manager_(sequence_manager),
      associated_thread_(sequence_manager
                             ? sequence_manager->associated_thread()
                             : AssociatedThreadId::CreateBound()),
      task_poster_(MakeRefCounted<GuardedTaskPoster>(this)),
      main_thread_only_(this, wake_up_queue),
      empty_queues_to_reload_handle_(
          sequence_manager
              ? sequence_manager->GetFlagToRequestReloadForEmptyQueue(this)
              : AtomicFlagSet::AtomicFlag()),
      should_monitor_quiescence_(spec.should_monitor_quiescence),
      should_notify_observers_(spec.should_notify_observers),
      delayed_fence_allowed_(spec.delayed_fence_allowed),
      default_task_runner_(CreateTaskRunner(kTaskTypeNone)) {
  UpdateCrossThreadQueueStateLocked();
  // SequenceManager can't be set later, so we need to prevent task runners
  // from posting any tasks.
  if (sequence_manager_)
    task_poster_->StartAcceptingOperations();
}

TaskQueueImpl::~TaskQueueImpl() {
#if DCHECK_IS_ON()
  base::internal::CheckedAutoLock lock(any_thread_lock_);
  // NOTE this check shouldn't fire because |SequenceManagerImpl::queues_|
  // contains a strong reference to this TaskQueueImpl and the
  // SequenceManagerImpl destructor calls UnregisterTaskQueue on all task
  // queues.
  DCHECK(any_thread_.unregistered)
      << "UnregisterTaskQueue must be called first!";
#endif
}

TaskQueueImpl::AnyThread::AnyThread() = default;
TaskQueueImpl::AnyThread::~AnyThread() = default;

TaskQueueImpl::AnyThread::TracingOnly::TracingOnly() = default;
TaskQueueImpl::AnyThread::TracingOnly::~TracingOnly() = default;

TaskQueueImpl::MainThreadOnly::MainThreadOnly(TaskQueueImpl* task_queue,
                                              WakeUpQueue* wake_up_queue)
    : wake_up_queue(wake_up_queue),
      delayed_work_queue(
          new WorkQueue(task_queue, "delayed", WorkQueue::QueueType::kDelayed)),
      immediate_work_queue(new WorkQueue(task_queue,
                                         "immediate",
                                         WorkQueue::QueueType::kImmediate)) {}

TaskQueueImpl::MainThreadOnly::~MainThreadOnly() = default;

scoped_refptr<SingleThreadTaskRunner> TaskQueueImpl::CreateTaskRunner(
    TaskType task_type) const {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  return MakeRefCounted<TaskRunner>(task_poster_, associated_thread_,
                                    task_type);
}

const scoped_refptr<SingleThreadTaskRunner>& TaskQueueImpl::task_runner()
    const {
  return default_task_runner_;
}

void TaskQueueImpl::UnregisterTaskQueue() {
  TRACE_EVENT0("base", "TaskQueueImpl::UnregisterTaskQueue");
  // Invalidate weak pointers now so no voters reference this in a partially
  // torn down state.
  voter_weak_ptr_factory_.InvalidateWeakPtrs();
  // Detach task runners.
  {
    ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
    task_poster_->ShutdownAndWaitForZeroOperations();
  }

  TaskDeque immediate_incoming_queue;
  base::flat_map<raw_ptr<OnTaskPostedCallbackHandleImpl>, OnTaskPostedHandler>
      on_task_posted_handlers;

  {
    base::internal::CheckedAutoLock lock(any_thread_lock_);
    any_thread_.unregistered = true;
    immediate_incoming_queue.swap(any_thread_.immediate_incoming_queue);

    for (auto& handler : any_thread_.on_task_posted_handlers)
      handler.first->UnregisterTaskQueue();
    any_thread_.on_task_posted_handlers.swap(on_task_posted_handlers);
  }

  if (main_thread_only().wake_up_queue) {
    main_thread_only().wake_up_queue->UnregisterQueue(this);
  }

  main_thread_only().on_task_started_handler = OnTaskStartedHandler();
  main_thread_only().on_task_completed_handler = OnTaskCompletedHandler();
  main_thread_only().wake_up_queue = nullptr;
  main_thread_only().throttler = nullptr;
  empty_queues_to_reload_handle_.ReleaseAtomicFlag();

  // It is possible for a task to hold a scoped_refptr to this, which
  // will lead to TaskQueueImpl destructor being called when deleting a task.
  // To avoid use-after-free, we need to clear all fields of a task queue
  // before starting to delete the tasks.
  // All work queues and priority queues containing tasks should be moved to
  // local variables on stack (std::move for unique_ptrs and swap for queues)
  // before clearing them and deleting tasks.

  // Flush the queues outside of the lock because TSAN complains about a lock
  // order inversion for tasks that are posted from within a lock, with a
  // destructor that acquires the same lock.

  DelayedIncomingQueue delayed_incoming_queue;
  delayed_incoming_queue.swap(&main_thread_only().delayed_incoming_queue);
  std::unique_ptr<WorkQueue> immediate_work_queue =
      std::move(main_thread_only().immediate_work_queue);
  std::unique_ptr<WorkQueue> delayed_work_queue =
      std::move(main_thread_only().delayed_work_queue);
}

const char* TaskQueueImpl::GetName() const {
  return perfetto::protos::pbzero::SequenceManagerTask::QueueName_Name(name_);
}

QueueName TaskQueueImpl::GetProtoName() const {
  return name_;
}

void TaskQueueImpl::PostTask(PostedTask task) {
  CurrentThread current_thread =
      associated_thread_->IsBoundToCurrentThread()
          ? TaskQueueImpl::CurrentThread::kMainThread
          : TaskQueueImpl::CurrentThread::kNotMainThread;

#if DCHECK_IS_ON()
  TimeDelta delay = GetTaskDelayAdjustment(current_thread);
  if (absl::holds_alternative<base::TimeTicks>(
          task.delay_or_delayed_run_time)) {
    absl::get<base::TimeTicks>(task.delay_or_delayed_run_time) += delay;
  } else {
    absl::get<base::TimeDelta>(task.delay_or_delayed_run_time) += delay;
  }
#endif  // DCHECK_IS_ON()

  if (!task.is_delayed()) {
    PostImmediateTaskImpl(std::move(task), current_thread);
  } else {
    PostDelayedTaskImpl(std::move(task), current_thread);
  }
}

void TaskQueueImpl::RemoveCancelableTask(HeapHandle heap_handle) {
  associated_thread_->AssertInSequenceWithCurrentThread();
  DCHECK(heap_handle.IsValid());

  main_thread_only().delayed_incoming_queue.remove(heap_handle);

  // Only update the delayed wake up if the top task is removed and we're
  // running on the main thread (a `RunOrPostTask` callback may run outside the
  // main thread, but in sequence with it -- it's not safe to invoke
  // `MessagePump::ScheduleDelayedWork()` in that context).
  if (heap_handle.index() == 0u &&
      associated_thread_->IsBoundToCurrentThread()) {
    LazyNow lazy_now(sequence_manager_->main_thread_clock());
    UpdateWakeUp(&lazy_now);
  }
}

TimeDelta TaskQueueImpl::GetTaskDelayAdjustment(CurrentThread current_thread) {
#if DCHECK_IS_ON()
  if (current_thread == TaskQueueImpl::CurrentThread::kNotMainThread) {
    base::internal::CheckedAutoLock lock(any_thread_lock_);
    // Add a per-priority delay to cross thread tasks. This can help diagnose
    // scheduler induced flakiness by making things flake most of the time.
    return sequence_manager_->settings()
        .priority_settings
        .per_priority_cross_thread_task_delay()[any_thread_.queue_set_index];
  } else {
    return sequence_manager_->settings()
        .priority_settings.per_priority_same_thread_task_delay()
            [main_thread_only().immediate_work_queue->work_queue_set_index()];
  }
#else
  // No delay adjustment.
  return TimeDelta();
#endif  // DCHECK_IS_ON()
}

void TaskQueueImpl::PostImmediateTaskImpl(PostedTask task,
                                          CurrentThread current_thread) {
  // Use CHECK instead of DCHECK to crash earlier. See http://crbug.com/711167
  // for details.
  CHECK(task.callback);

  bool should_schedule_work = false;
  {
    // TODO(alexclarke): Maybe add a main thread only immediate_incoming_queue
    // See https://crbug.com/901800
    base::internal::CheckedAutoLock lock(any_thread_lock_);
    bool add_queue_time_to_tasks = sequence_manager_->GetAddQueueTimeToTasks();
    TimeTicks queue_time;
    if (add_queue_time_to_tasks || delayed_fence_allowed_)
      queue_time = sequence_manager_->any_thread_clock()->NowTicks();

    // The sequence number must be incremented atomically with pushing onto the
    // incoming queue. Otherwise if there are several threads posting task we
    // risk breaking the assumption that sequence numbers increase monotonically
    // within a queue.
    EnqueueOrder sequence_number = sequence_manager_->GetNextSequenceNumber();
    bool was_immediate_incoming_queue_empty =
        any_thread_.immediate_incoming_queue.empty();
    any_thread_.immediate_incoming_queue.push_back(
        Task(std::move(task), sequence_number, sequence_number, queue_time));

#if DCHECK_IS_ON()
    any_thread_.immediate_incoming_queue.back().cross_thread_ =
        (current_thread == TaskQueueImpl::CurrentThread::kNotMainThread);
#endif

    sequence_manager_->WillQueueTask(
        &any_thread_.immediate_incoming_queue.back());
    MaybeReportIpcTaskQueuedFromAnyThreadLocked(
        any_thread_.immediate_incoming_queue.back());

    for (auto& handler : any_thread_.on_task_posted_handlers) {
      DCHECK(!handler.second.is_null());
      handler.second.Run(any_thread_.immediate_incoming_queue.back());
    }

    // If this queue was completely empty, then the SequenceManager needs to be
    // informed so it can reload the work queue and add us to the
    // TaskQueueSelector which can only be done from the main thread. In
    // addition it may need to schedule a DoWork if this queue isn't blocked.
    if (was_immediate_incoming_queue_empty &&
        any_thread_.immediate_work_queue_empty) {
      sequence_manager_->WillRequestReloadImmediateWorkQueue();
      empty_queues_to_reload_handle_.SetActive(true);
      should_schedule_work =
          any_thread_.post_immediate_task_should_schedule_work;
    }
  }

  // On windows it's important to call this outside of a lock because calling a
  // pump while holding a lock can result in priority inversions. See
  // http://shortn/_ntnKNqjDQT for a discussion.
  //
  // Calling ScheduleWork outside the lock should be safe, only the main thread
  // can mutate |any_thread_.post_immediate_task_should_schedule_work|. If it
  // transitions to false we call ScheduleWork redundantly that's harmless. If
  // it transitions to true, the side effect of
  // |empty_queues_to_reload_handle_SetActive(true)| is guaranteed to be picked
  // up by the ThreadController's call to SequenceManagerImpl::DelayTillNextTask
  // when it computes what continuation (if any) is needed.
  if (should_schedule_work)
    sequence_manager_->ScheduleWork();

  TraceQueueSize();
}

void TaskQueueImpl::PostDelayedTaskImpl(PostedTask posted_task,
                                        CurrentThread current_thread) {
  // Use CHECK instead of DCHECK to crash earlier. See http://crbug.com/711167
  // for details.
  CHECK(posted_task.callback);

  if (current_thread == CurrentThread::kMainThread) {
    LazyNow lazy_now(sequence_manager_->main_thread_clock());
    Task pending_task = MakeDelayedTask(std::move(posted_task), &lazy_now);
    sequence_manager_->MaybeAddLeewayToTask(pending_task);
    PushOntoDelayedIncomingQueueFromMainThread(
        std::move(pending_task), &lazy_now,
        /* notify_task_annotator */ true);
  } else {
    LazyNow lazy_now(sequence_manager_->any_thread_clock());
    PushOntoDelayedIncomingQueue(
        MakeDelayedTask(std::move(posted_task), &lazy_now));
  }
}

void TaskQueueImpl::PushOntoDelayedIncomingQueueFromMainThread(
    Task pending_task,
    LazyNow* lazy_now,
    bool notify_task_annotator) {
#if DCHECK_IS_ON()
  pending_task.cross_thread_ = false;
#endif

  if (notify_task_annotator) {
    sequence_manager_->WillQueueTask(&pending_task);
    MaybeReportIpcTaskQueuedFromMainThread(pending_task);
  }
  main_thread_only().delayed_incoming_queue.push(std::move(pending_task));
  UpdateWakeUp(lazy_now);

  TraceQueueSize();
}

void TaskQueueImpl::PushOntoDelayedIncomingQueue(Task pending_task) {
  sequence_manager_->WillQueueTask(&pending_task);
  MaybeReportIpcTaskQueuedFromAnyThreadUnlocked(pending_task);

#if DCHECK_IS_ON()
  pending_task.cross_thread_ = true;
#endif

  // TODO(altimin): Add a copy method to Task to capture metadata here.
  auto task_runner = pending_task.task_runner;
  const auto task_type = pending_task.task_type;
  PostImmediateTaskImpl(
      PostedTask(std::move(task_runner),
                 BindOnce(&TaskQueueImpl::ScheduleDelayedWorkTask,
                          Unretained(this), std::move(pending_task)),
                 FROM_HERE, TimeDelta(), Nestable::kNonNestable, task_type),
      CurrentThread::kNotMainThread);
}

void TaskQueueImpl::ScheduleDelayedWorkTask(Task pending_task) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  sequence_manager_->MaybeAddLeewayToTask(pending_task);
  TimeTicks now = sequence_manager_->main_thread_clock()->NowTicks();
  LazyNow lazy_now(now);
  // A delayed task is ready to run as soon as earliest_delayed_run_time() is
  // reached.
  if (pending_task.earliest_delayed_run_time() <= now) {
    // If |delayed_run_time| is in the past then push it onto the work queue
    // immediately. To ensure the right task ordering we need to temporarily
    // push it onto the |delayed_incoming_queue|.
    pending_task.delayed_run_time = now;
    main_thread_only().delayed_incoming_queue.push(std::move(pending_task));
    MoveReadyDelayedTasksToWorkQueue(
        &lazy_now, sequence_manager_->GetNextSequenceNumber());
  } else {
    // If |delayed_run_time| is in the future we can queue it as normal.
    PushOntoDelayedIncomingQueueFromMainThread(std::move(pending_task),
                                               &lazy_now, false);
  }
  TraceQueueSize();
}

void TaskQueueImpl::ReloadEmptyImmediateWorkQueue() {
  DCHECK(main_thread_only().immediate_work_queue->Empty());
  main_thread_only().immediate_work_queue->TakeImmediateIncomingQueueTasks();

  if (main_thread_only().throttler && IsQueueEnabled()) {
    main_thread_only().throttler->OnHasImmediateTask();
  }
}

void TaskQueueImpl::TakeImmediateIncomingQueueTasks(TaskDeque* queue) {
  DCHECK(queue->empty());
  // Now is a good time to consider reducing the empty queue's capacity if we're
  // wasting memory, before we make it the `immediate_incoming_queue`.
  queue->MaybeShrinkQueue();

  base::internal::CheckedAutoLock lock(any_thread_lock_);
  queue->swap(any_thread_.immediate_incoming_queue);

  // Activate delayed fence if necessary. This is ideologically similar to
  // ActivateDelayedFenceIfNeeded, but due to immediate tasks being posted
  // from any thread we can't generate an enqueue order for the fence there,
  // so we have to check all immediate tasks and use their enqueue order for
  // a fence.
  if (main_thread_only().delayed_fence) {
    for (const Task& task : *queue) {
      DCHECK(!task.queue_time.is_null());
      DCHECK(task.delayed_run_time.is_null());
      if (task.queue_time >= main_thread_only().delayed_fence.value()) {
        main_thread_only().delayed_fence = std::nullopt;
        DCHECK(!main_thread_only().current_fence);
        main_thread_only().current_fence = Fence(task.task_order());
        // Do not trigger WorkQueueSets notification when taking incoming
        // immediate queue.
        main_thread_only().immediate_work_queue->InsertFenceSilently(
            *main_thread_only().current_fence);
        main_thread_only().delayed_work_queue->InsertFenceSilently(
            *main_thread_only().current_fence);
        break;
      }
    }
  }

  UpdateCrossThreadQueueStateLocked();
}

bool TaskQueueImpl::IsEmpty() const {
  if (!main_thread_only().delayed_work_queue->Empty() ||
      !main_thread_only().delayed_incoming_queue.empty() ||
      !main_thread_only().immediate_work_queue->Empty()) {
    return false;
  }

  base::internal::CheckedAutoLock lock(any_thread_lock_);
  return any_thread_.immediate_incoming_queue.empty();
}

size_t TaskQueueImpl::GetNumberOfPendingTasks() const {
  size_t task_count = 0;
  task_count += main_thread_only().delayed_work_queue->Size();
  task_count += main_thread_only().delayed_incoming_queue.size();
  task_count += main_thread_only().immediate_work_queue->Size();

  base::internal::CheckedAutoLock lock(any_thread_lock_);
  task_count += any_thread_.immediate_incoming_queue.size();
  return task_count;
}

bool TaskQueueImpl::HasTaskToRunImmediatelyOrReadyDelayedTask() const {
  // Any work queue tasks count as immediate work.
  if (!main_thread_only().delayed_work_queue->Empty() ||
      !main_thread_only().immediate_work_queue->Empty()) {
    return true;
  }

  // Tasks on |delayed_incoming_queue| that could run now, count as
  // immediate work.
  if (!main_thread_only().delayed_incoming_queue.empty() &&
      main_thread_only().delayed_incoming_queue.top().delayed_run_time <=
          sequence_manager_->main_thread_clock()->NowTicks()) {
    return true;
  }

  // Finally tasks on |immediate_incoming_queue| count as immediate work.
  base::internal::CheckedAutoLock lock(any_thread_lock_);
  return !any_thread_.immediate_incoming_queue.empty();
}

std::optional<WakeUp> TaskQueueImpl::GetNextDesiredWakeUp() {
  // Note we don't scheduled a wake-up for disabled queues.
  if (main_thread_only().delayed_incoming_queue.empty() || !IsQueueEnabled())
    return std::nullopt;

  const auto& top_task = main_thread_only().delayed_incoming_queue.top();

  // High resolution is needed if the queue contains high resolution tasks and
  // has a priority index <= kNormalPriority (precise execution time is
  // unnecessary for a low priority queue).
  WakeUpResolution resolution = has_pending_high_resolution_tasks() &&
                                        GetQueuePriority() <= DefaultPriority()
                                    ? WakeUpResolution::kHigh
                                    : WakeUpResolution::kLow;
  subtle::DelayPolicy delay_policy = top_task.delay_policy;
  if (GetQueuePriority() > DefaultPriority() &&
      delay_policy == subtle::DelayPolicy::kPrecise) {
    delay_policy = subtle::DelayPolicy::kFlexibleNoSooner;
  }
  return WakeUp{top_task.delayed_run_time, top_task.leeway, resolution,
                delay_policy};
}

void TaskQueueImpl::OnWakeUp(LazyNow* lazy_now, EnqueueOrder enqueue_order) {
  MoveReadyDelayedTasksToWorkQueue(lazy_now, enqueue_order);
  if (main_thread_only().throttler) {
    main_thread_only().throttler->OnWakeUp(lazy_now);
  }
}

bool TaskQueueImpl::RemoveAllCanceledDelayedTasksFromFront(LazyNow* lazy_now) {
  // Because task destructors could have a side-effect of posting new tasks, we
  // move all the cancelled tasks into a temporary container before deleting
  // them. This is to avoid the queue from changing while iterating over it.
  absl::InlinedVector<Task, 8> tasks_to_delete;

  while (!main_thread_only().delayed_incoming_queue.empty()) {
    const Task& task = main_thread_only().delayed_incoming_queue.top();
    CHECK(task.task);
    if (!task.task.IsCancelled())
      break;

    tasks_to_delete.push_back(
        main_thread_only().delayed_incoming_queue.take_top());
  }

  if (!tasks_to_delete.empty()) {
    UpdateWakeUp(lazy_now);
    return true;
  }

  return false;
}

void TaskQueueImpl::MoveReadyDelayedTasksToWorkQueue(
    LazyNow* lazy_now,
    EnqueueOrder enqueue_order) {
  // Enqueue all delayed tasks that should be running now, skipping any that
  // have been canceled.
  WorkQueue::TaskPusher delayed_work_queue_task_pusher(
      main_thread_only().delayed_work_queue->CreateTaskPusher());

  // Because task destructors could have a side-effect of posting new tasks, we
  // move all the cancelled tasks into a temporary container before deleting
  // them. This is to avoid the queue from changing while iterating over it.
  absl::InlinedVector<Task, 8> tasks_to_delete;

  while (!main_thread_only().delayed_incoming_queue.empty()) {
    const Task& task = main_thread_only().delayed_incoming_queue.top();
    CHECK(task.task);

    // Leave the top task alone if it hasn't been canceled and it is not ready.
    const bool is_cancelled = task.task.IsCancelled();
    if (!is_cancelled && task.earliest_delayed_run_time() > lazy_now->Now())
      break;

    Task ready_task = main_thread_only().delayed_incoming_queue.take_top();
    if (is_cancelled) {
      tasks_to_delete.push_back(std::move(ready_task));
      continue;
    }

    // The top task is ready to run. Move it to the delayed work queue.
#if DCHECK_IS_ON()
    if (sequence_manager_->settings().log_task_delay_expiry)
      VLOG(0) << GetName() << " Delay expired for "
              << ready_task.posted_from.ToString();
#endif  // DCHECK_IS_ON()
    DCHECK(!ready_task.delayed_run_time.is_null());
    DCHECK(!ready_task.enqueue_order_set());
    ready_task.set_enqueue_order(enqueue_order);
    ActivateDelayedFenceIfNeeded(ready_task);

    delayed_work_queue_task_pusher.Push(std::move(ready_task));
  }

  // Explicitly delete tasks last.
  tasks_to_delete.clear();

  UpdateWakeUp(lazy_now);
}

void TaskQueueImpl::TraceQueueSize() const {
  bool is_tracing;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("sequence_manager"), &is_tracing);
  if (!is_tracing)
    return;

  // It's only safe to access the work queues from the main thread.
  // TODO(alexclarke): We should find another way of tracing this
  if (!associated_thread_->IsBoundToCurrentThread())
    return;

  size_t total_task_count;
  {
    base::internal::CheckedAutoLock lock(any_thread_lock_);
    total_task_count = any_thread_.immediate_incoming_queue.size() +
                       main_thread_only().immediate_work_queue->Size() +
                       main_thread_only().delayed_work_queue->Size() +
                       main_thread_only().delayed_incoming_queue.size();
  }
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("sequence_manager"), GetName(),
                 total_task_count);
}

void TaskQueueImpl::SetQueuePriority(TaskQueue::QueuePriority priority) {
  const TaskQueue::QueuePriority previous_priority = GetQueuePriority();
  if (priority == previous_priority)
    return;
  sequence_manager_->main_thread_only().selector.SetQueuePriority(this,
                                                                  priority);

#if BUILDFLAG(IS_WIN)
  // Updating queue priority can change whether high resolution timer is needed.
  LazyNow lazy_now(sequence_manager_->main_thread_clock());
  UpdateWakeUp(&lazy_now);
#endif

  if (priority > DefaultPriority()) {
    // |priority| is now lower than the default, so update accordingly.
    main_thread_only()
        .enqueue_order_at_which_we_became_unblocked_with_normal_priority =
        EnqueueOrder::max();
  } else if (previous_priority > DefaultPriority()) {
    // |priority| is no longer lower than the default, so record current
    // sequence number.
    DCHECK_EQ(
        main_thread_only()
            .enqueue_order_at_which_we_became_unblocked_with_normal_priority,
        EnqueueOrder::max());
    main_thread_only()
        .enqueue_order_at_which_we_became_unblocked_with_normal_priority =
        sequence_manager_->GetNextSequenceNumber();
  }
}

TaskQueue::QueuePriority TaskQueueImpl::GetQueuePriority() const {
  size_t set_index = immediate_work_queue()->work_queue_set_index();
  DCHECK_EQ(set_index, delayed_work_queue()->work_queue_set_index());
  return static_cast<TaskQueue::QueuePriority>(set_index);
}

Value::Dict TaskQueueImpl::AsValue(TimeTicks now, bool force_verbose) const {
  base::internal::CheckedAutoLock lock(any_thread_lock_);
  Value::Dict state;
  state.Set("name", GetName());
  if (any_thread_.unregistered) {
    state.Set("unregistered", true);
    return state;
  }
  DCHECK(main_thread_only().delayed_work_queue);
  DCHECK(main_thread_only().immediate_work_queue);

  state.Set("task_queue_id",
            StringPrintf("0x%" PRIx64, static_cast<uint64_t>(
                                           reinterpret_cast<uintptr_t>(this))));
  state.Set("enabled", IsQueueEnabled());
  // TODO(crbug.com/40228085): Make base::Value able to store an int64_t and
  // remove the various static_casts below.
  state.Set("any_thread_.immediate_incoming_queuesize",
            static_cast<int>(any_thread_.immediate_incoming_queue.size()));
  state.Set("delayed_incoming_queue_size",
            static_cast<int>(main_thread_only().delayed_incoming_queue.size()));
  state.Set("immediate_work_queue_size",
            static_cast<int>(main_thread_only().immediate_work_queue->Size()));
  state.Set("delayed_work_queue_size",
            static_cast<int>(main_thread_only().delayed_work_queue->Size()));

  state.Set("any_thread_.immediate_incoming_queuecapacity",
            static_cast<int>(any_thread_.immediate_incoming_queue.capacity()));
  state.Set("immediate_work_queue_capacity",
            static_cast<int>(immediate_work_queue()->Capacity()));
  state.Set("delayed_work_queue_capacity",
            static_cast<int>(delayed_work_queue()->Capacity()));

  if (!main_thread_only().delayed_incoming_queue.empty()) {
    TimeDelta delay_to_next_task =
        (main_thread_only().delayed_incoming_queue.top().delayed_run_time -
         sequence_manager_->main_thread_clock()->NowTicks());
    state.Set("delay_to_next_task_ms", delay_to_next_task.InMillisecondsF());
  }
  if (main_thread_only().current_fence) {
    Value::Dict fence_state;
    fence_state.Set(
        "enqueue_order",
        static_cast<int>(
            main_thread_only().current_fence->task_order().enqueue_order()));
    fence_state.Set("activated_in_wake_up", !main_thread_only()
                                                 .current_fence->task_order()
                                                 .delayed_run_time()
                                                 .is_null());
    state.Set("current_fence", std::move(fence_state));
  }
  if (main_thread_only().delayed_fence) {
    state.Set("delayed_fence_seconds_from_now",
              (main_thread_only().delayed_fence.value() - now).InSecondsF());
  }

  bool verbose = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("sequence_manager.verbose_snapshots"),
      &verbose);

  if (verbose || force_verbose) {
    state.Set("immediate_incoming_queue",
              QueueAsValue(any_thread_.immediate_incoming_queue, now));
    state.Set("delayed_work_queue",
              main_thread_only().delayed_work_queue->AsValue(now));
    state.Set("immediate_work_queue",
              main_thread_only().immediate_work_queue->AsValue(now));
    state.Set("delayed_incoming_queue",
              main_thread_only().delayed_incoming_queue.AsValue(now));
  }
  state.Set("priority", GetQueuePriority());
  return state;
}

void TaskQueueImpl::AddTaskObserver(TaskObserver* task_observer) {
  main_thread_only().task_observers.AddObserver(task_observer);
}

void TaskQueueImpl::RemoveTaskObserver(TaskObserver* task_observer) {
  main_thread_only().task_observers.RemoveObserver(task_observer);
}

void TaskQueueImpl::NotifyWillProcessTask(const Task& task,
                                          bool was_blocked_or_low_priority) {
  DCHECK(should_notify_observers_);

  for (auto& observer : main_thread_only().task_observers)
    observer.WillProcessTask(task, was_blocked_or_low_priority);
}

void TaskQueueImpl::NotifyDidProcessTask(const Task& task) {
  DCHECK(should_notify_observers_);
  for (auto& observer : main_thread_only().task_observers)
    observer.DidProcessTask(task);
}

void TaskQueueImpl::InsertFence(TaskQueue::InsertFencePosition position) {
  Fence new_fence = position == TaskQueue::InsertFencePosition::kNow
                        ? Fence::CreateWithEnqueueOrder(
                              sequence_manager_->GetNextSequenceNumber())
                        : Fence::BlockingFence();
  InsertFence(new_fence);
}

void TaskQueueImpl::InsertFence(Fence current_fence) {
  // Only one fence may be present at a time.
  main_thread_only().delayed_fence = std::nullopt;

  std::optional<Fence> previous_fence = main_thread_only().current_fence;

  // Tasks posted after this point will have a strictly higher enqueue order
  // and will be blocked from running.
  main_thread_only().current_fence = current_fence;
  bool front_task_unblocked =
      main_thread_only().immediate_work_queue->InsertFence(current_fence);
  front_task_unblocked |=
      main_thread_only().delayed_work_queue->InsertFence(current_fence);

  {
    base::internal::CheckedAutoLock lock(any_thread_lock_);
    if (!front_task_unblocked && previous_fence &&
        previous_fence->task_order() < current_fence.task_order()) {
      if (!any_thread_.immediate_incoming_queue.empty() &&
          any_thread_.immediate_incoming_queue.front().task_order() >
              previous_fence->task_order() &&
          any_thread_.immediate_incoming_queue.front().task_order() <
              current_fence.task_order()) {
        front_task_unblocked = true;
      }
    }

    UpdateCrossThreadQueueStateLocked();
  }

  if (IsQueueEnabled() && front_task_unblocked) {
    OnQueueUnblocked();
    sequence_manager_->ScheduleWork();
  }
}

void TaskQueueImpl::InsertFenceAt(TimeTicks time) {
  DCHECK(delayed_fence_allowed_)
      << "Delayed fences are not supported for this queue. Enable them "
         "explicitly in TaskQueue::Spec when creating the queue";

  // Task queue can have only one fence, delayed or not.
  RemoveFence();
  main_thread_only().delayed_fence = time;
}

void TaskQueueImpl::RemoveFence() {
  std::optional<Fence> previous_fence = main_thread_only().current_fence;
  main_thread_only().current_fence = std::nullopt;
  main_thread_only().delayed_fence = std::nullopt;

  bool front_task_unblocked =
      main_thread_only().immediate_work_queue->RemoveFence();
  front_task_unblocked |= main_thread_only().delayed_work_queue->RemoveFence();

  {
    base::internal::CheckedAutoLock lock(any_thread_lock_);
    if (!front_task_unblocked && previous_fence) {
      if (!any_thread_.immediate_incoming_queue.empty() &&
          any_thread_.immediate_incoming_queue.front().task_order() >
              previous_fence->task_order()) {
        front_task_unblocked = true;
      }
    }

    UpdateCrossThreadQueueStateLocked();
  }

  if (IsQueueEnabled() && front_task_unblocked) {
    OnQueueUnblocked();
    sequence_manager_->ScheduleWork();
  }
}

bool TaskQueueImpl::BlockedByFence() const {
  if (!main_thread_only().current_fence)
    return false;

  if (!main_thread_only().immediate_work_queue->BlockedByFence() ||
      !main_thread_only().delayed_work_queue->BlockedByFence()) {
    return false;
  }

  base::internal::CheckedAutoLock lock(any_thread_lock_);
  if (any_thread_.immediate_incoming_queue.empty())
    return true;

  return any_thread_.immediate_incoming_queue.front().task_order() >
         main_thread_only().current_fence->task_order();
}

bool TaskQueueImpl::HasActiveFence() {
  if (main_thread_only().delayed_fence &&
      sequence_manager_->main_thread_clock()->NowTicks() >
          main_thread_only().delayed_fence.value()) {
    return true;
  }
  return !!main_thread_only().current_fence;
}

bool TaskQueueImpl::CouldTaskRun(EnqueueOrder enqueue_order) const {
  if (!IsQueueEnabled())
    return false;

  if (!main_thread_only().current_fence)
    return true;

  // TODO(crbug.com/40791504): This should use TaskOrder. This is currently only
  // used for tests and is fine as-is, but we should be using `TaskOrder` for
  // task comparisons. Also this test should be renamed with a testing suffix as
  // it is not used in production.
  return enqueue_order <
         main_thread_only().current_fence->task_order().enqueue_order();
}

bool TaskQueueImpl::WasBlockedOrLowPriority(EnqueueOrder enqueue_order) const {
  return enqueue_order <
         main_thread_only()
             .enqueue_order_at_which_we_became_unblocked_with_normal_priority;
}

// static
Value::List TaskQueueImpl::QueueAsValue(const TaskDeque& queue, TimeTicks now) {
  Value::List state;
  for (const Task& task : queue)
    state.Append(TaskAsValue(task, now));
  return state;
}

// static
Value::Dict TaskQueueImpl::TaskAsValue(const Task& task, TimeTicks now) {
  Value::Dict state;
  state.Set("posted_from", task.posted_from.ToString());
  if (task.enqueue_order_set())
    state.Set("enqueue_order", static_cast<int>(task.enqueue_order()));
  state.Set("sequence_num", task.sequence_num);
  state.Set("nestable", task.nestable == Nestable::kNestable);
  state.Set("is_high_res", task.is_high_res);
  state.Set("is_cancelled", task.task.IsCancelled());
  state.Set("delayed_run_time",
            (task.delayed_run_time - TimeTicks()).InMillisecondsF());
  const TimeDelta delayed_run_time_milliseconds_from_now =
      task.delayed_run_time.is_null() ? TimeDelta()
                                      : (task.delayed_run_time - now);
  state.Set("delayed_run_time_milliseconds_from_now",
            delayed_run_time_milliseconds_from_now.InMillisecondsF());
  return state;
}

Task TaskQueueImpl::MakeDelayedTask(PostedTask delayed_task,
                                    LazyNow* lazy_now) const {
  EnqueueOrder sequence_number = sequence_manager_->GetNextSequenceNumber();
  base::TimeDelta delay;
  WakeUpResolution resolution = WakeUpResolution::kLow;
#if BUILDFLAG(IS_WIN)
  const bool explicit_high_resolution_timer_win =
      g_explicit_high_resolution_timer_win.load(std::memory_order_relaxed);
#endif  // BUILDFLAG(IS_WIN)
  if (absl::holds_alternative<base::TimeDelta>(
          delayed_task.delay_or_delayed_run_time)) {
    delay = absl::get<base::TimeDelta>(delayed_task.delay_or_delayed_run_time);
    delayed_task.delay_or_delayed_run_time = lazy_now->Now() + delay;
  } else {
    delay = absl::get<base::TimeTicks>(delayed_task.delay_or_delayed_run_time) -
            lazy_now->Now();
  }
#if BUILDFLAG(IS_WIN)
  if (!explicit_high_resolution_timer_win &&
      delay < (2 * base::Milliseconds(Time::kMinLowResolutionThresholdMs))) {
    // Outside the kExplicitHighResolutionTimerWin experiment, We consider the
    // task needs a high resolution timer if the delay is more than 0 and less
    // than 32ms. This caps the relative error to less than 50% : a 33ms wait
    // can wake at 48ms since the default resolution on Windows is between 10
    // and 15ms.
    resolution = WakeUpResolution::kHigh;
  }
#endif  // BUILDFLAG(IS_WIN)
  delayed_task.delay_policy = subtle::MaybeOverrideDelayPolicy(
      delayed_task.delay_policy, delay,
      g_max_precise_delay.load(std::memory_order_relaxed));
  // leeway isn't specified yet since this may be called from any thread.
  return Task(std::move(delayed_task), sequence_number, EnqueueOrder(),
              lazy_now->Now(), resolution);
}

bool TaskQueueImpl::IsQueueEnabled() const {
  return main_thread_only().is_enabled;
}

void TaskQueueImpl::SetQueueEnabled(bool enabled) {
  if (main_thread_only().is_enabled == enabled)
    return;

  // Update the |main_thread_only_| struct.
  main_thread_only().is_enabled = enabled;
  main_thread_only().disabled_time = std::nullopt;

  // |sequence_manager_| can be null in tests.
  if (!sequence_manager_)
    return;

  LazyNow lazy_now(sequence_manager_->main_thread_clock());

  if (!enabled) {
    bool tracing_enabled = false;
    TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("lifecycles"),
                                       &tracing_enabled);
    main_thread_only().disabled_time = lazy_now.Now();
  } else {
    // Override reporting if the queue is becoming enabled again.
    main_thread_only().should_report_posted_tasks_when_disabled = false;
  }

  // If there is a throttler, it will be notified of pending delayed and
  // immediate tasks inside UpdateWakeUp().
  UpdateWakeUp(&lazy_now);

  {
    base::internal::CheckedAutoLock lock(any_thread_lock_);
    UpdateCrossThreadQueueStateLocked();

    // Copy over the task-reporting related state.
    any_thread_.is_enabled = enabled;
    any_thread_.tracing_only.disabled_time = main_thread_only().disabled_time;
    any_thread_.tracing_only.should_report_posted_tasks_when_disabled =
        main_thread_only().should_report_posted_tasks_when_disabled;
  }

  // Finally, enable or disable the queue with the selector.
  if (enabled) {
    // Note the selector calls SequenceManager::OnTaskQueueEnabled which posts
    // a DoWork if needed.
    sequence_manager_->main_thread_only().selector.EnableQueue(this);

    if (!BlockedByFence())
      OnQueueUnblocked();
  } else {
    sequence_manager_->main_thread_only().selector.DisableQueue(this);
  }
}

void TaskQueueImpl::SetShouldReportPostedTasksWhenDisabled(bool should_report) {
  if (main_thread_only().should_report_posted_tasks_when_disabled ==
      should_report)
    return;

  // Only observe transitions turning the reporting on if tracing is enabled.
  if (should_report) {
    bool tracing_enabled = false;
    TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("lifecycles"),
                                       &tracing_enabled);
    if (!tracing_enabled)
      return;
  }

  main_thread_only().should_report_posted_tasks_when_disabled = should_report;

  // Mirror the state to the AnyThread struct as well.
  {
    base::internal::CheckedAutoLock lock(any_thread_lock_);
    any_thread_.tracing_only.should_report_posted_tasks_when_disabled =
        should_report;
  }
}

void TaskQueueImpl::UpdateCrossThreadQueueStateLocked() {
  any_thread_.immediate_work_queue_empty =
      main_thread_only().immediate_work_queue->Empty();
  any_thread_.is_enabled = main_thread_only().is_enabled;

  if (main_thread_only().throttler) {
    // If there's a Throttler, always ScheduleWork() when immediate work is
    // posted and the queue is enabled, to ensure that
    // Throttler::OnHasImmediateTask() is invoked.
    any_thread_.post_immediate_task_should_schedule_work = IsQueueEnabled();
  } else {
    // Otherwise, ScheduleWork() only if the queue is enabled and there isn't a
    // fence to prevent the task from being executed.
    any_thread_.post_immediate_task_should_schedule_work =
        IsQueueEnabled() && !main_thread_only().current_fence;
  }

#if DCHECK_IS_ON()
  any_thread_.queue_set_index =
      main_thread_only().immediate_work_queue->work_queue_set_index();
#endif
}

void TaskQueueImpl::ReclaimMemory(TimeTicks now) {
  if (main_thread_only().delayed_incoming_queue.empty())
    return;

  main_thread_only().delayed_incoming_queue.SweepCancelledTasks(
      sequence_manager_);

  // If deleting one of the cancelled tasks shut down this queue, bail out.
  // Note that in this scenario |this| is still valid, but some fields of the
  // queue have been cleared out by |UnregisterTaskQueue|.
  if (!main_thread_only().delayed_work_queue) {
    return;
  }

  LazyNow lazy_now(now);
  UpdateWakeUp(&lazy_now);

  // Also consider shrinking the work queue if it's wasting memory.
  main_thread_only().delayed_work_queue->MaybeShrinkQueue();
  main_thread_only().immediate_work_queue->MaybeShrinkQueue();

  {
    base::internal::CheckedAutoLock lock(any_thread_lock_);
    any_thread_.immediate_incoming_queue.MaybeShrinkQueue();
  }
}

void TaskQueueImpl::PushImmediateIncomingTaskForTest(Task task) {
  base::internal::CheckedAutoLock lock(any_thread_lock_);
  any_thread_.immediate_incoming_queue.push_back(std::move(task));
}

void TaskQueueImpl::RequeueDeferredNonNestableTask(
    DeferredNonNestableTask task) {
  DCHECK(task.task.nestable == Nestable::kNonNestable);

  // It's possible that the queue was unregistered since the task was posted.
  // Skip the task in that case.
  if (!main_thread_only().delayed_work_queue)
    return;

  // The re-queued tasks have to be pushed onto the front because we'd otherwise
  // violate the strict monotonically increasing enqueue order within the
  // WorkQueue.  We can't assign them a new enqueue order here because that will
  // not behave correctly with fences and things will break (e.g Idle TQ).
  if (task.work_queue_type == WorkQueueType::kDelayed) {
    main_thread_only().delayed_work_queue->PushNonNestableTaskToFront(
        std::move(task.task));
  } else {
    // We're about to push |task| onto an empty |immediate_work_queue|
    // (bypassing |immediate_incoming_queue_|). As such, we no longer need to
    // reload if we were planning to. The flag must be cleared while holding
    // the lock to avoid a cross-thread post task setting it again before
    // we actually make |immediate_work_queue| non-empty.
    if (main_thread_only().immediate_work_queue->Empty()) {
      base::internal::CheckedAutoLock lock(any_thread_lock_);
      empty_queues_to_reload_handle_.SetActive(false);

      any_thread_.immediate_work_queue_empty = false;
      main_thread_only().immediate_work_queue->PushNonNestableTaskToFront(
          std::move(task.task));

    } else {
      main_thread_only().immediate_work_queue->PushNonNestableTaskToFront(
          std::move(task.task));
    }
  }
}

void TaskQueueImpl::SetThrottler(TaskQueue::Throttler* throttler) {
  DCHECK(throttler);
  DCHECK(!main_thread_only().throttler)
      << "Can't assign two different throttlers to "
         "base::sequence_manager:TaskQueue";
  // `throttler` is guaranteed to outlive this object.
  main_thread_only().throttler = throttler;
}

void TaskQueueImpl::ResetThrottler() {
  main_thread_only().throttler = nullptr;
  LazyNow lazy_now(sequence_manager_->main_thread_clock());
  // The current delayed wake up may have been determined by the Throttler.
  // Update it now that there is no Throttler.
  UpdateWakeUp(&lazy_now);
}

void TaskQueueImpl::UpdateWakeUp(LazyNow* lazy_now) {
  std::optional<WakeUp> wake_up = GetNextDesiredWakeUp();
  if (main_thread_only().throttler && IsQueueEnabled()) {
    // GetNextAllowedWakeUp() may return a non-null wake_up even if |wake_up| is
    // nullopt, e.g. to throttle immediate tasks.
    wake_up = main_thread_only().throttler->GetNextAllowedWakeUp(
        lazy_now, wake_up, HasTaskToRunImmediatelyOrReadyDelayedTask());
  }
  SetNextWakeUp(lazy_now, wake_up);
}

void TaskQueueImpl::SetNextWakeUp(LazyNow* lazy_now,
                                  std::optional<WakeUp> wake_up) {
  if (main_thread_only().scheduled_wake_up == wake_up)
    return;
  main_thread_only().scheduled_wake_up = wake_up;
  main_thread_only().wake_up_queue->SetNextWakeUpForQueue(this, lazy_now,
                                                          wake_up);
}

bool TaskQueueImpl::HasTaskToRunImmediately() const {
  // Any work queue tasks count as immediate work.
  if (!main_thread_only().delayed_work_queue->Empty() ||
      !main_thread_only().immediate_work_queue->Empty()) {
    return true;
  }

  // Finally tasks on |immediate_incoming_queue| count as immediate work.
  base::internal::CheckedAutoLock lock(any_thread_lock_);
  return !any_thread_.immediate_incoming_queue.empty();
}

bool TaskQueueImpl::HasTaskToRunImmediatelyLocked() const {
  return !main_thread_only().delayed_work_queue->Empty() ||
         !main_thread_only().immediate_work_queue->Empty() ||
         !any_thread_.immediate_incoming_queue.empty();
}

void TaskQueueImpl::SetOnTaskStartedHandler(
    TaskQueueImpl::OnTaskStartedHandler handler) {
  DCHECK(should_notify_observers_ || handler.is_null());
  main_thread_only().on_task_started_handler = std::move(handler);
}

void TaskQueueImpl::OnTaskStarted(const Task& task,
                                  const TaskQueue::TaskTiming& task_timing) {
  if (!main_thread_only().on_task_started_handler.is_null())
    main_thread_only().on_task_started_handler.Run(task, task_timing);
}

void TaskQueueImpl::SetOnTaskCompletedHandler(
    TaskQueueImpl::OnTaskCompletedHandler handler) {
  DCHECK(should_notify_observers_ || handler.is_null());
  main_thread_only().on_task_completed_handler = std::move(handler);
}

void TaskQueueImpl::OnTaskCompleted(const Task& task,
                                    TaskQueue::TaskTiming* task_timing,
                                    LazyNow* lazy_now) {
  if (!main_thread_only().on_task_completed_handler.is_null()) {
    main_thread_only().on_task_completed_handler.Run(task, task_timing,
                                                     lazy_now);
  }
}

bool TaskQueueImpl::RequiresTaskTiming() const {
  return !main_thread_only().on_task_started_handler.is_null() ||
         !main_thread_only().on_task_completed_handler.is_null();
}

std::unique_ptr<TaskQueue::OnTaskPostedCallbackHandle>
TaskQueueImpl::AddOnTaskPostedHandler(OnTaskPostedHandler handler) {
  DCHECK(should_notify_observers_ && !handler.is_null());
  std::unique_ptr<OnTaskPostedCallbackHandleImpl> handle =
      std::make_unique<OnTaskPostedCallbackHandleImpl>(this,
                                                       associated_thread_);
  base::internal::CheckedAutoLock lock(any_thread_lock_);
  any_thread_.on_task_posted_handlers.insert(
      {handle.get(), std::move(handler)});
  return handle;
}

void TaskQueueImpl::RemoveOnTaskPostedHandler(
    TaskQueueImpl::OnTaskPostedCallbackHandleImpl*
        on_task_posted_callback_handle) {
  base::internal::CheckedAutoLock lock(any_thread_lock_);
  any_thread_.on_task_posted_handlers.erase(on_task_posted_callback_handle);
}

void TaskQueueImpl::SetTaskExecutionTraceLogger(
    TaskExecutionTraceLogger logger) {
  DCHECK(should_notify_observers_ || logger.is_null());
  main_thread_only().task_execution_trace_logger = std::move(logger);
}

bool TaskQueueImpl::IsUnregistered() const {
  base::internal::CheckedAutoLock lock(any_thread_lock_);
  return any_thread_.unregistered;
}

WeakPtr<SequenceManagerImpl> TaskQueueImpl::GetSequenceManagerWeakPtr() {
  return sequence_manager_->GetWeakPtr();
}

void TaskQueueImpl::ActivateDelayedFenceIfNeeded(const Task& task) {
  if (!main_thread_only().delayed_fence)
    return;
  if (main_thread_only().delayed_fence.value() > task.delayed_run_time)
    return;
  InsertFence(Fence(task.task_order()));
  main_thread_only().delayed_fence = std::nullopt;
}

void TaskQueueImpl::MaybeReportIpcTaskQueuedFromMainThread(
    const Task& pending_task) {
  if (!pending_task.ipc_hash)
    return;

  // It's possible that tracing was just enabled and no disabled time has been
  // stored. In that case, skip emitting the event.
  if (!main_thread_only().disabled_time)
    return;

  bool tracing_enabled = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("lifecycles"),
                                     &tracing_enabled);
  if (!tracing_enabled)
    return;

  if (main_thread_only().is_enabled ||
      !main_thread_only().should_report_posted_tasks_when_disabled) {
    return;
  }

  base::TimeDelta time_since_disabled =
      sequence_manager_->main_thread_clock()->NowTicks() -
      main_thread_only().disabled_time.value();

  ReportIpcTaskQueued(pending_task, time_since_disabled);
}

bool TaskQueueImpl::ShouldReportIpcTaskQueuedFromAnyThreadLocked(
    base::TimeDelta* time_since_disabled) {
  // It's possible that tracing was just enabled and no disabled time has been
  // stored. In that case, skip emitting the event.
  if (!any_thread_.tracing_only.disabled_time)
    return false;

  if (any_thread_.is_enabled ||
      any_thread_.tracing_only.should_report_posted_tasks_when_disabled) {
    return false;
  }

  *time_since_disabled = sequence_manager_->any_thread_clock()->NowTicks() -
                         any_thread_.tracing_only.disabled_time.value();
  return true;
}

void TaskQueueImpl::MaybeReportIpcTaskQueuedFromAnyThreadLocked(
    const Task& pending_task) {
  if (!pending_task.ipc_hash)
    return;

  bool tracing_enabled = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("lifecycles"),
                                     &tracing_enabled);
  if (!tracing_enabled)
    return;

  base::TimeDelta time_since_disabled;
  if (ShouldReportIpcTaskQueuedFromAnyThreadLocked(&time_since_disabled))
    ReportIpcTaskQueued(pending_task, time_since_disabled);
}

void TaskQueueImpl::MaybeReportIpcTaskQueuedFromAnyThreadUnlocked(
    const Task& pending_task) {
  if (!pending_task.ipc_hash)
    return;

  bool tracing_enabled = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("lifecycles"),
                                     &tracing_enabled);
  if (!tracing_enabled)
    return;

  base::TimeDelta time_since_disabled;
  bool should_report = false;
  {
    base::internal::CheckedAutoLock lock(any_thread_lock_);
    should_report =
        ShouldReportIpcTaskQueuedFromAnyThreadLocked(&time_since_disabled);
  }

  if (should_report)
    ReportIpcTaskQueued(pending_task, time_since_disabled);
}

void TaskQueueImpl::ReportIpcTaskQueued(
    const Task& pending_task,
    const base::TimeDelta& time_since_disabled) {
  TRACE_EVENT_INSTANT(
      TRACE_DISABLED_BY_DEFAULT("lifecycles"), "task_posted_to_disabled_queue",
      [&](perfetto::EventContext ctx) {
        auto* proto = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                          ->set_chrome_task_posted_to_disabled_queue();
        proto->set_time_since_disabled_ms(
            checked_cast<uint64_t>(time_since_disabled.InMilliseconds()));
        proto->set_ipc_hash(pending_task.ipc_hash);
        proto->set_source_location_iid(
            base::trace_event::InternedSourceLocation::Get(
                &ctx, pending_task.posted_from));
      });
}

void TaskQueueImpl::OnQueueUnblocked() {
  DCHECK(IsQueueEnabled());
  DCHECK(!BlockedByFence());

  main_thread_only().enqueue_order_at_which_we_became_unblocked =
      sequence_manager_->GetNextSequenceNumber();
  if (GetQueuePriority() <= DefaultPriority()) {
    // We are default priority or more important so update
    // |enqueue_order_at_which_we_became_unblocked_with_normal_priority|.
    main_thread_only()
        .enqueue_order_at_which_we_became_unblocked_with_normal_priority =
        main_thread_only().enqueue_order_at_which_we_became_unblocked;
  }
}

std::unique_ptr<TaskQueue::QueueEnabledVoter>
TaskQueueImpl::CreateQueueEnabledVoter() {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  return WrapUnique(
      new TaskQueue::QueueEnabledVoter(voter_weak_ptr_factory_.GetWeakPtr()));
}

void TaskQueueImpl::AddQueueEnabledVoter(bool voter_is_enabled,
                                         TaskQueue::QueueEnabledVoter& voter) {
  ++main_thread_only().voter_count;
  if (voter_is_enabled) {
    ++main_thread_only().enabled_voter_count;
  }
}

void TaskQueueImpl::RemoveQueueEnabledVoter(
    bool voter_is_enabled,
    TaskQueue::QueueEnabledVoter& voter) {
  bool was_enabled = AreAllQueueEnabledVotersEnabled();
  if (voter_is_enabled) {
    --main_thread_only().enabled_voter_count;
    DCHECK_GE(main_thread_only().enabled_voter_count, 0);
  }

  --main_thread_only().voter_count;
  DCHECK_GE(main_thread_only().voter_count, 0);

  bool is_enabled = AreAllQueueEnabledVotersEnabled();
  if (was_enabled != is_enabled) {
    SetQueueEnabled(is_enabled);
  }
}

void TaskQueueImpl::OnQueueEnabledVoteChanged(bool enabled) {
  bool was_enabled = AreAllQueueEnabledVotersEnabled();
  if (enabled) {
    ++main_thread_only().enabled_voter_count;
    DCHECK_LE(main_thread_only().enabled_voter_count,
              main_thread_only().voter_count);
  } else {
    --main_thread_only().enabled_voter_count;
    DCHECK_GE(main_thread_only().enabled_voter_count, 0);
  }

  bool is_enabled = AreAllQueueEnabledVotersEnabled();
  if (was_enabled != is_enabled) {
    SetQueueEnabled(is_enabled);
  }
}

void TaskQueueImpl::CompleteInitializationOnBoundThread() {
  voter_weak_ptr_factory_.BindToCurrentSequence(
      subtle::BindWeakPtrFactoryPassKey());
}

TaskQueue::QueuePriority TaskQueueImpl::DefaultPriority() const {
  return sequence_manager()->settings().priority_settings.default_priority();
}

bool TaskQueueImpl::IsQueueEnabledFromAnyThread() const {
  base::internal::CheckedAutoLock lock(any_thread_lock_);
  return any_thread_.is_enabled;
}

TaskQueueImpl::DelayedIncomingQueue::DelayedIncomingQueue() = default;
TaskQueueImpl::DelayedIncomingQueue::~DelayedIncomingQueue() = default;

void TaskQueueImpl::DelayedIncomingQueue::push(Task task) {
  // TODO(crbug.com/40789839): Remove this once the cause of corrupted tasks in
  // the queue is understood.
  CHECK(task.task);
  if (task.is_high_res)
    pending_high_res_tasks_++;
  queue_.insert(std::move(task));
}

void TaskQueueImpl::DelayedIncomingQueue::remove(HeapHandle heap_handle) {
  DCHECK(!empty());
  DCHECK_LT(heap_handle.index(), queue_.size());
  Task task = queue_.take(heap_handle);
  if (task.is_high_res) {
    pending_high_res_tasks_--;
    DCHECK_GE(pending_high_res_tasks_, 0);
  }
}

Task TaskQueueImpl::DelayedIncomingQueue::take_top() {
  DCHECK(!empty());
  if (queue_.top().is_high_res) {
    pending_high_res_tasks_--;
    DCHECK_GE(pending_high_res_tasks_, 0);
  }
  return queue_.take_top();
}

void TaskQueueImpl::DelayedIncomingQueue::swap(DelayedIncomingQueue* rhs) {
  std::swap(pending_high_res_tasks_, rhs->pending_high_res_tasks_);
  std::swap(queue_, rhs->queue_);
}

void TaskQueueImpl::DelayedIncomingQueue::SweepCancelledTasks(
    SequenceManagerImpl* sequence_manager) {
  // Note: IntrusiveHeap::EraseIf() is safe against re-entrancy caused by
  // deleted tasks posting new tasks.
  queue_.EraseIf([this](const Task& task) {
    if (task.task.IsCancelled()) {
      if (task.is_high_res) {
        --pending_high_res_tasks_;
        DCHECK_GE(pending_high_res_tasks_, 0);
      }
      return true;
    }
    return false;
  });
}

Value::List TaskQueueImpl::DelayedIncomingQueue::AsValue(TimeTicks now) const {
  Value::List state;
  for (const Task& task : queue_)
    state.Append(TaskAsValue(task, now));
  return state;
}

bool TaskQueueImpl::DelayedIncomingQueue::Compare::operator()(
    const Task& lhs,
    const Task& rhs) const {
  // Delayed tasks are ordered by latest_delayed_run_time(). The top task may
  // not be the first task eligible to run, but tasks will always become ripe
  // before their latest_delayed_run_time().
  const TimeTicks lhs_latest_delayed_run_time = lhs.latest_delayed_run_time();
  const TimeTicks rhs_latest_delayed_run_time = rhs.latest_delayed_run_time();
  if (lhs_latest_delayed_run_time == rhs_latest_delayed_run_time)
    return lhs.sequence_num > rhs.sequence_num;
  return lhs_latest_delayed_run_time > rhs_latest_delayed_run_time;
}

TaskQueueImpl::OnTaskPostedCallbackHandleImpl::OnTaskPostedCallbackHandleImpl(
    TaskQueueImpl* task_queue_impl,
    scoped_refptr<const AssociatedThreadId> associated_thread)
    : task_queue_impl_(task_queue_impl),
      associated_thread_(std::move(associated_thread)) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
}

TaskQueueImpl::OnTaskPostedCallbackHandleImpl::
    ~OnTaskPostedCallbackHandleImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  if (task_queue_impl_)
    task_queue_impl_->RemoveOnTaskPostedHandler(this);
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
