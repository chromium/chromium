// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_API_TASKS_TEST_TASKS_DELEGATE_H_
#define ASH_API_TASKS_TEST_TASKS_DELEGATE_H_

#include <optional>

#include "ash/api/tasks/tasks_delegate.h"
#include "ash/ash_export.h"
#include "base/containers/flat_map.h"
#include "components/account_id/account_id.h"

namespace ash::api {

class TasksClient;

class ASH_EXPORT TestTasksDelegate : public TasksDelegate {
 public:
  TestTasksDelegate();
  TestTasksDelegate(const TestTasksDelegate&) = delete;
  TestTasksDelegate& operator=(const TestTasksDelegate&) = delete;
  ~TestTasksDelegate() override;

  // TasksDelegate:
  void UpdateClientForProfileSwitch(const AccountId& account_id) override;
  void GetTaskLists(bool force_fetch,
                    TasksClient::GetTaskListsCallback callback) override;
  void GetTasks(const std::string& task_list_id,
                bool force_fetch,
                TasksClient::GetTasksCallback callback) override;
  void AddTask(const std::string& task_list_id,
               const std::string& title,
               TasksClient::OnTaskSavedCallback callback) override;
  void UpdateTask(const std::string& task_list_id,
                  const std::string& task_id,
                  const std::string& title,
                  bool completed,
                  TasksClient::OnTaskSavedCallback callback) override;

  // Helper function for adding pre-build `TasksClient` objects.
  void AddFakeTasksClient(const AccountId& account_id,
                          std::unique_ptr<TasksClient> tasks_client);

  // Switches the active account for the delegate.
  void SetActiveAccount(const AccountId& account_id);

 private:
  // Returns a reference to the client registered for a current
  // account. CHECK-fails if no client is registered for it.
  TasksClient& GetClientForActiveAccount();

  // The currently active account.
  std::optional<AccountId> active_account_id_;

  // Maps account IDs to clients.
  base::flat_map<AccountId, std::unique_ptr<TasksClient>> clients_;
};

}  // namespace ash::api

#endif  // ASH_API_TASKS_TEST_TASKS_DELEGATE_H_
