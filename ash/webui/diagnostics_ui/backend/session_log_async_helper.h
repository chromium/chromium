// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_SESSION_LOG_ASYNC_HELPER_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_SESSION_LOG_ASYNC_HELPER_H_

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"

namespace ash::diagnostics {

class TelemetryLog;
class RoutineLog;
class NetworkingLog;

// Holds state related to current session to prevent UaF.
class SessionLogAsyncHelper {
 public:
  SessionLogAsyncHelper();

  SessionLogAsyncHelper(const SessionLogAsyncHelper&) = delete;
  SessionLogAsyncHelper& operator=(const SessionLogAsyncHelper&) = delete;

  virtual ~SessionLogAsyncHelper();

  // Gathers session data from three log sources: TelemetryLog, RoutineLog, and
  // NetworkingLog then writes the combined information to a location specified
  // by |file_path|.
  bool CreateSessionLogOnBlockingPool(const base::FilePath file_path,
                                      TelemetryLog* telemetry_log,
                                      RoutineLog* routine_log,
                                      NetworkingLog* networking_log);
};

}  // namespace ash::diagnostics

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_SESSION_LOG_ASYNC_HELPER_H_
