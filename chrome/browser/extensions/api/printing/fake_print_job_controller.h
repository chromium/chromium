// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PRINTING_FAKE_PRINT_JOB_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_API_PRINTING_FAKE_PRINT_JOB_CONTROLLER_H_

#include "base/containers/flat_set.h"
#include "chrome/browser/extensions/api/printing/print_job_controller.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"

namespace extensions {

class PrintingAPIHandler;

// Fake print job controller which doesn't send print jobs to actual printing
// pipeline.
// It's used in unit and API integration tests.
class FakePrintJobController : public PrintJobController {
 public:
  FakePrintJobController();
  ~FakePrintJobController() override;

  void set_handler(PrintingAPIHandler* handler) { handler_ = handler; }

  const base::flat_set<std::string>& print_jobs() const { return print_jobs_; }

  void CreatePrintJob(const std::string& printer_id,
                      int job_id,
                      const std::string& extension_id,
                      crosapi::mojom::PrintJob::Source source) const;

  // PrintJobController:
  void StartPrintJob(const std::string& extension_id,
                     std::unique_ptr<printing::MetafileSkia> metafile,
                     std::unique_ptr<printing::PrintSettings> settings,
                     StartPrintJobCallback callback) override;
  void OnPrintJobCreated(const std::string& extension_id,
                         const std::string& job_id) override;
  void OnPrintJobFinished(const std::string& job_id) override;

 private:
  PrintingAPIHandler* handler_ = nullptr;

  // Stores ids for ongoing print jobs.
  base::flat_set<std::string> print_jobs_;

  // Current job id.
  int job_id_ = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PRINTING_FAKE_PRINT_JOB_CONTROLLER_H_
