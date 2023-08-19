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
  if (DiagnosticsLogController::IsInitialized()) {
    system_data_provider_ = std::make_unique<SystemDataProvider>();
    system_routine_controller_ = std::make_unique<SystemRoutineController>();
    network_health_provider_ = std::make_unique<NetworkHealthProvider>();
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
  if (!input_data_provider_) {
    input_data_provider_ = std::make_unique<InputDataProvider>(
        webui_->GetWebContents()->GetTopLevelNativeWindow());
  }
  return input_data_provider_.get();
}

}  // namespace diagnostics
}  // namespace ash
