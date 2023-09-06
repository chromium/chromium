// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_metrics.h"

#include <string>

#include "ash/system/unified/classroom_bubble_student_view.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"

namespace {

constexpr char kLoginToShowTimeHistogram[] =
    "Ash.Glanceables.TimeManagement.LoginToShowTime";
constexpr char kTotalShowTimeHistogram[] =
    "Ash.Glanceables.TimeManagement.TotalShowTime";

constexpr char kTimeManagementTaskPrefix[] =
    "Ash.Glanceables.TimeManagement.Tasks";
constexpr char kTimeManagementClassroomPrefix[] =
    "Ash.Glanceables.TimeManagement.Classroom";

}  // namespace

namespace ash {

void RecordActiveTaskListChanged() {
  base::RecordAction(
      base::UserMetricsAction("Glanceables_Tasks_ActiveTaskListChanged"));
}

void RecordTaskMarkedAsCompleted(bool complete) {
  if (complete) {
    base::RecordAction(
        base::UserMetricsAction("Glanceables_Tasks_TaskMarkedAsCompleted"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("Glanceables_Tasks_TaskMarkedAsIncomplete"));
  }
}

void RecordTasksLaunchSource(TasksLaunchSource source) {
  switch (source) {
    case TasksLaunchSource::kHeaderButton:
      base::RecordAction(base::UserMetricsAction(
          "Glanceables_Tasks_LaunchTasksApp_HeaderButton"));
      break;
    case TasksLaunchSource::kAddNewTaskButton:
      base::RecordAction(base::UserMetricsAction(
          "Glanceables_Tasks_LaunchTasksApp_AddNewTaskButton"));
      break;
    case TasksLaunchSource::kFooterButton:
      base::RecordAction(base::UserMetricsAction(
          "Glanceables_Tasks_LaunchTasksApp_FooterButton"));
      break;
  }
}

void RecordAddTaskButtonShown() {
  base::RecordAction(
      base::UserMetricsAction("Glanceables_Tasks_AddTaskButtonShown"));
}

void RecordLoginToShowTime(base::TimeDelta login_to_show_time) {
  base::UmaHistogramMediumTimes(kLoginToShowTimeHistogram, login_to_show_time);
}

void RecordTotalShowTime(base::TimeDelta total_show_time) {
  base::UmaHistogramMediumTimes(kTotalShowTimeHistogram, total_show_time);
}

void RecordClassromInitialLoadTime(bool first_occurrence,
                                   base::TimeDelta load_time) {
  std::string histogram_name =
      base::StrCat({kTimeManagementClassroomPrefix, ".OpenToInitialLoadTime"});

  if (first_occurrence) {
    histogram_name += ".FirstOccurence";
  } else {
    histogram_name += ".SubsequentOccurence";
  }

  base::UmaHistogramMediumTimes(histogram_name, load_time);
}

void RecordClassroomChangeLoadTime(bool success, base::TimeDelta load_time) {
  std::string histogram_name =
      base::StrCat({kTimeManagementClassroomPrefix, ".ChangeListToLoadTime"});

  if (success) {
    histogram_name += ".Success";
  } else {
    histogram_name += ".Fail";
  }

  base::UmaHistogramMediumTimes(histogram_name, load_time);
}

void RecordTasksInitialLoadTime(bool first_occurrence,
                                base::TimeDelta load_time) {
  std::string histogram_name =
      base::StrCat({kTimeManagementTaskPrefix, ".OpenToInitialLoadTime"});

  if (first_occurrence) {
    histogram_name += ".FirstOccurence";
  } else {
    histogram_name += ".SubsequentOccurence";
  }

  base::UmaHistogramMediumTimes(histogram_name, load_time);
}

void RecordTasksChangeLoadTime(base::TimeDelta load_time) {
  base::UmaHistogramMediumTimes(
      base::StrCat({kTimeManagementTaskPrefix, ".ChangeListToLoadTime"}),
      load_time);
}

void RecordTasksListChangeCount(int change_count) {
  base::UmaHistogramCounts100(
      "Ash.Glanceables.TimeManagement.Tasks.TasksListChangeCount",
      change_count);
}

void RecordStudentAssignmentListShowTime(StudentAssignmentsListType list_type,
                                         base::TimeDelta time_shown,
                                         bool default_list) {
  if (default_list) {
    switch (list_type) {
      case StudentAssignmentsListType::kAssigned:
        base::UmaHistogramMediumTimes(
            "Ash.Glanceables.Classroom.Student.AssignmentListShownTime."
            "DefaultList.Assigned",
            time_shown);
        break;
      case StudentAssignmentsListType::kNoDueDate:
        base::UmaHistogramMediumTimes(
            "Ash.Glanceables.Classroom.Student.AssignmentListShownTime."
            "DefaultList.NoDueDate",
            time_shown);
        break;
      case StudentAssignmentsListType::kMissing:
        base::UmaHistogramMediumTimes(
            "Ash.Glanceables.Classroom.Student.AssignmentListShownTime."
            "DefaultList.Missing",
            time_shown);
        break;
      case StudentAssignmentsListType::kDone:
        base::UmaHistogramMediumTimes(
            "Ash.Glanceables.Classroom.Student.AssignmentListShownTime."
            "DefaultList.Done",
            time_shown);
        break;
    }
  } else {
    switch (list_type) {
      case StudentAssignmentsListType::kAssigned:
        base::UmaHistogramMediumTimes(
            "Ash.Glanceables.Classroom.Student.AssignmentListShownTime."
            "ChangedList.Assigned",
            time_shown);
        break;
      case StudentAssignmentsListType::kNoDueDate:
        base::UmaHistogramMediumTimes(
            "Ash.Glanceables.Classroom.Student.AssignmentListShownTime."
            "ChangedList.NoDueDate",
            time_shown);
        break;
      case StudentAssignmentsListType::kMissing:
        base::UmaHistogramMediumTimes(
            "Ash.Glanceables.Classroom.Student.AssignmentListShownTime."
            "ChangedList.Missing",
            time_shown);
        break;
      case StudentAssignmentsListType::kDone:
        base::UmaHistogramMediumTimes(
            "Ash.Glanceables.Classroom.Student.AssignmentListShownTime."
            "ChangedList.Done",
            time_shown);
        break;
    }
  }
}

void RecordStudentSelectedListChangeCount(int change_count) {
  base::UmaHistogramCounts100(
      "Ash.Glanceables.Classroom.Student.SelectedListChangeCount",
      change_count);
}

void RecordStudentAssignmentListSelected(StudentAssignmentsListType list_type) {
  base::UmaHistogramEnumeration(
      "Ash.Glanceables.Classroom.Student.ListSelected", list_type);
}

}  // namespace ash
