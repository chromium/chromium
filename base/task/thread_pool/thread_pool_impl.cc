// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/thread_pool_impl.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/message_loop/message_pump_type.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/task/scoped_set_task_priority_for_current_thread.h"
#include "base/task/task_features.h"
#include "base/task/thread_pool/pooled_parallel_task_runner.h"
#include "base/task/thread_pool/pooled_sequenced_task_runner.h"
#include "base/task/thread_pool/task.h"
#include "base/task/thread_pool/task_source.h"
#include "base/task/thread_pool/task_source_sort_key.h"
#include "base/task/thread_pool/thread_group_impl.h"
#include "base/task/thread_pool/worker_thread.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if defined(OS_WIN)
#include "base/task/thread_pool/thread_group_native_win.h"
#endif

#if defined(OS_APPLE)
#include "base/task/thread_pool/thread_group_native_mac.h"
#endif

namespace base {
namespace internal {

namespace {

constexpr EnvironmentParams kForegroundPoolEnvironmentParams{
    "Foreground", base::ThreadPriority::NORMAL};

constexpr EnvironmentParams kBackgroundPoolEnvironmentParams{
    "Background", base::ThreadPriority::BACKGROUND};

constexpr int kMaxBestEffortTasks = 2;

// Indicates whether BEST_EFFORT tasks are disabled by a command line switch.
bool HasDisableBestEffortTasksSwitch() {
  // The CommandLine might not be initialized if ThreadPool is initialized in a
  // dynamic library which doesn't have access to argc/argv.
  return CommandLine::InitializedForCurrentProcess() &&
         CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kDisableBestEffortTasks);
}

// A global variable that can be set from test fixtures while no
// ThreadPoolInstance is active. Global instead of being a member variable to
// avoid having to add a public API to ThreadPoolInstance::InitParams for this
// internal edge case.
bool g_synchronous_thread_start_for_testing = false;

// Verifies that |traits| do not have properties that are banned in ThreadPool.
void AssertNoExtensionInTraits(const base::TaskTraits& traits) {
  DCHECK_EQ(traits.extension_id(),
            TaskTraitsExtensionStorage::kInvalidExtensionId);
}

}  // namespace

ThreadPoolImpl::ThreadPoolImpl(StringPiece histogram_label)
    : ThreadPoolImpl(histogram_label, std::make_unique<TaskTrackerImpl>()) {}

ThreadPoolImpl::ThreadPoolImpl(StringPiece histogram_label,
                               std::unique_ptr<TaskTrackerImpl> task_tracker)
    : task_tracker_(std::move(task_tracker)),
      single_thread_task_runner_manager_(task_tracker_->GetTrackedRef(),
                                         &delayed_task_manager_),
      has_disable_best_effort_switch_(HasDisableBestEffortTasksSwitch()),
      tracked_ref_factory_(this) {
  foreground_thread_group_ = std::make_unique<ThreadGroupImpl>(
      histogram_label.empty()
          ? std::string()
          : JoinString(
                {histogram_label, kForegroundPoolEnvironmentParams.name_suffix},
                "."),
      kForegroundPoolEnvironmentParams.name_suffix,
      kForegroundPoolEnvironmentParams.priority_hint,
      task_tracker_->GetTrackedRef(), tracked_ref_factory_.GetTrackedRef());

  if (CanUseBackgroundPriorityForWorkerThread()) {
    background_thread_group_ = std::make_unique<ThreadGroupImpl>(
        histogram_label.empty()
            ? std::string()
            : JoinString({histogram_label,
                          kBackgroundPoolEnvironmentParams.name_suffix},
                         "."),
        kBackgroundPoolEnvironmentParams.name_suffix,
        kBackgroundPoolEnvironmentParams.priority_hint,
        task_tracker_->GetTrackedRef(), tracked_ref_factory_.GetTrackedRef());
  }
}

ThreadPoolImpl::~ThreadPoolImpl() {
#if DCHECK_IS_ON()
  DCHECK(join_for_testing_returned_.IsSet());
#endif

  // Reset thread groups to release held TrackedRefs, which block teardown.
  foreground_thread_group_.reset();
  background_thread_group_.reset();
}

void ThreadPoolImpl::Start(const ThreadPoolInstance::InitParams& init_params,
                           WorkerThreadObserver* worker_thread_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!started_);

