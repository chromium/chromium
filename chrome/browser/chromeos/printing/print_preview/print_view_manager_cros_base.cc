// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/print_preview/print_view_manager_cros_base.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/device_event_log/device_event_log.h"
#include "components/printing/browser/print_manager_utils.h"
#include "components/printing/common/print_params.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings.h"
#include "printing/print_settings_conversion.h"
#include "stdint.h"
#include "ui/accessibility/ax_tree_update.h"

using printing::PrintSettings;
using printing::mojom::PrinterType;

namespace chromeos {

PrintViewManagerCrosBase::PrintViewManagerCrosBase(
    content::WebContents* web_contents)
    : PrintManager(web_contents) {}

// TODO(jimmyxgong): Implement stubs.
void PrintViewManagerCrosBase::DidGetPrintedPagesCount(int32_t cookie,
                                                       uint32_t number_pages) {}

void PrintViewManagerCrosBase::DidPrintDocument(
    ::printing::mojom::DidPrintDocumentParamsPtr params,
    DidPrintDocumentCallback callback) {}

void PrintViewManagerCrosBase::GetDefaultPrintSettings(
    GetDefaultPrintSettingsCallback callback) {}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
void PrintViewManagerCrosBase::UpdatePrintSettings(
    base::Value::Dict job_settings,
    UpdatePrintSettingsCallback callback) {
  // TODO(b/334993067): Handle policies and prefs related to print settings.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::optional<int> printer_type_value =
      job_settings.FindInt(printing::kSettingPrinterType);
  if (!printer_type_value) {
    // TODO(jimmyxgong): Confirm if these are safe to use with
    // mojo::ReportBadMessage.
    std::move(callback).Run(nullptr);
    return;
  }

  PrinterType printer_type = static_cast<PrinterType>(*printer_type_value);
  if (printer_type != PrinterType::kExtension &&
      printer_type != PrinterType::kPdf &&
      printer_type != PrinterType::kLocal) {
    // Only support local, pdf and extension printer settings.
    std::move(callback).Run(nullptr);
    return;
  }

  std::unique_ptr<PrintSettings> print_settings =
      printing::PrintSettingsFromJobSettings(job_settings);
  if (!print_settings) {
    std::move(callback).Run(nullptr);
    return;
  }

  printing::mojom::PrintPagesParamsPtr settings =
      printing::mojom::PrintPagesParams::New();
  settings->pages = printing::GetPageRangesFromJobSettings(job_settings);
  settings->params = printing::mojom::PrintParams::New();
  printing::RenderParamsFromPrintSettings(*print_settings,
                                          settings->params.get());
  settings->params->document_cookie = PrintSettings::NewCookie();
  if (!printing::PrintMsgPrintParamsIsValid(*settings->params)) {
    PRINTER_LOG(ERROR) << "ChromeOS Print Preview: Printer settings invalid "
                       << "for "
                       << base::UTF16ToUTF8(print_settings->device_name())
                       << " (destination type " << printer_type << ")";
    std::move(callback).Run(nullptr);
    return;
  }

  set_cookie(settings->params->document_cookie);
  std::move(callback).Run(std::move(settings));
}

void PrintViewManagerCrosBase::SetAccessibilityTree(
    int32_t cookie,
    const ui::AXTreeUpdate& accessibility_tree) {}
#endif

void PrintViewManagerCrosBase::IsPrintingEnabled(
    IsPrintingEnabledCallback callback) {}

void PrintViewManagerCrosBase::ScriptedPrint(
    ::printing::mojom::ScriptedPrintParamsPtr params,
    ScriptedPrintCallback callback) {}

void PrintViewManagerCrosBase::PrintingFailed(
    int32_t cookie,
    ::printing::mojom::PrintFailureReason reason) {}

bool PrintViewManagerCrosBase::PrintNow(content::RenderFrameHost* rfh,
                                        bool has_selection) {
  return false;
}

bool PrintViewManagerCrosBase::IsCrashed() {
  return web_contents()->IsCrashed();
}

}  // namespace chromeos
