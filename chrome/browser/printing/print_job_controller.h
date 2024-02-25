// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_JOB_CONTROLLER_H_
#define CHROME_BROWSER_PRINTING_PRINT_JOB_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"

namespace printing {

class MetafileSkia;
class PrintedDocument;
class PrintJob;
class PrintSettings;

struct PrintJobCreatedInfo {
  const int32_t job_id;
  const raw_ref<PrintedDocument> document;
};

// This class serves as an entry point to the printing pipeline. It sends print
// jobs to the printing system and waits for them to be acknowledged.
// Not to be confused with PrintJobManager -- these two are not part of the same
// printing pipeline.
class PrintJobController {
 public:
  class PendingJobStorage;

  using PrintJobCreatedCallback =
      base::OnceCallback<void(std::optional<PrintJobCreatedInfo>)>;

  PrintJobController();
  PrintJobController(const PrintJobController&) = delete;
  PrintJobController& operator=(const PrintJobController&) = delete;
  virtual ~PrintJobController();

  // Creates a print job for the given `pdf` file with provided `settings` and
  // `page_count`; `page_count` should be equal to the number of pages in the
  // `pdf`.
  // Invokes `callback` with job info once the job is acknowledged
  // by the printing system.
  // If the job fails to start for some reason, invokes `callback` with
  // std::nullopt.
  // `this` indirectly owns `callback`.
  // Virtual for testing.
  virtual void CreatePrintJob(std::unique_ptr<MetafileSkia> pdf,
                              std::unique_ptr<PrintSettings> settings,
                              uint32_t page_count,
                              crosapi::mojom::PrintJob::Source source,
                              const std::string& source_id,
                              PrintJobCreatedCallback callback);

 protected:
  // Starts watching the provided `print_job` and invokes `callback` with job
  // info once the job is acknowledged by the printing system. If the job fails
  // to start for some reason, invokes `callback` with std::nullopt.
  void StartWatchingPrintJob(scoped_refptr<PrintJob> print_job,
                             PrintJobCreatedCallback callback);

 private:
  const std::unique_ptr<PendingJobStorage> pending_job_storage_;
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_JOB_CONTROLLER_H_