  internal::InitializeThreadPrioritiesFeature();

  disable_job_yield_ = FeatureList::IsEnabled(kDisableJobYield);
  disable_fair_scheduling_ = FeatureList::IsEnabled(kDisableFairJobScheduling);
  disable_job_update_priority_ =
      FeatureList::IsEnabled(kDisableJobUpdatePriority);

  // The max number of concurrent BEST_EFFORT tasks is |kMaxBestEffortTasks|,
  // unless the max number of foreground threads is lower.
  const int max_best_effort_tasks =
      std::min(kMaxBestEffortTasks, init_params.max_num_foreground_threads);

#if HAS_NATIVE_THREAD_POOL()
  if (FeatureList::IsEnabled(kUseNativeThreadPool)) {
    std::unique_ptr<ThreadGroup> old_group =
        std::move(foreground_thread_group_);
    foreground_thread_group_ = std::make_unique<ThreadGroupNativeImpl>(
#if defined(OS_APPLE)
        ThreadPriority::NORMAL,
#endif
        task_tracker_->GetTrackedRef(), tracked_ref_factory_.GetTrackedRef(),
        old_group.get());
    old_group->InvalidateAndHandoffAllTaskSourcesToOtherThreadGroup(
        foreground_thread_group_.get());
  }

  if (FeatureList::IsEnabled(kUseBackgroundNativeThreadPool)) {
    std::unique_ptr<ThreadGroup> old_group =
        std::move(background_thread_group_);
    background_thread_group_ = std::make_unique<ThreadGroupNativeImpl>(
#if defined(OS_APPLE)
        ThreadPriority::BACKGROUND,
#endif
        task_tracker_->GetTrackedRef(), tracked_ref_factory_.GetTrackedRef(),
        old_group.get());
    old_group->InvalidateAndHandoffAllTaskSourcesToOtherThreadGroup(
        background_thread_group_.get());
  }
#endif

  // Start the service thread. On platforms that support it (POSIX except NaCL
  // SFI), the service thread runs a MessageLoopForIO which is used to support
  // FileDescriptorWatcher in the scope in which tasks run.
  ServiceThread::Options service_thread_options;
  service_thread_options.message_pump_type =
#if defined(OS_POSIX) && !defined(OS_NACL_SFI)
      MessagePumpType::IO;
#else
      MessagePumpType::DEFAULT;
#endif
  service_thread_options.timer_slack = TIMER_SLACK_MAXIMUM;
  CHECK(service_thread_.StartWithOptions(std::move(service_thread_options)));
  if (g_synchronous_thread_start_for_testing)
    service_thread_.WaitUntilThreadStarted();

#if defined(OS_POSIX) && !defined(OS_NACL_SFI)
  // Needs to happen after starting the service thread to get its
  // task_runner().
  task_tracker_->set_io_thread_task_runner(service_thread_.task_runner());
#endif  // defined(OS_POSIX) && !defined(OS_NACL_SFI)

  // Update the CanRunPolicy based on |has_disable_best_effort_switch_|.
  UpdateCanRunPolicy();

  // Needs to happen after starting the service thread to get its task_runner().
  auto service_thread_task_runner = service_thread_.task_runner();
  delayed_task_manager_.Start(service_thread_task_runner);

  single_thread_task_runner_manager_.Start(worker_thread_observer);

