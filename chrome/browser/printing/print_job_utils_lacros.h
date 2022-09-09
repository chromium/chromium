// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_JOB_UTILS_LACROS_H_
#define CHROME_BROWSER_PRINTING_PRINT_JOB_UTILS_LACROS_H_

namespace crosapi {
namespace mojom {
class LocalPrinter;
}
}  // namespace crosapi

namespace printing {

class PrintedDocument;
class PrintJob;

// Notify Ash Chrome of a new print job.
// If `local_printer` is null, this method fails.
void NotifyAshJobCreated(const PrintJob& job,
                         int job_id,
                         const PrintedDocument& document,
                         crosapi::mojom::LocalPrinter* local_printer);

// Same as above but gets the LocalPrinter from LacrosService.
void NotifyAshJobCreated(const PrintJob& job,
                         int job_id,
                         const PrintedDocument& document);

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_JOB_UTILS_LACROS_H_
