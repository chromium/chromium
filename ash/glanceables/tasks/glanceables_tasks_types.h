// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_TASKS_GLANCEABLES_TASKS_TYPES_H_
#define ASH_GLANCEABLES_TASKS_GLANCEABLES_TASKS_TYPES_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/time/time.h"

namespace ash {

// Lightweight TaskList definition to separate API and ash/ui-friendly types.
// Contains information that describes a single task list. All values are from
// the API resource
// https://developers.google.com/tasks/reference/rest/v1/tasklists.
struct ASH_EXPORT GlanceablesTaskList {
  GlanceablesTaskList() = delete;
  GlanceablesTaskList(const std::string& id,
                      const std::string& title,
                      const base::Time& updated);
  ~GlanceablesTaskList();

  // Task list identifier.
  const std::string id;

  // Title of the task list.
  const std::string title;

  // Last modification time of the task list.
  const base::Time updated;
};

// Lightweight Task definition to separate API and ash/ui-friendly types.
// The most significant difference is that API tasks are flat (subtask-task
// relationship is expressed by parent id property), but here they are
// represented as a tree structure. All values are from the API resource
// https://developers.google.com/tasks/reference/rest/v1/tasks.
struct ASH_EXPORT GlanceablesTask {
  GlanceablesTask() = delete;
  GlanceablesTask(const std::string& id,
                  const std::string& title,
                  bool completed,
                  const std::vector<GlanceablesTask>& subtasks);
  GlanceablesTask(const GlanceablesTask&);
  ~GlanceablesTask();

  // Task identifier.
  const std::string id;

  // Title of the task.
  const std::string title;

  // Indicates whether the task is completed (has "status" field equals to
  // "completed" on the API side).
  bool completed;

  // Subtasks of the task (pre-grouped tasks that have "parent" field equals to
  // `id` on the API side).
  const std::vector<GlanceablesTask> subtasks;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_TASKS_GLANCEABLES_TASKS_TYPES_H_
