// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_GLANCEABLES_METRICS_H_
#define ASH_GLANCEABLES_GLANCEABLES_METRICS_H_

#include "ash/ash_export.h"
#include "base/time/time.h"

namespace ash {

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

void RecordLoginToShowTime(base::TimeDelta login_to_show_time);

void RecordTotalShowTime(base::TimeDelta total_show_time);

void RecordClassromInitialLoadTime(bool first_occurrence,
                                   base::TimeDelta load_time);

void RecordClassroomChangeLoadTime(bool success, base::TimeDelta load_time);

void RecordTasksInitialLoadTime(bool first_occurrence,
                                base::TimeDelta load_time);

void RecordTasksChangeLoadTime(base::TimeDelta load_time);

}  // namespace ash

#endif  // ASH_GLANCEABLES_GLANCEABLES_METRICS_H_
