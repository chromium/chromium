// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/sequence_manager_impl.h"

#include <queue>
#include <vector>

#include "base/bind.h"
#include "base/bit_cast.h"
#include "base/compiler_specific.h"
#include "base/debug/crash_logging.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/task/sequence_manager/real_time_domain.h"
#include "base/task/sequence_manager/task_time_observer.h"
#include "base/task/sequence_manager/thread_controller_impl.h"
#include "base/task/sequence_manager/work_queue.h"
#include "base/task/sequence_manager/work_queue_sets.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"

namespace base {
namespace sequence_manager {

std::unique_ptr<SequenceManager> CreateSequenceManagerOnCurrentThread() {
  return internal::SequenceManagerImpl::CreateOnCurrentThread();
}

std::unique_ptr<SequenceManager> CreateUnboundSequenceManager(
    MessageLoop* message_loop) {
  return internal::SequenceManagerImpl::CreateUnbound(message_loop);
}

namespace internal {

namespace {

constexpr base::TimeDelta kLongTaskTraceEventThreshold =
    base::TimeDelta::FromMilliseconds(50);
// Proportion of tasks which will record thread time for metrics.
const double kTaskSamplingRateForRecordingCPUTime = 0.01;
// Proprortion of SequenceManagers which will record thread time for each task,
// enabling advanced metrics.
const double kThreadSamplingRateForRecordingCPUTime = 0.0001;

// Magic value to protect against memory corruption and bail out
// early when detected.
constexpr int kMemoryCorruptionSentinelValue = 0xdeadbeef;

void SweepCanceledDelayedTasksInQueue(
    internal::TaskQueueImpl* queue,
    std::map<TimeDomain*, TimeTicks>* time_domain_now) {
  TimeDomain* time_domain = queue->GetTimeDomain();
  if (time_domain_now->find(time_domain) == time_domain_now->end())
    time_domain_now->insert(std::make_pair(time_domain, time_domain->Now()));
  queue->SweepCanceledDelayedTasks(time_domain_now->at(time_domain));
}

SequenceManager::MetricRecordingSettings InitializeMetricRecordingSettings() {
  bool cpu_time_recording_always_on =
      base::RandDouble() < kThreadSamplingRateForRecordingCPUTime;
  return SequenceManager::MetricRecordingSettings(
      cpu_time_recording_always_on, kTaskSamplingRateForRecordingCPUTime);
}

}  // namespace

SequenceManagerImpl::SequenceManagerImpl(
    std::unique_ptr<internal::ThreadController> controller)
    : associated_thread_(controller->GetAssociatedThread()),
      controller_(std::move(controller)),
      metric_recording_settings_(InitializeMetricRecordingSettings()),
      memory_corruption_sentinel_(kMemoryCorruptionSentinelValue),
      main_thread_only_(associated_thread_),
      weak_factory_(this) {
  TRACE_EVENT_WARMUP_CATEGORY("sequence_manager");
  TRACE_EVENT_WARMUP_CATEGORY(TRACE_DISABLED_BY_DEFAULT("sequence_manager"));
  TRACE_EVENT_WARMUP_CATEGORY(
      TRACE_DISABLED_BY_DEFAULT("sequence_manager.debug"));
  TRACE_EVENT_WARMUP_CATEGORY(
      TRACE_DISABLED_BY_DEFAULT("sequence_manager.verbose_snapshots"));

  TRACE_EVENT_OBJECT_CREATED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("sequence_manager"), "SequenceManager", this);
  main_thread_only().selector.SetTaskQueueSelectorObserver(this);

  RegisterTimeDomain(main_thread_only().real_time_domain.get());

  controller_->SetSequencedTaskSource(this);
}

SequenceManagerImpl::~SequenceManagerImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("sequence_manager"), "SequenceManager", this);

  // TODO(altimin): restore default task runner automatically when
  // ThreadController is destroyed.
  controller_->RestoreDefaultTaskRunner();

  for (internal::TaskQueueImpl* queue : main_thread_only().active_queues) {
    main_thread_only().selector.RemoveQueue(queue);
    queue->UnregisterTaskQueue();
  }

  main_thread_only().active_queues.clear();
  main_thread_only().queues_to_gracefully_shutdown.clear();
  main_thread_only().selector.SetTaskQueueSelectorObserver(nullptr);

