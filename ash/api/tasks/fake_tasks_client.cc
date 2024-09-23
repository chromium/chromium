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
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "google_apis/common/api_error_codes.h"
#include "url/gurl.h"

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

bool IsHttpErrorSuccess(google_apis::ApiErrorCode http_error) {
  return http_error == google_apis::ApiErrorCode::HTTP_SUCCESS;
}

}  // namespace

FakeTasksClient::FakeTasksClient()
    : task_lists_(std::make_unique<ui::ListModel<TaskList>>()),
      cached_task_lists_(std::make_unique<ui::ListModel<TaskList>>()) {}

FakeTasksClient::~FakeTasksClient() = default;

bool FakeTasksClient::IsDisabledByAdmin() const {
  return is_disabled_by_admin_;
}

const ui::ListModel<api::TaskList>* FakeTasksClient::GetCachedTaskLists() {
  if (cached_task_lists_->item_count() == 0) {
    return nullptr;
  }

  return cached_task_lists_.get();
}

void FakeTasksClient::GetTaskLists(bool force_fetch,
                                   GetTaskListsCallback callback) {
  const bool need_fetch = force_fetch || cached_task_lists_->item_count() == 0;
  auto* task_lists_returned =
      need_fetch ? task_lists_.get() : cached_task_lists_.get();

  if (paused_ || (paused_on_fetch_ && need_fetch)) {
    pending_get_task_lists_callbacks_.push_back(base::BindOnce(
        [](ui::ListModel<TaskList>* task_lists, GetTaskListsCallback callback,
           bool success, std::optional<google_apis::ApiErrorCode> http_error) {
          std::move(callback).Run(success, http_error, task_lists);
        },
        task_lists_returned, std::move(callback), !get_task_lists_error_,
        http_error_));
  } else {
    std::move(callback).Run(/*success=*/!get_task_lists_error_, http_error_,
                            task_lists_returned);
  }
}

const ui::ListModel<api::Task>* FakeTasksClient::GetCachedTasksInTaskList(
    const std::string& task_list_id) {
  // TODO(b/321789067): Update `cached_tasks_` to a map of list id to tasks to
  // adapt more complicated tests.
  if (task_list_id != cached_task_list_id_) {
    return nullptr;
  }

  return cached_tasks_.get();
}

void FakeTasksClient::GetTasks(const std::string& task_list_id,
                               bool force_fetch,
                               GetTasksCallback callback) {
  auto iter = tasks_in_task_lists_.find(task_list_id);
  CHECK(iter != tasks_in_task_lists_.end());

  const bool need_fetch = force_fetch || task_list_id != cached_task_list_id_;
  auto* tasks_returned = need_fetch ? iter->second.get() : cached_tasks_.get();
  cached_task_list_id_ = task_list_id;

  if (paused_ || (paused_on_fetch_ && need_fetch)) {
    pending_get_tasks_callbacks_.push_back(base::BindOnce(
        [](ui::ListModel<Task>* tasks, GetTasksCallback callback, bool success,
           std::optional<google_apis::ApiErrorCode> http_error) {
          std::move(callback).Run(success, http_error, tasks);
        },
        tasks_returned, std::move(callback), !get_tasks_error_, http_error_));
  } else {
    std::move(callback).Run(/*success=*/!get_tasks_error_, http_error_,
                            tasks_returned);
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

void FakeTasksClient::OnGlanceablesBubbleClosed(base::OnceClosure callback) {
  ++bubble_closed_count_;
  RunPendingGetTaskListsCallbacks();
  RunPendingGetTasksCallbacks();
  RunPendingAddTaskCallbacks();
  RunPendingUpdateTaskCallbacks();
  completed_tasks_ += pending_completed_tasks_.size();
  pending_completed_tasks_.clear();
  CacheTaskLists();
  CacheTasks();
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

void FakeTasksClient::DeleteTaskList(const std::string& task_list_id) {
  // Find the task list iterator with id `task_list_id`.
  auto iter = std::find_if(
      task_lists_->begin(), task_lists_->end(),
      [task_list_id](const auto& list) { return list->id == task_list_id; });
  if (iter == task_lists_->end()) {
    return;
  }

  task_lists_->DeleteAt(iter - task_lists_->begin());
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
  CHECK(http_error_);
  if (!IsHttpErrorSuccess(http_error_.value())) {
    // Simulate there is an error when requesting data through the Google Task
    // API.
    std::move(callback).Run(http_error_.value(), /*task=*/nullptr);
    return;
  }

  auto task_list_iter = tasks_in_task_lists_.find(task_list_id);
  CHECK(task_list_iter != tasks_in_task_lists_.end());

  const auto new_task_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  auto pending_task = std::make_unique<Task>(
      new_task_id, title,
      /*due=*/std::nullopt, /*completed=*/false,
      /*has_subtasks=*/false, /*has_email_link=*/false,
      /*has_notes=*/false,
      /*updated=*/base::Time::Now(),
      /*web_view_link=*/
      GURL(base::StrCat({"https://tasks.google.com/task/", new_task_id})),
      Task::OriginSurfaceType::kRegular);

  const auto* const task = task_list_iter->second->AddAt(
      /*index=*/0, std::move(pending_task));
  // Simulate update the task successfully through the Google Task API.
  std::move(callback).Run(http_error_.value(), task);
}

void FakeTasksClient::UpdateTaskImpl(
    const std::string& task_list_id,
    const std::string& task_id,
    const std::string& title,
    bool completed,
    TasksClient::OnTaskSavedCallback callback) {
  CHECK(http_error_);
  if (!IsHttpErrorSuccess(http_error_.value())) {
    // Simulate there is an error when requesting data through the Google Task
    // API.
    std::move(callback).Run(http_error_.value(), /*task=*/nullptr);
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
  // Simulate update the task successfully through the Google Task API.
  std::move(callback).Run(http_error_.value(), task);
}

void FakeTasksClient::CacheTaskLists() {
  cached_task_lists_->DeleteAll();
  for (const auto& list : *task_lists_) {
    cached_task_lists_->Add(
        std::make_unique<TaskList>(list->id, list->title, list->updated));
  }
}

void FakeTasksClient::CacheTasks() {
  auto iter = tasks_in_task_lists_.find(cached_task_list_id_);
  if (iter == tasks_in_task_lists_.end()) {
    return;
  }

  if (!cached_tasks_) {
    cached_tasks_ = std::make_unique<ui::ListModel<Task>>();
  } else {
    cached_tasks_->DeleteAll();
  }

  for (const auto& task : *iter->second) {
    cached_tasks_->Add(std::make_unique<Task>(
        task->id, task->title, task->due, task->completed, task->has_subtasks,
        task->has_email_link, task->has_notes, task->updated,
        task->web_view_link, Task::OriginSurfaceType::kRegular));
  }
}

}  // namespace ash::api
