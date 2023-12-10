// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_API_TASKS_TEST_TASKS_DELEGATE_H_
#define ASH_API_TASKS_TEST_TASKS_DELEGATE_H_

#include "ash/api/tasks/tasks_delegate.h"
#include "ash/ash_export.h"

namespace ash::api {

class ASH_EXPORT TestTasksDelegate : public TasksDelegate {
 public:
  TestTasksDelegate();
  TestTasksDelegate(const TestTasksDelegate&) = delete;
  TestTasksDelegate& operator=(const TestTasksDelegate&) = delete;
  ~TestTasksDelegate() override;

  // TasksDelegate:
  void UpdateClientForProfileSwitch(const AccountId& account_id) override;
  void GetTaskLists(TasksClient::GetTaskListsCallback callback) override;
  void GetTasks(const std::string& task_list_id,
                TasksClient::GetTasksCallback callback) override;
  void MarkAsCompleted(const std::string& task_list_id,
                       const std::string& task_id,
                       bool completed) override;
  void SendCompletedTasks() override;
  void AddTask(const std::string& task_list_id,
               const std::string& title) override;
  void UpdateTaskTitle(const std::string& task_list_id,
                       const std::string& task_id,
                       const std::string& title) override;
};

}  // namespace ash::api

#endif  // ASH_API_TASKS_TEST_TASKS_DELEGATE_H_
