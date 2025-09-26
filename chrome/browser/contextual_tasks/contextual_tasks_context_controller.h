// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_CONTROLLER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_CONTROLLER_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sessions/core/session_id.h"

namespace contextual_tasks {

class ContextualTasksContextController : public KeyedService {
 public:
  ~ContextualTasksContextController() override;

  // Retrieve a snapshot of all current contextual tasks.
  // The result is a copy of the original data, so mutate operations will have
  // no effect on internal data.
  virtual void GetTasks(
      base::OnceCallback<void(std::vector<ContextualTask>)> callback) = 0;

  // Retrieve a specific `ContextualTask` with the given `task_id`.
  virtual void GetTask(
      const base::Uuid& task_id,
      base::OnceCallback<void(std::optional<ContextualTask>)> callback) = 0;

  // Associates a tab, identified by its `SessionID`, with a `ContextualTask`.
  // This is used to mark a task as "selected" for a given tab.
  virtual void AssociateTabWithTask(SessionID tab_session_id,
                                    const base::Uuid& task_id) = 0;

  // Gets the currently selected contextual task for a given tab.
  // The `selected_task_callback` will receive the task if one is selected,
  // or `std::nullopt` otherwise.
  virtual void GetSelectedTaskForTab(
      SessionID tab_session_id,
      base::OnceCallback<void(std::optional<ContextualTask>)>
          selected_task_callback) = 0;

 protected:
  ContextualTasksContextController();
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_CONTROLLER_H_
