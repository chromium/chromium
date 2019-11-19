// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/printer_error_codes.h"

#include "printing/backend/cups_jobs.h"

namespace chromeos {

using PrinterReason = ::printing::PrinterStatus::PrinterReason;

PrinterErrorCode PrinterErrorCodeFromPrinterStatusReasons(
    const ::printing::PrinterStatus& printer_status) {
  for (const auto& reason : printer_status.reasons) {
    switch (reason.reason) {
      case PrinterReason::MEDIA_EMPTY:
      case PrinterReason::MEDIA_NEEDED:
      case PrinterReason::MEDIA_LOW:
        return PrinterErrorCode::OUT_OF_PAPER;
      case PrinterReason::MEDIA_JAM:
        return PrinterErrorCode::PAPER_JAM;
      case PrinterReason::TONER_EMPTY:
      case PrinterReason::TONER_LOW:
        return PrinterErrorCode::OUT_OF_INK;
      case PrinterReason::TIMED_OUT:
        return PrinterErrorCode::PRINTER_UNREACHABLE;
      case PrinterReason::DOOR_OPEN:
      case PrinterReason::COVER_OPEN:
        return PrinterErrorCode::DOOR_OPEN;
      default:
        break;
    }
  }
  return PrinterErrorCode::NO_ERROR;
}

}  // namespace chromeos
