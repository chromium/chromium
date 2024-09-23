// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_metrics.h"

#include <string>

#include "ash/glanceables/classroom/glanceables_classroom_student_view.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
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

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TasksUserAction {
  kActiveTaskListChanged = 0,
  kTaskMarkedComplete = 1,
  kTaskMarkedIncomplete = 2,
  kAddTaskStarted = 3,
  kModifyTaskStarted = 4,
  kHeaderButtonClicked = 5,
  kAddNewTaskButtonClicked = 6,
  kFooterButtonClicked = 7,
  kEditInGoogleTasksButtonClicked = 8,
  kMaxValue = kEditInGoogleTasksButtonClicked
};

void RecordTasksUserAction(TasksUserAction action) {
  base::UmaHistogramEnumeration(
      base::JoinString({kTimeManagementTaskPrefix, "UserAction"}, "."), action);
  base::RecordAction(base::UserMetricsAction("Glanceables_Tasks_UserAction"));
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ClassroomUserAction {
  kAssignmentListSelected = 0,
  kHeaderIconPressed = 1,
  kStudentAssignmentPressed = 2,
  kMaxValue = kStudentAssignmentPressed,
};

void RecordClassroomUserAction(ClassroomUserAction action) {
  base::UmaHistogramEnumeration(
      base::JoinString({kTimeManagementClassroomPrefix, "UserAction"}, "."),
      action);
  base::RecordAction(
      base::UserMetricsAction("Glanceables_Classroom_UserAction"));
}

}  // namespace

