// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_CUPS_PRINT_JOB_MANAGER_UTILS_H_
#define CHROME_BROWSER_ASH_PRINTING_CUPS_PRINT_JOB_MANAGER_UTILS_H_

#include <string>

#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace printing {
class PrintedDocument;
struct PrinterStatus;
struct CupsJob;
}  // namespace printing

namespace ash {
class CupsPrintJob;

// Updates the state of a print job based on `printer_status` and `job`.
// Returns true if `print_job` has been modified.
//
// `last_logged_status` maps a CUPS job id to the status message most recently
// logged for it, and is used to avoid logging the same status twice. The caller
// owns it so that the state is scoped to the caller's lifetime rather than the
// process's.
bool UpdatePrintJob(const ::printing::PrinterStatus& printer_status,
                    const ::printing::CupsJob& job,
                    CupsPrintJob* print_job,
                    absl::flat_hash_map<int, std::string>& last_logged_status);

// Determines the correct total_page_count for a print job given the number of
// pages in the document and copies being made.
int CalculatePrintJobTotalPages(const ::printing::PrintedDocument* document);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_CUPS_PRINT_JOB_MANAGER_UTILS_H_
