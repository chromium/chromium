// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/test_utils.h"

#include <utility>

#include "base/check.h"
#include "base/debug/leak_annotations.h"
#include "base/functional/bind.h"
#include "base/functional/overloaded.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/condition_variable.h"
#include "base/task/thread_pool/pooled_parallel_task_runner.h"
#include "base/task/thread_pool/pooled_sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/threading/scoped_blocking_call_internal.h"
#include "base/threading/thread_restrictions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace base {
namespace internal {
namespace test {

namespace {

// A task runner that posts each task as a MockJobTaskSource that runs a single
// task. This is used to run ThreadGroupTests which require a TaskRunner with
// kJob execution mode. Delayed tasks are not supported.
class MockJobTaskRunner : public TaskRunner {
 public:
  MockJobTaskRunner(const TaskTraits& traits,
                    PooledTaskRunnerDelegate* pooled_task_runner_delegate)
      : traits_(traits),
        pooled_task_runner_delegate_(pooled_task_runner_delegate) {}

  MockJobTaskRunner(const MockJobTaskRunner&) = delete;
  MockJobTaskRunner& operator=(const MockJobTaskRunner&) = delete;

  // TaskRunner:
  bool PostDelayedTask(const Location& from_here,
                       OnceClosure closure,
                       TimeDelta delay) override;

 private:
  ~MockJobTaskRunner() override = default;

