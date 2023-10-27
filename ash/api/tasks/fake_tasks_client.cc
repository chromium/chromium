// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/api/tasks/fake_tasks_client.h"

#include <list>
#include <utility>

#include "ash/api/tasks/tasks_client.h"
#include "ash/api/tasks/tasks_types.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"

namespace ash::api {

FakeTasksClient::FakeTasksClient(base::Time tasks_due_time) {
  PopulateTasks(tasks_due_time);
  PopulateTaskLists(tasks_due_time);
}

FakeTasksClient::~FakeTasksClient() = default;

void FakeTasksClient::GetTaskLists(GetTaskListsCallback callback) {
  if (!paused_) {
    std::move(callback).Run(task_lists_.get());
  } else {
    pending_get_task_lists_callbacks_.push_back(base::BindOnce(
        [](ui::ListModel<TaskList>* task_lists, GetTaskListsCallback callback) {
          std::move(callback).Run(task_lists);
        },
        task_lists_.get(), std::move(callback)));
  }
}

void FakeTasksClient::GetTasks(const std::string& task_list_id,
                               GetTasksCallback callback) {
  auto iter = tasks_in_task_lists_.find(task_list_id);
  CHECK(iter != tasks_in_task_lists_.end());

  if (!paused_) {
    std::move(callback).Run(iter->second.get());
  } else {
    pending_get_tasks_callbacks_.push_back(base::BindOnce(
        [](ui::ListModel<Task>* tasks, GetTasksCallback callback) {
          std::move(callback).Run(tasks);
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
    pending_completed_tasks_.erase(std::find(
        pending_completed_tasks_.begin(), pending_completed_tasks_.end(),
        base::JoinString({task_list_id, task_id}, ":")));
  }
}

void FakeTasksClient::AddTask(const std::string& task_list_id,
                              const std::string& title) {
  NOTIMPLEMENTED();
}

void FakeTasksClient::UpdateTask(const std::string& task_list_id,
                                 const std::string& task_id,
                                 const std::string& title,
                                 TasksClient::UpdateTaskCallback callback) {}

void FakeTasksClient::OnGlanceablesBubbleClosed(
    TasksClient::OnAllPendingCompletedTasksSavedCallback callback) {
  ++bubble_closed_count_;
  RunPendingGetTaskListsCallbacks();
  RunPendingGetTasksCallbacks();
  completed_tasks_ += pending_completed_tasks_.size();
  pending_completed_tasks_.clear();
  std::move(callback).Run();
}

int FakeTasksClient::GetAndResetBubbleClosedCount() {
  int result = bubble_closed_count_;
  bubble_closed_count_ = 0;
  return result;
}

size_t FakeTasksClient::RunPendingGetTasksCallbacks() {
  std::list<base::OnceClosure> callbacks;
  pending_get_tasks_callbacks_.swap(callbacks);
  for (auto& callback : callbacks) {
    std::move(callback).Run();
  }
  return callbacks.size();
}

size_t FakeTasksClient::RunPendingGetTaskListsCallbacks() {
  std::list<base::OnceClosure> callbacks;
  pending_get_task_lists_callbacks_.swap(callbacks);
  for (auto& callback : callbacks) {
    std::move(callback).Run();
  }
  return callbacks.size();
}

void FakeTasksClient::PopulateTasks(base::Time tasks_due_time) {
  task_lists_ = std::make_unique<ui::ListModel<TaskList>>();

  task_lists_->Add(std::make_unique<TaskList>(
      "TaskListID1", "Task List 1 Title", /*updated=*/tasks_due_time));
  task_lists_->Add(std::make_unique<TaskList>(
      "TaskListID2", "Task List 2 Title", /*updated=*/tasks_due_time));
  task_lists_->Add(std::make_unique<TaskList>("TaskListID3",
                                              "Task List 3 Title (empty)",
                                              /*updated=*/tasks_due_time));
  task_lists_->Add(std::make_unique<TaskList>("TaskListID4",
                                              "Task List 4 Title (empty)",
                                              /*updated=*/tasks_due_time));
  task_lists_->Add(std::make_unique<TaskList>("TaskListID5",
                                              "Task List 5 Title (empty)",
                                              /*updated=*/tasks_due_time));
  task_lists_->Add(std::make_unique<TaskList>("TaskListID6",
                                              "Task List 6 Title (empty)",
                                              /*updated=*/tasks_due_time));
}

void FakeTasksClient::PopulateTaskLists(base::Time tasks_due_time) {
  std::unique_ptr<ui::ListModel<Task>> task_list_1 =
      std::make_unique<ui::ListModel<Task>>();
  task_list_1->Add(std::make_unique<Task>(
      "TaskListItem1", "Task List 1 Item 1 Title", /*completed=*/false,
      /*due=*/tasks_due_time,
      /*has_subtasks=*/false, /*has_email_link=*/false, /*has_notes=*/false));
  task_list_1->Add(std::make_unique<Task>(
      "TaskListItem2", "Task List 1 Item 2 Title", /*completed=*/false,
      /*due=*/tasks_due_time,
      /*has_subtasks=*/false, /*has_email_link=*/false, /*has_notes=*/false));
  std::unique_ptr<ui::ListModel<Task>> task_list_2 =
      std::make_unique<ui::ListModel<Task>>();
  task_list_2->Add(std::make_unique<Task>(
      "TaskListItem3", "Task List 2 Item 1 Title", /*completed=*/false,
      /*due=*/tasks_due_time,
      /*has_subtasks=*/false, /*has_email_link=*/false, /*has_notes=*/false));
  task_list_2->Add(std::make_unique<Task>(
      "TaskListItem4", "Task List 2 Item 2 Title", /*completed=*/false,
      /*due=*/tasks_due_time,
      /*has_subtasks=*/false, /*has_email_link=*/false, /*has_notes=*/false));
  task_list_2->Add(std::make_unique<Task>(
      "TaskListItem5", "Task List 2 Item 3 Title", /*completed=*/false,
      /*due=*/tasks_due_time,
      /*has_subtasks=*/false, /*has_email_link=*/false, /*has_notes=*/false));
  tasks_in_task_lists_.emplace("TaskListID1", std::move(task_list_1));
  tasks_in_task_lists_.emplace("TaskListID2", std::move(task_list_2));
  tasks_in_task_lists_.emplace("TaskListID3",
                               std::make_unique<ui::ListModel<Task>>());
  tasks_in_task_lists_.emplace("TaskListID4",
                               std::make_unique<ui::ListModel<Task>>());
  tasks_in_task_lists_.emplace("TaskListID5",
                               std::make_unique<ui::ListModel<Task>>());
  tasks_in_task_lists_.emplace("TaskListID6",
                               std::make_unique<ui::ListModel<Task>>());
}

}  // namespace ash::api
