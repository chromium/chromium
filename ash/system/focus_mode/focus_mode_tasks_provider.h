// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TASKS_PROVIDER_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TASKS_PROVIDER_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"

namespace ash {

namespace api {
struct Task;
}  // namespace api

// A specialized interface that Focus Mode can use to fetch a filtered list of
// tasks to display.
class ASH_EXPORT FocusModeTasksProvider {
 public:
  FocusModeTasksProvider();
  FocusModeTasksProvider(const FocusModeTasksProvider&) = delete;
  FocusModeTasksProvider& operator=(const FocusModeTasksProvider&) = delete;
  ~FocusModeTasksProvider();

  // Provides a filtered list of tasks that can be displayed in Focus Mode.
  // Tasks are prioritized by earliest due date, then by the timestamp of their
  // last update.
  std::vector<const api::Task*> GetTaskList() const;

  // Adds `task` to `tasks_data_`.
  void AddTask(std::unique_ptr<api::Task> task);

  // Creates a new task with name `task_title` and adds it to `tasks_data_`.
  // TODO(b/306271332): Create a new task.
  void CreateTask(const std::string& task_title);

  // Removes the task with `task_id` from `tasks_data_`.
  void MarkAsCompleted(const std::string& task_id);

 private:
  // ID counter for creating tasks. Start from above where IDs in
  // `kTaskInitializationData` end to avoid conflicts.
  // TODO(b/306271332): Create a new task.
  int task_id_ = 10;
  // Tasks for the loaded list.
  std::vector<std::unique_ptr<api::Task>> tasks_data_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TASKS_PROVIDER_H_
