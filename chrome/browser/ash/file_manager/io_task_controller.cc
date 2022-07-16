// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/io_task_controller.h"

#include "base/task/bind_post_task.h"

namespace file_manager {

namespace io_task {

IOTaskController::IOTaskController() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

IOTaskController::~IOTaskController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IOTaskController::NotifyIOTaskObservers(const ProgressStatus& status) {
  for (IOTaskController::Observer& observer : observers_) {
    observer.OnIOTaskStatus(status);
  }
}

void IOTaskController::OnIOTaskProgress(const ProgressStatus& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NotifyIOTaskObservers(status);
}

void IOTaskController::OnIOTaskComplete(IOTaskId task_id,
                                        ProgressStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NotifyIOTaskObservers(status);
  tasks_.erase(task_id);
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
  task->progress_.task_id = task_id;

  // Notify observers that the task has been queued.
  NotifyIOTaskObservers(task->progress());

  // TODO(b/199807189): Queue the task.
  task->Execute(base::BindRepeating(&IOTaskController::OnIOTaskProgress,
                                    weak_ptr_factory_.GetWeakPtr()),
                base::BindPostTask(
                    base::SequencedTaskRunnerHandle::Get(),
                    base::BindOnce(&IOTaskController::OnIOTaskComplete,
                                   weak_ptr_factory_.GetWeakPtr(), task_id)));
  tasks_[task_id] = std::move(task);
  return task_id;
}

void IOTaskController::Cancel(IOTaskId task_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = tasks_.find(task_id);
  if (it != tasks_.end()) {
    IOTask* task = it->second.get();
    task->Cancel();
    NotifyIOTaskObservers(task->progress());
    tasks_.erase(it);
  } else {
    LOG(WARNING) << "Failed to cancel task: " << task_id << " not found";
  }
}

}  // namespace io_task

}  // namespace file_manager
