// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/task_provider.h"

namespace task_manager {

TaskProvider::TaskProvider()
    : observer_(nullptr) {
}

TaskProvider::~TaskProvider() = default;

void TaskProvider::SetObserver(TaskProviderObserver* observer) {
  DCHECK(observer);
  DCHECK(!observer_);
  observer_ = observer;
  StartUpdating();
}

void TaskProvider::ClearObserver() {
  DCHECK(observer_);
  observer_ = nullptr;
  StopUpdating();
}

bool TaskProvider::IsUpdating() const {
  return observer_ != nullptr;
}

void TaskProvider::NotifyObserverTaskAdded(Task* task) const {
  DCHECK(observer_);
  observer_->TaskAdded(task);
}

void TaskProvider::NotifyObserverTaskRemoved(Task* task) const {
  DCHECK(observer_);
  observer_->TaskRemoved(task);
}

void TaskProvider::NotifyObserverTaskUnresponsive(Task* task) const {
  DCHECK(observer_);
  observer_->TaskUnresponsive(task);
}

void TaskProvider::UpdateTaskProcessInfoAndNotifyObserver(
    Task* existing_task,
    base::ProcessHandle new_process_handle,
    base::ProcessId new_process_id) const {
  DCHECK(observer_);
  existing_task->UpdateProcessInfo(new_process_handle, new_process_id,
                                   observer_);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void TaskProvider::NotifyObserverTaskIdsListToBeInvalidated() const {
  DCHECK(observer_);
  observer_->TaskIdsListToBeInvalidated();
}

void TaskProvider::NotifyObserverActiveTaskFetched(TaskId task_id) const {
  observer_->ActiveTaskFetched(task_id);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace task_manager
