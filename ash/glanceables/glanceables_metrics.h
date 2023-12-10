// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_GLANCEABLES_METRICS_H_
#define ASH_GLANCEABLES_GLANCEABLES_METRICS_H_

#include "ash/ash_export.h"
#include "base/time/time.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace ash {

enum class StudentAssignmentsListType;

enum class TasksLaunchSource {
  kHeaderButton = 0,
  kAddNewTaskButton = 1,
  kFooterButton = 2,
  kMaxValue = kFooterButton,
};

void RecordActiveTaskListChanged();

void RecordTaskMarkedAsCompleted(bool complete);

void RecordTasksLaunchSource(TasksLaunchSource source);

void RecordAddTaskButtonShown();

// Records "Add new task" button impression vs. interaction for new Tasks users
// (users with only one tasks list and zero tasks in it) and only for Trusted
// Testers UI.
void RecordAddTaskButtonUsageForNewTasksUsersTT(bool pressed);

void RecordLoginToShowTime(base::TimeDelta login_to_show_time);

void RecordTotalShowTime(base::TimeDelta total_show_time);

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

// Record the number of times that the student assignment list changed.
void RecordStudentSelectedListChangeCount(int change_count);

// Record that the `list_type` was selected.
void RecordStudentAssignmentListSelected(StudentAssignmentsListType list_type);

}  // namespace ash

#endif  // ASH_GLANCEABLES_GLANCEABLES_METRICS_H_
