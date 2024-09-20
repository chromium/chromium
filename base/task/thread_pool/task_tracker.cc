// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/task_tracker.h"

#include <atomic>
#include <optional>
#include <string>
#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/sequence_token.h"
#include "base/strings/string_util.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/scoped_set_task_priority_for_current_thread.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/job_task_source.h"
#include "base/task/thread_pool/task_source.h"
#include "base/threading/sequence_local_storage_map.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"
#include "base/values.h"
#include "build/build_config.h"

namespace base {
namespace internal {

namespace {

#if BUILDFLAG(ENABLE_BASE_TRACING)
using perfetto::protos::pbzero::ChromeThreadPoolTask;
using perfetto::protos::pbzero::ChromeTrackEvent;
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

constexpr const char* kExecutionModeString[] = {"parallel", "sequenced",
                                                "single thread", "job"};
static_assert(
    std::size(kExecutionModeString) ==
        static_cast<size_t>(TaskSourceExecutionMode::kMax) + 1,
    "Array kExecutionModeString is out of sync with TaskSourceExecutionMode.");

bool HasLogBestEffortTasksSwitch() {
  // The CommandLine might not be initialized if ThreadPool is initialized in a
  // dynamic library which doesn't have access to argc/argv.
  return CommandLine::InitializedForCurrentProcess() &&
         CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kLogBestEffortTasks);
}

#if BUILDFLAG(ENABLE_BASE_TRACING)
ChromeThreadPoolTask::Priority TaskPriorityToProto(TaskPriority priority) {
  switch (priority) {
    case TaskPriority::BEST_EFFORT:
      return ChromeThreadPoolTask::PRIORITY_BEST_EFFORT;
    case TaskPriority::USER_VISIBLE:
      return ChromeThreadPoolTask::PRIORITY_USER_VISIBLE;
    case TaskPriority::USER_BLOCKING:
      return ChromeThreadPoolTask::PRIORITY_USER_BLOCKING;
  }
}

ChromeThreadPoolTask::ExecutionMode ExecutionModeToProto(
    TaskSourceExecutionMode mode) {
  switch (mode) {
    case TaskSourceExecutionMode::kParallel:
      return ChromeThreadPoolTask::EXECUTION_MODE_PARALLEL;
    case TaskSourceExecutionMode::kSequenced:
      return ChromeThreadPoolTask::EXECUTION_MODE_SEQUENCED;
    case TaskSourceExecutionMode::kSingleThread:
      return ChromeThreadPoolTask::EXECUTION_MODE_SINGLE_THREAD;
    case TaskSourceExecutionMode::kJob:
      return ChromeThreadPoolTask::EXECUTION_MODE_JOB;
  }
}

ChromeThreadPoolTask::ShutdownBehavior ShutdownBehaviorToProto(
    TaskShutdownBehavior shutdown_behavior) {
  switch (shutdown_behavior) {
    case TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN:
      return ChromeThreadPoolTask::SHUTDOWN_BEHAVIOR_CONTINUE_ON_SHUTDOWN;
    case TaskShutdownBehavior::SKIP_ON_SHUTDOWN:
      return ChromeThreadPoolTask::SHUTDOWN_BEHAVIOR_SKIP_ON_SHUTDOWN;
    case TaskShutdownBehavior::BLOCK_SHUTDOWN:
      return ChromeThreadPoolTask::SHUTDOWN_BEHAVIOR_BLOCK_SHUTDOWN;
  }
}
#endif  //  BUILDFLAG(ENABLE_BASE_TRACING)

auto EmitThreadPoolTraceEventMetadata(perfetto::EventContext& ctx,
                                      const TaskTraits& traits,
                                      TaskSource* task_source,
                                      const SequenceToken& token) {
#if BUILDFLAG(ENABLE_BASE_TRACING)
  // Other parameters are included only when "scheduler" category is enabled.
  const uint8_t* scheduler_category_enabled =
      TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED("scheduler");

  if (!*scheduler_category_enabled)
    return;
  auto* task = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                   ->set_thread_pool_task();
  task->set_task_priority(TaskPriorityToProto(traits.priority()));
  task->set_execution_mode(ExecutionModeToProto(task_source->execution_mode()));
  task->set_shutdown_behavior(
      ShutdownBehaviorToProto(traits.shutdown_behavior()));
  if (token.IsValid())
    task->set_sequence_token(token.ToInternalValue());
#endif  //  BUILDFLAG(ENABLE_BASE_TRACING)
}

// If this is greater than 0 on a given thread, it will ignore the DCHECK which
// prevents posting BLOCK_SHUTDOWN tasks after shutdown. There are cases where
// posting back to a BLOCK_SHUTDOWN sequence is a coincidence rather than part
// of a shutdown blocking series of tasks, this prevents racy DCHECKs in those
// cases.
constinit thread_local int fizzle_block_shutdown_tasks_ref = 0;

}  // namespace

// Atomic internal state used by TaskTracker to track items that are blocking
// Shutdown. An "item" consist of either:
// - A running SKIP_ON_SHUTDOWN task
// - A queued/running BLOCK_SHUTDOWN TaskSource.
// Sequential consistency shouldn't be assumed from these calls (i.e. a thread
// reading |HasShutdownStarted() == true| isn't guaranteed to see all writes
// made before |StartShutdown()| on the thread that invoked it).
class TaskTracker::State {
 public:
  State() = default;
  State(const State&) = delete;
  State& operator=(const State&) = delete;