namespace ash {

void RecordContextualGoogleIntegrationStatus(
    const std::string& integration_name,
    ContextualGoogleIntegrationStatus status) {
  base::UmaHistogramEnumeration(
      base::StringPrintf("Ash.ContextualGoogleIntegrations.%s.Status",
                         integration_name.data()),
      status);
}

void RecordActiveTaskListChanged() {
  RecordTasksUserAction(TasksUserAction::kActiveTaskListChanged);
  base::RecordAction(
      base::UserMetricsAction("Glanceables_Tasks_ActiveTaskListChanged"));
}

void RecordTaskMarkedAsCompleted(bool complete) {
  RecordTasksUserAction(complete ? TasksUserAction::kTaskMarkedComplete
                                 : TasksUserAction::kTaskMarkedIncomplete);

  if (complete) {
    base::RecordAction(
        base::UserMetricsAction("Glanceables_Tasks_TaskMarkedAsCompleted"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("Glanceables_Tasks_TaskMarkedAsIncomplete"));
  }
}

void RecordUserStartedAddingTask() {
  RecordTasksUserAction(TasksUserAction::kAddTaskStarted);

  base::RecordAction(
      base::UserMetricsAction("Glanceables_Tasks_AddTaskStarted"));
}

void RecordTaskAdditionResult(TaskModificationResult result) {
  base::UmaHistogramEnumeration(
      base::StrCat({kTimeManagementTaskPrefix, ".AddTaskResult"}), result);
}

void RecordNumberOfAddedTasks(int add_task_action_count,
                              bool in_empty_task_list,
                              bool first_usage) {
  if (first_usage) {
    base::UmaHistogramCounts100(
        "Ash.Glanceables.TimeManagement.Tasks."
        "AddedTasksForFirstUsage",
        add_task_action_count);
  }
  base::UmaHistogramCounts100(
      base::StrCat({kTimeManagementTaskPrefix, ".AddedTasks",
                    in_empty_task_list ? ".InEmptyList" : ".InNonEmptyList"}),
      add_task_action_count);
}

void RecordUserModifyingTask() {
  RecordTasksUserAction(TasksUserAction::kModifyTaskStarted);

  base::RecordAction(
      base::UserMetricsAction("Glanceables_Tasks_ModifyTaskStarted"));
}

void RecordTaskModificationResult(TaskModificationResult result) {
  base::UmaHistogramEnumeration(
      base::StrCat({kTimeManagementTaskPrefix, ".ModifyTaskResult"}), result);
}

void RecordTasksLaunchSource(TasksLaunchSource source) {

  switch (source) {
    case TasksLaunchSource::kHeaderButton:
      RecordTasksUserAction(TasksUserAction::kHeaderButtonClicked);
      base::RecordAction(base::UserMetricsAction(
          "Glanceables_Tasks_LaunchTasksApp_HeaderButton"));
      break;
    case TasksLaunchSource::kAddNewTaskButton:
      RecordTasksUserAction(TasksUserAction::kAddNewTaskButtonClicked);
      base::RecordAction(base::UserMetricsAction(
          "Glanceables_Tasks_LaunchTasksApp_AddNewTaskButton"));
      break;
    case TasksLaunchSource::kFooterButton:
      RecordTasksUserAction(TasksUserAction::kFooterButtonClicked);
      base::RecordAction(base::UserMetricsAction(
          "Glanceables_Tasks_LaunchTasksApp_FooterButton"));
      break;
    case TasksLaunchSource::kEditInGoogleTasksButton:
      RecordTasksUserAction(TasksUserAction::kEditInGoogleTasksButtonClicked);
      base::RecordAction(base::UserMetricsAction(
          "Glanceables_Tasks_LaunchTasksApp_EditInGoogleTasksButton"));
      break;
  }
}

void RecordUserWithNoTasksRedictedToTasksUI() {
  base::RecordAction(
      base::UserMetricsAction("Glanceables_Tasks_NewUserNavigatedToTasks"));
}

void RecordAddTaskButtonShownForTT() {
  base::RecordAction(
      base::UserMetricsAction("Glanceables_Tasks_AddTaskButtonShown"));
}

void RecordAddTaskButtonUsageForNewTasksUsersTT(bool pressed) {
  base::UmaHistogramBoolean(
      "Ash.Glanceables.TimeManagement.AddTaskButtonUsageForNewTasksUsersTT",
      pressed);
}

void RecordLoginToShowTime(base::TimeDelta login_to_show_time) {
  base::UmaHistogramMediumTimes(kLoginToShowTimeHistogram, login_to_show_time);
}

void RecordTotalShowTime(base::TimeDelta total_show_time) {
  base::UmaHistogramMediumTimes(kTotalShowTimeHistogram, total_show_time);
}

void RecordTotalShowTimeForClassroom(base::TimeDelta total_show_time) {
  base::UmaHistogramMediumTimes(
      base::StrCat({kTimeManagementClassroomPrefix, ".TotalShowTime"}),
      total_show_time);
}

void RecordTotalShowTimeForTasks(base::TimeDelta total_show_time) {
  base::UmaHistogramMediumTimes(
      base::StrCat({kTimeManagementTaskPrefix, ".TotalShowTime"}),
      total_show_time);
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

void RecordStudentAssignmentPressed(bool default_list) {
  RecordClassroomUserAction(ClassroomUserAction::kStudentAssignmentPressed);

  base::RecordAction(
      base::UserMetricsAction("Glanceables_Classroom_AssignmentPressed"));

  if (default_list) {
    base::RecordAction(base::UserMetricsAction(
        "Glanceables_Classroom_AssignmentPressed_DefaultList"));
  }
}

void RecordClassroomHeaderIconPressed() {
  RecordClassroomUserAction(ClassroomUserAction::kHeaderIconPressed);

  base::RecordAction(
      base::UserMetricsAction("Glanceables_Classroom_HeaderIconPressed"));
}

void RecordStudentAssignmentListSelected(StudentAssignmentsListType list_type) {
  RecordClassroomUserAction(ClassroomUserAction::kAssignmentListSelected);

  base::UmaHistogramEnumeration(
      "Ash.Glanceables.Classroom.Student.ListSelected", list_type);
}

}  // namespace ash
