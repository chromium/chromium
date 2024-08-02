// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/connectivity_diagnostics/connectivity_diagnostics_ui.h"

#include <utility>

#include "ash/webui/common/trusted_types_util.h"
#include "ash/webui/connectivity_diagnostics/url_constants.h"
#include "ash/webui/grit/connectivity_diagnostics_resources.h"
#include "ash/webui/grit/connectivity_diagnostics_resources_map.h"
#include "ash/webui/network_ui/network_diagnostics_resource_provider.h"
#include "ash/webui/network_ui/network_health_resource_provider.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health.mojom.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/resources/grit/webui_resources.h"

namespace ash {

namespace {

// TODO(crbug.com/40673941): Replace with webui::SetUpWebUIDataSource() once it
// no longer requires a dependency on //chrome/browser.
void SetUpWebUIDataSource(content::WebUIDataSource* source,
                          base::span<const webui::ResourcePath> resources,
                          int default_resource) {
  source->AddResourcePaths(resources);
  source->SetDefaultResource(default_resource);
  source->AddResourcePath("test_loader.html", IDR_WEBUI_TEST_LOADER_HTML);
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER_JS);
  source->AddResourcePath("test_loader_util.js",
                          IDR_WEBUI_JS_TEST_LOADER_UTIL_JS);
}

}  // namespace

class ConnectivityDiagnosticsMessageHandler
    : public content::WebUIMessageHandler {
 public:
  ConnectivityDiagnosticsMessageHandler(
      ConnectivityDiagnosticsUI::SendFeedbackReportCallback
          send_feedback_report_callback,
      bool show_feedback_button)
      : send_feedback_report_callback_(
            std::move(send_feedback_report_callback)),
        show_feedback_button_(show_feedback_button) {}
  ~ConnectivityDiagnosticsMessageHandler() override = default;

  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        "sendFeedbackReport",
        base::BindRepeating(
            &ConnectivityDiagnosticsMessageHandler::SendFeedbackReportRequest,
            base::Unretained(this)));

    web_ui()->RegisterMessageCallback(
        "getShowFeedbackButton",
        base::BindRepeating(
            &ConnectivityDiagnosticsMessageHandler::GetShowFeedbackButton,
            base::Unretained(this)));
  }

 private:
  void SendFeedbackReportRequest(const base::Value::List& value) {
    send_feedback_report_callback_.Run(/*extra_diagnostics*/ "");
  }

  // TODO(crbug/1220965): Remove conditional feedback button when WebUI feedback
  // is launched.
  void GetShowFeedbackButton(const base::Value::List& args) {
    if (args.size() < 1 || !args[0].is_string())
      return;

    auto callback_id = args[0].GetString();
    base::Value::List response;
    response.Append(base::Value(show_feedback_button_));

    AllowJavascript();
    ResolveJavascriptCallback(base::Value(callback_id), response);
  }

  ConnectivityDiagnosticsUI::SendFeedbackReportCallback
      send_feedback_report_callback_;

  bool show_feedback_button_ = false;
};

ConnectivityDiagnosticsUI::ConnectivityDiagnosticsUI(
    content::WebUI* web_ui,
    BindNetworkDiagnosticsServiceCallback bind_network_diagnostics_callback,
    BindNetworkHealthServiceCallback bind_network_health_callback,
    SendFeedbackReportCallback send_feedback_report_callback,
    bool show_feedback_button)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true),
      bind_network_diagnostics_service_callback_(
          std::move(bind_network_diagnostics_callback)),
      bind_network_health_service_callback_(
          std::move(bind_network_health_callback)) {
  DCHECK(bind_network_diagnostics_service_callback_);
  DCHECK(bind_network_health_service_callback_);
  DCHECK(send_feedback_report_callback);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      kChromeUIConnectivityDiagnosticsHost);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self';");

  ash::EnableTrustedTypesCSP(source);
  source->UseStringsJs();
  source->EnableReplaceI18nInJS();

  web_ui->AddMessageHandler(
      std::make_unique<ConnectivityDiagnosticsMessageHandler>(
          std::move(send_feedback_report_callback), show_feedback_button));

  const auto resources = base::make_span(kConnectivityDiagnosticsResources,
                                         kConnectivityDiagnosticsResourcesSize);
  SetUpWebUIDataSource(source, resources,
                       IDR_CONNECTIVITY_DIAGNOSTICS_INDEX_HTML);
  source->AddLocalizedString("appTitle", IDS_CONNECTIVITY_DIAGNOSTICS_TITLE);
  source->AddLocalizedString("networkDevicesLabel",
                             IDS_CONNECTIVITY_DIAGNOSTICS_NETWORK_DEVICES);
  source->AddLocalizedString("diagnosticRoutinesLabel",
                             IDS_CONNECTIVITY_DIAGNOSTICS_DIAGNOSTIC_ROUTINES);
  source->AddLocalizedString("rerunRoutinesBtn",
                             IDS_CONNECTIVITY_DIAGNOSTICS_RERUN_ROUTINES);
  source->AddLocalizedString("closeBtn", IDS_CONNECTIVITY_DIAGNOSTICS_CLOSE);
  source->AddLocalizedString("sendFeedbackBtn",
                             IDS_CONNECTIVITY_DIAGNOSTICS_SEND_FEEDBACK);
  network_diagnostics::AddResources(source);
  network_health::AddResources(source);
}

ConnectivityDiagnosticsUI::~ConnectivityDiagnosticsUI() = default;

void ConnectivityDiagnosticsUI::BindInterface(
    mojo::PendingReceiver<
        chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines>
        receiver) {
  bind_network_diagnostics_service_callback_.Run(std::move(receiver));
}

void ConnectivityDiagnosticsUI::BindInterface(
    mojo::PendingReceiver<chromeos::network_health::mojom::NetworkHealthService>
        receiver) {
  bind_network_health_service_callback_.Run(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(ConnectivityDiagnosticsUI)

}  // namespace ash
