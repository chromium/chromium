// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/delayed_task_manager.h"

#include <algorithm>
#include <optional>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/task/common/checked_lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_features.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool/task.h"

namespace base {
namespace internal {

DelayedTaskManager::DelayedTask::DelayedTask() = default;

DelayedTaskManager::DelayedTask::DelayedTask(Task task,
                                             PostTaskNowCallback callback)
    : task(std::move(task)), callback(std::move(callback)) {}

DelayedTaskManager::DelayedTask::DelayedTask(
    DelayedTaskManager::DelayedTask&& other) = default;

DelayedTaskManager::DelayedTask::~DelayedTask() = default;

DelayedTaskManager::DelayedTask& DelayedTaskManager::DelayedTask::operator=(
    DelayedTaskManager::DelayedTask&& other) = default;

bool DelayedTaskManager::DelayedTask::operator>(
    const DelayedTask& other) const {
  TimeTicks latest_delayed_run_time = task.latest_delayed_run_time();
  TimeTicks other_latest_delayed_run_time =
      other.task.latest_delayed_run_time();
  return std::tie(latest_delayed_run_time, task.sequence_num) >
         std::tie(other_latest_delayed_run_time, other.task.sequence_num);
}

DelayedTaskManager::DelayedTaskManager(const TickClock* tick_clock)
    : process_ripe_tasks_closure_(
          BindRepeating(&DelayedTaskManager::ProcessRipeTasks,
                        Unretained(this))),
      schedule_process_ripe_tasks_closure_(BindRepeating(
          &DelayedTaskManager::ScheduleProcessRipeTasksOnServiceThread,
          Unretained(this))),
      tick_clock_(tick_clock) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(tick_clock_);
}

DelayedTaskManager::~DelayedTaskManager() {
  delayed_task_handle_.CancelTask();
}

void DelayedTaskManager::Start(
    scoped_refptr<SequencedTaskRunner> service_thread_task_runner) {
  DCHECK(service_thread_task_runner);

  TimeTicks process_ripe_tasks_time;
  subtle::DelayPolicy delay_policy;
  {
    CheckedAutoLock auto_lock(queue_lock_);
    DCHECK(!service_thread_task_runner_);
    service_thread_task_runner_ = std::move(service_thread_task_runner);
    max_precise_delay = kMaxPreciseDelay.Get();
    std::tie(process_ripe_tasks_time, delay_policy) =
        GetTimeAndDelayPolicyToScheduleProcessRipeTasksLockRequired();
  }
  if (!process_ripe_tasks_time.is_max()) {
    service_thread_task_runner_->PostTask(FROM_HERE,
                                          schedule_process_ripe_tasks_closure_);
  }
}

void DelayedTaskManager::AddDelayedTask(
    Task task,
    PostTaskNowCallback post_task_now_callback) {
  DCHECK(task.task);
  DCHECK(!task.delayed_run_time.is_null());
  DCHECK(!task.queue_time.is_null());

  // Use CHECK instead of DCHECK to crash earlier. See http://crbug.com/711167
  // for details.
  CHECK(task.task);
  TimeTicks process_ripe_tasks_time;
  subtle::DelayPolicy delay_policy;
  {
    CheckedAutoLock auto_lock(queue_lock_);
    task.delay_policy = subtle::MaybeOverrideDelayPolicy(
        task.delay_policy, task.delayed_run_time - task.queue_time,
        max_precise_delay);

    auto [old_process_ripe_tasks_time, old_delay_policy] =
        GetTimeAndDelayPolicyToScheduleProcessRipeTasksLockRequired();
    delayed_task_queue_.insert(
        DelayedTask(std::move(task), std::move(post_task_now_callback)));
    // Not started or already shutdown.
    if (service_thread_task_runner_ == nullptr)
      return;

    std::tie(process_ripe_tasks_time, delay_policy) =
        GetTimeAndDelayPolicyToScheduleProcessRipeTasksLockRequired();
    // The next invocation of ProcessRipeTasks() doesn't need to change.
    if (old_process_ripe_tasks_time == process_ripe_tasks_time &&
        old_delay_policy == delay_policy) {
      return;
    }
  }
  if (!process_ripe_tasks_time.is_max()) {
    service_thread_task_runner_->PostTask(FROM_HERE,
                                          schedule_process_ripe_tasks_closure_);
  }
}

void DelayedTaskManager::ProcessRipeTasks() {
  std::vector<DelayedTask> ripe_delayed_tasks;
  TimeTicks process_ripe_tasks_time;

  {
    CheckedAutoLock auto_lock(queue_lock_);

    // Already shutdown.
    if (!service_thread_task_runner_)
      return;

    const TimeTicks now = tick_clock_->NowTicks();
    // A delayed task is ripe if it reached its delayed run time or if it is
    // canceled. If it is canceled, schedule its deletion on the correct
    // sequence now rather than in the future, to minimize CPU wake ups and save
    // power.
    while (!delayed_task_queue_.empty() &&
           (delayed_task_queue_.top().task.earliest_delayed_run_time() <= now ||
            !delayed_task_queue_.top().task.task.MaybeValid())) {
      // The const_cast on top is okay since the DelayedTask is
      // transactionally being popped from |delayed_task_queue_| right after
      // and the move doesn't alter the sort order.
      ripe_delayed_tasks.push_back(
          std::move(const_cast<DelayedTask&>(delayed_task_queue_.top())));
      delayed_task_queue_.pop();
    }
    std::tie(process_ripe_tasks_time, std::ignore) =
        GetTimeAndDelayPolicyToScheduleProcessRipeTasksLockRequired();
  }
  if (!process_ripe_tasks_time.is_max()) {
    if (service_thread_task_runner_->RunsTasksInCurrentSequence()) {
      ScheduleProcessRipeTasksOnServiceThread();
    } else {
      // ProcessRipeTasks may be called on another thread under tests.
      service_thread_task_runner_->PostTask(
          FROM_HERE, schedule_process_ripe_tasks_closure_);
    }
  }

  for (auto& delayed_task : ripe_delayed_tasks) {
    std::move(delayed_task.callback).Run(std::move(delayed_task.task));
  }
}

std::optional<TimeTicks> DelayedTaskManager::NextScheduledRunTime() const {
  CheckedAutoLock auto_lock(queue_lock_);
  if (delayed_task_queue_.empty())
    return std::nullopt;
  return delayed_task_queue_.top().task.delayed_run_time;
}

subtle::DelayPolicy DelayedTaskManager::TopTaskDelayPolicyForTesting() const {
  CheckedAutoLock auto_lock(queue_lock_);
  return delayed_task_queue_.top().task.delay_policy;
}

void DelayedTaskManager::Shutdown() {
  scoped_refptr<SequencedTaskRunner> service_thread_task_runner;

  {
    CheckedAutoLock auto_lock(queue_lock_);
    // Prevent delayed tasks from being posted or processed after this.
    service_thread_task_runner = service_thread_task_runner_;
  }

  if (service_thread_task_runner) {
    // Cancel our delayed task on the service thread. This cannot be done from
    // ~DelayedTaskManager because the delayed task handle is sequence-affine.
    service_thread_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](DelayedTaskManager* manager) {
              DCHECK_CALLED_ON_VALID_SEQUENCE(manager->sequence_checker_);
              manager->delayed_task_handle_.CancelTask();
            },
            // Unretained() is safe because the caller must flush tasks posted
            // to the service thread before deleting `this`.
            Unretained(this)));
  }
}

