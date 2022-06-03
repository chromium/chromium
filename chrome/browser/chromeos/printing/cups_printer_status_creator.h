// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINTER_STATUS_CREATOR_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINTER_STATUS_CREATOR_H_

#include <string>

#include "chromeos/printing/cups_printer_status.h"
#include "printing/printer_status.h"

namespace printing {
struct PrinterStatus;
}  // namespace printing

namespace chromeos {

class CupsPrinterStatus;

CupsPrinterStatus PrinterStatusToCupsPrinterStatus(
    const std::string& printer_id,
    const printing::PrinterStatus& printer_status);

CupsPrinterStatus::CupsPrinterStatusReason::Reason PrinterReasonToCupsReason(
    const printing::PrinterStatus::PrinterReason::Reason& reason);

CupsPrinterStatus::CupsPrinterStatusReason::Severity
PrinterSeverityToCupsSeverity(
    const printing::PrinterStatus::PrinterReason::Severity& severity);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINTER_STATUS_CREATOR_H_