  // In some tests a NestingObserver may not have been registered.
  if (main_thread_only().nesting_observer_registered_)
    controller_->RemoveNestingObserver(this);
}

SequenceManagerImpl::AnyThread::AnyThread() = default;

SequenceManagerImpl::AnyThread::~AnyThread() = default;

SequenceManagerImpl::MainThreadOnly::MainThreadOnly(
    const scoped_refptr<AssociatedThreadId>& associated_thread)
    : random_generator(RandUint64()),
      uniform_distribution(0.0, 1.0),
      selector(associated_thread),
      real_time_domain(new internal::RealTimeDomain()) {}

SequenceManagerImpl::MainThreadOnly::~MainThreadOnly() = default;

// static
std::unique_ptr<SequenceManagerImpl>
SequenceManagerImpl::CreateOnCurrentThread() {
  auto manager = CreateUnbound(MessageLoop::current());
  manager->BindToCurrentThread();
  manager->CompleteInitializationOnBoundThread();
  return manager;
}

// static
std::unique_ptr<SequenceManagerImpl> SequenceManagerImpl::CreateUnbound(
    MessageLoop* message_loop) {
  return WrapUnique(
      new SequenceManagerImpl(internal::ThreadControllerImpl::Create(
          message_loop, DefaultTickClock::GetInstance())));
}

void SequenceManagerImpl::BindToMessageLoop(MessageLoop* message_loop) {
  controller_->SetMessageLoop(message_loop);
  CompleteInitializationOnBoundThread();
}

void SequenceManagerImpl::BindToCurrentThread() {
  associated_thread_->BindToCurrentThread();
}

void SequenceManagerImpl::CompleteInitializationOnBoundThread() {
  controller_->AddNestingObserver(this);
  main_thread_only().nesting_observer_registered_ = true;
}

void SequenceManagerImpl::RegisterTimeDomain(TimeDomain* time_domain) {
  main_thread_only().time_domains.insert(time_domain);
  time_domain->OnRegisterWithSequenceManager(this);
}

void SequenceManagerImpl::UnregisterTimeDomain(TimeDomain* time_domain) {
  main_thread_only().time_domains.erase(time_domain);
}

TimeDomain* SequenceManagerImpl::GetRealTimeDomain() const {
  return main_thread_only().real_time_domain.get();
}

std::unique_ptr<internal::TaskQueueImpl>
SequenceManagerImpl::CreateTaskQueueImpl(const TaskQueue::Spec& spec) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  TimeDomain* time_domain = spec.time_domain
                                ? spec.time_domain
                                : main_thread_only().real_time_domain.get();
  DCHECK(main_thread_only().time_domains.find(time_domain) !=
         main_thread_only().time_domains.end());
  std::unique_ptr<internal::TaskQueueImpl> task_queue =
      std::make_unique<internal::TaskQueueImpl>(this, time_domain, spec);
  main_thread_only().active_queues.insert(task_queue.get());
  main_thread_only().selector.AddQueue(task_queue.get());
  return task_queue;
}

void SequenceManagerImpl::SetObserver(Observer* observer) {
  main_thread_only().observer = observer;
}

bool SequenceManagerImpl::AddToIncomingImmediateWorkList(
    internal::TaskQueueImpl* task_queue,
    internal::EnqueueOrder enqueue_order) {
  AutoLock lock(any_thread_lock_);
  // Check if |task_queue| is already in the linked list.
  if (task_queue->immediate_work_list_storage()->queue)
    return false;

  // Insert into the linked list.
  task_queue->immediate_work_list_storage()->queue = task_queue;
  task_queue->immediate_work_list_storage()->order = enqueue_order;
  task_queue->immediate_work_list_storage()->next =
      any_thread().incoming_immediate_work_list;
  any_thread().incoming_immediate_work_list =
      task_queue->immediate_work_list_storage();
  return true;
}

void SequenceManagerImpl::RemoveFromIncomingImmediateWorkList(
    internal::TaskQueueImpl* task_queue) {
  AutoLock lock(any_thread_lock_);
  internal::IncomingImmediateWorkList** prev =
      &any_thread().incoming_immediate_work_list;
  while (*prev) {
    if ((*prev)->queue == task_queue) {
      *prev = (*prev)->next;
      break;
    }
    prev = &(*prev)->next;
  }

  task_queue->immediate_work_list_storage()->next = nullptr;
  task_queue->immediate_work_list_storage()->queue = nullptr;
}

