// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_TASKS_GLANCEABLES_TASKS_CLIENT_H_
#define ASH_GLANCEABLES_TASKS_GLANCEABLES_TASKS_CLIENT_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"
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

  // Fetches all the authenticated user's task lists and invokes `callback` when
  // done.
  virtual void GetTaskLists(GetTaskListsCallback callback) = 0;

  // Fetches all tasks in the specified task list (`task_list_id` must not be
  // empty) and invokes `callback` when done.
  virtual void GetTasks(const std::string& task_list_id,
                        GetTasksCallback callback) = 0;

 protected:
  virtual ~GlanceablesTasksClient() = default;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_TASKS_GLANCEABLES_TASKS_CLIENT_H_
