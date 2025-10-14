// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_CONTROLLER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_CONTROLLER_H_

#include <optional>
#include <set>
#include <vector>

#include "base/functional/callback.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sessions/core/session_id.h"

namespace contextual_tasks {

// Represents the eligibility status for contextual tasks features.
// This is used to determine if any backend is available and if the feature
// is enabled.
struct FeatureEligibility {
  // Whether the contextual tasks feature flag is enabled.
  bool contextual_tasks_enabled;
  // Whether the AIM backend is eligible for use.
  bool aim_eligible;

  bool IsEligible() { return contextual_tasks_enabled && aim_eligible; }
};

class ContextualTasksContextController : public KeyedService {
 public:
  ~ContextualTasksContextController() override;

  // Creates a new `ContextualTask` which represents a user's journey to
  // accomplish a goal.
  virtual ContextualTask CreateTask() = 0;

  // Assigns a server-side conversation, identified by `server_id`, to a task.
  // If a task with the given `task_id` does not exist, it will be created.
  virtual void AssignThreadToTask(const base::Uuid& task_id,
                                  ThreadType thread_type,
                                  const std::string& server_id,
                                  const std::string& conversation_turn_id,
                                  std::optional<std::string> title) = 0;

  // Updates the `conversation_turn_id` for a thread associated with a task.
  // The thread is identified by its `server_id`.
  // If a task with the given `task_id` does not exist, it will be created.
  virtual void UpdateThreadTurnId(const base::Uuid& task_id,
                                  ThreadType thread_type,
                                  const std::string& server_id,
                                  const std::string& conversation_turn_id) = 0;

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

  // Attaches a URL to a `ContextualTask`.
  virtual void AttachUrlToTask(const base::Uuid& task_id, const GURL& url) = 0;
  // Detaches a URL from a `ContextualTask`.
  virtual void DetachUrlFromTask(const base::Uuid& task_id,
                                 const GURL& url) = 0;

  // Gets the context for a given task. The `context_callback` will receive the
  // a contextual task. If the `sources` set is empty, all available sources
  // will be used. The callback will be invoked with the enriched context, or
  // `nullptr` if the task is not found.
  virtual void GetContextForTask(
      const base::Uuid& task_id,
      const std::set<ContextualTaskContextSource>& sources,
      base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
          context_callback) = 0;

  // Returns whether there are any available backends that are eligible for use.
  virtual FeatureEligibility GetFeatureEligibility() = 0;

 protected:
  ContextualTasksContextController();
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_CONTROLLER_H_