void SequenceManagerImpl::ShutdownTaskQueueGracefully(
    std::unique_ptr<internal::TaskQueueImpl> task_queue) {
  main_thread_only().queues_to_gracefully_shutdown[task_queue.get()] =
      std::move(task_queue);
}

void SequenceManagerImpl::UnregisterTaskQueueImpl(
    std::unique_ptr<internal::TaskQueueImpl> task_queue) {
  TRACE_EVENT1("sequence_manager", "SequenceManagerImpl::UnregisterTaskQueue",
               "queue_name", task_queue->GetName());
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);

  main_thread_only().selector.RemoveQueue(task_queue.get());

  // After UnregisterTaskQueue returns no new tasks can be posted.
  // It's important to call it first to avoid race condition between removing
  // the task queue from various lists here and adding it to the same lists
  // when posting a task.
  task_queue->UnregisterTaskQueue();

  // Remove |task_queue| from the linked list if present.
  // This is O(n).  We assume this will be a relatively infrequent operation.
  RemoveFromIncomingImmediateWorkList(task_queue.get());

  // Add |task_queue| to |main_thread_only().queues_to_delete| so we can prevent
  // it from being freed while any of our structures hold hold a raw pointer to
  // it.
  main_thread_only().active_queues.erase(task_queue.get());
  main_thread_only().queues_to_delete[task_queue.get()] = std::move(task_queue);
}

void SequenceManagerImpl::ReloadEmptyWorkQueues() {
  // There are two cases where a queue needs reloading.  First, it might be
  // completely empty and we've just posted a task (this method handles that
  // case). Secondly if the work queue becomes empty in when calling
  // WorkQueue::TakeTaskFromWorkQueue (handled there).
  for (internal::TaskQueueImpl* queue : main_thread_only().queues_to_reload) {
    queue->ReloadImmediateWorkQueueIfEmpty();
  }
}

void SequenceManagerImpl::WakeUpReadyDelayedQueues(LazyNow* lazy_now) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("sequence_manager"),
               "SequenceManagerImpl::WakeUpReadyDelayedQueues");

  for (TimeDomain* time_domain : main_thread_only().time_domains) {
    if (time_domain == main_thread_only().real_time_domain.get()) {
      time_domain->WakeUpReadyDelayedQueues(lazy_now);
    } else {
      LazyNow time_domain_lazy_now = time_domain->CreateLazyNow();
      time_domain->WakeUpReadyDelayedQueues(&time_domain_lazy_now);
    }
  }
}

void SequenceManagerImpl::OnBeginNestedRunLoop() {
  main_thread_only().nesting_depth++;
  if (main_thread_only().observer)
    main_thread_only().observer->OnBeginNestedRunLoop();
}

void SequenceManagerImpl::OnExitNestedRunLoop() {
  main_thread_only().nesting_depth--;
  DCHECK_GE(main_thread_only().nesting_depth, 0);
  if (main_thread_only().nesting_depth == 0) {
    // While we were nested some non-nestable tasks may have been deferred.
    // We push them back onto the *front* of their original work queues,
    // that's why we iterate |non_nestable_task_queue| in FIFO order.
    while (!main_thread_only().non_nestable_task_queue.empty()) {
      internal::TaskQueueImpl::DeferredNonNestableTask& non_nestable_task =
          main_thread_only().non_nestable_task_queue.back();
      non_nestable_task.task_queue->RequeueDeferredNonNestableTask(
          std::move(non_nestable_task));
      main_thread_only().non_nestable_task_queue.pop_back();
    }
  }
  if (main_thread_only().observer)
    main_thread_only().observer->OnExitNestedRunLoop();
}

void SequenceManagerImpl::OnQueueHasIncomingImmediateWork(
    internal::TaskQueueImpl* queue,
    internal::EnqueueOrder enqueue_order,
    bool queue_is_blocked) {
  if (AddToIncomingImmediateWorkList(queue, enqueue_order) && !queue_is_blocked)
    controller_->ScheduleWork();
}

