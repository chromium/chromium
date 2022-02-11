// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_CUPS_PRINTER_STATUS_CREATOR_H_
#define CHROME_BROWSER_ASH_PRINTING_CUPS_PRINTER_STATUS_CREATOR_H_

#include <string>

#include "chromeos/printing/cups_printer_status.h"
#include "printing/printer_status.h"

namespace chromeos {
class CupsPrinterStatus;
}

namespace printing {
struct PrinterStatus;
}  // namespace printing

namespace ash {

chromeos::CupsPrinterStatus PrinterStatusToCupsPrinterStatus(
    const std::string& printer_id,
    const printing::PrinterStatus& printer_status);

chromeos::CupsPrinterStatus::CupsPrinterStatusReason::Reason
PrinterReasonToCupsReason(
    const printing::PrinterStatus::PrinterReason::Reason& reason);

chromeos::CupsPrinterStatus::CupsPrinterStatusReason::Severity
PrinterSeverityToCupsSeverity(
    const printing::PrinterStatus::PrinterReason::Severity& severity);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_CUPS_PRINTER_STATUS_CREATOR_H_
