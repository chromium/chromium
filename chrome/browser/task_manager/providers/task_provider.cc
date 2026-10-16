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
  CHECK(observer, base::NotFatalUntil::M162);
  CHECK(!observer_, base::NotFatalUntil::M162);
  observer_ = observer;
  StartUpdating();
}

void TaskProvider::ClearObserver() {
  CHECK(observer_, base::NotFatalUntil::M162);
  observer_ = nullptr;
  StopUpdating();
}

bool TaskProvider::IsUpdating() const {
  return observer_ != nullptr;
}

void TaskProvider::NotifyObserverTaskAdded(Task* task) const {
  CHECK(observer_, base::NotFatalUntil::M162);
  observer_->TaskAdded(task);
}

void TaskProvider::NotifyObserverTaskRemoved(Task* task) const {
  CHECK(observer_, base::NotFatalUntil::M162);
  observer_->TaskRemoved(task);
}

void TaskProvider::NotifyObserverTaskUnresponsive(Task* task) const {
  CHECK(observer_, base::NotFatalUntil::M162);
  observer_->TaskUnresponsive(task);
}

void TaskProvider::UpdateTaskProcessInfoAndNotifyObserver(
    Task* existing_task,
    base::ProcessHandle new_process_handle,
    base::ProcessId new_process_id) const {
  CHECK(observer_, base::NotFatalUntil::M162);
  existing_task->UpdateProcessInfo(new_process_handle, new_process_id,
                                   observer_);
}

#if BUILDFLAG(IS_CHROMEOS)
void TaskProvider::NotifyObserverTaskIdsListToBeInvalidated() const {
  CHECK(observer_, base::NotFatalUntil::M162);
  observer_->TaskIdsListToBeInvalidated();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace task_manager