  ThreadGroup::WorkerEnvironment worker_environment;
  switch (init_params.common_thread_pool_environment) {
    case InitParams::CommonThreadPoolEnvironment::DEFAULT:
      worker_environment = ThreadGroup::WorkerEnvironment::NONE;
      break;
#if defined(OS_WIN)
    case InitParams::CommonThreadPoolEnvironment::COM_MTA:
      worker_environment = ThreadGroup::WorkerEnvironment::COM_MTA;
      break;
    case InitParams::CommonThreadPoolEnvironment::
        DEPRECATED_COM_STA_IN_FOREGROUND_GROUP:
      worker_environment = ThreadGroup::WorkerEnvironment::COM_STA;
      break;
#endif
  }

  const base::TimeDelta suggested_reclaim_time =
      FeatureList::IsEnabled(kUseFiveMinutesThreadReclaimTime)
          ? base::TimeDelta::FromMinutes(5)
          : init_params.suggested_reclaim_time;

#if HAS_NATIVE_THREAD_POOL()
  if (FeatureList::IsEnabled(kUseNativeThreadPool)) {
    static_cast<ThreadGroupNative*>(foreground_thread_group_.get())
        ->Start(worker_environment);
  } else
#endif
  {
    // On platforms that can't use the background thread priority, best-effort
    // tasks run in foreground pools. A cap is set on the number of best-effort
    // tasks that can run in foreground pools to ensure that there is always
    // room for incoming foreground tasks and to minimize the performance impact
    // of best-effort tasks.
    static_cast<ThreadGroupImpl*>(foreground_thread_group_.get())
        ->Start(init_params.max_num_foreground_threads, max_best_effort_tasks,
                suggested_reclaim_time, service_thread_task_runner,
                worker_thread_observer, worker_environment,
                g_synchronous_thread_start_for_testing);
  }

  if (background_thread_group_) {
#if HAS_NATIVE_THREAD_POOL()
    if (FeatureList::IsEnabled(kUseBackgroundNativeThreadPool)) {
      static_cast<ThreadGroupNative*>(background_thread_group_.get())
          ->Start(worker_environment);
    } else
#endif
    {
      static_cast<ThreadGroupImpl*>(background_thread_group_.get())
          ->Start(max_best_effort_tasks, max_best_effort_tasks,
                  suggested_reclaim_time, service_thread_task_runner,
                  worker_thread_observer,
#if defined(OS_WIN)
                  // COM STA is a backward-compatibility feature for the
                  // foreground thread group only.
                  worker_environment == ThreadGroup::WorkerEnvironment::COM_STA
                      ? ThreadGroup::WorkerEnvironment::NONE
                      :
#endif
                      worker_environment,
                  g_synchronous_thread_start_for_testing);
    }
  }

  started_ = true;
}

bool ThreadPoolImpl::PostDelayedTask(const Location& from_here,
                                     const TaskTraits& traits,
                                     OnceClosure task,
                                     TimeDelta delay) {
  AssertNoExtensionInTraits(traits);
  // Post |task| as part of a one-off single-task Sequence.
  return PostTaskWithSequence(
      Task(from_here, std::move(task), TimeTicks::Now(), delay),
      MakeRefCounted<Sequence>(traits, nullptr,
                               TaskSourceExecutionMode::kParallel));
}

scoped_refptr<TaskRunner> ThreadPoolImpl::CreateTaskRunner(
    const TaskTraits& traits) {
  AssertNoExtensionInTraits(traits);
  return MakeRefCounted<PooledParallelTaskRunner>(traits, this);
}

scoped_refptr<SequencedTaskRunner> ThreadPoolImpl::CreateSequencedTaskRunner(
    const TaskTraits& traits) {
  AssertNoExtensionInTraits(traits);
  return MakeRefCounted<PooledSequencedTaskRunner>(traits, this);
}

