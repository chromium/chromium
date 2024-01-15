// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/local_printer_utils_chromeos.h"

#include <optional>
#include <string>

#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "chromeos/printing/cups_printer_status.h"
#include "chromeos/printing/printer_configuration.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/local_printer_ash.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/check_op.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/lacros/lacros_service.h"
#include "printing/print_settings.h"
#include "printing/printed_document.h"
#include "printing/printing_utils.h"
#include "ui/base/l10n/l10n_util.h"
#endif

namespace printing {

#if BUILDFLAG(IS_CHROMEOS_LACROS)
namespace {

crosapi::mojom::PrintJobPtr PrintJobToMojom(int job_id,
                                            const PrintedDocument& document,
                                            PrintJob::Source source,
                                            const std::string& source_id) {
  std::u16string title = SimplifyDocumentTitle(document.name());
  if (title.empty()) {
    title = SimplifyDocumentTitle(
        l10n_util::GetStringUTF16(IDS_DEFAULT_PRINT_DOCUMENT_TITLE));
  }
  const PrintSettings& settings = document.settings();
  int duplex = static_cast<int>(settings.duplex_mode());
  CHECK_GE(duplex, 0);
  CHECK_LT(duplex, 3);

  CHECK_NE(settings.color(), mojom::ColorModel::kUnknownColorModel);
  return crosapi::mojom::PrintJob::New(
      base::UTF16ToUTF8(settings.device_name()), base::UTF16ToUTF8(title),
      job_id, document.page_count(), source, source_id, settings.color(),
      static_cast<crosapi::mojom::PrintJob::DuplexMode>(duplex),
      settings.requested_media().size_microns,
      settings.requested_media().vendor_id, settings.copies());
}

}  // namespace

void NotifyAshJobCreated(int job_id,
                         const PrintedDocument& document,
                         const crosapi::mojom::PrintJob::Source& source,
                         const std::string& source_id,
                         crosapi::mojom::LocalPrinter* local_printer) {
  CHECK(local_printer);
  local_printer->CreatePrintJob(
      PrintJobToMojom(job_id, document, source, source_id), base::DoNothing());
}

void NotifyAshJobCreated(const PrintJob& job,
                         int job_id,
                         const PrintedDocument& document) {
  NotifyAshJobCreated(job_id, document, job.source(), job.source_id(),
                      GetLocalPrinterInterface());
}

#endif

crosapi::mojom::LocalPrinter* GetLocalPrinterInterface() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  CHECK(crosapi::CrosapiManager::IsInitialized());
  return crosapi::CrosapiManager::Get()->crosapi_ash()->local_printer_ash();
#else
  auto* service = chromeos::LacrosService::Get();
  CHECK(service->IsAvailable<crosapi::mojom::LocalPrinter>());
  return service->GetRemote<crosapi::mojom::LocalPrinter>().get();
#endif
}

crosapi::mojom::CapabilitiesResponsePtr PrinterWithCapabilitiesToMojom(
    const chromeos::Printer& printer,
    const std::optional<printing::PrinterSemanticCapsAndDefaults>& caps) {
  return crosapi::mojom::CapabilitiesResponse::New(
      PrinterToMojom(printer), printer.HasSecureProtocol(),
      caps,     // comment to prevent git cl format
      0, 0, 0,  // deprecated
      printing::mojom::PinModeRestriction::kUnset,     // deprecated
      printing::mojom::ColorModeRestriction::kUnset,   // deprecated
      printing::mojom::DuplexModeRestriction::kUnset,  // deprecated
      printing::mojom::PinModeRestriction::kUnset);    // deprecated
}

crosapi::mojom::LocalDestinationInfoPtr PrinterToMojom(
    const chromeos::Printer& printer) {
  return crosapi::mojom::LocalDestinationInfo::New(
      printer.id(), printer.display_name(), printer.description(),
      printer.source() == chromeos::Printer::SRC_POLICY,
      printer.uri().GetNormalized(/*always_print_port=*/true),
      StatusToMojom(printer.printer_status()));
}

crosapi::mojom::PrinterStatusPtr StatusToMojom(
    const chromeos::CupsPrinterStatus& status) {
  auto ptr = crosapi::mojom::PrinterStatus::New();
  ptr->printer_id = status.GetPrinterId();
  ptr->timestamp = status.GetTimestamp();
  for (const auto& reason : status.GetStatusReasons()) {
    if (reason.GetReason() == crosapi::mojom::StatusReason::Reason::kNoError) {
      continue;
    }
    ptr->status_reasons.push_back(crosapi::mojom::StatusReason::New(
        reason.GetReason(), reason.GetSeverity()));
  }
  return ptr;
}

}  // namespace printing
