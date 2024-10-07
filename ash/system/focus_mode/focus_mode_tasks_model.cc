// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_tasks_model.h"

#include <absl/cleanup/cleanup.h>

#include <algorithm>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/observer_list.h"

namespace ash {

namespace {

// Returns a pointer to the task with a matching `task_id` from `tasks`. Returns
// nullptr if a matching task does not exist in `tasks`.
FocusModeTask* FindTaskById(const TaskId& task_id,
                            std::vector<FocusModeTask>& tasks) {
  auto iter = base::ranges::find(tasks, task_id, &FocusModeTask::task_id);
  return (iter == tasks.end()) ? nullptr : &(*iter);
}

void NotifyCompletedTask(
    base::ObserverList<FocusModeTasksModel::Observer>& observers,
    const FocusModeTask& task) {
  for (FocusModeTasksModel::Observer& observer : observers) {
    observer.OnTaskCompleted(task);
  }
}

void NotifySelectedTask(
    base::ObserverList<FocusModeTasksModel::Observer>& observers,
    const FocusModeTask* selected_task) {
  const std::optional<FocusModeTask> task =
      selected_task ? std::make_optional(*selected_task) : std::nullopt;
  for (FocusModeTasksModel::Observer& observer : observers) {
    observer.OnSelectedTaskChanged(task);
  }
}

void NotifyTaskListChanged(
    base::ObserverList<FocusModeTasksModel::Observer>& observers,
    const std::vector<FocusModeTask>& tasks) {
  for (FocusModeTasksModel::Observer& observer : observers) {
    observer.OnTasksUpdated(tasks);
  }
}

}  // namespace

FocusModeTasksModel::TaskUpdate::TaskUpdate() = default;
FocusModeTasksModel::TaskUpdate::TaskUpdate(const TaskUpdate&) = default;
FocusModeTasksModel::TaskUpdate::~TaskUpdate() = default;

// static
FocusModeTasksModel::TaskUpdate
FocusModeTasksModel::TaskUpdate::CompletedUpdate(const TaskId& task_id) {
  TaskUpdate update;
  update.task_id = task_id;
  update.completed = true;
  return update;
}

// static
FocusModeTasksModel::TaskUpdate FocusModeTasksModel::TaskUpdate::TitleUpdate(
    const TaskId& task_id,
    std::string_view title) {
  TaskUpdate update;
  update.task_id = task_id;
  update.title = std::string{title};
  return update;
}

// static
FocusModeTasksModel::TaskUpdate FocusModeTasksModel::TaskUpdate::NewTask(
    std::string_view title) {
  TaskUpdate update;
  update.title = std::string{title};
  return update;
}

FocusModeTasksModel::FocusModeTasksModel() = default;

FocusModeTasksModel::~FocusModeTasksModel() = default;

void FocusModeTasksModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FocusModeTasksModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FocusModeTasksModel::SetDelegate(base::WeakPtr<Delegate> delegate) {
  delegate_ = std::move(delegate);
}

void FocusModeTasksModel::RequestUpdate() {
  if (!tasks_.empty()) {
    NotifyTaskListChanged(observers_, tasks_);
  }

  if (selected_task_) {
    NotifySelectedTask(observers_, selected_task_);
  }

  if (delegate_) {
    // If there is a currently selected task, we fetch the task to see if the
    // title was updated or if it has been completed.
    if (selected_task_ && selected_task_->task_id.IsValid()) {
      delegate_->FetchTask(
          selected_task_->task_id,
          base::BindOnce(&FocusModeTasksModel::OnSelectedTaskFetched,
                         weak_ptr_factory_.GetWeakPtr()));
      return;
    }

    delegate_->FetchTasks();
  }
}

bool FocusModeTasksModel::SetSelectedTask(const TaskId& task_id) {
  CHECK(!task_id.empty());
  auto iter = base::ranges::find(tasks_, task_id, &FocusModeTask::task_id);

  if (iter == tasks_.end()) {
    // 'task_id' was not found in the task list. Task cannot be selected.
    return false;
  }

  // Clear the pref task since an existing task is selected.
  pref_task_id_.reset();

  if (selected_task_ && selected_task_->task_id == task_id) {
    // Task is the same as the currently selected task. Skip update.
    return true;
  }

  // Move selected task to the front of `tasks_` if it is not already there.
  if (iter != tasks_.begin()) {
    // Save `pending_task_` to be found later.
    const std::optional<TaskId> pending_task_id =
        pending_task_ ? std::make_optional(pending_task_->task_id)
                      : std::nullopt;

    pending_task_ = nullptr;
    selected_task_ = nullptr;
    // Move the selected task to the front.
    const auto desired = iter;
    iter++;
    std::rotate(tasks_.begin(), desired, iter);
    if (pending_task_id) {
      pending_task_ = FindTaskById(*pending_task_id, tasks_);
    }
  }
  selected_task_ = &tasks_[0];
  NotifySelectedTask(observers_, selected_task_);
  return true;
}

void FocusModeTasksModel::SetSelectedTask(const FocusModeTask& task) {
  CHECK(!task.task_id.empty());
  FocusModeTask* task_in_list = FindTaskById(task.task_id, tasks_);

  if (task_in_list) {
    *task_in_list = task;
  } else {
    InsertTaskIntoTaskList(FocusModeTask(task));
    NotifyTaskListChanged(observers_, tasks_);
  }

  CHECK(SetSelectedTask(task.task_id));
}

void FocusModeTasksModel::ClearSelectedTask() {
  pref_task_id_.reset();
  if (selected_task_) {
    selected_task_ = nullptr;
    NotifySelectedTask(observers_, nullptr);
  }
}

void FocusModeTasksModel::Reset() {
  NotifySelectedTask(observers_, nullptr);
  NotifyTaskListChanged(observers_, {});
  selected_task_ = nullptr;
  pending_task_ = nullptr;
  pref_task_id_.reset();
  tasks_.clear();
}

void FocusModeTasksModel::SetSelectedTaskFromPrefs(const TaskId& task_id) {
  if (selected_task_) {
    // Never override a selected task with pref data.
    return;
  }

  FocusModeTask* matching_task = FindTaskById(task_id, tasks_);
  if (matching_task) {
    selected_task_ = matching_task;
    NotifySelectedTask(observers_, selected_task_);
    return;
  }

  // Preference task is not in the cache. Try to fetch it.
  pref_task_id_ = task_id;
  FocusModeTasksModel::Delegate::FetchTaskCallback callback = base::BindOnce(
      &FocusModeTasksModel::OnPrefTaskFetched, weak_ptr_factory_.GetWeakPtr());
  if (delegate_ && pref_task_id_->IsValid()) {
    delegate_->FetchTask(*pref_task_id_, std::move(callback));
  }
}

void FocusModeTasksModel::SetTaskList(std::vector<FocusModeTask>&& tasks) {
  TaskId desired_task_id;
  if (selected_task_) {
    desired_task_id = selected_task_->task_id;
  } else if (pref_task_id_) {
    desired_task_id = *pref_task_id_;
  }

  bool desired_task_in_list = false;
  if (!desired_task_id.empty()) {
    FocusModeTask* new_selected_task = FindTaskById(desired_task_id, tasks);
    if (new_selected_task) {
      desired_task_in_list = true;
    } else if (selected_task_) {
      // There is a selected task but its not in `tasks`. Add it ourselves.
      tasks.insert(tasks.begin(), *selected_task_);
      desired_task_in_list = true;
    }
  }

  // Clear pointers to tasks in the old list since they will become invalid.
  pending_task_ = nullptr;
  selected_task_ = nullptr;
  tasks_ = tasks;

  if (desired_task_in_list) {
    // Find the task again since the address changed after `tasks_` was
    // replaced.
    selected_task_ = FindTaskById(desired_task_id, tasks_);
    pref_task_id_.reset();
  }

  NotifyTaskListChanged(observers_, tasks_);

  if (desired_task_id.empty()) {
    // Only notify if there was an id we were looking for.
    return;
  }

  if (pref_task_id_) {
    // Preference tasks might not be in the list. Wait for the fetch request
    // to return before notification.
    return;
  }

  NotifySelectedTask(observers_, selected_task_);
}

void FocusModeTasksModel::UpdateTask(const TaskUpdate& task_update) {
  FocusModeTask* task = nullptr;
  if (task_update.task_id) {
    // Find in list.
    task = FindTaskById(*task_update.task_id, tasks_);
  } else {
    // New task
    FocusModeTask new_task;
    new_task.task_id.pending = true;
    selected_task_ = nullptr;
    pending_task_ = nullptr;
    task = &(*tasks_.insert(tasks_.begin(), std::move(new_task)));
    // New tasks must always become the selected task.
    pending_task_ = task;
    selected_task_ = pending_task_;
  }

  if (task == nullptr) {
    return;
  }

  if (task_update.title) {
    task->title = *task_update.title;
  }

  if (task_update.completed.has_value()) {
    CHECK(task_update.task_id);
    const TaskId& id = *task_update.task_id;
    auto iter = base::ranges::find(tasks_, id, &FocusModeTask::task_id);
    CHECK(iter != tasks_.end());

    NotifyCompletedTask(observers_, *iter);
    if (selected_task_ == task) {
      if (pending_task_ == selected_task_) {
        pending_task_ = nullptr;
      }
      selected_task_ = nullptr;
    }
    tasks_.erase(iter);
  }

  if (delegate_) {
    if (!task_update.task_id) {
      delegate_->AddTask(task_update,
                         base::BindOnce(&FocusModeTasksModel::OnTaskAdded,
                                        weak_ptr_factory_.GetWeakPtr()));
    } else {
      if (!task_update.task_id->pending) {
        // Pending tasks don't exist on the server so this is invalid.
        delegate_->UpdateTask(task_update);
      }
    }
  }

  NotifyTaskListChanged(observers_, tasks_);
  NotifySelectedTask(observers_, selected_task_);
}

const std::vector<FocusModeTask>& FocusModeTasksModel::tasks() const {
  return tasks_;
}

const FocusModeTask* FocusModeTasksModel::selected_task() const {
  return selected_task_;
}

const TaskId& FocusModeTasksModel::PrefTaskIdForTesting() const {
  return *pref_task_id_;
}

void FocusModeTasksModel::OnTaskAdded(
    const std::optional<FocusModeTask>& fetched_task) {
  if (!pending_task_) {
    LOG(WARNING) << "Update for a task that is no longer pending";
    return;
  }

  if (!pending_task_->task_id.pending) {
    LOG(WARNING) << "Pending task already has an id";
    return;
  }

  if (!fetched_task) {
    LOG(WARNING) << "Adding task failed";
    return;
  }

  // Update the pending task (in place) with the new data.
  *pending_task_ = *fetched_task;
  if (pending_task_ == selected_task_) {
    NotifySelectedTask(observers_, selected_task_);
  }

  // Clear the pending task.
  pending_task_ = nullptr;
}

void FocusModeTasksModel::OnPrefTaskFetched(
    const std::optional<FocusModeTask>& fetched_task) {
  if (!fetched_task) {
    LOG(WARNING) << "Fetching Pref task failed. Try again later";
    return;
  }
  if (selected_task_ || !pref_task_id_) {
    // If a task was selected while we were waiting, discard the response and
    // the pref task.
    pref_task_id_.reset();
    return;
  }

  const TaskId& task_id = fetched_task->task_id;
  if (task_id != *pref_task_id_) {
    // Fetched task does not match the task we are currently looking for.
    return;
  }

  // Controls if the list update is emitted.
  bool list_updated = true;

  auto iter = base::ranges::find(tasks_, task_id, &FocusModeTask::task_id);
  if (iter != tasks_.end()) {
    if (fetched_task->completed) {
      tasks_.erase(iter);
    } else {
      // Suppress updates if the task is in the list.
      list_updated = false;
      selected_task_ = &(*iter);
    }
  } else {
    if (!fetched_task->completed) {
      selected_task_ = InsertTaskIntoTaskList(FocusModeTask(*fetched_task));
    }
  }

  pref_task_id_.reset();

  if (list_updated) {
    NotifyTaskListChanged(observers_, tasks_);
  }
  NotifySelectedTask(observers_, selected_task_);
}

void FocusModeTasksModel::OnSelectedTaskFetched(
    const std::optional<FocusModeTask>& fetched_task) {
  CHECK(delegate_);

  if (!fetched_task) {
    LOG(WARNING) << "Fetching Selected task failed. Try again later";
    return;
  }

  // Ensure that `FetchTasks` is called afterwards. This will trigger a
  // `NotifyTaskListChanged`.
  absl::Cleanup fetch_tasks = [this] { delegate_->FetchTasks(); };

  const TaskId& task_id = fetched_task->task_id;
  FocusModeTask* task = FindTaskById(task_id, tasks_);
  if (task == nullptr) {
    // 'task_id' was not found in the task list. Nothing to update.
    return;
  }

  // Update the title if it has changed.
  bool has_task_title_changed = task->title != fetched_task->title;
  if (has_task_title_changed) {
    task->title = fetched_task->title;
  }

  if (fetched_task->completed) {
    UpdateTask(TaskUpdate::CompletedUpdate(task_id));
    return;
  }

  if (has_task_title_changed && selected_task_ &&
      selected_task_->task_id == task_id) {
    NotifySelectedTask(observers_, selected_task_);
  }
}

FocusModeTask* FocusModeTasksModel::InsertTaskIntoTaskList(
    FocusModeTask&& task) {
  std::optional<TaskId> selected_task_id =
      selected_task_ ? std::make_optional(selected_task_->task_id)
                     : std::nullopt;
  std::optional<TaskId> pending_task_id =
      pending_task_ ? std::make_optional(pending_task_->task_id) : std::nullopt;

  pending_task_ = nullptr;
  selected_task_ = nullptr;
  auto inserted = tasks_.insert(tasks_.begin(), task);

  if (selected_task_id) {
    selected_task_ = FindTaskById(*selected_task_id, tasks_);
  }
  if (pending_task_id) {
    // `pending_task_` and `selected_task_` are frequently the same. Skip the
    // search if possible.
    if (pending_task_id == selected_task_id) {
      pending_task_ = selected_task_;
    } else {
      pending_task_ = FindTaskById(*pending_task_id, tasks_);
    }
  }

  return &(*inserted);
}

}  // namespace ash