void SequenceManagerImpl::MaybeScheduleImmediateWork(
    const Location& from_here) {
  controller_->ScheduleWork();
}

void SequenceManagerImpl::SetNextDelayedDoWork(LazyNow* lazy_now,
                                               TimeTicks run_time) {
  controller_->SetNextDelayedDoWork(lazy_now, run_time);
}

Optional<PendingTask> SequenceManagerImpl::TakeTask() {
  Optional<PendingTask> task = TakeTaskImpl();
  if (!task)
    return base::nullopt;

  ExecutingTask& executing_task =
      *main_thread_only().task_execution_stack.rbegin();

  // It's important that there are no active trace events here which will
  // terminate before we finish executing the task.
  TRACE_EVENT_BEGIN2(TRACE_DISABLED_BY_DEFAULT("sequence_manager"),
                     "SequenceManager::RunTask", "queue_type",
                     executing_task.task_queue->GetName(), "task_type",
                     executing_task.task_type);

  return task;
}

Optional<PendingTask> SequenceManagerImpl::TakeTaskImpl() {
  CHECK(Validate());

  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  TRACE_EVENT0("sequence_manager", "SequenceManagerImpl::TakeTask");

  {
    AutoLock lock(any_thread_lock_);
    main_thread_only().queues_to_reload.clear();

    for (internal::IncomingImmediateWorkList* iter =
             any_thread().incoming_immediate_work_list;
         iter; iter = iter->next) {
      main_thread_only().queues_to_reload.push_back(iter->queue);
      iter->queue = nullptr;
    }

    any_thread().incoming_immediate_work_list = nullptr;
  }

  // It's important we call ReloadEmptyWorkQueues out side of the lock to
  // avoid a lock order inversion.
  ReloadEmptyWorkQueues();
  LazyNow lazy_now(controller_->GetClock());
  WakeUpReadyDelayedQueues(&lazy_now);

  while (true) {
    internal::WorkQueue* work_queue = nullptr;
    bool should_run =
        main_thread_only().selector.SelectWorkQueueToService(&work_queue);
    TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
        TRACE_DISABLED_BY_DEFAULT("sequence_manager.debug"), "SequenceManager",
        this, AsValueWithSelectorResult(should_run, work_queue));

    if (!should_run)
      return nullopt;

    // If the head task was canceled, remove it and run the selector again.
    if (work_queue->RemoveAllCanceledTasksFromFront())
      continue;

    if (work_queue->GetFrontTask()->nestable == Nestable::kNonNestable &&
        main_thread_only().nesting_depth > 0) {
      // Defer non-nestable work. NOTE these tasks can be arbitrarily delayed so
      // the additional delay should not be a problem.
      // Note because we don't delete queues while nested, it's perfectly OK to
      // store the raw pointer for |queue| here.
      internal::TaskQueueImpl::DeferredNonNestableTask deferred_task{
          work_queue->TakeTaskFromWorkQueue(), work_queue->task_queue(),
          work_queue->queue_type()};
      main_thread_only().non_nestable_task_queue.push_back(
          std::move(deferred_task));
      continue;
    }

    main_thread_only().task_execution_stack.emplace_back(
        work_queue->TakeTaskFromWorkQueue(), work_queue->task_queue(),
        InitializeTaskTiming(work_queue->task_queue()));

    ExecutingTask& executing_task =
        *main_thread_only().task_execution_stack.rbegin();
    NotifyWillProcessTask(&executing_task, &lazy_now);

    return std::move(executing_task.pending_task);
  }
}

void SequenceManagerImpl::DidRunTask() {
  LazyNow lazy_now(controller_->GetClock());
  ExecutingTask& executing_task =
      *main_thread_only().task_execution_stack.rbegin();

  TRACE_EVENT_END0(TRACE_DISABLED_BY_DEFAULT("sequence_manager"),
                   "SequenceManagerImpl::RunTask");

  NotifyDidProcessTask(&executing_task, &lazy_now);
  main_thread_only().task_execution_stack.pop_back();

  if (main_thread_only().nesting_depth == 0)
    CleanUpQueues();
}

