// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/tasks/test/glanceables_tasks_test_util.h"

#include <memory>

#include "ash/api/tasks/fake_tasks_client.h"
#include "ash/api/tasks/tasks_types.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace ash::glanceables_tasks_test_util {

namespace {

constexpr char kList1Id[] = "TaskListID1";
constexpr char kList2Id[] = "TaskListID2";
constexpr char kList3Id[] = "TaskListID3";
constexpr char kList4Id[] = "TaskListID4";
constexpr char kList5Id[] = "TaskListID5";
constexpr char kList6Id[] = "TaskListID6";

struct TaskListData {
  const char* id;
  const char* title;
};

struct TaskData {
  const char* list_id;
  const char* task_id;
  const char* title;
  const bool completed;
};

constexpr TaskListData kTaskListInitializationData[] = {
    {.id = kList1Id, .title = "Task List 1 Title"},
    {.id = kList2Id, .title = "Task List 2 Title"},
    {.id = kList3Id, .title = "Task List 3 Title (empty)"},
    {.id = kList4Id, .title = "Task List 4 Title (empty)"},
    {.id = kList5Id, .title = "Task List 5 Title (empty)"},
    {.id = kList6Id, .title = "Task List 6 Title (empty)"}};

constexpr TaskData kTaskInitializationData[] = {
    {.list_id = kList1Id,
     .task_id = "TaskListItem1",
     .title = "Task List 1 Item 1 Title",
     .completed = false},
    {.list_id = kList1Id,
     .task_id = "TaskListItem2",
     .title = "Task List 1 Item 2 Title",
     .completed = false},
    {.list_id = kList2Id,
     .task_id = "TaskListItem3",
     .title = "Task List 2 Item 1 Title",
     .completed = false},
    {.list_id = kList2Id,
     .task_id = "TaskListItem4",
     .title = "Task List 2 Item 2 Title",
     .completed = false},
    {.list_id = kList2Id,
     .task_id = "TaskListItem5",
     .title = "Task List 2 Item 3 Title",
     .completed = false}};

}  // namespace

std::unique_ptr<api::FakeTasksClient> InitializeFakeTasksClient(
    const base::Time& tasks_time) {
  std::unique_ptr<api::FakeTasksClient> tasks_client =
      std::make_unique<api::FakeTasksClient>();
  for (auto [id, title] : kTaskListInitializationData) {
    tasks_client->AddTaskList(
        std::make_unique<api::TaskList>(id, title, /*updated=*/tasks_time));
  }

  for (auto [list_id, task_id, title, completed] : kTaskInitializationData) {
    tasks_client->AddTask(
        list_id,
        std::make_unique<api::Task>(
            task_id, title, /*due=*/tasks_time, completed,
            /*has_subtasks=*/false, /*has_email_link=*/false,
            /*has_notes=*/false, /*updated=*/tasks_time,
            /*web_view_link=*/
            GURL(base::StrCat({"https://tasks.google.com/task/", task_id})),
            api::Task::OriginSurfaceType::kRegular));
  }

  return tasks_client;
}

}  // namespace ash::glanceables_tasks_test_util
