// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_DIAGNOSTICS_DIAGNOSTICS_LOG_CONTROLLER_H_
#define ASH_SYSTEM_DIAGNOSTICS_DIAGNOSTICS_LOG_CONTROLLER_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/login_status.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/diagnostics/diagnostics_browser_delegate.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"

namespace ash {
namespace diagnostics {

class KeyboardInputLog;
class NetworkingLog;
class RoutineLog;
class TelemetryLog;

// DiagnosticsLogController manages the lifetime of Diagnostics log writers such
// as the RoutineLog and ensures logs are written to the correct directory path
// for the current user. See go/cros-shared-diagnostics-session-log-dd.
class ASH_EXPORT DiagnosticsLogController : SessionObserver {
 public:
  // Holds the in-memory contents and file paths for the logs. This data is
  // extracted on the UI thread and passed to the blocking pool to generate
  // the final session log without causing cross-thread data races.
  struct SessionLogData {
    SessionLogData();
    ~SessionLogData();
    SessionLogData(const SessionLogData&) = delete;
    SessionLogData& operator=(const SessionLogData&) = delete;
    SessionLogData(SessionLogData&&);
    SessionLogData& operator=(SessionLogData&&);

    std::string telemetry_contents;
    std::string network_info;
    base::FilePath system_routine_log_path;
    base::FilePath network_routine_log_path;
    base::FilePath network_events_path;
    base::FilePath keyboard_input_path;
  };

  DiagnosticsLogController();
  DiagnosticsLogController(const DiagnosticsLogController&) = delete;
  DiagnosticsLogController& operator=(const DiagnosticsLogController&) = delete;
  ~DiagnosticsLogController() override;

  // DiagnosticsLogController is created and destroyed with
  // the ash::Shell. DiagnosticsLogController::Get may be nullptr if accessed
  // outside the expected lifetime or when the
  // `ash::features::kEnableLogControllerForDiagnosticsApp` is false.
  static DiagnosticsLogController* Get();

  // Check if DiagnosticsLogController is ready for use.
  static bool IsInitialized();
  static void Initialize(std::unique_ptr<DiagnosticsBrowserDelegate> delegate);

  // GenerateSessionStringOnBlockingPool needs to be run on blocking thread.
  // Generates a string of the combined Diagnostics logs.
  static std::string GenerateSessionStringOnBlockingPool(
      SessionLogData log_data);

  // GenerateSessionLogOnBlockingPool needs to be run on blocking
  // thread. Stores combined log at |save_file_path| and returns
  // whether file creation is successful.
  static bool GenerateSessionLogOnBlockingPool(
      const base::FilePath& save_file_path,
      SessionLogData log_data);

  // Call this on the UI thread to safely grab the in-memory data and paths
  // without any data races.
  SessionLogData GetSessionLogData() const;

  // Ensures DiagnosticsLogController is configured to match the current
  // environment. To be called from DiagnosticsDialog::ShowDialog prior to the
  // Diagnostics app being shown.
  void ResetAndInitializeLogWriters();

  // SessionObserver:
  // Ensure DiagnosticsLogController is re-configured to match the current
  // environment when LoginStatus changes. See ash/ash_login_status.h for
  // description of LoginStatus types.
  void OnLoginStatusChanged(LoginStatus login_status) override;

  KeyboardInputLog& GetKeyboardInputLog();
  NetworkingLog& GetNetworkingLog();
  RoutineLog& GetRoutineLog();
  TelemetryLog& GetTelemetryLog();

  void SetKeyboardInputLogForTesting(
      std::unique_ptr<KeyboardInputLog> keyboard_input_log);
  void SetNetworkingLogForTesting(std::unique_ptr<NetworkingLog> network_log);
  void SetRoutineLogForTesting(std::unique_ptr<RoutineLog> routine_log);
  void SetTelemetryLogForTesting(std::unique_ptr<TelemetryLog> telemetry_log);

 private:
  friend class DiagnosticsLogControllerTest;

  // Ensure log_base_path_ set based on current session and ash::LoginStatus.
  void ResetLogBasePath();

  // Removes directory at |path|.
  void RemoveDirectory(const base::FilePath& path);

  LoginStatus previous_status_;
  std::unique_ptr<DiagnosticsBrowserDelegate> delegate_;
  base::FilePath log_base_path_;
  std::unique_ptr<KeyboardInputLog> keyboard_input_log_;
  std::unique_ptr<NetworkingLog> networking_log_;
  std::unique_ptr<RoutineLog> routine_log_;
  std::unique_ptr<TelemetryLog> telemetry_log_;
  SEQUENCE_CHECKER(sequence_checker_);

  // Must be last.
  base::WeakPtrFactory<DiagnosticsLogController> weak_ptr_factory_{this};
};

}  // namespace diagnostics
}  // namespace ash

#endif  // ASH_SYSTEM_DIAGNOSTICS_DIAGNOSTICS_LOG_CONTROLLER_H_