TimeDelta SequenceManagerImpl::DelayTillNextTask(LazyNow* lazy_now) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);

  // If the selector has non-empty queues we trivially know there is immediate
  // work to be done.
  if (!main_thread_only().selector.AllEnabledWorkQueuesAreEmpty())
    return TimeDelta();

  // Its possible the selectors state is dirty because ReloadEmptyWorkQueues
  // hasn't been called yet. This check catches the case of fresh incoming work.
  {
    AutoLock lock(any_thread_lock_);
    for (const internal::IncomingImmediateWorkList* iter =
             any_thread().incoming_immediate_work_list;
         iter; iter = iter->next) {
      if (iter->queue->CouldTaskRun(iter->order))
        return TimeDelta();
    }
  }

  // Otherwise we need to find the shortest delay, if any.  NB we don't need to
  // call WakeUpReadyDelayedQueues because it's assumed DelayTillNextTask will
  // return TimeDelta>() if the delayed task is due to run now.
  TimeDelta delay_till_next_task = TimeDelta::Max();
  for (TimeDomain* time_domain : main_thread_only().time_domains) {
    Optional<TimeDelta> delay = time_domain->DelayTillNextTask(lazy_now);
    if (!delay)
      continue;

    if (*delay < delay_till_next_task)
      delay_till_next_task = *delay;
  }
  return delay_till_next_task;
}

bool SequenceManagerImpl::HasPendingHighResolutionTasks() {
  for (TimeDomain* time_domain : main_thread_only().time_domains) {
    if (time_domain->HasPendingHighResolutionTasks())
      return true;
  }
  return false;
}

void SequenceManagerImpl::WillQueueTask(Task* pending_task) {
  controller_->WillQueueTask(pending_task);
}

TaskQueue::TaskTiming SequenceManagerImpl::InitializeTaskTiming(
    internal::TaskQueueImpl* task_queue) {
  bool records_wall_time = ShouldRecordTaskTiming(task_queue);
  bool records_thread_time = records_wall_time && ShouldRecordCPUTimeForTask();
  return TaskQueue::TaskTiming(records_wall_time, records_thread_time);
}

bool SequenceManagerImpl::ShouldRecordTaskTiming(
    const internal::TaskQueueImpl* task_queue) {
  if (task_queue->RequiresTaskTiming())
    return true;
  return main_thread_only().nesting_depth == 0 &&
         main_thread_only().task_time_observers.might_have_observers();
}

void SequenceManagerImpl::NotifyWillProcessTask(ExecutingTask* executing_task,
                                                LazyNow* time_before_task) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("sequence_manager"),
               "SequenceManagerImpl::NotifyWillProcessTaskObservers");

  if (executing_task->task_queue->GetQuiescenceMonitored())
    main_thread_only().task_was_run_on_quiescence_monitored_queue = true;

#if !defined(OS_NACL)
  debug::SetCrashKeyString(
      main_thread_only().file_name_crash_key,
      executing_task->pending_task.posted_from.file_name());
  debug::SetCrashKeyString(
      main_thread_only().function_name_crash_key,
      executing_task->pending_task.posted_from.function_name());
#endif  // OS_NACL

  bool record_task_timing = ShouldRecordTaskTiming(executing_task->task_queue);
  if (record_task_timing)
    executing_task->task_timing.RecordTaskStart(time_before_task);

  if (!executing_task->task_queue->GetShouldNotifyObservers())
    return;

  {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("sequence_manager"),
                 "SequenceManager.WillProcessTaskObservers");
    for (auto& observer : main_thread_only().task_observers)
      observer.WillProcessTask(executing_task->pending_task);
  }

  {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("sequence_manager"),
                 "SequenceManager.QueueNotifyWillProcessTask");
    executing_task->task_queue->NotifyWillProcessTask(
        executing_task->pending_task);
  }

  if (!record_task_timing)
    return;

  if (main_thread_only().nesting_depth == 0) {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("sequence_manager"),
                 "SequenceManager.WillProcessTaskTimeObservers");
    for (auto& observer : main_thread_only().task_time_observers)
      observer.WillProcessTask(executing_task->task_timing.start_time());
  }

  {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("sequence_manager"),
                 "SequenceManager.QueueOnTaskStarted");
    executing_task->task_queue->OnTaskStarted(executing_task->pending_task,
                                              executing_task->task_timing);
  }
}