scoped_refptr<SingleThreadTaskRunner>
ThreadPoolImpl::CreateSingleThreadTaskRunner(
    const TaskTraits& traits,
    SingleThreadTaskRunnerThreadMode thread_mode) {
  AssertNoExtensionInTraits(traits);
  return single_thread_task_runner_manager_.CreateSingleThreadTaskRunner(
      traits, thread_mode);
}

#if defined(OS_WIN)
scoped_refptr<SingleThreadTaskRunner> ThreadPoolImpl::CreateCOMSTATaskRunner(
    const TaskTraits& traits,
    SingleThreadTaskRunnerThreadMode thread_mode) {
  AssertNoExtensionInTraits(traits);
  return single_thread_task_runner_manager_.CreateCOMSTATaskRunner(traits,
                                                                   thread_mode);
}
#endif  // defined(OS_WIN)

scoped_refptr<UpdateableSequencedTaskRunner>
ThreadPoolImpl::CreateUpdateableSequencedTaskRunner(const TaskTraits& traits) {
  AssertNoExtensionInTraits(traits);
  return MakeRefCounted<PooledSequencedTaskRunner>(traits, this);
}

absl::optional<TimeTicks> ThreadPoolImpl::NextScheduledRunTimeForTesting()
    const {
  if (task_tracker_->HasIncompleteTaskSourcesForTesting())
    return TimeTicks::Now();
  return delayed_task_manager_.NextScheduledRunTime();
}

void ThreadPoolImpl::ProcessRipeDelayedTasksForTesting() {
  delayed_task_manager_.ProcessRipeTasks();
}

// static
void ThreadPoolImpl::SetSynchronousThreadStartForTesting(bool enabled) {
  DCHECK(!ThreadPoolInstance::Get());
  g_synchronous_thread_start_for_testing = enabled;
}

int ThreadPoolImpl::GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated(
    const TaskTraits& traits) const {
  // This method does not support getting the maximum number of BEST_EFFORT
  // tasks that can run concurrently in a pool.
  DCHECK_NE(traits.priority(), TaskPriority::BEST_EFFORT);
  return GetThreadGroupForTraits(traits)
      ->GetMaxConcurrentNonBlockedTasksDeprecated();
}

void ThreadPoolImpl::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Stop() the ServiceThread before triggering shutdown. This ensures that no
  // more delayed tasks or file descriptor watches will trigger during shutdown
  // (preventing http://crbug.com/698140). None of these asynchronous tasks
  // being guaranteed to happen anyways, stopping right away is valid behavior
  // and avoids the more complex alternative of shutting down the service thread
  // atomically during TaskTracker shutdown.
  service_thread_.Stop();

  task_tracker_->StartShutdown();

  // Allow all tasks to run. Done after initiating shutdown to ensure that non-
  // BLOCK_SHUTDOWN tasks don't get a chance to run and that BLOCK_SHUTDOWN
  // tasks run with a normal thread priority.
  UpdateCanRunPolicy();

  task_tracker_->CompleteShutdown();
}

void ThreadPoolImpl::FlushForTesting() {
  task_tracker_->FlushForTesting();
}

void ThreadPoolImpl::FlushAsyncForTesting(OnceClosure flush_callback) {
  task_tracker_->FlushAsyncForTesting(std::move(flush_callback));
}

void ThreadPoolImpl::JoinForTesting() {
#if DCHECK_IS_ON()
  DCHECK(!join_for_testing_returned_.IsSet());
#endif
  // The service thread must be stopped before the workers are joined, otherwise
  // tasks scheduled by the DelayedTaskManager might be posted between joining
  // those workers and stopping the service thread which will cause a CHECK. See
  // https://crbug.com/771701.
  service_thread_.Stop();
  single_thread_task_runner_manager_.JoinForTesting();
  foreground_thread_group_->JoinForTesting();
  if (background_thread_group_)
    background_thread_group_->JoinForTesting();
#if DCHECK_IS_ON()
  join_for_testing_returned_.Set();
#endif
}

