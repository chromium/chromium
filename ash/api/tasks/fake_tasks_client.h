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
#include "ui/base/models/list_model.h"

namespace ash::api {

struct Task;
struct TaskList;

class ASH_EXPORT FakeTasksClient : public TasksClient {
 public:
  explicit FakeTasksClient(base::Time tasks_due_time);
  FakeTasksClient(const FakeTasksClient&) = delete;
  FakeTasksClient& operator=(const FakeTasksClient&) = delete;
  ~FakeTasksClient() override;

  std::vector<std::string> pending_completed_tasks() const {
    return pending_completed_tasks_;
  }

  int completed_task_count() { return completed_tasks_; }

  // TasksClient:
  void GetTaskLists(GetTaskListsCallback callback) override;
  void GetTasks(const std::string& task_list_id,
                GetTasksCallback callback) override;
  void MarkAsCompleted(const std::string& task_list_id,
                       const std::string& task_id,
                       bool checked) override;
  void AddTask(const std::string& task_list_id,
               const std::string& title) override;
  void UpdateTask(const std::string& task_list_id,
                  const std::string& task_id,
                  const std::string& title,
                  TasksClient::UpdateTaskCallback callback) override;
  void OnGlanceablesBubbleClosed(OnAllPendingCompletedTasksSavedCallback
                                     callback = base::DoNothing()) override;

  // Returns `bubble_closed_count_`, while also resetting the counter.
  int GetAndResetBubbleClosedCount();

  // Runs `pending_get_tasks_callbacks_` and returns their number.
  size_t RunPendingGetTasksCallbacks();

  // Runs `pending_get_task_lists_callbacks_` and returns their number.
  size_t RunPendingGetTaskListsCallbacks();

  void set_paused(bool paused) { paused_ = paused; }

  ui::ListModel<TaskList>* task_lists() { return task_lists_.get(); }

 private:
  void PopulateTasks(base::Time tasks_due_time);
  void PopulateTaskLists(base::Time tasks_due_time);

  // All available task lists.
  std::unique_ptr<ui::ListModel<TaskList>> task_lists_;

  // Tracks completed tasks and the task list they belong to.
  std::vector<std::string> pending_completed_tasks_;

  // All available tasks grouped by task list id.
  base::flat_map<std::string, std::unique_ptr<ui::ListModel<Task>>>
      tasks_in_task_lists_;

  // Number of times `OnGlanceablesBubbleClosed()` has been called.
  int bubble_closed_count_ = 0;
  int completed_tasks_ = 0;

  // If `false` - callbacks executed immediately. If `true` - callbacks get
  // saved to the corresponding list and executed once
  // `RunPending**Callbacks()` is called.
  bool paused_ = false;
  std::list<base::OnceClosure> pending_get_tasks_callbacks_;
  std::list<base::OnceClosure> pending_get_task_lists_callbacks_;
};

}  // namespace ash::api

#endif  // ASH_API_TASKS_FAKE_TASKS_CLIENT_H_
