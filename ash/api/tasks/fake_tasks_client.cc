// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/api/tasks/fake_tasks_client.h"

#include <list>
#include <memory>
#include <optional>
#include <utility>

#include "ash/api/tasks/tasks_client.h"
#include "ash/api/tasks/tasks_types.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/uuid.h"

namespace ash::api {
namespace {

size_t RunPendingCallbacks(std::list<base::OnceClosure>& pending_callbacks) {
  std::list<base::OnceClosure> callbacks;
  pending_callbacks.swap(callbacks);
  for (auto& callback : callbacks) {
    std::move(callback).Run();
  }
  return callbacks.size();
}

}  // namespace

FakeTasksClient::FakeTasksClient()
    : task_lists_(std::make_unique<ui::ListModel<TaskList>>()) {}

FakeTasksClient::~FakeTasksClient() = default;

void FakeTasksClient::GetTaskLists(bool force_fetch,
                                   GetTaskListsCallback callback) {
  if (!paused_) {
    std::move(callback).Run(/*succes=*/true, task_lists_.get());
  } else {
    pending_get_task_lists_callbacks_.push_back(base::BindOnce(
        [](ui::ListModel<TaskList>* task_lists, GetTaskListsCallback callback) {
          std::move(callback).Run(/*success=*/true, task_lists);
        },
        task_lists_.get(), std::move(callback)));
  }
}

void FakeTasksClient::GetTasks(const std::string& task_list_id,
                               bool force_fetch,
                               GetTasksCallback callback) {
  auto iter = tasks_in_task_lists_.find(task_list_id);
  CHECK(iter != tasks_in_task_lists_.end());

  if (!paused_) {
    std::move(callback).Run(/*success=*/true, iter->second.get());
  } else {
    pending_get_tasks_callbacks_.push_back(base::BindOnce(
        [](ui::ListModel<Task>* tasks, GetTasksCallback callback) {
          std::move(callback).Run(/*success=*/true, tasks);
        },
        iter->second.get(), std::move(callback)));
  }
}

void FakeTasksClient::MarkAsCompleted(const std::string& task_list_id,
                                      const std::string& task_id,
                                      bool completed) {
  if (completed) {
    pending_completed_tasks_.push_back(
        base::JoinString({task_list_id, task_id}, ":"));
  } else {
    pending_completed_tasks_.erase(
        base::ranges::find(pending_completed_tasks_,
                           base::JoinString({task_list_id, task_id}, ":")));
  }
}

void FakeTasksClient::AddTask(const std::string& task_list_id,
                              const std::string& title,
                              TasksClient::OnTaskSavedCallback callback) {
  if (paused_) {
    pending_add_task_callbacks_.push_back(
        base::BindOnce(&FakeTasksClient::AddTaskImpl, base::Unretained(this),
                       task_list_id, title, std::move(callback)));
  } else {
    AddTaskImpl(task_list_id, title, std::move(callback));
  }
}

void FakeTasksClient::UpdateTask(const std::string& task_list_id,
                                 const std::string& task_id,
                                 const std::string& title,
                                 bool completed,
                                 TasksClient::OnTaskSavedCallback callback) {
  if (paused_) {
    pending_update_task_callbacks_.push_back(base::BindOnce(
        &FakeTasksClient::UpdateTaskImpl, base::Unretained(this), task_list_id,
        task_id, title, completed, std::move(callback)));
  } else {
    UpdateTaskImpl(task_list_id, task_id, title, completed,
                   std::move(callback));
  }
}

std::optional<base::Time> FakeTasksClient::GetTasksLastUpdateTime(
    const std::string& task_list_id) const {
  return last_updated_time_;
}

void FakeTasksClient::OnGlanceablesBubbleClosed(
    TasksClient::OnAllPendingCompletedTasksSavedCallback callback) {
  ++bubble_closed_count_;
  RunPendingGetTaskListsCallbacks();
  RunPendingGetTasksCallbacks();
  RunPendingAddTaskCallbacks();
  RunPendingUpdateTaskCallbacks();
  completed_tasks_ += pending_completed_tasks_.size();
  pending_completed_tasks_.clear();
  std::move(callback).Run();
}

void FakeTasksClient::AddTaskList(std::unique_ptr<TaskList> task_list_data) {
  CHECK(!base::Contains(*task_lists_, task_list_data->id, &TaskList::id));
  tasks_in_task_lists_.emplace(task_list_data->id,
                               std::make_unique<ui::ListModel<Task>>());
  task_lists_->Add(std::move(task_list_data));
}

void FakeTasksClient::AddTask(const std::string& task_list_id,
                              std::unique_ptr<Task> task_data) {
  auto task_list_iter = tasks_in_task_lists_.find(task_list_id);
  CHECK(task_list_iter != tasks_in_task_lists_.end());

  auto& tasks = task_list_iter->second;
  CHECK(!base::Contains(*tasks, task_data->id, &Task::id));
  tasks->Add(std::move(task_data));
}

void FakeTasksClient::SetTasksLastUpdateTime(base::Time time) {
  last_updated_time_ = time;
}

int FakeTasksClient::GetAndResetBubbleClosedCount() {
  int result = bubble_closed_count_;
  bubble_closed_count_ = 0;
  return result;
}

size_t FakeTasksClient::RunPendingGetTasksCallbacks() {
  return RunPendingCallbacks(pending_get_tasks_callbacks_);
}

size_t FakeTasksClient::RunPendingGetTaskListsCallbacks() {
  return RunPendingCallbacks(pending_get_task_lists_callbacks_);
}

size_t FakeTasksClient::RunPendingAddTaskCallbacks() {
  return RunPendingCallbacks(pending_add_task_callbacks_);
}

size_t FakeTasksClient::RunPendingUpdateTaskCallbacks() {
  return RunPendingCallbacks(pending_update_task_callbacks_);
}

void FakeTasksClient::AddTaskImpl(const std::string& task_list_id,
                                  const std::string& title,
                                  TasksClient::OnTaskSavedCallback callback) {
  if (run_with_errors_) {
    std::move(callback).Run(/*task=*/nullptr);
    return;
  }

  auto task_list_iter = tasks_in_task_lists_.find(task_list_id);
  CHECK(task_list_iter != tasks_in_task_lists_.end());

  auto pending_task = std::make_unique<Task>(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), title,
      /*due=*/std::nullopt, /*completed=*/false,
      /*has_subtasks=*/false, /*has_email_link=*/false,
      /*has_notes=*/false,
      /*updated=*/base::Time::Now());

  const auto* const task = task_list_iter->second->AddAt(
      /*index=*/0, std::move(pending_task));
  std::move(callback).Run(task);
}

void FakeTasksClient::UpdateTaskImpl(
    const std::string& task_list_id,
    const std::string& task_id,
    const std::string& title,
    bool completed,
    TasksClient::OnTaskSavedCallback callback) {
  if (run_with_errors_) {
    std::move(callback).Run(/*task=*/nullptr);
    return;
  }

  auto task_list_iter = tasks_in_task_lists_.find(task_list_id);
  CHECK(task_list_iter != tasks_in_task_lists_.end());

  const auto task_iter = std::find_if(
      task_list_iter->second->begin(), task_list_iter->second->end(),
      [&task_id](const auto& task) { return task->id == task_id; });
  CHECK(task_iter != task_list_iter->second->end());

  Task* task = task_iter->get();
  task->title = title;
  task->completed = completed;
  std::move(callback).Run(task);
}

}  // namespace ash::api
