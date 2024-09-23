// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/print_management/print_management_ui.h"

#include <memory>
#include <utility>

#include "ash/webui/common/trusted_types_util.h"
#include "ash/webui/grit/ash_print_management_resources.h"
#include "ash/webui/grit/ash_print_management_resources_map.h"
#include "ash/webui/print_management/backend/print_management_delegate.h"
#include "ash/webui/print_management/backend/print_management_handler.h"
#include "ash/webui/print_management/url_constants.h"
#include "chromeos/components/print_management/mojom/printing_manager.mojom.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/webui_resources.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

namespace ash {
namespace printing {
namespace printing_manager {
namespace {

void SetUpWebUIDataSource(content::WebUIDataSource* source,
                          base::span<const webui::ResourcePath> resources,
                          int default_resource) {
  for (const auto& resource : resources) {
    source->AddResourcePath(resource.path, resource.id);
  }
  source->SetDefaultResource(default_resource);
  source->AddResourcePath("test_loader.html", IDR_WEBUI_TEST_LOADER_HTML);
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER_JS);
  source->AddResourcePath("test_loader_util.js",
                          IDR_WEBUI_JS_TEST_LOADER_UTIL_JS);
}

void AddPrintManagementStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"completionStatusCanceled",
       IDS_PRINT_MANAGEMENT_COMPLETION_STATUS_CANCELED},
      {"completionStatusPrinted",
       IDS_PRINT_MANAGEMENT_COMPLETION_STATUS_PRINTED},
      {"fileNameColumn", IDS_PRINT_MANAGEMENT_FILE_NAME_COLUMN},
      {"printerNameColumn", IDS_PRINT_MANAGEMENT_PRINTER_NAME_COLUMN},
      {"dateColumn", IDS_PRINT_MANAGEMENT_DATE_COLUMN},
      {"statusColumn", IDS_PRINT_MANAGEMENT_STATUS_COLUMN},
      {"printJobTitle", IDS_PRINT_MANAGEMENT_TITLE},
      {"clearAllHistoryDialogTitle",
       IDS_PRINT_MANAGEMENT_CLEAR_ALL_HISTORY_DIALOG_TITLE},
      {"clearAllHistoryLabel",
       IDS_PRINT_MANAGEMENT_CLEAR_ALL_HISTORY_BUTTON_TEXT},
      {"clearHistoryConfirmationText",
       IDS_PRINT_MANAGEMENT_CLEAR_ALL_HISTORY_CONFIRMATION_TEXT},
      {"cancelButtonLabel", IDS_PRINT_MANAGEMENT_CANCEL_BUTTON_LABEL},
      {"clearButtonLabel", IDS_PRINT_MANAGEMENT_CLEAR_BUTTON_LABEL},
      {"historyHeader", IDS_PRINT_MANAGEMENT_HISTORY_HEADER_LABEL},
      {"printJobHistoryExpirationPeriod",
       IDS_PRINT_MANAGEMENT_HISTORY_TOOL_TIP_MULTIPLE_DAYS_EXPIRATION},
      {"printJobHistoryIndefinitePeriod",
       IDS_PRINT_MANAGEMENT_HISTORY_TOOL_TIP_INDEFINITE},
      {"printJobHistorySingleDay",
       IDS_PRINT_MANAGEMENT_HISTORY_TOOL_TIP_SINGLE_DAY_EXPIRATION},
      {"printedPageLabel", IDS_PRINT_MANAGEMENT_PRINTED_PAGES_ARIA_LABEL},
      {"printedPagesFraction",
       IDS_PRINT_MANAGEMENT_PRINTED_PAGES_PROGRESS_FRACTION},
      {"completePrintJobLabel", IDS_PRINT_MANAGEMENT_COMPLETED_JOB_ARIA_LABEL},
      {"ongoingPrintJobLabel", IDS_PRINT_MANAGEMENT_ONGOING_JOB_ARIA_LABEL},
      {"stoppedOngoingPrintJobLabel",
       IDS_PRINT_MANAGEMENT_STOPPED_ONGOING_JOB_ARIA_LABEL},
      {"paperJam", IDS_PRINT_MANAGEMENT_PAPER_JAM_ERROR_STATUS},
      {"outOfPaper", IDS_PRINT_MANAGEMENT_OUT_OF_PAPER_ERROR_STATUS},
      {"outOfInk", IDS_PRINT_MANAGEMENT_OUT_OF_INK_ERROR_STATUS},
      {"doorOpen", IDS_PRINT_MANAGEMENT_DOOR_OPEN_ERROR_STATUS},
      {"printerUnreachable",
       IDS_PRINT_MANAGEMENT_PRINTER_UNREACHABLE_ERROR_STATUS},
      {"trayMissing", IDS_PRINT_MANAGEMENT_TRAY_MISSING_ERROR_STATUS},
      {"outputFull", IDS_PRINT_MANAGEMENT_OUTPUT_FULL_ERROR_STATUS},
      {"stopped", IDS_PRINT_MANAGEMENT_STOPPED_ERROR_STATUS},
      {"clientUnauthorized",
       IDS_PRINT_MANAGEMENT_CLIENT_UNAUTHORIZED_ERROR_STATUS},
      {"expiredCertificate",
       IDS_PRINT_MANAGEMENT_EXPIRED_CERTIFICATE_ERROR_STATUS},
      {"filterFailed", IDS_PRINT_MANAGEMENT_FILTERED_FAILED_ERROR_STATUS},
      {"unknownPrinterError", IDS_PRINT_MANAGEMENT_UNKNOWN_ERROR_STATUS},
      {"paperJamStopped", IDS_PRINT_MANAGEMENT_PAPER_JAM_STOPPED_ERROR_STATUS},
      {"outOfPaperStopped",
       IDS_PRINT_MANAGEMENT_OUT_OF_PAPER_STOPPED_ERROR_STATUS},
      {"outOfInkStopped", IDS_PRINT_MANAGEMENT_OUT_OF_INK_STOPPED_ERROR_STATUS},
      {"doorOpenStopped", IDS_PRINT_MANAGEMENT_DOOR_OPEN_STOPPED_ERROR_STATUS},
      {"trayMissingStopped",
       IDS_PRINT_MANAGEMENT_TRAY_MISSING_STOPPED_ERROR_STATUS},
      {"outputFullStopped",
       IDS_PRINT_MANAGEMENT_OUTPUT_FULL_STOPPED_ERROR_STATUS},
      {"printerUnreachableStopped",
       IDS_PRINT_MANAGEMENT_PRINTER_UNREACHABLE_STOPPED_ERROR_STATUS},
      {"stoppedGeneric", IDS_PRINT_MANAGEMENT_GENERIC_STOPPED_ERROR_STATUS},
      {"unknownPrinterErrorStopped",
       IDS_PRINT_MANAGEMENT_UNKNOWN_STOPPED_ERROR_STATUS},
      {"noPrintJobInProgress",
       IDS_PRINT_MANAGEMENT_NO_PRINT_JOBS_IN_PROGRESS_MESSAGE},
      {"clearAllPrintJobPolicyIndicatorToolTip",
       IDS_PRINT_MANAGEMENT_CLEAR_ALL_POLICY_PRINT_JOB_INDICATOR_MESSAGE},
      {"cancelPrintJobButtonLabel",
       IDS_PRINT_MANAGEMENT_CANCEL_PRINT_JOB_BUTTON_LABEL},
      {"cancelledPrintJob",
       IDS_PRINT_MANAGEMENT_CANCELED_PRINT_JOB_ARIA_ANNOUNCEMENT},
      {"collapsedPrintingText", IDS_PRINT_MANAGEMENT_COLLAPSE_PRINTING_STATUS},
      {"emptyStateNoJobsMessage",
       IDS_PRINT_MANAGEMENT_EMPTY_STATE_NO_JOBS_MESSAGE},
      {"emptyStatePrinterSettingsMessage",
       IDS_PRINT_MANAGEMENT_EMPTY_STATE_PRINTER_SETTINGS_MESSAGE},
      {"managePrintersButtonLabel",
       IDS_PRINT_MANAGEMENT_EMPTY_STATE_MANAGE_PRINTERS_LABEL}};

