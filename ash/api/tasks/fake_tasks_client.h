// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_API_TASKS_FAKE_TASKS_CLIENT_H_
#define ASH_API_TASKS_FAKE_TASKS_CLIENT_H_

#include <list>
#include <string>
#include <vector>

#include "ash/api/tasks/tasks_client.h"
#include "ash/ash_export.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "google_apis/common/api_error_codes.h"
#include "ui/base/models/list_model.h"

namespace ash::api {

struct Task;
struct TaskList;

class ASH_EXPORT FakeTasksClient : public TasksClient {
 public:
  FakeTasksClient();
  FakeTasksClient(const FakeTasksClient&) = delete;
  FakeTasksClient& operator=(const FakeTasksClient&) = delete;
  ~FakeTasksClient() override;

  std::vector<std::string> pending_completed_tasks() const {
    return pending_completed_tasks_;
  }

  int completed_task_count() { return completed_tasks_; }

  // TasksClient:
  bool IsDisabledByAdmin() const override;
  const ui::ListModel<api::TaskList>* GetCachedTaskLists() override;
  void GetTaskLists(bool force_fetch, GetTaskListsCallback callback) override;
  const ui::ListModel<api::Task>* GetCachedTasksInTaskList(
      const std::string& task_list_id) override;
  void GetTasks(const std::string& task_list_id,
                bool force_fetch,
                GetTasksCallback callback) override;
  void MarkAsCompleted(const std::string& task_list_id,
                       const std::string& task_id,
                       bool checked) override;
  void AddTask(const std::string& task_list_id,
               const std::string& title,
               TasksClient::OnTaskSavedCallback callback) override;
  void UpdateTask(const std::string& task_list_id,
                  const std::string& task_id,
                  const std::string& title,
                  bool completed,
                  TasksClient::OnTaskSavedCallback callback) override;
  void InvalidateCache() override {}
  std::optional<base::Time> GetTasksLastUpdateTime(
      const std::string& task_list_id) const override;
  void OnGlanceablesBubbleClosed(base::OnceClosure callback) override;

  // Helper function for loading in pre-built `TaskList` objects.
  void AddTaskList(std::unique_ptr<TaskList> task_list_data);

  // Helper function for loading in pre-built `Task` objects.
  void AddTask(const std::string& task_list_id,
               std::unique_ptr<Task> task_data);

  // Deletes the task list with `task_list_id` from `task_lists_`.
  void DeleteTaskList(const std::string& task_list_id);

  void SetTasksLastUpdateTime(base::Time time);

  // Returns `bubble_closed_count_`, while also resetting the counter.
  int GetAndResetBubbleClosedCount();

  // Runs `pending_get_tasks_callbacks_` and returns their number.
  size_t RunPendingGetTasksCallbacks();

  // Runs `pending_get_task_lists_callbacks_` and returns their number.
  size_t RunPendingGetTaskListsCallbacks();

  // Runs `pending_add_task_callbacks_` and returns their number.
  size_t RunPendingAddTaskCallbacks();

  // Runs `pending_update_task_callbacks_` and returns their number.
  size_t RunPendingUpdateTaskCallbacks();

  void set_is_disabled_by_admin(bool is_disabled_by_admin) {
    is_disabled_by_admin_ = is_disabled_by_admin;
  }
  void set_paused(bool paused) { paused_ = paused; }
  void set_paused_on_fetch(bool paused) { paused_on_fetch_ = paused; }
  void set_get_task_lists_error(bool error) { get_task_lists_error_ = error; }
  void set_get_tasks_error(bool error) { get_tasks_error_ = error; }
  void set_http_error(std::optional<google_apis::ApiErrorCode> http_error) {
    http_error_ = http_error;
  }

  ui::ListModel<TaskList>* task_lists() { return task_lists_.get(); }

 private:
  void AddTaskImpl(const std::string& task_list_id,
                   const std::string& title,
                   TasksClient::OnTaskSavedCallback callback);
  void UpdateTaskImpl(const std::string& task_list_id,
                      const std::string& task_id,
                      const std::string& title,
                      bool completed,
                      TasksClient::OnTaskSavedCallback callback);

  // Copies `task_lists_` to `cached_task_lists_` to save the task lists
  // state when the glanceables are closed.
  void CacheTaskLists();

  // Copies the current showing tasks to `cached_tasks_` to save the tasks state
  // when the glanceables are closed.
  void CacheTasks();

  // All available task lists.
  std::unique_ptr<ui::ListModel<TaskList>> task_lists_;

  // The cached task lists that is used before `task_lists_` is simulated to be
  // fetched.
  std::unique_ptr<ui::ListModel<TaskList>> cached_task_lists_;

  // The cached tasks that was shown when the glanceables are closed.
  std::unique_ptr<ui::ListModel<Task>> cached_tasks_;

  // The id of the task list that was shown when the glanceables are closed.
  std::string cached_task_list_id_;

  // Tracks completed tasks and the task list they belong to.
  std::vector<std::string> pending_completed_tasks_;

  // All available tasks grouped by task list id.
  base::flat_map<std::string, std::unique_ptr<ui::ListModel<Task>>>
      tasks_in_task_lists_;

  // Number of times `OnGlanceablesBubbleClosed()` has been called.
  int bubble_closed_count_ = 0;
  int completed_tasks_ = 0;

  // If `true`, GetTaskListsCallback run with failure after data fetching in
  // `GetTaskLists()` is done. This should be set before `GetTaskLists()` is
  // called.
  bool get_task_lists_error_ = false;
  // If `true`, GetTasksCallback run with failure after data fetching in
  // `GetTasks()` is done. This should be set before `GetTasks()` is called.
  bool get_tasks_error_ = false;

  // A http error is required to be set if we simulate send a request to the
  // API. For `GetTaskLists()` and `GetTasks()`, this is optional. For
  // `AddTaskImpl()` or `UpdateTaskImpl()`, this is required.
  std::optional<google_apis::ApiErrorCode> http_error_ = std::nullopt;

  // The last time when the tasks were updated. This is manually set by
  // `SetTasksLastUpdateTime`.
  base::Time last_updated_time_;

  bool is_disabled_by_admin_ = false;

  // If `false` - callbacks are executed immediately; if `true` - callbacks get
  // saved to the corresponding list and executed once
  // `RunPending**Callbacks()` is called.
  bool paused_ = false;

  // Similar to `paused_`, but only moves callbacks to pending callbacks when
  // data fetching is simulated, that is, callbacks are run immediately if the
  // cached data is used.
  bool paused_on_fetch_ = false;
  std::list<base::OnceClosure> pending_get_tasks_callbacks_;
  std::list<base::OnceClosure> pending_get_task_lists_callbacks_;
  std::list<base::OnceClosure> pending_add_task_callbacks_;
  std::list<base::OnceClosure> pending_update_task_callbacks_;
};

}  // namespace ash::api

#endif  // ASH_API_TASKS_FAKE_TASKS_CLIENT_H_
