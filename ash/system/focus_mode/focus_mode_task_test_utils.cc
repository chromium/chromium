// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/api/tasks/fake_tasks_client.h"
#include "ash/api/tasks/tasks_controller.h"
#include "ash/api/tasks/tasks_types.h"
#include "ash/api/tasks/test_tasks_delegate.h"

namespace ash {

api::FakeTasksClient& CreateFakeTasksClient(const AccountId& account_id) {
  api::TestTasksDelegate* tasks_delegate = static_cast<api::TestTasksDelegate*>(
      api::TasksController::Get()->tasks_delegate());

  auto tasks_client = std::make_unique<api::FakeTasksClient>();
  auto* tasks_client_ptr = tasks_client.get();

  tasks_delegate->AddFakeTasksClient(account_id, std::move(tasks_client));
  tasks_delegate->SetActiveAccount(account_id);

  return *tasks_client_ptr;
}

// Utility functions to add sample tasks to the fake tasks client.
void AddFakeTaskList(api::FakeTasksClient& client,
                     const std::string& task_list_id) {
  client.AddTaskList(std::make_unique<api::TaskList>(task_list_id, /*title=*/"",
                                                     /*updated=*/base::Time{}));
}

void AddFakeTask(api::FakeTasksClient& client,
                 const std::string& task_list_id,
                 const std::string& task_id,
                 const std::string& title) {
  client.AddTask(task_list_id,
                 std::make_unique<api::Task>(
                     task_id, title, /*due=*/std::nullopt, /*completed=*/false,
                     /*has_subtasks=*/false,
                     /*has_email_link=*/false, /*has_notes=*/false,
                     /*updated=*/base::Time{}, /*web_view_link=*/GURL{},
                     api::Task::OriginSurfaceType::kRegular));
}

}  // namespace ash
