// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_TASKS_FAKE_GLANCEABLES_TASKS_CLIENT_H_
#define ASH_GLANCEABLES_TASKS_FAKE_GLANCEABLES_TASKS_CLIENT_H_

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

  FakeGlanceablesTasksClient();
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

 private:
  void PopulateTasks();
  void PopulateTaskLists();

  // All available task lists.
  std::unique_ptr<ui::ListModel<GlanceablesTaskList>> task_lists_;

  // Tracks completed tasks and the task list they belong to.
  std::vector<std::string> completed_tasks_;

  // All available tasks grouped by task list id.
  base::flat_map<std::string, std::unique_ptr<ui::ListModel<GlanceablesTask>>>
      tasks_in_task_lists_;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_TASKS_FAKE_GLANCEABLES_TASKS_CLIENT_H_
