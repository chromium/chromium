// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/local_printer_utils_chromeos.h"

#include <optional>
#include <string>

#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "chromeos/printing/cups_printer_status.h"
#include "chromeos/printing/printer_configuration.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/local_printer_ash.h"

namespace printing {

crosapi::mojom::LocalPrinter* GetLocalPrinterInterface() {
  CHECK(crosapi::CrosapiManager::IsInitialized());
  return crosapi::CrosapiManager::Get()->crosapi_ash()->local_printer_ash();
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
