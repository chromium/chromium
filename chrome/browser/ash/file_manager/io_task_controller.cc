// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/io_task_controller.h"

#include <vector>

#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "content/public/browser/device_service.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"

namespace file_manager {

namespace io_task {

constexpr auto kThrottleInterval = base::Milliseconds(200);

IOTaskController::IOTaskController() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

IOTaskController::~IOTaskController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IOTaskController::MaybeNotifyIOTaskObservers(
    const ProgressStatus& status) {
  auto last_update = tasks_last_update_[status.task_id];

  if (base::Time::Now() - last_update < kThrottleInterval) {
    return;
  }

  NotifyIOTaskObservers(status);
}

void IOTaskController::NotifyIOTaskObservers(const ProgressStatus& status) {
  for (IOTaskController::Observer& observer : observers_) {
    observer.OnIOTaskStatus(status);
  }
  tasks_last_update_[status.task_id] = base::Time::Now();
}

void IOTaskController::OnIOTaskProgress(const ProgressStatus& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeNotifyIOTaskObservers(status);
}

void IOTaskController::OnIOTaskComplete(IOTaskId task_id,
                                        ProgressStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NotifyIOTaskObservers(status);
  RemoveIOTask(task_id);
}

void IOTaskController::AddObserver(IOTaskController::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void IOTaskController::RemoveObserver(IOTaskController::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

IOTaskId IOTaskController::Add(std::unique_ptr<IOTask> task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  IOTaskId task_id = ++last_id_;
  task->SetTaskID(task_id);

  // Notify observers that the task has been queued.
  NotifyIOTaskObservers(task->progress());

  // Make sure the first "in progress" event after "queued" is always sent.
  // Some listeners require at least one in progress event.
  tasks_last_update_[task_id] -= kThrottleInterval;

  // TODO(b/199807189): Queue the task.
  PutIOTask(task_id, std::move(task))
      ->Execute(base::BindRepeating(&IOTaskController::OnIOTaskProgress,
                                    weak_ptr_factory_.GetWeakPtr()),
                base::BindPostTaskToCurrentDefault(
                    base::BindOnce(&IOTaskController::OnIOTaskComplete,
                                   weak_ptr_factory_.GetWeakPtr(), task_id)));
  return task_id;
}

void IOTaskController::Pause(IOTaskId task_id, PauseParams params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = tasks_.find(task_id);
  if (it != tasks_.end()) {
    IOTask* task = it->second.get();
    task->Pause(std::move(params));
    NotifyIOTaskObservers(task->progress());
  } else {
    LOG(WARNING) << "Failed to pause task: " << task_id << " not found";
  }
}

void IOTaskController::Resume(IOTaskId task_id, ResumeParams params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = tasks_.find(task_id);
  if (it != tasks_.end()) {
    IOTask* task = it->second.get();
    task->Resume(std::move(params));
    NotifyIOTaskObservers(task->progress());
  } else {
    LOG(WARNING) << "Failed to resume task: " << task_id << " not found";
  }
}

void IOTaskController::Cancel(IOTaskId task_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = tasks_.find(task_id);
  if (it != tasks_.end()) {
    IOTask* task = it->second.get();
    task->Cancel();
    NotifyIOTaskObservers(task->progress());
    RemoveIOTask(task_id);
  } else {
    LOG(WARNING) << "Failed to cancel task: " << task_id << " not found";
  }
}

void IOTaskController::ProgressPausedTasks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(b/255264604): TaskId order is potentially racey when multiple
  // files app windows open. Fix this: develop a concept of the current
  // PAUSED task in this code, and always progress that task.
  for (auto it = tasks_.begin(); it != tasks_.end(); ++it) {
    IOTask* task = it->second.get();
    if (task->progress().IsPaused()) {
      NotifyIOTaskObservers(task->progress());
      break;
    }
  }
}

void IOTaskController::CompleteWithError(IOTaskId task_id,
                                         PolicyError policy_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = tasks_.find(task_id);
  if (it != tasks_.end()) {
    IOTask* task = it->second.get();
    task->CompleteWithError(std::move(policy_error));
    NotifyIOTaskObservers(task->progress());
    RemoveIOTask(task_id);
  } else {
    LOG(WARNING) << "Failed to abort task: " << task_id << " not found";
  }
}

device::mojom::WakeLock* IOTaskController::GetWakeLock() {
  if (!wake_lock_) {
    mojo::Remote<device::mojom::WakeLockProvider> provider;
    content::GetDeviceService().BindWakeLockProvider(
        provider.BindNewPipeAndPassReceiver());
    provider->GetWakeLockWithoutContext(
        device::mojom::WakeLockType::kPreventDisplaySleep,
        device::mojom::WakeLockReason::kOther, "IOTask",
        wake_lock_.BindNewPipeAndPassReceiver());
  }
  return wake_lock_.get();
}

IOTask* IOTaskController::PutIOTask(const IOTaskId task_id,
                                    std::unique_ptr<IOTask> task) {
  // TODO(b/255264604): fix me: PAUSED tasks can hold the wake lock and
  // prevent the device from sleeping.
  if (tasks_.empty()) {
    GetWakeLock()->RequestWakeLock();
    ++wake_lock_counter_for_tests_;
  }

  IOTask* task_ptr = task.get();
  tasks_[task_id] = std::move(task);
  return task_ptr;
}

void IOTaskController::RemoveIOTask(const IOTaskId task_id) {
  tasks_last_update_.erase(task_id);
  tasks_.erase(task_id);

  // TODO(b/255264604): fix me: PAUSED tasks can hold the wake lock and
  // prevent the device from sleeping.
  if (tasks_.empty()) {
    GetWakeLock()->CancelWakeLock();
    --wake_lock_counter_for_tests_;
  }
}

std::vector<std::reference_wrapper<const ProgressStatus>>
IOTaskController::TaskStatuses() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<std::reference_wrapper<const ProgressStatus>> status_vector;
  for (auto& it : tasks_) {
    IOTask* task = it.second.get();
    status_vector.push_back(task->progress());
  }
  return status_vector;
}

}  // namespace io_task

}  // namespace file_manager
