// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TASKS_PROVIDER_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TASKS_PROVIDER_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"

namespace ash {

namespace api {
struct Task;
}  // namespace api

// A specialized interface that Focus Mode can use to fetch a filtered list of
// tasks to display. Currently only provides dummy data.
class ASH_EXPORT FocusModeTasksProvider {
 public:
  // Done callback for `AddTask` and `UpdateTaskTitle`. If the request completes
  // successfully, `task` points to the newly created or updated task, or
  // `nullptr` otherwise.
  using OnTaskSavedCallback = base::OnceCallback<void(const api::Task* task)>;

  FocusModeTasksProvider();
  FocusModeTasksProvider(const FocusModeTasksProvider&) = delete;
  FocusModeTasksProvider& operator=(const FocusModeTasksProvider&) = delete;
  ~FocusModeTasksProvider();

  // Provides a filtered list of tasks that can be displayed in Focus Mode.
  // Tasks are prioritized by earliest due date, then by the timestamp of their
  // last update.
  std::vector<const api::Task*> GetTaskList() const;

  // Creates a new task with name `title` and adds it to `tasks_data_`.
  void AddTask(const std::string& title, OnTaskSavedCallback callback);

  // Finds the task by `task_id` and updates the task title. Returns a nullptr
  // if the task cannot be found.
  void UpdateTaskTitle(const std::string& task_id,
                       const std::string& title,
                       OnTaskSavedCallback callback);

  // Removes the task with `task_id` from `tasks_data_`.
  void MarkAsCompleted(const std::string& task_id);

 private:
  // Helper function for inserting tasks into `tasks_data_`.
  void InsertTask(std::unique_ptr<api::Task> task);

  // ID counter for creating tasks. Start from above where IDs in
  // `kTaskInitializationData` end to avoid conflicts.
  // TODO(b/306271332): Create a new task.
  int task_id_ = 10;
  // Tasks for the loaded list.
  std::vector<std::unique_ptr<api::Task>> tasks_data_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TASKS_PROVIDER_H_
