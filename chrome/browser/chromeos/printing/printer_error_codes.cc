// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/printer_error_codes.h"

#include "chromeos/components/print_management/mojom/printing_manager.mojom.h"
#include "printing/backend/cups_jobs.h"
#include "printing/printer_status.h"

namespace chromeos {

namespace {

#ifndef STATIC_ASSERT_ENUM
#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enums: " #a)
#endif

STATIC_ASSERT_ENUM(
    PrinterErrorCode::NO_ERROR,
    printing::printing_manager::mojom::PrinterErrorCode::kNoError);
STATIC_ASSERT_ENUM(
    PrinterErrorCode::PAPER_JAM,
    printing::printing_manager::mojom::PrinterErrorCode::kPaperJam);
STATIC_ASSERT_ENUM(
    PrinterErrorCode::OUT_OF_PAPER,
    printing::printing_manager::mojom::PrinterErrorCode::kOutOfPaper);
STATIC_ASSERT_ENUM(
    PrinterErrorCode::OUT_OF_INK,
    printing::printing_manager::mojom::PrinterErrorCode::kOutOfInk);
STATIC_ASSERT_ENUM(
    PrinterErrorCode::DOOR_OPEN,
    printing::printing_manager::mojom::PrinterErrorCode::kDoorOpen);
STATIC_ASSERT_ENUM(
    PrinterErrorCode::PRINTER_UNREACHABLE,
    printing::printing_manager::mojom::PrinterErrorCode::kPrinterUnreachable);
STATIC_ASSERT_ENUM(
    PrinterErrorCode::TRAY_MISSING,
    printing::printing_manager::mojom::PrinterErrorCode::kTrayMissing);
STATIC_ASSERT_ENUM(
    PrinterErrorCode::OUTPUT_FULL,
    printing::printing_manager::mojom::PrinterErrorCode::kOutputFull);
STATIC_ASSERT_ENUM(
    PrinterErrorCode::STOPPED,
    printing::printing_manager::mojom::PrinterErrorCode::kStopped);
STATIC_ASSERT_ENUM(
    PrinterErrorCode::FILTER_FAILED,
    printing::printing_manager::mojom::PrinterErrorCode::kFilterFailed);
STATIC_ASSERT_ENUM(
    PrinterErrorCode::UNKNOWN_ERROR,
    printing::printing_manager::mojom::PrinterErrorCode::kUnknownError);
STATIC_ASSERT_ENUM(
    PrinterErrorCode::CLIENT_UNAUTHORIZED,
    printing::printing_manager::mojom::PrinterErrorCode::kClientUnauthorized);
STATIC_ASSERT_ENUM(
    PrinterErrorCode::EXPIRED_CERTIFICATE,
    printing::printing_manager::mojom::PrinterErrorCode::kExpiredCertificate);
}  // namespace

using PrinterReason = ::printing::PrinterStatus::PrinterReason;

PrinterErrorCode PrinterErrorCodeFromPrinterStatusReasons(
    const ::printing::PrinterStatus& printer_status) {
  for (const auto& reason : printer_status.reasons) {
    if (reason.severity != PrinterReason::Severity::kError &&
        reason.severity != PrinterReason::Severity::kWarning) {
      continue;
    }

    switch (reason.reason) {
      case PrinterReason::Reason::kMediaEmpty:
      case PrinterReason::Reason::kMediaNeeded:
        return PrinterErrorCode::OUT_OF_PAPER;
      case PrinterReason::Reason::kMediaJam:
        return PrinterErrorCode::PAPER_JAM;
      case PrinterReason::Reason::kTonerEmpty:
      case PrinterReason::Reason::kDeveloperEmpty:
      case PrinterReason::Reason::kMarkerSupplyEmpty:
      case PrinterReason::Reason::kMarkerWasteFull:
        return PrinterErrorCode::OUT_OF_INK;
      case PrinterReason::Reason::kTimedOut:
      case PrinterReason::Reason::kShutdown:
        return PrinterErrorCode::PRINTER_UNREACHABLE;
      case PrinterReason::Reason::kDoorOpen:
      case PrinterReason::Reason::kCoverOpen:
      case PrinterReason::Reason::kInterlockOpen:
        return PrinterErrorCode::DOOR_OPEN;
      case PrinterReason::Reason::kInputTrayMissing:
      case PrinterReason::Reason::kOutputTrayMissing:
        return PrinterErrorCode::TRAY_MISSING;
      case PrinterReason::Reason::kOutputAreaFull:
      case PrinterReason::Reason::kOutputAreaAlmostFull:
        return PrinterErrorCode::OUTPUT_FULL;
      case PrinterReason::Reason::kStopping:
      case PrinterReason::Reason::kStoppedPartly:
      case PrinterReason::Reason::kPaused:
      case PrinterReason::Reason::kMovingToPaused:
        return PrinterErrorCode::STOPPED;
      case PrinterReason::Reason::kCupsPkiExpired:
        return PrinterErrorCode::EXPIRED_CERTIFICATE;
      case PrinterReason::Reason::kMediaLow:
      case PrinterReason::Reason::kTonerLow:
      case PrinterReason::Reason::kDeveloperLow:
      case PrinterReason::Reason::kMarkerSupplyLow:
      case PrinterReason::Reason::kMarkerWasteAlmostFull:
      default:
        break;
    }
  }
  return PrinterErrorCode::NO_ERROR;
}

}  // namespace chromeos
