// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TASKS_PROVIDER_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TASKS_PROVIDER_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"

namespace ash {

// Represents a task.
struct ASH_EXPORT FocusModeTask {
  FocusModeTask();
  ~FocusModeTask();
  FocusModeTask(const FocusModeTask&);
  FocusModeTask(FocusModeTask&&);
  FocusModeTask& operator=(const FocusModeTask&);
  FocusModeTask& operator=(FocusModeTask&&);

  // TODO: Replace the condition below with `FocusModeTask::IsValid()`.
  bool empty() const { return task_list_id.empty(); }

  std::string task_list_id;
  std::string task_id;
  std::string title;

  // The time when this task was last updated.
  base::Time updated;

  // Optional due time for the task.
  std::optional<base::Time> due;
};

// A specialized interface that Focus Mode can use to fetch a filtered list of
// tasks to display. Currently only provides dummy data.
class ASH_EXPORT FocusModeTasksProvider {
 public:
  // Done callback for `AddTask` and `UpdateTaskTitle`. If the request completes
  // successfully, `task_entry` points to the newly created or updated
  // `FocusModeTask`, or an empty `FocusModeTask` with nullptr members
  // otherwise.
  using OnTaskSavedCallback =
      base::OnceCallback<void(const FocusModeTask& task_entry)>;

  FocusModeTasksProvider();
  FocusModeTasksProvider(const FocusModeTasksProvider&) = delete;
  FocusModeTasksProvider& operator=(const FocusModeTasksProvider&) = delete;
  ~FocusModeTasksProvider();

  // Provides a sorted list of `FocusModeTask` instances that can be displayed
  // in Focus Mode.
  std::vector<FocusModeTask> GetSortedTaskList() const;

  // Creates a new task with name `title` and adds it to `task_list_`. Returns
  // the added `FocusModeTask` in `callback`, or an empty `FocusModeTask` if an
  // error has occurred.
  void AddTask(const std::string& title, OnTaskSavedCallback callback);

  // Finds the task by `task_list_id` and `task_id` and updates the task title
  // and completion status. Returns a `FocusModeTask` in `callback`, or an empty
  // `FocusModeTask` if the task could not be found or an error has occurred.
  void UpdateTask(const std::string& task_list_id,
                  const std::string& task_id,
                  const std::string& title,
                  bool completed,
                  OnTaskSavedCallback callback);

 private:
  // Helper function for inserting `FocusModeTask` instances into
  // `sorted_tasks_`.
  void InsertTask(FocusModeTask task);

  // ID counter for creating tasks. Start from above where IDs in
  // `kTaskInitializationData` end to avoid conflicts.
  // TODO(b/306271332): Create a new task.
  int task_id_ = 10;

  // Entries here are sorted in the following priority order:
  // 1. Entries containing `Task`s which are past due.
  // 2. Entries containing `Task`s which are due in the next 24 hours.
  // 3. All other entries.
  // Entries within each group are sorted by their `Task`'s update date.
  std::vector<FocusModeTask> sorted_tasks_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TASKS_PROVIDER_H_
