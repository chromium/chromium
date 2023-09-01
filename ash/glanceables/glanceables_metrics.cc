// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_metrics.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"

namespace {

constexpr char kLoginToShowTime[] =
    "Ash.Glanceables.TimeManagement.LoginToShowTime";

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
  base::UmaHistogramMediumTimes(kLoginToShowTime, login_to_show_time);
}

}  // namespace ash
