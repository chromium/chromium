// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/diagnostics_manager.h"

#include <ui/aura/window.h>
#include "ash/constants/ash_features.h"
#include "ash/system/diagnostics/diagnostics_log_controller.h"
#include "ash/webui/diagnostics_ui/backend/connectivity/network_health_provider.h"
#include "ash/webui/diagnostics_ui/backend/input/input_data_provider.h"
#include "ash/webui/diagnostics_ui/backend/session_log_handler.h"
#include "ash/webui/diagnostics_ui/backend/system/system_data_provider.h"
#include "ash/webui/diagnostics_ui/backend/system/system_routine_controller.h"

namespace ash {
namespace diagnostics {

DiagnosticsManager::DiagnosticsManager(SessionLogHandler* session_log_handler,
                                       content::WebUI* webui)
    : webui_(webui) {
  // Configure providers with logs from DiagnosticsLogController when flag
  // enabled.
  if (features::IsLogControllerForDiagnosticsAppEnabled() &&
      DiagnosticsLogController::IsInitialized()) {
    system_data_provider_ = std::make_unique<SystemDataProvider>(
        DiagnosticsLogController::Get()->GetTelemetryLog());
    system_routine_controller_ = std::make_unique<SystemRoutineController>(
        DiagnosticsLogController::Get()->GetRoutineLog());
    network_health_provider_ = std::make_unique<NetworkHealthProvider>(
        DiagnosticsLogController::Get()->GetNetworkingLog());
  } else {
    // TODO(b/226574520): Remove else block as part of DiagnosticsLogController
    // flag clean up.
    system_data_provider_ = std::make_unique<SystemDataProvider>(
        session_log_handler->GetTelemetryLog());
    system_routine_controller_ = std::make_unique<SystemRoutineController>(
        session_log_handler->GetRoutineLog());
    network_health_provider_ = std::make_unique<NetworkHealthProvider>(
        session_log_handler->GetNetworkingLog());
  }
}

DiagnosticsManager::~DiagnosticsManager() = default;

NetworkHealthProvider* DiagnosticsManager::GetNetworkHealthProvider() const {
  return network_health_provider_.get();
}

SystemDataProvider* DiagnosticsManager::GetSystemDataProvider() const {
  return system_data_provider_.get();
}

SystemRoutineController* DiagnosticsManager::GetSystemRoutineController()
    const {
  return system_routine_controller_.get();
}

InputDataProvider* DiagnosticsManager::GetInputDataProvider() {
  // Do not construct the InputDataProvider until it is requested;
  // performing this in the constructor is too early, and the native
  // window will not be available.
  if (features::IsInputInDiagnosticsAppEnabled() && !input_data_provider_) {
    input_data_provider_ = std::make_unique<InputDataProvider>(
        webui_->GetWebContents()->GetTopLevelNativeWindow(),
        DiagnosticsLogController::Get()->GetKeyboardInputLog());
  }
  return input_data_provider_.get();
}

}  // namespace diagnostics
}  // namespace ash
