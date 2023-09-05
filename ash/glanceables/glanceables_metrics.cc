// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_metrics.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"

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

}  // namespace ash
