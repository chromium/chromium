// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_TASKS_GLANCEABLES_TASKS_TYPES_H_
#define ASH_GLANCEABLES_TASKS_GLANCEABLES_TASKS_TYPES_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// Lightweight TaskList definition to separate API and ash/ui-friendly types.
// Contains information that describes a single task list. All values are from
// the API resource
// https://developers.google.com/tasks/reference/rest/v1/tasklists.
struct ASH_EXPORT GlanceablesTaskList {
  GlanceablesTaskList(const std::string& id,
                      const std::string& title,
                      const base::Time& updated);
  GlanceablesTaskList(const GlanceablesTaskList&) = delete;
  GlanceablesTaskList& operator=(const GlanceablesTaskList&) = delete;
  ~GlanceablesTaskList();

  // Task list identifier.
  const std::string id;

  // Title of the task list.
  const std::string title;

  // Last modification time of the task list.
  const base::Time updated;
};

// Lightweight Task definition to separate API and ash/ui-friendly types.
// All values are from the API resource
// https://developers.google.com/tasks/reference/rest/v1/tasks.
struct ASH_EXPORT GlanceablesTask {
  GlanceablesTask(const std::string& id,
                  const std::string& title,
                  bool completed,
                  const absl::optional<base::Time>& due,
                  bool has_subtasks,
                  bool has_email_link,
                  bool has_notes);
  GlanceablesTask(const GlanceablesTask&) = delete;
  GlanceablesTask& operator=(const GlanceablesTask&) = delete;
  ~GlanceablesTask();

  // Task identifier.
  const std::string id;

  // Title of the task.
  const std::string title;

  // Indicates whether the task is completed (has "status" field equals to
  // "completed" on the API side).
  const bool completed;

  // Optional due date of the task.
  const absl::optional<base::Time> due;

  // Indicates whether the task has subtasks in it.
  const bool has_subtasks;

  // Indicates whether the task has an attached email link.
  const bool has_email_link;

  // Indicates whether the task has additional notes.
  const bool has_notes;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_TASKS_GLANCEABLES_TASKS_TYPES_H_
