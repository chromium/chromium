// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_API_TASKS_TASKS_TYPES_H_
#define ASH_API_TASKS_TASKS_TYPES_H_

#include <memory>
#include <optional>
#include <string>

#include "ash/ash_export.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace ash::api {

// Lightweight TaskList definition to separate API and ash/ui-friendly types.
// Contains information that describes a single task list. All values are from
// the API resource
// https://developers.google.com/tasks/reference/rest/v1/tasklists.
struct ASH_EXPORT TaskList {
  TaskList(const std::string& id,
           const std::string& title,
           const base::Time& updated);
  TaskList(const TaskList&) = delete;
  TaskList& operator=(const TaskList&) = delete;
  ~TaskList();

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
struct ASH_EXPORT Task {
  // The type of surface a task originates from.
  enum class OriginSurfaceType {
    // The task is created via regular Tasks UI (app, web).
    kRegular,

    // The task is created and assigned from a document.
    kDocument,

    // The task is created and assigned from a Chat Space.
    kSpace,

    // The task is created and assigned from an unknown surface.
    kUnknown,
  };

  Task(const std::string& id,
       const std::string& title,
       const std::optional<base::Time>& due,
       bool completed,
       bool has_subtasks,
       bool has_email_link,
       bool has_notes,
       const base::Time& updated,
       const GURL& web_view_link,
       OriginSurfaceType origin_surface_type);
  Task(const Task&) = delete;
  Task& operator=(const Task&) = delete;
  ~Task();

  // Task identifier.
  const std::string id;

  // Title of the task.
  std::string title;

  // Optional due date of the task.
  const std::optional<base::Time> due;

  // Indicates whether the task is completed (has "status" field equals to
  // "completed" on the API side).
  bool completed;

  // Indicates whether the task has subtasks in it.
  const bool has_subtasks;

  // Indicates whether the task has an attached email link.
  const bool has_email_link;

  // Indicates whether the task has additional notes.
  const bool has_notes;

  // When the task was last updated.
  base::Time updated;

  // Absolute link to the task in the Google Tasks Web UI.
  GURL web_view_link;

  // The type of surface the task originates from.
  const OriginSurfaceType origin_surface_type = OriginSurfaceType::kRegular;
};

}  // namespace ash::api

#endif  // ASH_API_TASKS_TASKS_TYPES_H_
