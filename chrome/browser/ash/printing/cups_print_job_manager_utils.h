// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_CUPS_PRINT_JOB_MANAGER_UTILS_H_
#define CHROME_BROWSER_ASH_PRINTING_CUPS_PRINT_JOB_MANAGER_UTILS_H_

namespace printing {
struct PrinterStatus;
struct CupsJob;
}  // namespace printing

namespace ash {
class CupsPrintJob;

// Updates the state of a print job based on `printer_status` and `job`.
// Returns true if `print_job` has been modified.
bool UpdatePrintJob(const ::printing::PrinterStatus& printer_status,
                    const ::printing::CupsJob& job,
                    CupsPrintJob* print_job);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_CUPS_PRINT_JOB_MANAGER_UTILS_H_
