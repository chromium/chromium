// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_metrics.h"

#include <string>

#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"

namespace ash {

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

}  // namespace ash
