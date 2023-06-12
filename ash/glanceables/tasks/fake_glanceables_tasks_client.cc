// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/tasks/fake_glanceables_tasks_client.h"

#include "ash/glanceables/tasks/glanceables_tasks_client.h"
#include "ash/glanceables/tasks/glanceables_tasks_types.h"
#include "base/functional/callback.h"

namespace ash {

FakeGlanceablesTasksClient::FakeGlanceablesTasksClient() {
  PopulateTasks();
  PopulateTaskLists();
}

FakeGlanceablesTasksClient::~FakeGlanceablesTasksClient() = default;

void FakeGlanceablesTasksClient::GetTaskLists(GetTaskListsCallback callback) {
  std::move(callback).Run(task_lists_.get());
}

void FakeGlanceablesTasksClient::GetTasks(const std::string& task_list_id,
                                          GetTasksCallback callback) {
  auto iter = tasks_in_task_lists_.find(task_list_id);
  CHECK(iter != tasks_in_task_lists_.end());
  std::move(callback).Run(iter->second.get());
}

void FakeGlanceablesTasksClient::MarkAsCompleted(
    const std::string& task_list_id,
    const std::string& task_id,
    MarkAsCompletedCallback callback) {
  // TODO(b:277268122): Add once UI is further along and ready to test.
}

void FakeGlanceablesTasksClient::PopulateTasks() {
  task_lists_ = std::make_unique<ui::ListModel<GlanceablesTaskList>>();

  task_lists_->Add(std::make_unique<GlanceablesTaskList>(
      "TaskListID1", "Task List 1 Title", base::Time::Now()));
  task_lists_->Add(std::make_unique<GlanceablesTaskList>(
      "TaskListID2", "Task List 2 Title", base::Time::Now()));
}

void FakeGlanceablesTasksClient::PopulateTaskLists() {
  std::unique_ptr<ui::ListModel<GlanceablesTask>> task_list_1 =
      std::make_unique<ui::ListModel<GlanceablesTask>>();
  task_list_1->Add(std::make_unique<GlanceablesTask>(
      "TaskListItem1", "Task List 1 Item 1 Title", /*completed=*/false,
      /*due=*/base::Time::Now(),
      /*has_subtasks=*/false, /*has_email_link=*/false));
  task_list_1->Add(std::make_unique<GlanceablesTask>(
      "TaskListItem2", "Task List 1 Item 2 Title", /*completed=*/false,
      /*due=*/base::Time::Now(),
      /*has_subtasks=*/false, /*has_email_link=*/false));
  std::unique_ptr<ui::ListModel<GlanceablesTask>> task_list_2 =
      std::make_unique<ui::ListModel<GlanceablesTask>>();
  task_list_2->Add(std::make_unique<GlanceablesTask>(
      "TaskListItem3", "Task List 2 Item 1 Title", /*completed=*/false,
      /*due=*/base::Time::Now(),
      /*has_subtasks=*/false, /*has_email_link=*/false));
  task_list_2->Add(std::make_unique<GlanceablesTask>(
      "TaskListItem4", "Task List 2 Item 2 Title", /*completed=*/false,
      /*due=*/base::Time::Now(),
      /*has_subtasks=*/false, /*has_email_link=*/false));
  tasks_in_task_lists_.emplace("TaskListID1", std::move(task_list_1));
  tasks_in_task_lists_.emplace("TaskListID2", std::move(task_list_2));
}

}  // namespace ash
