// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_TASKS_GLANCEABLES_TASKS_CLIENT_H_
#define ASH_GLANCEABLES_TASKS_GLANCEABLES_TASKS_CLIENT_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"

namespace ash {

struct GlanceablesTask;
struct GlanceablesTaskList;

// Interface for the tasks browser client.
class ASH_EXPORT GlanceablesTasksClient {
 public:
  using GetTaskListsCallback = base::OnceCallback<void(
      const std::vector<GlanceablesTaskList>& task_lists)>;
  using GetTasksCallback =
      base::OnceCallback<void(const std::vector<GlanceablesTask>& tasks)>;

  // Fetches all the authenticated user's task lists and invokes `callback` when
  // done.
  // Returned `base::OnceClosure` can cancel the api call.
  virtual base::OnceClosure GetTaskLists(GetTaskListsCallback callback) = 0;

  // Fetches all tasks in the specified task list (`task_list_id` must not be
  // empty) and invokes `callback` when done.
  // Returned `base::OnceClosure` can cancel the api call.
  virtual base::OnceClosure GetTasks(GetTasksCallback callback,
                                     const std::string& task_list_id) = 0;

 protected:
  virtual ~GlanceablesTasksClient() = default;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_TASKS_GLANCEABLES_TASKS_CLIENT_H_