void SequenceManagerImpl::NotifyDidProcessTask(ExecutingTask* executing_task,
                                               LazyNow* time_after_task) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("sequence_manager"),
               "SequenceManagerImpl::NotifyDidProcessTaskObservers");
  if (!executing_task->task_queue->GetShouldNotifyObservers())
    return;

  bool record_task_timing = ShouldRecordTaskTiming(executing_task->task_queue);

  // Record end time ASAP to avoid bias due to the overhead of observers.
  if (record_task_timing)
    executing_task->task_timing.RecordTaskEnd(time_after_task);

  const TaskQueue::TaskTiming& task_timing = executing_task->task_timing;

  if (task_timing.has_wall_time() && main_thread_only().nesting_depth == 0) {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("sequence_manager"),
                 "SequenceManager.DidProcessTaskTimeObservers");
    for (auto& observer : main_thread_only().task_time_observers) {
      observer.DidProcessTask(task_timing.start_time(), task_timing.end_time());
    }
  }

  {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("sequence_manager"),
                 "SequenceManager.DidProcessTaskObservers");
    for (auto& observer : main_thread_only().task_observers)
      observer.DidProcessTask(executing_task->pending_task);
  }

  {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("sequence_manager"),
                 "SequenceManager.QueueNotifyDidProcessTask");
    executing_task->task_queue->NotifyDidProcessTask(
        executing_task->pending_task);
  }

  {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("sequence_manager"),
                 "SequenceManager.QueueOnTaskCompleted");
    if (task_timing.has_wall_time())
      executing_task->task_queue->OnTaskCompleted(executing_task->pending_task,
                                                  task_timing);
  }

  // TODO(altimin): Move this back to blink.
  if (task_timing.has_wall_time() &&
      task_timing.wall_duration() > kLongTaskTraceEventThreshold &&
      main_thread_only().nesting_depth == 0) {
    TRACE_EVENT_INSTANT1("blink", "LongTask", TRACE_EVENT_SCOPE_THREAD,
                         "duration", task_timing.wall_duration().InSecondsF());
  }
}

void SequenceManagerImpl::SetWorkBatchSize(int work_batch_size) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK_GE(work_batch_size, 1);
  controller_->SetWorkBatchSize(work_batch_size);
}

void SequenceManagerImpl::SetTimerSlack(TimerSlack timer_slack) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  controller_->SetTimerSlack(timer_slack);
}

void SequenceManagerImpl::AddTaskObserver(
    MessageLoop::TaskObserver* task_observer) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  main_thread_only().task_observers.AddObserver(task_observer);
}

void SequenceManagerImpl::RemoveTaskObserver(
    MessageLoop::TaskObserver* task_observer) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  main_thread_only().task_observers.RemoveObserver(task_observer);
}

void SequenceManagerImpl::AddTaskTimeObserver(
    TaskTimeObserver* task_time_observer) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  main_thread_only().task_time_observers.AddObserver(task_time_observer);
}

void SequenceManagerImpl::RemoveTaskTimeObserver(
    TaskTimeObserver* task_time_observer) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  main_thread_only().task_time_observers.RemoveObserver(task_time_observer);
}

bool SequenceManagerImpl::GetAndClearSystemIsQuiescentBit() {
  bool task_was_run =
      main_thread_only().task_was_run_on_quiescence_monitored_queue;
  main_thread_only().task_was_run_on_quiescence_monitored_queue = false;
  return !task_was_run;
}

internal::EnqueueOrder SequenceManagerImpl::GetNextSequenceNumber() {
  return enqueue_order_generator_.GenerateNext();
}