  // Sets a flag indicating that shutdown has started. Returns true if there are
  // items blocking shutdown. Can only be called once.
  bool StartShutdown() {
    const auto new_value =
        subtle::NoBarrier_AtomicIncrement(&bits_, kShutdownHasStartedMask);

    // Check that the "shutdown has started" bit isn't zero. This would happen
    // if it was incremented twice.
    DCHECK(new_value & kShutdownHasStartedMask);

    const auto num_items_blocking_shutdown =
        new_value >> kNumItemsBlockingShutdownBitOffset;
    return num_items_blocking_shutdown != 0;
  }

  // Returns true if shutdown has started.
  bool HasShutdownStarted() const {
    return subtle::NoBarrier_Load(&bits_) & kShutdownHasStartedMask;
  }

  // Returns true if there are items blocking shutdown.
  bool AreItemsBlockingShutdown() const {
    const auto num_items_blocking_shutdown =
        subtle::NoBarrier_Load(&bits_) >> kNumItemsBlockingShutdownBitOffset;
    DCHECK_GE(num_items_blocking_shutdown, 0);
    return num_items_blocking_shutdown != 0;
  }

  // Increments the number of items blocking shutdown. Returns true if
  // shutdown has started.
  bool IncrementNumItemsBlockingShutdown() {
#if DCHECK_IS_ON()
    // Verify that no overflow will occur.
    const auto num_items_blocking_shutdown =
        subtle::NoBarrier_Load(&bits_) >> kNumItemsBlockingShutdownBitOffset;
    DCHECK_LT(num_items_blocking_shutdown,
              std::numeric_limits<subtle::Atomic32>::max() -
                  kNumItemsBlockingShutdownIncrement);
#endif

    const auto new_bits = subtle::NoBarrier_AtomicIncrement(
        &bits_, kNumItemsBlockingShutdownIncrement);
    return new_bits & kShutdownHasStartedMask;
  }

  // Decrements the number of items blocking shutdown. Returns true if shutdown
  // has started and the number of tasks blocking shutdown becomes zero.
  bool DecrementNumItemsBlockingShutdown() {
    const auto new_bits = subtle::NoBarrier_AtomicIncrement(
        &bits_, -kNumItemsBlockingShutdownIncrement);
    const bool shutdown_has_started = new_bits & kShutdownHasStartedMask;
    const auto num_items_blocking_shutdown =
        new_bits >> kNumItemsBlockingShutdownBitOffset;
    DCHECK_GE(num_items_blocking_shutdown, 0);
    return shutdown_has_started && num_items_blocking_shutdown == 0;
  }

