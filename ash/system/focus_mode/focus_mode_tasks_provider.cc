// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_tasks_provider.h"

#include <optional>
#include <vector>

#include "ash/api/tasks/tasks_types.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace ash {

namespace {

// Struct representing everything we need to create a basic task.
struct DummyTaskData {
  const char* id;
  const char* title;
  bool completed;
  const char* due_string;
  const char* updated_string;
};

// Task data that we provide the user with initially to play around with.
constexpr DummyTaskData kTaskInitializationData[] = {
    {.id = "9",
     .title = "Task 9",
     .completed = false,
     .due_string = "12 Nov 2023 0:00 GMT",
     .updated_string = "12 Nov 2023 1:00 GMT"},
    {.id = "8",
     .title = "Task 8",
     .completed = false,
     .due_string = "19 Nov 2023 0:00 GMT",
     .updated_string = "14 Nov 2023 6:00 GMT"},
    {.id = "1",
     .title = "Task 1",
     .completed = false,
     .due_string = "20 Nov 2023 0:00 GMT",
     .updated_string = "19 Nov 2023 12:00 GMT"},
    {.id = "4",
     .title = "Task 4",
     .completed = false,
     .due_string = "21 Nov 2023 0:00 GMT",
     .updated_string = "13 Nov 2023 21:00 GMT"},
    {.id = "5",
     .title = "Task 5",
     .completed = false,
     .due_string = "23 Nov 2023 0:00 GMT",
     .updated_string = "18 Nov 2023 8:00 GMT"},
    {.id = "7",
     .title = "Task 7",
     .completed = false,
     .due_string = "24 Nov 2023 0:00 GMT",
     .updated_string = "19 Nov 2023 6:00 GMT"},
    {.id = "2",
     .title = "Task 2",
     .completed = false,
     .due_string = "30 Nov 2023 0:00 GMT",
     .updated_string = "14 Nov 2023 5:00 GMT"},
    {.id = "3",
     .title = "Task 3",
     .completed = false,
     .due_string = "",
     .updated_string = "18 Nov 2023 7:00 GMT"},
    {.id = "6",
     .title = "Task 6",
     .completed = false,
     .due_string = "",
     .updated_string = "15 Nov 2023 13:00 GMT"},
    {.id = "0",
     .title = "Task 0",
     .completed = false,
     .due_string = "",
     .updated_string = "10 Nov 2023 0:00 GMT"}};

FocusModeTask GetTaskFromDummyTask(const DummyTaskData& task_data) {
  FocusModeTask task;

  base::Time update_date, due_date;
  CHECK(base::Time::FromString(task_data.updated_string, &update_date));

  task.task_id = task_data.id;
  task.task_list_id = "1";
  task.due = base::Time::FromString(task_data.due_string, &due_date)
                 ? std::make_optional<base::Time>(due_date)
                 : std::nullopt;
  task.updated = update_date;
  task.title = task_data.title;

  return task;
}

// Used to sort tasks for the carousel.
struct TaskComparator {
  enum class TaskGroupOrdering {
    kPastDue,
    kDueSoon,
    kDueLater,
  };

  bool operator()(const FocusModeTask& lhs, const FocusModeTask& rhs) const {
    auto lhs_group = GetOrdering(lhs);
    auto rhs_group = GetOrdering(rhs);
    if (lhs_group != rhs_group) {
      return lhs_group < rhs_group;
    }

    return lhs.updated > rhs.updated;
  }

  TaskGroupOrdering GetOrdering(const FocusModeTask& entry) const {
    auto remaining = entry.due.value_or(base::Time::Max()) - now;
    if (remaining < base::Hours(0)) {
      return TaskGroupOrdering::kPastDue;
    } else if (remaining < base::Hours(24)) {
      return TaskGroupOrdering::kDueSoon;
    }
    return TaskGroupOrdering::kDueLater;
  }

  base::Time now;
};

}  // namespace

FocusModeTask::FocusModeTask() = default;
FocusModeTask::~FocusModeTask() = default;
FocusModeTask::FocusModeTask(const FocusModeTask&) = default;
FocusModeTask::FocusModeTask(FocusModeTask&&) = default;
FocusModeTask& FocusModeTask::operator=(const FocusModeTask&) = default;
FocusModeTask& FocusModeTask::operator=(FocusModeTask&&) = default;

FocusModeTasksProvider::FocusModeTasksProvider() {
  for (const DummyTaskData& task_data : kTaskInitializationData) {
    InsertTask(GetTaskFromDummyTask(task_data));
  }
}

FocusModeTasksProvider::~FocusModeTasksProvider() = default;

std::vector<FocusModeTask> FocusModeTasksProvider::GetSortedTaskList() const {
  return sorted_tasks_;
}

void FocusModeTasksProvider::AddTask(const std::string& title,
                                     OnTaskSavedCallback callback) {
  FocusModeTask task;
  task.task_list_id = "1";
  task.task_id = base::NumberToString(task_id_++);
  task.title = title;
  task.updated = base::Time::Now();

  InsertTask(task);
  std::move(callback).Run(task);
}

void FocusModeTasksProvider::UpdateTask(const std::string& task_list_id,
                                        const std::string& task_id,
                                        const std::string& title,
                                        bool completed,
                                        OnTaskSavedCallback callback) {
  auto task = base::ranges::find_if(sorted_tasks_, [&](const auto& task) {
    return task.task_id == task_id && task.task_list_id == task_list_id;
  });

  if (task == sorted_tasks_.end()) {
    std::move(callback).Run(FocusModeTask{});
    return;
  }

  task->title = title;

  auto copy = *task;
  if (completed) {
    sorted_tasks_.erase(task);
  }
  std::move(callback).Run(copy);
}

void FocusModeTasksProvider::InsertTask(FocusModeTask task) {
  sorted_tasks_.push_back(std::move(task));
  base::ranges::sort(sorted_tasks_, TaskComparator{base::Time::Now()});
}

}  // namespace ash
