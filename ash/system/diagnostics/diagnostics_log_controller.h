// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_DIAGNOSTICS_DIAGNOSTICS_LOG_CONTROLLER_H_
#define ASH_SYSTEM_DIAGNOSTICS_DIAGNOSTICS_LOG_CONTROLLER_H_

namespace ash {
namespace diagnostics {

// DiagnosticsLogController manages the lifetime of Diagnostics log writers such
// as the RoutineLog and ensures logs are written to the correct directory path
// for the current user. See go/cros-shared-diagnostics-session-log-dd.
class DiagnosticsLogController {
 public:
  DiagnosticsLogController();
  DiagnosticsLogController(const DiagnosticsLogController&) = delete;
  DiagnosticsLogController& operator=(const DiagnosticsLogController&) = delete;
  ~DiagnosticsLogController();

  static DiagnosticsLogController* Get();
};

}  // namespace diagnostics
}  // namespace ash

#endif  // ASH_SYSTEM_DIAGNOSTICS_DIAGNOSTICS_LOG_CONTROLLER_H_
