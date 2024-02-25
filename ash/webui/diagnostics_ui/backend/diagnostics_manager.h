// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_DIAGNOSTICS_MANAGER_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_DIAGNOSTICS_MANAGER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace ash {
namespace diagnostics {

class NetworkHealthProvider;
class SessionLogHandler;
class SystemDataProvider;
class SystemRoutineController;
class InputDataProvider;

// DiagnosticsManager is responsible for managing the lifetime of the services
// used by the Diagnostics SWA.
class DiagnosticsManager {
 public:
  DiagnosticsManager(SessionLogHandler* session_log_handler,
                     content::WebUI* webui);
  ~DiagnosticsManager();

  DiagnosticsManager(const DiagnosticsManager&) = delete;
  DiagnosticsManager& operator=(const DiagnosticsManager&) = delete;

  NetworkHealthProvider* GetNetworkHealthProvider() const;
  SystemDataProvider* GetSystemDataProvider() const;
  SystemRoutineController* GetSystemRoutineController() const;
  InputDataProvider* GetInputDataProvider();

 private:
  std::unique_ptr<NetworkHealthProvider> network_health_provider_;
  std::unique_ptr<SystemDataProvider> system_data_provider_;
  std::unique_ptr<SystemRoutineController> system_routine_controller_;
  std::unique_ptr<InputDataProvider> input_data_provider_;
  raw_ptr<content::WebUI> webui_;
};

}  // namespace diagnostics
}  // namespace ash

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_DIAGNOSTICS_MANAGER_H_