void ThreadPoolImpl::BeginFence() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++num_fences_;
  UpdateCanRunPolicy();
}

void ThreadPoolImpl::EndFence() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(num_fences_, 0);
  --num_fences_;
  UpdateCanRunPolicy();
}

void ThreadPoolImpl::BeginBestEffortFence() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++num_best_effort_fences_;
  UpdateCanRunPolicy();
}

void ThreadPoolImpl::EndBestEffortFence() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(num_best_effort_fences_, 0);
  --num_best_effort_fences_;
  UpdateCanRunPolicy();
}

bool ThreadPoolImpl::PostTaskWithSequenceNow(Task task,
                                             scoped_refptr<Sequence> sequence) {
  auto transaction = sequence->BeginTransaction();
  const bool sequence_should_be_queued = transaction.WillPushTask();
  RegisteredTaskSource task_source;
  if (sequence_should_be_queued) {
    task_source = task_tracker_->RegisterTaskSource(sequence);
    // We shouldn't push |task| if we're not allowed to queue |task_source|.
    if (!task_source)
      return false;
  }
  if (!task_tracker_->WillPostTaskNow(task, transaction.traits().priority()))
    return false;
  transaction.PushTask(std::move(task));
  if (task_source) {
    const TaskTraits traits = transaction.traits();
    GetThreadGroupForTraits(traits)->PushTaskSourceAndWakeUpWorkers(
        {std::move(task_source), std::move(transaction)});
  }
  return true;
}

bool ThreadPoolImpl::PostTaskWithSequence(Task task,
                                          scoped_refptr<Sequence> sequence) {
  // Use CHECK instead of DCHECK to crash earlier. See http://crbug.com/711167
  // for details.
  CHECK(task.task);
  DCHECK(sequence);

#if defined(OS_WIN)
  // Force reading |task.posted_from.file_name()| to produce a useful crash
  // report if the address is invalid. A crash report generated later when the
  // task is executed would not contain the PostTask stack.
  //
  // TODO(crbug.com/1224432): Remove after resolving the crash.
  DEBUG_ALIAS_FOR_CSTR(task_posted_from, task.posted_from.file_name(), 32);
#endif

  if (!task_tracker_->WillPostTask(&task, sequence->shutdown_behavior()))
    return false;

  if (task.delayed_run_time.is_null()) {
    return PostTaskWithSequenceNow(std::move(task), std::move(sequence));
  } else {
    // It's safe to take a ref on this pointer since the caller must have a ref
    // to the TaskRunner in order to post.
    scoped_refptr<TaskRunner> task_runner = sequence->task_runner();
    delayed_task_manager_.AddDelayedTask(
        std::move(task),
        BindOnce(
            [](scoped_refptr<Sequence> sequence,
               ThreadPoolImpl* thread_pool_impl, Task task) {
              thread_pool_impl->PostTaskWithSequenceNow(std::move(task),
                                                        std::move(sequence));
            },
            std::move(sequence), Unretained(this)),
        std::move(task_runner));
  }

  return true;
}

bool ThreadPoolImpl::ShouldYield(const TaskSource* task_source) {
  if (disable_job_yield_)
    return false;
  const TaskPriority priority = task_source->priority_racy();
  auto* const thread_group =
      GetThreadGroupForTraits({priority, task_source->thread_policy()});
  // A task whose priority changed and is now running in the wrong thread group
  // should yield so it's rescheduled in the right one.
  if (!thread_group->IsBoundToCurrentThread())
    return true;
  return GetThreadGroupForTraits({priority, task_source->thread_policy()})
      ->ShouldYield(task_source->GetSortKey(disable_fair_scheduling_));
}

bool ThreadPoolImpl::EnqueueJobTaskSource(
    scoped_refptr<JobTaskSource> task_source) {
  auto registered_task_source =
      task_tracker_->RegisterTaskSource(std::move(task_source));
  if (!registered_task_source)
    return false;
  auto transaction = registered_task_source->BeginTransaction();
  const TaskTraits traits = transaction.traits();
  GetThreadGroupForTraits(traits)->PushTaskSourceAndWakeUpWorkers(
      {std::move(registered_task_source), std::move(transaction)});
  return true;
}

