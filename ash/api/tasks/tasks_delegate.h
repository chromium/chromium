// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_API_TASKS_TASKS_DELEGATE_H_
#define ASH_API_TASKS_TASKS_DELEGATE_H_

#include "ash/api/tasks/tasks_client.h"
#include "ash/ash_export.h"

class AccountId;

namespace ash::api {

// Interface for communicating with the Google Tasks API.
class ASH_EXPORT TasksDelegate {
 public:
  virtual ~TasksDelegate() = default;

  // Notifies the delegate that a different profile is being used.
  virtual void UpdateClientForProfileSwitch(const AccountId& account_id) = 0;

  // Retrieves all task lists to be used in the provided `callback`. If
  // `force_fetch` is true, new data will be fetched from the Google Tasks API.
  virtual void GetTaskLists(bool force_fetch,
                            TasksClient::GetTaskListsCallback callback) = 0;

  // Retrieves the tasks in the list with the provided `task_list_id` to be used
  // in the provided `callback`. If `force_fetch` is true, new data will be
  // fetched from the Google Tasks API.
  virtual void GetTasks(const std::string& task_list_id,
                        bool force_fetch,
                        TasksClient::GetTasksCallback callback) = 0;

  // Adds a task with the given `title` to the task list with id `task_list_id`.
  // `callback` provides a pointer to the saved Task if the operation was
  // successful, or a nullptr if not.
  virtual void AddTask(const std::string& task_list_id,
                       const std::string& title,
                       TasksClient::OnTaskSavedCallback callback) = 0;

  // Updates the task in the task list with id `task_list_id` and with id
  // `task_id`. `callback` provides a pointer to the saved Task if the operation
  // was successful, or a nullptr if not.
  virtual void UpdateTask(const std::string& task_list_id,
                          const std::string& task_id,
                          const std::string& title,
                          bool completed,
                          TasksClient::OnTaskSavedCallback callback) = 0;
};

}  // namespace ash::api

#endif  // ASH_API_TASKS_TASKS_DELEGATE_H_