std::unique_ptr<trace_event::ConvertableToTraceFormat>
SequenceManagerImpl::AsValueWithSelectorResult(
    bool should_run,
    internal::WorkQueue* selected_work_queue) const {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  std::unique_ptr<trace_event::TracedValue> state(
      new trace_event::TracedValue());
  TimeTicks now = NowTicks();
  state->BeginArray("active_queues");
  for (auto* const queue : main_thread_only().active_queues)
    queue->AsValueInto(now, state.get());
  state->EndArray();
  state->BeginArray("queues_to_gracefully_shutdown");
  for (const auto& pair : main_thread_only().queues_to_gracefully_shutdown)
    pair.first->AsValueInto(now, state.get());
  state->EndArray();
  state->BeginArray("queues_to_delete");
  for (const auto& pair : main_thread_only().queues_to_delete)
    pair.first->AsValueInto(now, state.get());
  state->EndArray();
  state->BeginDictionary("selector");
  main_thread_only().selector.AsValueInto(state.get());
  state->EndDictionary();
  if (should_run) {
    state->SetString("selected_queue",
                     selected_work_queue->task_queue()->GetName());
    state->SetString("work_queue_name", selected_work_queue->name());
  }

  state->BeginArray("time_domains");
  for (auto* time_domain : main_thread_only().time_domains)
    time_domain->AsValueInto(state.get());
  state->EndArray();
  {
    AutoLock lock(any_thread_lock_);
    state->BeginArray("has_incoming_immediate_work");
    for (const internal::IncomingImmediateWorkList* iter =
             any_thread().incoming_immediate_work_list;
         iter; iter = iter->next) {
      state->AppendString(iter->queue->GetName());
    }
    state->EndArray();
  }
  return std::move(state);
}

void SequenceManagerImpl::OnTaskQueueEnabled(internal::TaskQueueImpl* queue) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK(queue->IsQueueEnabled());
  // Only schedule DoWork if there's something to do.
  if (queue->HasTaskToRunImmediately() && !queue->BlockedByFence())
    MaybeScheduleImmediateWork(FROM_HERE);
}

void SequenceManagerImpl::SweepCanceledDelayedTasks() {
  std::map<TimeDomain*, TimeTicks> time_domain_now;
  for (auto* const queue : main_thread_only().active_queues)
    SweepCanceledDelayedTasksInQueue(queue, &time_domain_now);
  for (const auto& pair : main_thread_only().queues_to_gracefully_shutdown)
    SweepCanceledDelayedTasksInQueue(pair.first, &time_domain_now);
}

void SequenceManagerImpl::CleanUpQueues() {
  for (auto it = main_thread_only().queues_to_gracefully_shutdown.begin();
       it != main_thread_only().queues_to_gracefully_shutdown.end();) {
    if (it->first->IsEmpty()) {
      UnregisterTaskQueueImpl(std::move(it->second));
      main_thread_only().active_queues.erase(it->first);
      main_thread_only().queues_to_gracefully_shutdown.erase(it++);
    } else {
      ++it;
    }
  }
  main_thread_only().queues_to_delete.clear();
}

WeakPtr<SequenceManagerImpl> SequenceManagerImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void SequenceManagerImpl::SetDefaultTaskRunner(
    scoped_refptr<SingleThreadTaskRunner> task_runner) {
  controller_->SetDefaultTaskRunner(task_runner);
}

const TickClock* SequenceManagerImpl::GetTickClock() const {
  return controller_->GetClock();
}

TimeTicks SequenceManagerImpl::NowTicks() const {
  return controller_->GetClock()->NowTicks();
}

bool SequenceManagerImpl::ShouldRecordCPUTimeForTask() {
  return ThreadTicks::IsSupported() &&
         main_thread_only().uniform_distribution(
             main_thread_only().random_generator) <
             metric_recording_settings_
                 .task_sampling_rate_for_recording_cpu_time;
}

const SequenceManager::MetricRecordingSettings&
SequenceManagerImpl::GetMetricRecordingSettings() const {
  return metric_recording_settings_;
}

NOINLINE bool SequenceManagerImpl::Validate() {
  return memory_corruption_sentinel_ == kMemoryCorruptionSentinelValue;
}

void SequenceManagerImpl::EnableCrashKeys(
    const char* file_name_crash_key_name,
    const char* function_name_crash_key_name) {
  DCHECK(!main_thread_only().file_name_crash_key);
  DCHECK(!main_thread_only().function_name_crash_key);
#if !defined(OS_NACL)
  main_thread_only().file_name_crash_key = debug::AllocateCrashKeyString(
      file_name_crash_key_name, debug::CrashKeySize::Size64);
  main_thread_only().function_name_crash_key = debug::AllocateCrashKeyString(
      function_name_crash_key_name, debug::CrashKeySize::Size64);
#endif  // OS_NACL
}

internal::TaskQueueImpl* SequenceManagerImpl::currently_executing_task_queue()
    const {
  if (main_thread_only().task_execution_stack.empty())
    return nullptr;
  return main_thread_only().task_execution_stack.rbegin()->task_queue;
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
