// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TASKS_MODEL_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TASKS_MODEL_H_

#include <optional>
#include <vector>

#include "ash/ash_export.h"
#include "ash/system/focus_mode/focus_mode_tasks_provider.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace ash {

class ASH_EXPORT FocusModeTasksModel final {
 public:
  // Options struct used to add or update a task.
  struct TaskUpdate {
    TaskUpdate();
    TaskUpdate(const TaskUpdate&);
    ~TaskUpdate();

    static TaskUpdate CompletedUpdate(const TaskId& task_id);
    static TaskUpdate TitleUpdate(const TaskId& task_id,
                                  std::string_view title);
    static TaskUpdate NewTask(std::string_view title);

    std::optional<TaskId> task_id;
    std::optional<std::string> title;
    std::optional<bool> completed;
  };

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnSelectedTaskChanged(
        const std::optional<FocusModeTask>& selected_task) = 0;
    virtual void OnTasksUpdated(const std::vector<FocusModeTask>& tasks) = 0;
    virtual void OnTaskCompleted(const FocusModeTask& completed_task) = 0;
  };

  // Interface to enable the model to propagate updates/requests to the server.
  class Delegate {
   public:
    // Fetch a single task by id. Primarily used for tasks loaded from
    // preferences.
    using FetchTaskCallback =
        base::OnceCallback<void(const std::optional<FocusModeTask>& task)>;
    virtual void FetchTask(const TaskId& task_id,
                           FetchTaskCallback callback) = 0;

    // Initiate a request for tasks. Implementer is expected to call
    // `SetTaskList()` if this is successful.
    virtual void FetchTasks() = 0;

    // Initiate a request to add the task specified by `update`. Set a callback
    // to update the id.
    virtual void AddTask(const TaskUpdate& update,
                         FetchTaskCallback callback) = 0;

    // Update an existing task described by `update`.
    virtual void UpdateTask(const TaskUpdate& update) = 0;
  };

  FocusModeTasksModel();

  FocusModeTasksModel(const FocusModeTasksModel&) = delete;
  FocusModeTasksModel& operator=(const FocusModeTasksModel&) = delete;

  ~FocusModeTasksModel();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void SetDelegate(base::WeakPtr<Delegate> delegate);

  // Initiate a request for updated task data from the server.
  void RequestUpdate();

  // Mark the task with `task_id` as the selected task. If it is different than
  // the currently selected task, it will cause an `OnSelectedTaskChanged()`
  // event.
  bool SetSelectedTask(const TaskId& task_id);

  // Select `task` as the selected task. Will attempt to find the task in the
  // task list by matching the id. If it cannot be found, the task will be added
  // to the list and submitted as a new task to the server.
  void SetSelectedTask(const FocusModeTask& task);

  // Set the selected task from preferences which may be partially constructed.
  // This may trigger multiple events if the downloaded task data updates
  // `task`.
  void SetSelectedTaskFromPrefs(const TaskId& task);

  // Clears the selected task and triggers `Observer::OnSelectedTaskChanged()`
  // if appropriate.
  void ClearSelectedTask();

  // Clears all the cached tasks data immediately. Will also notify observers of
  // this change.
  void Reset();

  // Set the current task list as `tasks`. This will trigger an
  // `OnTasksUpdated()` event. If this causes a change to the selected task, may
  // cause an `OnSelectedTaskChanged()` event.
  void SetTaskList(std::vector<FocusModeTask>&& tasks);

  // Updates the task with the matching `task_id` with the information from
  // `task_update`. If the task is the selected task, this will cause an
  // `OnSelectedTaskChanged()` event. This does NOT trigger `OnTasksUpdated()`.
  void UpdateTask(const TaskUpdate& task_update);

  const std::vector<FocusModeTask>& tasks() const;
  const FocusModeTask* selected_task() const;

  const TaskId& PrefTaskIdForTesting() const;

 private:
  void OnTaskAdded(const std::optional<FocusModeTask>& fetched_task);

  void OnPrefTaskFetched(const std::optional<FocusModeTask>& fetched_task);

  void OnSelectedTaskFetched(const std::optional<FocusModeTask>& fetched_task);

  // Inserts `task` at the front of `tasks_` then resets `selected_task_` and
  // `pending_task_` pointers to the matching tasks in `tasks_`.
  FocusModeTask* InsertTaskIntoTaskList(FocusModeTask&& task);

  base::WeakPtr<Delegate> delegate_;

  // A partially populated task set from preferences while we wait to finish
  // fetching tasks.
  std::optional<TaskId> pref_task_id_;

  std::vector<FocusModeTask> tasks_;
  raw_ptr<FocusModeTask> selected_task_;

  // A task that was created by the user but has not been synced to the server
  // so it does not have TaskId.
  raw_ptr<FocusModeTask> pending_task_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<FocusModeTasksModel> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TASKS_MODEL_H_
