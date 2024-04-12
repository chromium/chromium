// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/api/tasks/test_tasks_delegate.h"

#include <memory>

#include "ash/api/tasks/fake_tasks_client.h"
#include "ash/api/tasks/tasks_types.h"
#include "base/logging.h"
#include "components/account_id/account_id.h"

namespace ash::api {

TestTasksDelegate::TestTasksDelegate() {
  AccountId account_id = EmptyAccountId();
  AddFakeTasksClient(account_id, std::make_unique<FakeTasksClient>());
  SetActiveAccount(account_id);
}

TestTasksDelegate::~TestTasksDelegate() = default;

void TestTasksDelegate::UpdateClientForProfileSwitch(
    const AccountId& account_id) {
  if (clients_.find(account_id) == clients_.end()) {
    LOG(WARNING) << "No FakeTasksClient exists for active account";
  }

  if (active_client_) {
    active_client_->InvalidateCache();
  }

  SetActiveAccount(account_id);
}

void TestTasksDelegate::GetTaskLists(
    bool force_fetch,
    TasksClient::GetTaskListsCallback callback) {
  CHECK(active_client_);
  active_client_->GetTaskLists(force_fetch, std::move(callback));
}

void TestTasksDelegate::GetTasks(const std::string& task_list_id,
                                 bool force_fetch,
                                 TasksClient::GetTasksCallback callback) {
  CHECK(active_client_);
  active_client_->GetTasks(task_list_id, force_fetch, std::move(callback));
}

void TestTasksDelegate::AddTask(const std::string& task_list_id,
                                const std::string& title,
                                TasksClient::OnTaskSavedCallback callback) {
  CHECK(active_client_);
  active_client_->AddTask(task_list_id, title, std::move(callback));
}

void TestTasksDelegate::UpdateTask(const std::string& task_list_id,
                                   const std::string& task_id,
                                   const std::string& title,
                                   bool completed,
                                   TasksClient::OnTaskSavedCallback callback) {
  CHECK(active_client_);
  active_client_->UpdateTask(task_list_id, task_id, title, completed,
                             std::move(callback));
}

void TestTasksDelegate::AddFakeTasksClient(
    const AccountId& account_id,
    std::unique_ptr<FakeTasksClient> tasks_client) {
  clients_.insert_or_assign(account_id, std::move(tasks_client));
}

void TestTasksDelegate::SetActiveAccount(const AccountId& account_id) {
  const auto iter = clients_.find(account_id);
  CHECK(iter != clients_.end());
  active_client_ = iter->second.get();
}

}  // namespace ash::api
