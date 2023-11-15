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

  // Retrieves all task lists to be used in the provided `callback`.
  virtual void GetTaskLists(TasksClient::GetTaskListsCallback callback) = 0;

  // Retrieves the tasks in the list with the provided `task_list_id` to be used
  // in the provided `callback`.
  virtual void GetTasks(const std::string& task_list_id,
                        TasksClient::GetTasksCallback callback) = 0;

  // Marks the completion state of the task with the given `task_list_id` and
  // `task_id` as `completed`. Does not immediately send the cached completion
  // data to the Google Tasks API. `completed` indicates whether the caller
  // wants to send to the server that the given task is completed. If
  // `completed` is false, the API will not tell the server that the given task
  // was completed when updates are sent to the server. Likewise, if the server
  // already has the tasks marked as complete, this will not mark the task as
  // incomplete. See `SendCompletedTasks`.
  virtual void MarkAsCompleted(const std::string& task_list_id,
                               const std::string& task_id,
                               bool completed) = 0;

  // Sends cached tasks completion data to the Google Tasks API.
  virtual void SendCompletedTasks() = 0;

  // Adds a task with the given `title` to the task list with id `task_list_id`.
  virtual void AddTask(const std::string& task_list_id,
                       const std::string& title) = 0;

  // Updates the title of the task in the task list with id `task_list_id`
  // with id `task_id`.
  virtual void UpdateTaskTitle(const std::string& task_list_id,
                               const std::string& task_id,
                               const std::string& title) = 0;
};

}  // namespace ash::api

#endif  // ASH_API_TASKS_TASKS_DELEGATE_H_
