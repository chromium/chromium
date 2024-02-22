// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_COMMON_GLANCEABLES_TASKS_ERROR_TYPE_H_
#define ASH_GLANCEABLES_COMMON_GLANCEABLES_TASKS_ERROR_TYPE_H_

namespace ash {

enum class GlanceablesTasksErrorType {
  // The tasks view data wasn't successfully fetched so the list can not be
  // updated.
  kCantUpdateList,

  // The tasks weren't marked as completed because of failing to commit the
  // change. This normally shows when the user reopens the glanceables after
  // they marked some tasks as complete.
  kCantMarkComplete,

  // The tasks can't be marked as complete because the current network is not
  // connected. This shows when users try to click on the check button to mark
  // tasks as complete when they don't have a network connection.
  kCantMarkCompleteNoNetwork,

  // The task title couldn't be updated because of failing to commit the
  // change.
  kCantUpdateTitle,

  // The task title couldn't be edited because the current network is not
  // connected. This shows when users try to click on the title to edit a task
  // when they don't have a network connection.
  kCantUpdateTitleNoNetwork
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_COMMON_GLANCEABLES_TASKS_ERROR_TYPE_H_
