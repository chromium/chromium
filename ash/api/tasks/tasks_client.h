// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_API_TASKS_TASKS_CLIENT_H_
#define ASH_API_TASKS_TASKS_CLIENT_H_

#include <string>

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "google_apis/common/api_error_codes.h"
#include "ui/base/models/list_model.h"

namespace ash::api {

struct Task;
struct TaskList;

// Interface for the tasks browser client.
class ASH_EXPORT TasksClient {
 public:
  // This function can fetch data from either the local cache or from the API
  // server. `success` indicates if the function fetches data successfully. The
  // optional `http_error` indicates the HTTP error encountered if exists.
  using GetTaskListsCallback = base::OnceCallback<void(
      bool success,
      std::optional<google_apis::ApiErrorCode> http_error,
      const ui::ListModel<TaskList>* task_lists)>;
  using GetTasksCallback = base::OnceCallback<void(
      bool success,
      std::optional<google_apis::ApiErrorCode> http_error,
      const ui::ListModel<Task>* tasks)>;

  // Done callback for `AddTask` and `UpdateTask`. If the request completes
  // successfully, `task` points to the newly created or updated task, or
  // `nullptr` otherwise. `http_error` is the http error code returned from the
  // request of the Google Tasks API.
  using OnTaskSavedCallback =
      base::OnceCallback<void(google_apis::ApiErrorCode http_error,
                              const Task* task)>;

  // Verifies if the Tasks integration is disabled by admin by checking:
  // 1) if the integrations is not listed in
  //    `prefs::kGoogleCalendarIntegrationName`,
  // 2) if the Calendar web app (home app for Tasks) is disabled by policy,
  // 3) if access to the Tasks web UI is blocked by policy.
  virtual bool IsDisabledByAdmin() const = 0;

  // Returns the list model of the task list that was cached when the
  // glanceables was previously opened. Returns a nullptr if there is no cached
  // list.
  virtual const ui::ListModel<api::TaskList>* GetCachedTaskLists() = 0;

  // Retrieves all the authenticated user's task lists and invokes `callback`
  // when done. If `force_fetch` is true, new data will be pulled from the
  // Google Tasks API.
  virtual void GetTaskLists(bool force_fetch,
                            GetTaskListsCallback callback) = 0;

  // Returns the list model of the tasks in task list with `task_list_id` that
  // was cached when the glanceables was previously opened. Returns a nullptr if
  // there is no cached tasks or the list does not exist.
  virtual const ui::ListModel<api::Task>* GetCachedTasksInTaskList(
      const std::string& task_list_id) = 0;

  // Retrieves all tasks in the specified task list (`task_list_id` must not be
  // empty) and invokes `callback` when done. If `force_fetch` is true, new data
  // will be pulled from the Google Tasks API.
  virtual void GetTasks(const std::string& task_list_id,
                        bool force_fetch,
                        GetTasksCallback callback) = 0;

  // Marks the specified task in the specified task list as completed. Only root
  // tasks can be marked as completed (all subtasks will be marked as completed
  // automatically by the API). Changes are propagated server side after calling
  // OnGlanceablesBubbleClosed.
  virtual void MarkAsCompleted(const std::string& task_list_id,
                               const std::string& task_id,
                               bool completed) = 0;

  // Adds a new task to the specified task list with the specified title.
  virtual void AddTask(const std::string& task_list_id,
                       const std::string& title,
                       OnTaskSavedCallback callback) = 0;

  // Updates the specified task in the specified task list.
  virtual void UpdateTask(const std::string& task_list_id,
                          const std::string& task_id,
                          const std::string& title,
                          bool completed,
                          OnTaskSavedCallback callback) = 0;

  // Marks cached Task and TaskList data as "not fresh". This will also fail any
  // pending callbacks.
  virtual void InvalidateCache() = 0;

  // Returns the time when the tasks in the task list with `task_list_id` is
  // last updated from the client. Returns a nullptr if the task list with
  // `task_list_id` has not been updated in current session.
  virtual std::optional<base::Time> GetTasksLastUpdateTime(
      const std::string& task_list_id) const = 0;

  // Method called when the glanceables bubble UI closes. The client can use
  // this as a signal to invalidate cached tasks data.
  virtual void OnGlanceablesBubbleClosed(base::OnceClosure callback) = 0;

  virtual ~TasksClient() = default;
};

}  // namespace ash::api

#endif  // ASH_API_TASKS_TASKS_CLIENT_H_