void ThreadPoolImpl::RemoveJobTaskSource(
    scoped_refptr<JobTaskSource> task_source) {
  auto transaction = task_source->BeginTransaction();
  ThreadGroup* const current_thread_group =
      GetThreadGroupForTraits(transaction.traits());
  current_thread_group->RemoveTaskSource(*task_source);
}

void ThreadPoolImpl::UpdatePriority(scoped_refptr<TaskSource> task_source,
                                    TaskPriority priority) {
  auto transaction = task_source->BeginTransaction();

  if (transaction.traits().priority() == priority)
    return;

  if (transaction.traits().priority() == TaskPriority::BEST_EFFORT) {
    DCHECK(transaction.traits().thread_policy_set_explicitly())
        << "A ThreadPolicy must be specified in the TaskTraits of an "
           "UpdateableSequencedTaskRunner whose priority is increased from "
           "BEST_EFFORT. See ThreadPolicy documentation.";
  }

  ThreadGroup* const current_thread_group =
      GetThreadGroupForTraits(transaction.traits());
  transaction.UpdatePriority(priority);
  ThreadGroup* const new_thread_group =
      GetThreadGroupForTraits(transaction.traits());

  if (new_thread_group == current_thread_group) {
    // |task_source|'s position needs to be updated within its current thread
    // group.
    current_thread_group->UpdateSortKey(std::move(transaction));
  } else {
    // |task_source| is changing thread groups; remove it from its current
    // thread group and reenqueue it.
    auto registered_task_source =
        current_thread_group->RemoveTaskSource(*task_source);
    if (registered_task_source) {
      DCHECK(task_source);
      new_thread_group->PushTaskSourceAndWakeUpWorkers(
          {std::move(registered_task_source), std::move(transaction)});
    }
  }
}

void ThreadPoolImpl::UpdateJobPriority(scoped_refptr<TaskSource> task_source,
                                       TaskPriority priority) {
  if (disable_job_update_priority_)
    return;
  UpdatePriority(std::move(task_source), priority);
}

const ThreadGroup* ThreadPoolImpl::GetThreadGroupForTraits(
    const TaskTraits& traits) const {
  return const_cast<ThreadPoolImpl*>(this)->GetThreadGroupForTraits(traits);
}

ThreadGroup* ThreadPoolImpl::GetThreadGroupForTraits(const TaskTraits& traits) {
  if (traits.priority() == TaskPriority::BEST_EFFORT &&
      traits.thread_policy() == ThreadPolicy::PREFER_BACKGROUND &&
      background_thread_group_) {
    return background_thread_group_.get();
  }

  return foreground_thread_group_.get();
}

void ThreadPoolImpl::UpdateCanRunPolicy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CanRunPolicy can_run_policy;
  if ((num_fences_ == 0 && num_best_effort_fences_ == 0 &&
       !has_disable_best_effort_switch_) ||
      task_tracker_->HasShutdownStarted()) {
    can_run_policy = CanRunPolicy::kAll;
  } else if (num_fences_ != 0) {
    can_run_policy = CanRunPolicy::kNone;
  } else {
    DCHECK(num_best_effort_fences_ > 0 || has_disable_best_effort_switch_);
    can_run_policy = CanRunPolicy::kForegroundOnly;
  }

  task_tracker_->SetCanRunPolicy(can_run_policy);
  foreground_thread_group_->DidUpdateCanRunPolicy();
  if (background_thread_group_)
    background_thread_group_->DidUpdateCanRunPolicy();
  single_thread_task_runner_manager_.DidUpdateCanRunPolicy();
}

}  // namespace internal
}  // namespace base
