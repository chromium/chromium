// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TASKS_PROVIDER_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TASKS_PROVIDER_H_

#include <compare>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/system/focus_mode/focus_mode_retry_util.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "google_apis/common/api_error_codes.h"
#include "ui/base/models/list_model.h"

namespace ash {

namespace api {
struct Task;
}

class TaskFetcher;

// Encapsulate information required to uniquely identify a task. Tasks are
// expected to be referenced within a list. However, we treat tasks as a flat
// collection so the list id needs to be retained.
struct TaskId {
  bool empty() const { return !pending && (id.empty() || list_id.empty()); }

  // Returns true if the task id is suitable for retrieval from the Tasks API.
  bool IsValid() const;

  std::strong_ordering operator<=>(const TaskId& other) const;
  bool operator==(const TaskId& other) const = default;
  bool operator<(const TaskId& other) const = default;

  std::string list_id;
  std::string id;

  // If true, the task is waiting to be sent to the server and does not have a
  // valid `list_id` or `id`.
  bool pending = false;
};

// Represents a task.
struct ASH_EXPORT FocusModeTask {
  FocusModeTask();
  ~FocusModeTask();
  FocusModeTask(const FocusModeTask&);
  FocusModeTask(FocusModeTask&&);
  FocusModeTask& operator=(const FocusModeTask&);
  FocusModeTask& operator=(FocusModeTask&&);

  // TODO: Replace the condition below with `FocusModeTask::IsValid()`.
  bool empty() const { return task_id.empty(); }

  TaskId task_id;
  std::string title;

  bool completed;

  // The time when this task was last updated.
  base::Time updated;

  // Optional due time for the task.
  std::optional<base::Time> due;
};

// A specialized interface that Focus Mode can use to fetch a filtered list of
// tasks to display.
class ASH_EXPORT FocusModeTasksProvider {
 public:
  // Done callback for `AddTask` and `UpdateTaskTitle`. If the request completes
  // successfully, `task_entry` points to the newly created or updated
  // `FocusModeTask`, or an empty `FocusModeTask` with nullptr members
  // otherwise.
  using OnTaskSavedCallback =
      base::OnceCallback<void(const FocusModeTask& task_entry)>;

  using OnGetTasksCallback =
      base::OnceCallback<void(const std::vector<FocusModeTask>& tasks)>;

  using OnGetTaskCallback =
      base::OnceCallback<void(const FocusModeTask& task_entry)>;

  FocusModeTasksProvider();
  FocusModeTasksProvider(const FocusModeTasksProvider&) = delete;
  FocusModeTasksProvider& operator=(const FocusModeTasksProvider&) = delete;
  ~FocusModeTasksProvider();

  // Provides a sorted list of `FocusModeTask` instances that can be displayed
  // in Focus Mode. The provided `callback` is invoked asynchronously when tasks
  // have been fetched.
  void GetSortedTaskList(OnGetTasksCallback callback);

  // Gets an individual task from the `task_list_id` with `task_id`. Since
  // completed tasks will not be returned by the delegate, we will update the
  // `completed` field to signifiy if the task has been completed or not.
  // Returns a `FocusModeTask` in `callback`, or an empty `FocusModeTask` if an
  // error has occurred.
  void GetTask(const std::string& task_list_id,
               const std::string& task_id,
               OnGetTaskCallback callback);

  // Creates a new task with name `title` and adds it to `task_list_`. Returns
  // the added `FocusModeTask` in `callback`, or an empty `FocusModeTask` if an
  // error has occurred. Note that this will clear the internal cache.
  void AddTask(const std::string& title, OnTaskSavedCallback callback);

  // Finds the task by `task_list_id` and `task_id` and updates the task title
  // and completion status. Returns a `FocusModeTask` in `callback`, or an empty
  // `FocusModeTask` if the task could not be found or an error has occurred.
  void UpdateTask(const std::string& task_list_id,
                  const std::string& task_id,
                  const std::string& title,
                  bool completed,
                  OnTaskSavedCallback callback);

  // This kicks off a fetch of tasks from the backend.
  void ScheduleTaskListUpdate();

  // Clears all the cached tasks data.
  void Reset();

  const std::vector<FocusModeTask> TasksForTesting() const;

 private:
  void OnTasksFetched();
  void OnTasksFetchedForTask(
      const base::Time start_time,
      const std::string& task_list_id,
      const std::string& task_id,
      OnGetTaskCallback callback,
      bool success,
      std::optional<google_apis::ApiErrorCode> http_error,
      const ui::ListModel<api::Task>* api_tasks);
  void OnTaskAdded(const base::Time start_time,
                   const std::string& title,
                   OnTaskSavedCallback callback,
                   google_apis::ApiErrorCode http_error,
                   const api::Task* api_task);
  void OnTaskUpdated(const base::Time start_time,
                     const std::string& task_list_id,
                     const std::string& task_id,
                     const std::string& title,
                     bool completed,
                     OnTaskSavedCallback callback,
                     google_apis::ApiErrorCode http_error,
                     const api::Task* api_task);

  // Requests the server to add the new task.
  void AddTaskInternal(const std::string& title, OnTaskSavedCallback callback);

  // Requests the server to update the existing task.
  void UpdateTaskInternal(const std::string& task_list_id,
                          const std::string& task_id,
                          const std::string& title,
                          bool completed,
                          OnTaskSavedCallback callback);

  // Called only after the add or update request is successful.
  void UpdateOrInsertTask(const std::string& task_list_id,
                          const api::Task* api_task,
                          OnTaskSavedCallback callback);

  // Returns cached tasks according to this sort order:
  // 1. Entries added/updated by the user during the lifetime of this provider.
  // 2. Entries containing `Task`s which are past due.
  // 3. Entries containing `Task`s which are due in the next 24 hours.
  // 4. All other entries.
  // Entries within each group are sorted by their `Task`'s update date.
  std::vector<FocusModeTask> GetSortedTasksImpl();

  // Cache of tasks retrieved from the API.
  std::vector<FocusModeTask> tasks_;

  // Pending UI requests to get all tasks.
  std::vector<OnGetTasksCallback> get_tasks_requests_;

  // The ID of the task list to use when creating new tasks. This will be empty
  // until tasks have been fetched.
  std::string task_list_for_new_task_;

  // Holds a set of tasks that have been created or updated during the lifetime
  // of the provider. These tasks are pushed to the front of the sort order.
  base::flat_set<TaskId> created_task_ids_;

  // Holds a set of tasks that have been deleted during the lifetime of the
  // provider.
  base::flat_set<TaskId> deleted_task_ids_;

  // Populated when the provider is requesting tasks from the API, otherwise
  // empty.
  std::unique_ptr<TaskFetcher> task_fetcher_;

  // The timestamp of the last task fetch.
  base::Time task_fetch_time_;

  FocusModeRetryState get_task_retry_state_;

  // Retry states for adding and updating tasks.
  FocusModeRetryState add_task_retry_state_;
  FocusModeRetryState update_task_retry_state_;

  base::WeakPtrFactory<FocusModeTasksProvider> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TASKS_PROVIDER_H_
