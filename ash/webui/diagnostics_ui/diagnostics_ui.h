// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_DIAGNOSTICS_UI_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_DIAGNOSTICS_UI_H_

#include "ash/webui/diagnostics_ui/backend/session_log_handler.h"
#include "ash/webui/diagnostics_ui/mojom/input_data_provider.mojom-forward.h"
#include "ash/webui/diagnostics_ui/mojom/network_health_provider.mojom-forward.h"
#include "ash/webui/diagnostics_ui/mojom/system_data_provider.mojom-forward.h"
#include "ash/webui/diagnostics_ui/mojom/system_routine_controller.mojom-forward.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace ash {

class HoldingSpaceClient;

namespace diagnostics {
class DiagnosticsManager;
}  // namespace diagnostics

// The WebDialogUI for chrome://diagnostics.
class DiagnosticsDialogUI : public ui::MojoWebDialogUI {
 public:
  explicit DiagnosticsDialogUI(
      content::WebUI* web_ui,
      const diagnostics::SessionLogHandler::SelectFilePolicyCreator&
          select_file_policy_creator,
      HoldingSpaceClient* holding_space_client);
  ~DiagnosticsDialogUI() override;

  DiagnosticsDialogUI(const DiagnosticsDialogUI&) = delete;
  DiagnosticsDialogUI& operator=(const DiagnosticsDialogUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<diagnostics::mojom::NetworkHealthProvider>
          receiver);

  void BindInterface(
      mojo::PendingReceiver<diagnostics::mojom::SystemDataProvider> receiver);

  void BindInterface(
      mojo::PendingReceiver<diagnostics::mojom::SystemRoutineController>
          receiver);

  void BindInterface(
      mojo::PendingReceiver<diagnostics::mojom::InputDataProvider> receiver);

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();

  // Timestamp of when the app was opened. Used to calculate a duration for
  // metrics.
  base::Time open_timestamp_;

  std::unique_ptr<diagnostics::SessionLogHandler> session_log_handler_;
  std::unique_ptr<diagnostics::DiagnosticsManager> diagnostics_manager_;
};

}  // namespace chromeos

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_DIAGNOSTICS_UI_H_
