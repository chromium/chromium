// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_DIAGNOSTICS_UI_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_DIAGNOSTICS_UI_H_

#include "ash/webui/common/backend/plural_string_handler.h"
#include "ash/webui/common/chrome_os_webui_config.h"
#include "ash/webui/diagnostics_ui/backend/input/input_data_provider.h"
#include "ash/webui/diagnostics_ui/backend/session_log_handler.h"
#include "ash/webui/diagnostics_ui/diagnostics_metrics.h"
#include "ash/webui/diagnostics_ui/diagnostics_metrics_message_handler.h"
#include "ash/webui/diagnostics_ui/mojom/input_data_provider.mojom-forward.h"
#include "ash/webui/diagnostics_ui/mojom/network_health_provider.mojom-forward.h"
#include "ash/webui/diagnostics_ui/mojom/system_data_provider.mojom-forward.h"
#include "ash/webui/diagnostics_ui/mojom/system_routine_controller.mojom-forward.h"
#include "ash/webui/diagnostics_ui/url_constants.h"
#include "base/time/time.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace base {
class FilePath;
}  // namespace base

namespace ui {
class ColorChangeHandler;
}  // namespace ui

namespace ash {

class HoldingSpaceClient;
class DiagnosticsDialogUI;

namespace diagnostics {
class DiagnosticsManager;
class InputDataProvider;
}  // namespace diagnostics

// The WebDialogUIConfig for chrome://diagnostics.
class DiagnosticsDialogUIConfig
    : public ChromeOSWebUIConfig<DiagnosticsDialogUI> {
 public:
  explicit DiagnosticsDialogUIConfig(
      CreateWebUIControllerFunc create_controller_func)
      : ChromeOSWebUIConfig(content::kChromeUIScheme,
                            ash::kChromeUIDiagnosticsAppHost,
                            create_controller_func) {}
};

// The WebDialogUI for chrome://diagnostics.
class DiagnosticsDialogUI : public ui::MojoWebDialogUI {
 public:
  explicit DiagnosticsDialogUI(
      content::WebUI* web_ui,
      const diagnostics::SessionLogHandler::SelectFilePolicyCreator&
          select_file_policy_creator,
      HoldingSpaceClient* holding_space_client,
      const base::FilePath& log_directory_path);
  ~DiagnosticsDialogUI() override;

  DiagnosticsDialogUI(const DiagnosticsDialogUI&) = delete;
  DiagnosticsDialogUI& operator=(const DiagnosticsDialogUI&) = delete;

  static bool ShouldCloseDialogOnEscape() {
    return diagnostics::InputDataProvider::ShouldCloseDialogOnEscape();
  }

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

  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();

  // Timestamp of when the app was opened. Used to calculate a duration for
  // metrics.
  base::Time open_timestamp_;

  std::unique_ptr<diagnostics::DiagnosticsManager> diagnostics_manager_;
  std::unique_ptr<diagnostics::metrics::DiagnosticsMetrics>
      diagnostics_metrics_;
  std::unique_ptr<diagnostics::InputDataProvider> input_data_provider_;
  // The color change handler notifies the WebUI when the color provider
  // changes.
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;
};

}  // namespace ash

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_DIAGNOSTICS_UI_H_
