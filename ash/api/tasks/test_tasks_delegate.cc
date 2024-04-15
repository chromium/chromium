// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/api/tasks/test_tasks_delegate.h"

#include "ash/api/tasks/tasks_client.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "components/account_id/account_id.h"

namespace ash::api {

TestTasksDelegate::TestTasksDelegate() = default;

TestTasksDelegate::~TestTasksDelegate() = default;

void TestTasksDelegate::UpdateClientForProfileSwitch(
    const AccountId& account_id) {
  SetActiveAccount(account_id);

  if (clients_.find(account_id) == clients_.end()) {
    LOG(WARNING) << "No FakeTasksClient exists for active account";
  } else {
    GetClientForActiveAccount().InvalidateCache();
  }
}

void TestTasksDelegate::GetTaskLists(
    bool force_fetch,
    TasksClient::GetTaskListsCallback callback) {
  GetClientForActiveAccount().GetTaskLists(force_fetch, std::move(callback));
}

void TestTasksDelegate::GetTasks(const std::string& task_list_id,
                                 bool force_fetch,
                                 TasksClient::GetTasksCallback callback) {
  GetClientForActiveAccount().GetTasks(task_list_id, force_fetch,
                                       std::move(callback));
}

void TestTasksDelegate::AddTask(const std::string& task_list_id,
                                const std::string& title,
                                TasksClient::OnTaskSavedCallback callback) {
  GetClientForActiveAccount().AddTask(task_list_id, title, std::move(callback));
}

void TestTasksDelegate::UpdateTask(const std::string& task_list_id,
                                   const std::string& task_id,
                                   const std::string& title,
                                   bool completed,
                                   TasksClient::OnTaskSavedCallback callback) {
  GetClientForActiveAccount().UpdateTask(task_list_id, task_id, title,
                                         completed, std::move(callback));
}

void TestTasksDelegate::AddFakeTasksClient(
    const AccountId& account_id,
    std::unique_ptr<TasksClient> tasks_client) {
  clients_.insert_or_assign(account_id, std::move(tasks_client));
}

void TestTasksDelegate::SetActiveAccount(const AccountId& account_id) {
  active_account_id_ = account_id;
}

TasksClient& TestTasksDelegate::GetClientForActiveAccount() {
  CHECK(active_account_id_.has_value());
  auto it = clients_.find(*active_account_id_);
  CHECK(it != clients_.end())
      << "No client registered for account: " << *active_account_id_;
  return *it->second;
}

}  // namespace ash::api
