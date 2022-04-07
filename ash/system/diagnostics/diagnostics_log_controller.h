// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_DIAGNOSTICS_DIAGNOSTICS_LOG_CONTROLLER_H_
#define ASH_SYSTEM_DIAGNOSTICS_DIAGNOSTICS_LOG_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/diagnostics/diagnostics_browser_delegate.h"
#include "base/files/file_path.h"

namespace ash {
namespace diagnostics {

// DiagnosticsLogController manages the lifetime of Diagnostics log writers such
// as the RoutineLog and ensures logs are written to the correct directory path
// for the current user. See go/cros-shared-diagnostics-session-log-dd.
class ASH_EXPORT DiagnosticsLogController {
 public:
  DiagnosticsLogController();
  DiagnosticsLogController(const DiagnosticsLogController&) = delete;
  DiagnosticsLogController& operator=(const DiagnosticsLogController&) = delete;
  ~DiagnosticsLogController();

  // DiagnosticsLogController is created and destroyed with
  // the ash::Shell. DiagnosticsLogController::Get may be nullptr if accessed
  // outside the expected lifetime or when the
  // `ash::features::kEnableLogControllerForDiagnosticsApp` is false.
  static DiagnosticsLogController* Get();

  // Check if DiagnosticsLogController is ready for use.
  static bool IsInitialized();
  static void Initialize(std::unique_ptr<DiagnosticsBrowserDelegate> delegate);

  // GenerateSessionLogOnBlockingPool needs to be run on blocking
  // thread. Stores combined log at |save_file_path| and returns
  // whether file creation is successful.
  bool GenerateSessionLogOnBlockingPool(const base::FilePath& save_file_path);

 private:
  std::unique_ptr<DiagnosticsBrowserDelegate> delegate_;
};

}  // namespace diagnostics
}  // namespace ash

#endif  // ASH_SYSTEM_DIAGNOSTICS_DIAGNOSTICS_LOG_CONTROLLER_H_
