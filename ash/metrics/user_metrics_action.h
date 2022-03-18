// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_USER_METRICS_ACTION_H_
#define ASH_METRICS_USER_METRICS_ACTION_H_

namespace ash {

// Ash-owned user metrics.
// DEPRECATED: Use base::RecordAction(base::UserMetricsAction("my_action"))
// instead of adding things here.
enum UserMetricsAction {
  UMA_DESKTOP_SWITCH_TASK,
  UMA_LAUNCHER_LAUNCH_TASK,
  UMA_LAUNCHER_MINIMIZE_TASK,
  UMA_LAUNCHER_SWITCH_TASK,

  // DEPRECATED: Do not add new values. See top of file.
};

}  // namespace ash

#endif  // ASH_METRICS_USER_METRICS_ACTION_H_
