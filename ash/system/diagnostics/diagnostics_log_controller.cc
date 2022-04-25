// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/diagnostics/diagnostics_log_controller.h"

#include <memory>

#include "ash/public/cpp/session/session_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/diagnostics/diagnostics_browser_delegate.h"
#include "ash/system/diagnostics/networking_log.h"
#include "ash/system/diagnostics/routine_log.h"
#include "ash/system/diagnostics/telemetry_log.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "components/session_manager/session_manager_types.h"

namespace ash {
namespace diagnostics {

namespace {

DiagnosticsLogController* g_instance = nullptr;
// Default path for storing logs.
const char kDiaganosticsTmpDir[] = "/tmp/diagnostics";
const char kDiaganosticsDirName[] = "diagnostics";

// Determines if profile should be accessed with current session state.  If at
// sign-in screen, guest user, kiosk app, or before the profile has
// successfully loaded temporary path should be used for storing logs.
bool ShouldUseActiveUserProfileDir(session_manager::SessionState state,
                                   LoginStatus status) {
  return state == session_manager::SessionState::ACTIVE &&
         status == ash::LoginStatus::USER;
}

// Placeholder session log contents.
const char kLogFileContents[] = "Diagnostics Log";

}  // namespace

DiagnosticsLogController::DiagnosticsLogController()
    : log_base_path_(kDiaganosticsTmpDir) {
  DCHECK_EQ(nullptr, g_instance);
  ash::Shell::Get()->session_controller()->AddObserver(this);
  g_instance = this;
}

DiagnosticsLogController::~DiagnosticsLogController() {
  DCHECK_EQ(this, g_instance);
  ash::Shell::Get()->session_controller()->RemoveObserver(this);
  g_instance = nullptr;
}

// static
DiagnosticsLogController* DiagnosticsLogController::Get() {
  return g_instance;
}

// static
bool DiagnosticsLogController::IsInitialized() {
  return g_instance && g_instance->delegate_;
}

// static
void DiagnosticsLogController::Initialize(
    std::unique_ptr<DiagnosticsBrowserDelegate> delegate) {
  DCHECK(g_instance);
  g_instance->delegate_ = std::move(delegate);
  g_instance->ResetAndInitializeLogWriters();
}

bool DiagnosticsLogController::GenerateSessionLogOnBlockingPool(
    const base::FilePath& save_file_path) {
  DCHECK(!save_file_path.empty());

  // TODO(ashleydp): Replace |kLogFileContents| when actual log contents
  // available to write to file.
  return base::WriteFile(save_file_path, kLogFileContents);
}

void DiagnosticsLogController::ResetAndInitializeLogWriters() {
  if (!DiagnosticsLogController::IsInitialized()) {
    return;
  }

  ResetLogBasePath();

  networking_log_ = std::make_unique<NetworkingLog>(log_base_path_);
  routine_log_ = std::make_unique<RoutineLog>(log_base_path_);
  telemetry_log_ = std::make_unique<TelemetryLog>();
}

NetworkingLog* DiagnosticsLogController::GetNetworkingLog() {
  return networking_log_.get();
}

RoutineLog* DiagnosticsLogController::GetRoutineLog() {
  return routine_log_.get();
}

TelemetryLog* DiagnosticsLogController::GetTelemetryLog() {
  return telemetry_log_.get();
}

void DiagnosticsLogController::ResetLogBasePath() {
  const session_manager::SessionState state =
      ash::Shell::Get()->session_controller()->GetSessionState();
  const LoginStatus status =
      ash::Shell::Get()->session_controller()->login_status();

  // Check if there is an active user and profile is ready based on session and
  // login state.
  if (ShouldUseActiveUserProfileDir(state, status)) {
    base::FilePath user_dir = g_instance->delegate_->GetActiveUserProfileDir();

    // Update |log_base_path_| when path is non-empty. Otherwise fallback to
    // |kDiaganosticsTmpDir|.
    if (!user_dir.empty()) {
      g_instance->log_base_path_ = user_dir.Append(kDiaganosticsDirName);
      return;
    }
  }

  // Use diagnostics temporary path for Guest, KioskApp, and no user states.
  g_instance->log_base_path_ = base::FilePath(kDiaganosticsTmpDir);
}

void DiagnosticsLogController::OnLoginStatusChanged(LoginStatus login_status) {
  if (!DiagnosticsLogController::IsInitialized()) {
    return;
  }

  g_instance->ResetLogBasePath();
}

}  // namespace diagnostics
}  // namespace ash