  html_source->AddLocalizedStrings(kLocalizedStrings);
  html_source->UseStringsJs();
}
}  // namespace

PrintManagementUI::PrintManagementUI(
    content::WebUI* web_ui,
    BindPrintingMetadataProviderCallback callback,
    std::unique_ptr<PrintManagementDelegate> delegate)
    : ui::MojoWebUIController(web_ui),
      bind_pending_receiver_callback_(std::move(callback)),
      print_management_handler_(
          std::make_unique<PrintManagementHandler>(std::move(delegate))) {
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          kChromeUIPrintManagementHost);
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self';");
  ash::EnableTrustedTypesCSP(html_source);

  const auto resources = base::make_span(kAshPrintManagementResources,
                                         kAshPrintManagementResourcesSize);
  SetUpWebUIDataSource(html_source, resources,
                       IDR_ASH_PRINT_MANAGEMENT_INDEX_HTML);

  html_source->AddResourcePath(
      "printing_manager.mojom-webui.js",
      IDR_ASH_PRINT_MANAGEMENT_PRINTING_MANAGER_MOJOM_WEBUI_JS);

  AddPrintManagementStrings(html_source);
}

PrintManagementUI::~PrintManagementUI() = default;

void PrintManagementUI::BindInterface(
    mojo::PendingReceiver<
        chromeos::printing::printing_manager::mojom::PrintingMetadataProvider>
        receiver) {
  bind_pending_receiver_callback_.Run(std::move(receiver));
}

void PrintManagementUI::BindInterface(
    mojo::PendingReceiver<
        chromeos::printing::printing_manager::mojom::PrintManagementHandler>
        receiver) {
  print_management_handler_->BindInterface(std::move(receiver));
}

void PrintManagementUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(PrintManagementUI)

}  // namespace printing_manager
}  // namespace printing
}  // namespace ash