 private:
  static constexpr subtle::Atomic32 kShutdownHasStartedMask = 1;
  static constexpr subtle::Atomic32 kNumItemsBlockingShutdownBitOffset = 1;
  static constexpr subtle::Atomic32 kNumItemsBlockingShutdownIncrement =
      1 << kNumItemsBlockingShutdownBitOffset;

  // The LSB indicates whether shutdown has started. The other bits count the
  // number of items blocking shutdown.
  // No barriers are required to read/write |bits_| as this class is only used
  // as an atomic state checker, it doesn't provide sequential consistency
  // guarantees w.r.t. external state. Sequencing of the TaskTracker::State
  // operations themselves is guaranteed by the AtomicIncrement RMW (read-
  // modify-write) semantics however. For example, if two threads are racing to
  // call IncrementNumItemsBlockingShutdown() and StartShutdown() respectively,
  // either the first thread will win and the StartShutdown() call will see the
  // blocking task or the second thread will win and
  // IncrementNumItemsBlockingShutdown() will know that shutdown has started.
  subtle::Atomic32 bits_ = 0;
};

TaskTracker::TaskTracker()
    : has_log_best_effort_tasks_switch_(HasLogBestEffortTasksSwitch()),
      state_(new State),
      can_run_policy_(CanRunPolicy::kAll),
      flush_cv_(flush_lock_.CreateConditionVariable()),
      shutdown_lock_(&flush_lock_),
      tracked_ref_factory_(this) {
  // |flush_cv_| is only waited upon in FlushForTesting(), avoid instantiating a
  // ScopedBlockingCallWithBaseSyncPrimitives from test threads intentionally
  // idling themselves to wait on the ThreadPool.
  flush_cv_.declare_only_used_while_idle();
}

TaskTracker::~TaskTracker() = default;

void TaskTracker::StartShutdown() {
  CheckedAutoLock auto_lock(shutdown_lock_);

  // This method can only be called once.
  DCHECK(!shutdown_event_);
  DCHECK(!state_->HasShutdownStarted());

  shutdown_event_.emplace();

  const bool tasks_are_blocking_shutdown = state_->StartShutdown();

  // From now, if a thread causes the number of tasks blocking shutdown to
  // become zero, it will call OnBlockingShutdownTasksComplete().

  if (!tasks_are_blocking_shutdown) {
    // If another thread posts a BLOCK_SHUTDOWN task at this moment, it will
    // block until this method releases |shutdown_lock_|. Then, it will fail
    // DCHECK(!shutdown_event_->IsSignaled()). This is the desired behavior
    // because posting a BLOCK_SHUTDOWN task after StartShutdown() when no
    // tasks are blocking shutdown isn't allowed.
    shutdown_event_->Signal();
    return;
  }
}

void TaskTracker::CompleteShutdown() {
  // It is safe to access |shutdown_event_| without holding |lock_| because the
  // pointer never changes after being set by StartShutdown(), which must
  // happen-before this.
  DCHECK(TS_UNCHECKED_READ(shutdown_event_));

  {
    base::ScopedAllowBaseSyncPrimitives allow_wait;
    // Allow tests to wait for and introduce logging about the shutdown tasks
    // before we block this thread.
    BeginCompleteShutdown(*TS_UNCHECKED_READ(shutdown_event_));
    // Now block the thread until all tasks are done.
    TS_UNCHECKED_READ(shutdown_event_)->Wait();
  }

  // Unblock FlushForTesting() and perform the FlushAsyncForTesting callback
  // when shutdown completes.
  {
    CheckedAutoLock auto_lock(flush_lock_);
    flush_cv_.Broadcast();
  }
  InvokeFlushCallbacksForTesting();
}

void TaskTracker::FlushForTesting() {
  AssertFlushForTestingAllowed();
  CheckedAutoLock auto_lock(flush_lock_);
  while (num_incomplete_task_sources_.load(std::memory_order_acquire) != 0 &&
         !IsShutdownComplete()) {
    flush_cv_.Wait();
  }
}

void TaskTracker::FlushAsyncForTesting(OnceClosure flush_callback) {
  DCHECK(flush_callback);
  {
    CheckedAutoLock auto_lock(flush_lock_);
    flush_callbacks_for_testing_.push_back(std::move(flush_callback));
  }

  if (num_incomplete_task_sources_.load(std::memory_order_acquire) == 0 ||
      IsShutdownComplete()) {
    InvokeFlushCallbacksForTesting();
  }
}

void TaskTracker::SetCanRunPolicy(CanRunPolicy can_run_policy) {
  can_run_policy_.store(can_run_policy);
}

void TaskTracker::WillEnqueueJob(JobTaskSource* task_source) {
  task_source->WillEnqueue(sequence_nums_.GetNext(), task_annotator_);
}

bool TaskTracker::WillPostTask(Task* task,
                               TaskShutdownBehavior shutdown_behavior) {
  DCHECK(task);
  DCHECK(task->task);

  task->sequence_num = sequence_nums_.GetNext();
  if (state_->HasShutdownStarted()) {
    // A non BLOCK_SHUTDOWN task is allowed to be posted iff shutdown hasn't
    // started and the task is not delayed.
    if (shutdown_behavior != TaskShutdownBehavior::BLOCK_SHUTDOWN ||
        !task->delayed_run_time.is_null() ||
        fizzle_block_shutdown_tasks_ref > 0) {
      return false;
    }

    // A BLOCK_SHUTDOWN task posted after shutdown has completed is an ordering
    // bug. This aims to catch those early. In some cases it's a racy
    // coincidence (i.e. posting back to a BLOCK_SHUTDOWN sequence from a task
    // that wasn't itself guaranteed to finish before shutdown), in those cases
    // a ScopedFizzleBlockShutdownTasks can bump
    // `fizzle_block_shutdown_tasks_ref` to bypass this DCHECK.
    CheckedAutoLock auto_lock(shutdown_lock_);
    DCHECK(shutdown_event_);
    DCHECK(!shutdown_event_->IsSignaled())
        << "posted_from: " << task->posted_from.ToString();
  }

  // TODO(scheduler-dev): Record the task traits here.
  task_annotator_.WillQueueTask("ThreadPool_PostTask", task);

  return true;
}

bool TaskTracker::WillPostTaskNow(const Task& task,
                                  TaskPriority priority) const {
  // Delayed tasks's TaskShutdownBehavior is implicitly capped at
  // SKIP_ON_SHUTDOWN. i.e. it cannot BLOCK_SHUTDOWN, TaskTracker will not wait
  // for a delayed task in a BLOCK_SHUTDOWN TaskSource and will also skip
  // delayed tasks that happen to become ripe during shutdown.
  if (!task.delayed_run_time.is_null() && state_->HasShutdownStarted())
    return false;

  if (has_log_best_effort_tasks_switch_ &&
      priority == TaskPriority::BEST_EFFORT) {
    // A TaskPriority::BEST_EFFORT task is being posted.
    LOG(INFO) << task.posted_from.ToString();
  }
  return true;
}

RegisteredTaskSource TaskTracker::RegisterTaskSource(
    scoped_refptr<TaskSource> task_source) {
  DCHECK(task_source);

  TaskShutdownBehavior shutdown_behavior = task_source->shutdown_behavior();
  if (!BeforeQueueTaskSource(shutdown_behavior))
    return nullptr;

  num_incomplete_task_sources_.fetch_add(1, std::memory_order_relaxed);
  return RegisteredTaskSource(std::move(task_source), this);
}

bool TaskTracker::CanRunPriority(TaskPriority priority) const {
  auto can_run_policy = can_run_policy_.load();

  if (can_run_policy == CanRunPolicy::kAll)
    return true;

  if (can_run_policy == CanRunPolicy::kForegroundOnly &&
      priority >= TaskPriority::USER_VISIBLE) {
    return true;
  }

  return false;
}

RegisteredTaskSource TaskTracker::RunAndPopNextTask(
    RegisteredTaskSource task_source) {
  DCHECK(task_source);

  const bool should_run_tasks = BeforeRunTask(task_source->shutdown_behavior());

  // Run the next task in |task_source|.
  std::optional<Task> task;
  TaskTraits traits;
  {
    auto transaction = task_source->BeginTransaction();
    task = should_run_tasks ? task_source.TakeTask(&transaction)
                            : task_source.Clear(&transaction);
    traits = transaction.traits();
  }

  if (task) {
    // Skip delayed tasks if shutdown started.
    if (!task->delayed_run_time.is_null() && state_->HasShutdownStarted())
      task->task = base::DoNothingWithBoundArgs(std::move(task->task));

    // Run the |task| (whether it's a worker task or the Clear() closure).
    RunTask(std::move(task.value()), task_source.get(), traits);
  }
  if (should_run_tasks)
    AfterRunTask(task_source->shutdown_behavior());

  const bool task_source_must_be_queued = task_source.DidProcessTask();
  // |task_source| should be reenqueued iff requested by DidProcessTask().
  if (task_source_must_be_queued)
    return task_source;
  return nullptr;
}

bool TaskTracker::HasShutdownStarted() const {
  return state_->HasShutdownStarted();
}

bool TaskTracker::IsShutdownComplete() const {
  CheckedAutoLock auto_lock(shutdown_lock_);
  return shutdown_event_ && shutdown_event_->IsSignaled();
}

void TaskTracker::BeginFizzlingBlockShutdownTasks() {
  ++fizzle_block_shutdown_tasks_ref;
}

void TaskTracker::EndFizzlingBlockShutdownTasks() {
  CHECK_GE(--fizzle_block_shutdown_tasks_ref, 0);
}

void TaskTracker::RunTask(Task task,
                          TaskSource* task_source,
                          const TaskTraits& traits) {
  DCHECK(task_source);

  const auto environment = task_source->GetExecutionEnvironment();

  struct BlockShutdownTaskFizzler {
    BlockShutdownTaskFizzler() {
      // Nothing outside RunTask should be bumping
      // `fizzle_block_shutdown_tasks_ref`.
      DCHECK_EQ(fizzle_block_shutdown_tasks_ref, 0);
      ++fizzle_block_shutdown_tasks_ref;
    }
    ~BlockShutdownTaskFizzler() {
      --fizzle_block_shutdown_tasks_ref;
      // The refs should be balanced after running the task.
      DCHECK_EQ(fizzle_block_shutdown_tasks_ref, 0);
    }
  };
  std::optional<ScopedDisallowSingleton> disallow_singleton;
  std::optional<ScopedDisallowBlocking> disallow_blocking;
  std::optional<ScopedDisallowBaseSyncPrimitives> disallow_sync_primitives;
  std::optional<BlockShutdownTaskFizzler> fizzle_block_shutdown_tasks;
  if (traits.shutdown_behavior() ==
      TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN) {
    disallow_singleton.emplace();
    fizzle_block_shutdown_tasks.emplace();
  }
  if (!traits.may_block())
    disallow_blocking.emplace();
  if (!traits.with_base_sync_primitives())
    disallow_sync_primitives.emplace();

  {
    DCHECK(environment.token.IsValid());
    TaskScope task_scope(environment.token,
                         /* is_thread_bound=*/task_source->execution_mode() ==
                             TaskSourceExecutionMode::kSingleThread);
    ScopedSetTaskPriorityForCurrentThread
        scoped_set_task_priority_for_current_thread(traits.priority());

    // Local storage map used if none is provided by |environment|.
    std::optional<SequenceLocalStorageMap> local_storage_map;
    if (!environment.sequence_local_storage)
      local_storage_map.emplace();

    ScopedSetSequenceLocalStorageMapForCurrentThread
        scoped_set_sequence_local_storage_map_for_current_thread(
            environment.sequence_local_storage
                ? environment.sequence_local_storage
                : &local_storage_map.value());

    // Set up TaskRunner CurrentDefaultHandle as expected for the scope of the
    // task.
    std::optional<SequencedTaskRunner::CurrentDefaultHandle>
        sequenced_task_runner_current_default_handle;
    std::optional<SingleThreadTaskRunner::CurrentDefaultHandle>
        single_thread_task_runner_current_default_handle;
    if (environment.sequenced_task_runner) {
      DCHECK_EQ(TaskSourceExecutionMode::kSequenced,
                task_source->execution_mode());
      sequenced_task_runner_current_default_handle.emplace(
          environment.sequenced_task_runner);
    } else if (environment.single_thread_task_runner) {
      DCHECK_EQ(TaskSourceExecutionMode::kSingleThread,
                task_source->execution_mode());
      single_thread_task_runner_current_default_handle.emplace(
          environment.single_thread_task_runner);
    } else {
      DCHECK_NE(TaskSourceExecutionMode::kSequenced,
                task_source->execution_mode());
      DCHECK_NE(TaskSourceExecutionMode::kSingleThread,
                task_source->execution_mode());
    }

    RunTaskWithShutdownBehavior(task, traits, task_source, environment.token);

    // Make sure the arguments bound to the callback are deleted within the
    // scope in which the callback runs.
    task.task = OnceClosure();
  }
}

void TaskTracker::BeginCompleteShutdown(base::WaitableEvent& shutdown_event) {
  // Do nothing in production, tests may override this.
}

bool TaskTracker::HasIncompleteTaskSourcesForTesting() const {
  return num_incomplete_task_sources_.load(std::memory_order_acquire) != 0;
}

bool TaskTracker::BeforeQueueTaskSource(
    TaskShutdownBehavior shutdown_behavior) {
  if (shutdown_behavior == TaskShutdownBehavior::BLOCK_SHUTDOWN) {
    // BLOCK_SHUTDOWN task sources block shutdown between the moment they are
    // queued and the moment their last task completes its execution.
    const bool shutdown_started = state_->IncrementNumItemsBlockingShutdown();

    if (shutdown_started) {
      // A BLOCK_SHUTDOWN task posted after shutdown has completed is an
      // ordering bug. This aims to catch those early.
      CheckedAutoLock auto_lock(shutdown_lock_);
      DCHECK(shutdown_event_);
      DCHECK(!shutdown_event_->IsSignaled());
    }

    return true;
  }

  // A non BLOCK_SHUTDOWN task is allowed to be posted iff shutdown hasn't
  // started.
  return !state_->HasShutdownStarted();
}

bool TaskTracker::BeforeRunTask(TaskShutdownBehavior shutdown_behavior) {
  switch (shutdown_behavior) {
    case TaskShutdownBehavior::BLOCK_SHUTDOWN: {
      // The number of tasks blocking shutdown has been incremented when the
      // task was posted.
      DCHECK(state_->AreItemsBlockingShutdown());

      // Trying to run a BLOCK_SHUTDOWN task after shutdown has completed is
      // unexpected as it either shouldn't have been posted if shutdown
      // completed or should be blocking shutdown if it was posted before it
      // did.
      DCHECK(!state_->HasShutdownStarted() || !IsShutdownComplete());

      return true;
    }

    case TaskShutdownBehavior::SKIP_ON_SHUTDOWN: {
      // SKIP_ON_SHUTDOWN tasks block shutdown while they are running.
      const bool shutdown_started = state_->IncrementNumItemsBlockingShutdown();

      if (shutdown_started) {
        // The SKIP_ON_SHUTDOWN task isn't allowed to run during shutdown.
        // Decrement the number of tasks blocking shutdown that was wrongly
        // incremented.
        DecrementNumItemsBlockingShutdown();
        return false;
      }

      return true;
    }

    case TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN: {
      return !state_->HasShutdownStarted();
    }
  }

  NOTREACHED();
}

void TaskTracker::AfterRunTask(TaskShutdownBehavior shutdown_behavior) {
  if (shutdown_behavior == TaskShutdownBehavior::SKIP_ON_SHUTDOWN) {
    DecrementNumItemsBlockingShutdown();
  }
}

scoped_refptr<TaskSource> TaskTracker::UnregisterTaskSource(
    scoped_refptr<TaskSource> task_source) {
  DCHECK(task_source);
  if (task_source->shutdown_behavior() ==
      TaskShutdownBehavior::BLOCK_SHUTDOWN) {
    DecrementNumItemsBlockingShutdown();
  }
  DecrementNumIncompleteTaskSources();
  return task_source;
}

void TaskTracker::DecrementNumItemsBlockingShutdown() {
  const bool shutdown_started_and_no_items_block_shutdown =
      state_->DecrementNumItemsBlockingShutdown();
  if (!shutdown_started_and_no_items_block_shutdown)
    return;

  CheckedAutoLock auto_lock(shutdown_lock_);
  DCHECK(shutdown_event_);
  shutdown_event_->Signal();
}

void TaskTracker::DecrementNumIncompleteTaskSources() {
  const auto prev_num_incomplete_task_sources =
      num_incomplete_task_sources_.fetch_sub(1);
  DCHECK_GE(prev_num_incomplete_task_sources, 1);
  if (prev_num_incomplete_task_sources == 1) {
    {
      CheckedAutoLock auto_lock(flush_lock_);
      flush_cv_.Broadcast();
    }
    InvokeFlushCallbacksForTesting();
  }
}

void TaskTracker::InvokeFlushCallbacksForTesting() {
  base::circular_deque<OnceClosure> flush_callbacks;
  {
    CheckedAutoLock auto_lock(flush_lock_);
    flush_callbacks = std::move(flush_callbacks_for_testing_);
  }
  for (auto& flush_callback : flush_callbacks)
    std::move(flush_callback).Run();
}

NOINLINE void TaskTracker::RunContinueOnShutdown(Task& task,
                                                 const TaskTraits& traits,
                                                 TaskSource* task_source,
                                                 const SequenceToken& token) {
  NO_CODE_FOLDING();
  RunTaskImpl(task, traits, task_source, token);
}

NOINLINE void TaskTracker::RunSkipOnShutdown(Task& task,
                                             const TaskTraits& traits,
                                             TaskSource* task_source,
                                             const SequenceToken& token) {
  NO_CODE_FOLDING();
  RunTaskImpl(task, traits, task_source, token);
}

NOINLINE void TaskTracker::RunBlockShutdown(Task& task,
                                            const TaskTraits& traits,
                                            TaskSource* task_source,
                                            const SequenceToken& token) {
  NO_CODE_FOLDING();
  RunTaskImpl(task, traits, task_source, token);
}

void TaskTracker::RunTaskImpl(Task& task,
                              const TaskTraits& traits,
                              TaskSource* task_source,
                              const SequenceToken& token) {
  task_annotator_.RunTask(
      "ThreadPool_RunTask", task, [&](perfetto::EventContext& ctx) {
        EmitThreadPoolTraceEventMetadata(ctx, traits, task_source, token);
      });
}

void TaskTracker::RunTaskWithShutdownBehavior(Task& task,
                                              const TaskTraits& traits,
                                              TaskSource* task_source,
                                              const SequenceToken& token) {
  switch (traits.shutdown_behavior()) {
    case TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN:
      RunContinueOnShutdown(task, traits, task_source, token);
      return;
    case TaskShutdownBehavior::SKIP_ON_SHUTDOWN:
      RunSkipOnShutdown(task, traits, task_source, token);
      return;
    case TaskShutdownBehavior::BLOCK_SHUTDOWN:
      RunBlockShutdown(task, traits, task_source, token);
      return;
  }
}

}  // namespace internal
}  // namespace base
