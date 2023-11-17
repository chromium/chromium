// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_API_TASKS_TASKS_CLIENT_H_
#define ASH_API_TASKS_TASKS_CLIENT_H_

#include <string>

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "ui/base/models/list_model.h"

namespace ash::api {

struct Task;
struct TaskList;

// Interface for the tasks browser client.
class ASH_EXPORT TasksClient {
 public:
  using GetTaskListsCallback =
      base::OnceCallback<void(const ui::ListModel<TaskList>* task_lists)>;
  using GetTasksCallback =
      base::OnceCallback<void(const ui::ListModel<Task>* tasks)>;

  // Done callback for `AddTask` and `UpdateTask`. If the request completes
  // successfully, `task` points to the newly created or updated task, or
  // `nullptr` otherwise.
  using OnTaskSavedCallback = base::OnceCallback<void(const Task* task)>;
  using OnAllPendingCompletedTasksSavedCallback = base::OnceClosure;

  // Fetches all the authenticated user's task lists and invokes `callback` when
  // done.
  virtual void GetTaskLists(GetTaskListsCallback callback) = 0;

  // Fetches all tasks in the specified task list (`task_list_id` must not be
  // empty) and invokes `callback` when done.
  virtual void GetTasks(const std::string& task_list_id,
                        GetTasksCallback callback) = 0;

  // Marks the specified task in the specified task list as completed. Only root
  // tasks can be marked as completed (all subtasks will be marked as completed
  // automatically by the API). Changes are propagated server side after calling
  // OnGlanceablesBubbleClosed.
  virtual void MarkAsCompleted(const std::string& task_list_id,
                               const std::string& task_id,
                               bool completed) = 0;

  // Adds a new task to the specified task list with the specified title.
  virtual void AddTask(const std::string& task_list_id,
                       const std::string& title,
                       OnTaskSavedCallback callback) = 0;

  // Updates the specified task in the specified task list.
  virtual void UpdateTask(const std::string& task_list_id,
                          const std::string& task_id,
                          const std::string& title,
                          OnTaskSavedCallback callback) = 0;

  // Method called when the glanceables bubble UI closes. The client can use
  // this as a signal to invalidate cached tasks data.
  virtual void OnGlanceablesBubbleClosed(
      OnAllPendingCompletedTasksSavedCallback callback = base::DoNothing()) = 0;

  virtual ~TasksClient() = default;
};

}  // namespace ash::api

#endif  // ASH_API_TASKS_TASKS_CLIENT_H_
