// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_TASKS_FAKE_GLANCEABLES_TASKS_CLIENT_H_
#define ASH_GLANCEABLES_TASKS_FAKE_GLANCEABLES_TASKS_CLIENT_H_

#include <list>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/glanceables/tasks/glanceables_tasks_client.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "ui/base/models/list_model.h"

namespace ash {

struct GlanceablesTask;
struct GlanceablesTaskList;

class ASH_EXPORT FakeGlanceablesTasksClient : public GlanceablesTasksClient {
 public:
  using GetTaskListsCallback =
      base::OnceCallback<void(ui::ListModel<GlanceablesTaskList>* task_lists)>;
  using GetTasksCallback =
      base::OnceCallback<void(ui::ListModel<GlanceablesTask>* tasks)>;
  using MarkAsCompletedCallback = base::OnceCallback<void(bool success)>;

  explicit FakeGlanceablesTasksClient(base::Time tasks_due_time);
  FakeGlanceablesTasksClient(const FakeGlanceablesTasksClient&) = delete;
  FakeGlanceablesTasksClient& operator=(const FakeGlanceablesTasksClient&) =
      delete;
  ~FakeGlanceablesTasksClient() override;

  std::vector<std::string> completed_tasks() const { return completed_tasks_; }

  // GlanceablesTasksClient:
  void GetTaskLists(GetTaskListsCallback callback) override;
  void GetTasks(const std::string& task_list_id,
                GetTasksCallback callback) override;
  void MarkAsCompleted(const std::string& task_list_id,
                       const std::string& task_id,
                       MarkAsCompletedCallback callback) override;
  void OnGlanceablesBubbleClosed() override;

  // Returns `bubble_closed_count_`, while also resetting the counter.
  int GetAndResetBubbleClosedCount();

  // Runs `pending_get_tasks_callbacks_` and returns their number.
  size_t RunPendingGetTasksCallbacks();

  void set_paused(bool paused) { paused_ = paused; }

 private:
  void PopulateTasks(base::Time tasks_due_time);
  void PopulateTaskLists(base::Time tasks_due_time);

  // All available task lists.
  std::unique_ptr<ui::ListModel<GlanceablesTaskList>> task_lists_;

  // Tracks completed tasks and the task list they belong to.
  std::vector<std::string> completed_tasks_;

  // All available tasks grouped by task list id.
  base::flat_map<std::string, std::unique_ptr<ui::ListModel<GlanceablesTask>>>
      tasks_in_task_lists_;

  // Number of times `OnGlanceablesBubbleClosed()` has been called.
  int bubble_closed_count_ = 0;

  // If `false` - callbacks executed immediately. If `true` - callbacks get
  // saved to the corresponding list and executed once
  // `RunPending**Callbacks()` is called.
  bool paused_ = false;
  std::list<base::OnceClosure> pending_get_tasks_callbacks_;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_TASKS_FAKE_GLANCEABLES_TASKS_CLIENT_H_
