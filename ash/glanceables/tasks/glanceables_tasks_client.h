// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_TASKS_GLANCEABLES_TASKS_CLIENT_H_
#define ASH_GLANCEABLES_TASKS_GLANCEABLES_TASKS_CLIENT_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "ui/base/models/list_model.h"

namespace ash {

struct GlanceablesTask;
struct GlanceablesTaskList;

// Interface for the tasks browser client.
class ASH_EXPORT GlanceablesTasksClient {
 public:
  using GetTaskListsCallback =
      base::OnceCallback<void(ui::ListModel<GlanceablesTaskList>* task_lists)>;
  using GetTasksCallback =
      base::OnceCallback<void(ui::ListModel<GlanceablesTask>* tasks)>;
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

  // Method called when the glanceables bubble UI closes. The client can use
  // this as a signal to invalidate cached tasks data.
  virtual void OnGlanceablesBubbleClosed(
      OnAllPendingCompletedTasksSavedCallback callback = base::DoNothing()) = 0;

  virtual ~GlanceablesTasksClient() = default;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_TASKS_GLANCEABLES_TASKS_CLIENT_H_