  const TaskTraits traits_;
  const raw_ptr<PooledTaskRunnerDelegate> pooled_task_runner_delegate_;
};

bool MockJobTaskRunner::PostDelayedTask(const Location& from_here,
                                        OnceClosure closure,
                                        TimeDelta delay) {
  DCHECK_EQ(delay, TimeDelta());  // Jobs doesn't support delayed tasks.

  if (!PooledTaskRunnerDelegate::MatchesCurrentDelegate(
          pooled_task_runner_delegate_)) {
    return false;
  }

  auto job_task = base::MakeRefCounted<MockJobTask>(std::move(closure));
  scoped_refptr<JobTaskSource> task_source = job_task->GetJobTaskSource(
      from_here, traits_, pooled_task_runner_delegate_);
  return pooled_task_runner_delegate_->EnqueueJobTaskSource(
      std::move(task_source));
}

scoped_refptr<TaskRunner> CreateJobTaskRunner(
    const TaskTraits& traits,
    MockPooledTaskRunnerDelegate* mock_pooled_task_runner_delegate) {
  return MakeRefCounted<MockJobTaskRunner>(traits,
                                           mock_pooled_task_runner_delegate);
}

}  // namespace

MockWorkerThreadObserver::MockWorkerThreadObserver()
    : on_main_exit_cv_(lock_.CreateConditionVariable()) {}

MockWorkerThreadObserver::~MockWorkerThreadObserver() {
  WaitCallsOnMainExit();
}

void MockWorkerThreadObserver::AllowCallsOnMainExit(int num_calls) {
  CheckedAutoLock auto_lock(lock_);
  EXPECT_EQ(0, allowed_calls_on_main_exit_);
  allowed_calls_on_main_exit_ = num_calls;
}

void MockWorkerThreadObserver::WaitCallsOnMainExit() {
  CheckedAutoLock auto_lock(lock_);
  while (allowed_calls_on_main_exit_ != 0)
    on_main_exit_cv_.Wait();
}

void MockWorkerThreadObserver::OnWorkerThreadMainExit() {
  CheckedAutoLock auto_lock(lock_);
  EXPECT_GE(allowed_calls_on_main_exit_, 0);
  --allowed_calls_on_main_exit_;
  if (allowed_calls_on_main_exit_ == 0)
    on_main_exit_cv_.Signal();
}

scoped_refptr<Sequence> CreateSequenceWithTask(
    Task task,
    const TaskTraits& traits,
    scoped_refptr<SequencedTaskRunner> task_runner,
    TaskSourceExecutionMode execution_mode) {
  scoped_refptr<Sequence> sequence =
      MakeRefCounted<Sequence>(traits, task_runner.get(), execution_mode);
  auto transaction = sequence->BeginTransaction();
  transaction.WillPushImmediateTask();
  transaction.PushImmediateTask(std::move(task));
  return sequence;
}

scoped_refptr<TaskRunner> CreatePooledTaskRunnerWithExecutionMode(
    TaskSourceExecutionMode execution_mode,
    MockPooledTaskRunnerDelegate* mock_pooled_task_runner_delegate,
    const TaskTraits& traits) {
  switch (execution_mode) {
    case TaskSourceExecutionMode::kParallel:
      return CreatePooledTaskRunner(traits, mock_pooled_task_runner_delegate);
    case TaskSourceExecutionMode::kSequenced:
      return CreatePooledSequencedTaskRunner(traits,
                                             mock_pooled_task_runner_delegate);
    case TaskSourceExecutionMode::kJob:
      return CreateJobTaskRunner(traits, mock_pooled_task_runner_delegate);
    default:
      // Fall through.
      break;
  }
  ADD_FAILURE() << "Unexpected ExecutionMode";
  return nullptr;
}

scoped_refptr<TaskRunner> CreatePooledTaskRunner(
    const TaskTraits& traits,
    MockPooledTaskRunnerDelegate* mock_pooled_task_runner_delegate) {
  return MakeRefCounted<PooledParallelTaskRunner>(
      traits, mock_pooled_task_runner_delegate);
}

scoped_refptr<SequencedTaskRunner> CreatePooledSequencedTaskRunner(
    const TaskTraits& traits,
    MockPooledTaskRunnerDelegate* mock_pooled_task_runner_delegate) {
  return MakeRefCounted<PooledSequencedTaskRunner>(
      traits, mock_pooled_task_runner_delegate);
}

MockPooledTaskRunnerDelegate::MockPooledTaskRunnerDelegate(
    TrackedRef<TaskTracker> task_tracker,
    DelayedTaskManager* delayed_task_manager)
    : task_tracker_(task_tracker),
      delayed_task_manager_(delayed_task_manager) {}

MockPooledTaskRunnerDelegate::~MockPooledTaskRunnerDelegate() = default;

bool MockPooledTaskRunnerDelegate::PostTaskWithSequence(
    Task task,
    scoped_refptr<Sequence> sequence) {
  // |thread_group_| must be initialized with SetThreadGroup() before
  // proceeding.
  DCHECK(thread_group_);
  DCHECK(task.task);
  DCHECK(sequence);

  if (!task_tracker_->WillPostTask(&task, sequence->shutdown_behavior())) {
    // `task`'s destructor may run sequence-affine code, so it must be leaked
    // when `WillPostTask` returns false.
    auto leak = std::make_unique<Task>(std::move(task));
    ANNOTATE_LEAKING_OBJECT_PTR(leak.get());
    leak.release();
    return false;
  }

  if (task.delayed_run_time.is_null()) {
    PostTaskWithSequenceNow(std::move(task), std::move(sequence));
  } else {
    // It's safe to take a ref on this pointer since the caller must have a ref
    // to the TaskRunner in order to post.
    scoped_refptr<TaskRunner> task_runner = sequence->task_runner();
    delayed_task_manager_->AddDelayedTask(
        std::move(task),
        BindOnce(
            [](scoped_refptr<Sequence> sequence,
               MockPooledTaskRunnerDelegate* self,
               scoped_refptr<TaskRunner> task_runner, Task task) {
              self->PostTaskWithSequenceNow(std::move(task),
                                            std::move(sequence));
            },
            std::move(sequence), Unretained(this), std::move(task_runner)));
  }

  return true;
}

void MockPooledTaskRunnerDelegate::PostTaskWithSequenceNow(
    Task task,
    scoped_refptr<Sequence> sequence) {
  auto transaction = sequence->BeginTransaction();
  const bool sequence_should_be_queued = transaction.WillPushImmediateTask();
  RegisteredTaskSource task_source;
  if (sequence_should_be_queued) {
    task_source = task_tracker_->RegisterTaskSource(std::move(sequence));
    // We shouldn't push |task| if we're not allowed to queue |task_source|.
    if (!task_source)
      return;
  }
  transaction.PushImmediateTask(std::move(task));
  if (task_source) {
    thread_group_->PushTaskSourceAndWakeUpWorkers(
        {std::move(task_source), std::move(transaction)});
  }
}

bool MockPooledTaskRunnerDelegate::ShouldYield(const TaskSource* task_source) {
  return thread_group_->ShouldYield(task_source->GetSortKey());
}

bool MockPooledTaskRunnerDelegate::EnqueueJobTaskSource(
    scoped_refptr<JobTaskSource> task_source) {
  // |thread_group_| must be initialized with SetThreadGroup() before
  // proceeding.
  DCHECK(thread_group_);
  DCHECK(task_source);

  auto registered_task_source =
      task_tracker_->RegisterTaskSource(std::move(task_source));
  if (!registered_task_source)
    return false;
  auto transaction = registered_task_source->BeginTransaction();
  thread_group_->PushTaskSourceAndWakeUpWorkers(
      {std::move(registered_task_source), std::move(transaction)});
  return true;
}

void MockPooledTaskRunnerDelegate::RemoveJobTaskSource(
    scoped_refptr<JobTaskSource> task_source) {
  thread_group_->RemoveTaskSource(*task_source);
}

void MockPooledTaskRunnerDelegate::UpdatePriority(
    scoped_refptr<TaskSource> task_source,
    TaskPriority priority) {
  auto transaction = task_source->BeginTransaction();
  transaction.UpdatePriority(priority);
  thread_group_->UpdateSortKey(std::move(transaction));
}

void MockPooledTaskRunnerDelegate::UpdateJobPriority(
    scoped_refptr<TaskSource> task_source,
    TaskPriority priority) {
  UpdatePriority(std::move(task_source), priority);
}

void MockPooledTaskRunnerDelegate::SetThreadGroup(ThreadGroup* thread_group) {
  thread_group_ = thread_group;
}

MockJobTask::~MockJobTask() = default;

MockJobTask::MockJobTask(
    base::RepeatingCallback<void(JobDelegate*)> worker_task,
    size_t num_tasks_to_run)
    : task_(std::move(worker_task)),
      remaining_num_tasks_to_run_(num_tasks_to_run) {
  CHECK(!absl::get<decltype(worker_task)>(task_).is_null());
}

MockJobTask::MockJobTask(base::OnceClosure worker_task)
    : task_(std::move(worker_task)), remaining_num_tasks_to_run_(1) {
  CHECK(!absl::get<decltype(worker_task)>(task_).is_null());
}

void MockJobTask::SetNumTasksToRun(size_t num_tasks_to_run) {
  if (num_tasks_to_run == 0) {
    remaining_num_tasks_to_run_ = 0;
    return;
  }
  if (auto* closure = absl::get_if<base::OnceClosure>(&task_); closure) {
    // 0 is already handled above, so this can only be an attempt to set to
    // a non-zero value for a OnceClosure. In that case, the only permissible
    // value is 1, and the closure must not be null.
    //
    // Note that there is no need to check `!is_null()` for repeating callbacks,
    // since `Run(JobDelegate*)` never consumes the repeating callback variant.
    CHECK(!closure->is_null());
    CHECK_EQ(1u, num_tasks_to_run);
  }
  remaining_num_tasks_to_run_ = num_tasks_to_run;
}

size_t MockJobTask::GetMaxConcurrency(size_t /* worker_count */) const {
  return remaining_num_tasks_to_run_.load();
}

void MockJobTask::Run(JobDelegate* delegate) {
  absl::visit(
      base::Overloaded{
          [](OnceClosure& closure) { std::move(closure).Run(); },
          [delegate](const RepeatingCallback<void(JobDelegate*)>& callback) {
            callback.Run(delegate);
          }},
      task_);
  CHECK_GT(remaining_num_tasks_to_run_.fetch_sub(1), 0u);
}

scoped_refptr<JobTaskSource> MockJobTask::GetJobTaskSource(
    const Location& from_here,
    const TaskTraits& traits,
    PooledTaskRunnerDelegate* delegate) {
  return MakeRefCounted<JobTaskSource>(
      from_here, traits, base::BindRepeating(&test::MockJobTask::Run, this),
      base::BindRepeating(&test::MockJobTask::GetMaxConcurrency, this),
      delegate);
}

RegisteredTaskSource QueueAndRunTaskSource(
    TaskTracker* task_tracker,
    scoped_refptr<TaskSource> task_source) {
  auto registered_task_source =
      task_tracker->RegisterTaskSource(std::move(task_source));
  EXPECT_TRUE(registered_task_source);
  EXPECT_NE(registered_task_source.WillRunTask(),
            TaskSource::RunStatus::kDisallowed);
  return task_tracker->RunAndPopNextTask(std::move(registered_task_source));
}

void ShutdownTaskTracker(TaskTracker* task_tracker) {
  task_tracker->StartShutdown();
  task_tracker->CompleteShutdown();
}

}  // namespace test
}  // namespace internal
}  // namespace base
