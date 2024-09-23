// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_GLANCEABLES_METRICS_H_
#define ASH_GLANCEABLES_GLANCEABLES_METRICS_H_

#include <string>

#include "ash/ash_export.h"
#include "base/time/time.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace ash {

enum class StudentAssignmentsListType;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Note this should be kept in sync with `ContextualGoogleIntegrationStatus`
// enum in tools/metrics/histograms/metadata/ash/enums.xml.
enum class ContextualGoogleIntegrationStatus {
  kEnabled = 0,
  kDisabledByPolicy = 1,
  kDisabledByAppBlock = 2,
  kDisabledByUrlBlock = 3,
  kMaxValue = kDisabledByUrlBlock,
};

enum class TasksLaunchSource {
  kHeaderButton = 0,
  kAddNewTaskButton = 1,
  kFooterButton = 2,
  kEditInGoogleTasksButton = 3,
  kMaxValue = kEditInGoogleTasksButton,
};

enum class TaskModificationResult {
  kCommitted = 0,
  kCancelled = 1,
  kMaxValue = kCancelled
};

ASH_EXPORT void RecordContextualGoogleIntegrationStatus(
    const std::string& integration_name,
    ContextualGoogleIntegrationStatus status);

void RecordActiveTaskListChanged();

void RecordTaskMarkedAsCompleted(bool complete);

void RecordTasksLaunchSource(TasksLaunchSource source);

// Records a user action that indicates that a user with no tasks preformed an
// action that redirected them to tasks web UI.
void RecordUserWithNoTasksRedictedToTasksUI();

// Records an user action indicating the user triggered tasks glanceables UI to
// add a new task.
void RecordUserStartedAddingTask();

// Records a histogram that tracks whether the UI to add a task resulted in an
// tasks API request to create a new task, or whether the task creation was
// cancelled.
void RecordTaskAdditionResult(TaskModificationResult result);

// Records the number of tasks the user added from the tasks UI. The count is
// scoped to the time a task list is being shown - it will be reset when the
// bubble is reopened, or the user changes the selected task list.
// `added_tasks` - number of tasks added.
// `in_empty_task_list` - whether the task list was empty when selected.
// `first_usage` - whether the user had any tasks at the time the task list was
// selected - this will be true for users that had a single empty task list.
void RecordNumberOfAddedTasks(int added_tasks,
                              bool in_empty_task_list,
                              bool first_usage);

// Records an user action indicating the user clicked on a task title, which
// triggers UI to modify the task.
void RecordUserModifyingTask();

// Records a histogram that tracks whether the UI to modify a task resulted in
// an tasks API request to update the task, or whether the task modification
// was no-op.
void RecordTaskModificationResult(TaskModificationResult result);

void RecordAddTaskButtonShownForTT();

// Records "Add new task" button impression vs. interaction for new Tasks users
// (users with only one tasks list and zero tasks in it) and only for Trusted
// Testers UI.
void RecordAddTaskButtonUsageForNewTasksUsersTT(bool pressed);

void RecordLoginToShowTime(base::TimeDelta login_to_show_time);

void RecordTotalShowTime(base::TimeDelta total_show_time);

void RecordTotalShowTimeForClassroom(base::TimeDelta total_show_time);

void RecordTotalShowTimeForTasks(base::TimeDelta total_show_time);

void RecordClassromInitialLoadTime(bool first_occurrence,
                                   base::TimeDelta load_time);

void RecordClassroomChangeLoadTime(bool success, base::TimeDelta load_time);

void RecordTasksInitialLoadTime(bool first_occurrence,
                                base::TimeDelta load_time);

void RecordTasksChangeLoadTime(base::TimeDelta load_time);

void RecordTasksListChangeCount(int change_count);

// Record the length of time that the `list_type` was shown.
void RecordStudentAssignmentListShowTime(StudentAssignmentsListType list_type,
                                         base::TimeDelta time_shown,
                                         bool default_list);

// Records user actions for user press on an assignment.
// `default_list` - whether the assignment was pressed for the assignment list
// that was shown initially.
void RecordStudentAssignmentPressed(bool default_list);

// Records that the user pressed a header icon in the classroom bubble.
void RecordClassroomHeaderIconPressed();

// Record the number of times that the student assignment list changed.
void RecordStudentSelectedListChangeCount(int change_count);

// Record that the `list_type` was selected.
void RecordStudentAssignmentListSelected(StudentAssignmentsListType list_type);

}  // namespace ash

#endif  // ASH_GLANCEABLES_GLANCEABLES_METRICS_H_
