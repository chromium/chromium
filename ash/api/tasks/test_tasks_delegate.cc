// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/api/tasks/test_tasks_delegate.h"

#include "ash/api/tasks/tasks_client.h"
#include "base/notreached.h"
#include "components/account_id/account_id.h"

namespace ash::api {

TestTasksDelegate::TestTasksDelegate() = default;

TestTasksDelegate::~TestTasksDelegate() = default;

void TestTasksDelegate::UpdateClientForProfileSwitch(
    const AccountId& account_id) {
  NOTIMPLEMENTED();
}

void TestTasksDelegate::GetTaskLists(
    TasksClient::GetTaskListsCallback callback) {
  NOTIMPLEMENTED();
}

void TestTasksDelegate::GetTasks(const std::string& task_list_id,
                                 TasksClient::GetTasksCallback callback) {
  NOTIMPLEMENTED();
}

void TestTasksDelegate::MarkAsCompleted(const std::string& task_list_id,
                                        const std::string& task_id,
                                        bool completed) {
  NOTIMPLEMENTED();
}

void TestTasksDelegate::SendCompletedTasks() {
  NOTIMPLEMENTED();
}

void TestTasksDelegate::AddTask(const std::string& task_list_id,
                                const std::string& title) {
  NOTIMPLEMENTED();
}

void TestTasksDelegate::UpdateTaskTitle(const std::string& task_list_id,
                                        const std::string& task_id,
                                        const std::string& title) {
  NOTIMPLEMENTED();
}

}  // namespace ash::api
