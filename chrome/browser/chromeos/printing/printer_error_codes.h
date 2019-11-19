// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINTER_ERROR_CODES_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINTER_ERROR_CODES_H_

namespace printing {
struct PrinterStatus;
}  // namespace printing

namespace chromeos {

enum class PrinterErrorCode {
  NO_ERROR,
  PAPER_JAM,
  OUT_OF_PAPER,
  OUT_OF_INK,
  DOOR_OPEN,
  PRINTER_UNREACHABLE,
  FILTER_FAILED,
  UNKNOWN_ERROR,
};

// Extracts an PrinterErrorCode from PrinterStatus#reasons. Returns NO_ERROR if
// there are no reasons which indicate an error.
PrinterErrorCode PrinterErrorCodeFromPrinterStatusReasons(
    const ::printing::PrinterStatus& printer_status);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINTER_ERROR_CODES_H_