std::pair<TimeTicks, subtle::DelayPolicy> DelayedTaskManager::
    GetTimeAndDelayPolicyToScheduleProcessRipeTasksLockRequired() {
  queue_lock_.AssertAcquired();
  if (delayed_task_queue_.empty()) {
    return std::make_pair(TimeTicks::Max(),
                          subtle::DelayPolicy::kFlexibleNoSooner);
  }

  const DelayedTask& ripest_delayed_task = delayed_task_queue_.top();
  subtle::DelayPolicy delay_policy = ripest_delayed_task.task.delay_policy;
  return std::make_pair(ripest_delayed_task.task.delayed_run_time,
                        delay_policy);
}

void DelayedTaskManager::ScheduleProcessRipeTasksOnServiceThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  TimeTicks process_ripe_tasks_time;
  subtle::DelayPolicy delay_policy;
  {
    CheckedAutoLock auto_lock(queue_lock_);
    std::tie(process_ripe_tasks_time, delay_policy) =
        GetTimeAndDelayPolicyToScheduleProcessRipeTasksLockRequired();
  }
  DCHECK(!process_ripe_tasks_time.is_null());
  if (process_ripe_tasks_time.is_max())
    return;
  delayed_task_handle_.CancelTask();
  delayed_task_handle_ =
      service_thread_task_runner_->PostCancelableDelayedTaskAt(
          subtle::PostDelayedTaskPassKey(), FROM_HERE,
          process_ripe_tasks_closure_, process_ripe_tasks_time, delay_policy);
}

}  // namespace internal
}  // namespace base
