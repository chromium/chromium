// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PRINTING_FAKE_PRINT_JOB_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_API_PRINTING_FAKE_PRINT_JOB_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/api/printing/print_job_controller.h"

namespace extensions {

// Fake print job controller which doesn't send print jobs to actual printing
// pipeline.
// It's used in unit and API integration tests.
class FakePrintJobController : public PrintJobController {
 public:
  FakePrintJobController();
  ~FakePrintJobController() override;

  // If `fail_` is set, call OnFailed() instead of OnDocDone().
  void set_fail(bool fail) { fail_ = fail; }

  // PrintJobController:
  scoped_refptr<printing::PrintJob> StartPrintJob(
      const std::string& extension_id,
      std::unique_ptr<printing::MetafileSkia> metafile,
      std::unique_ptr<printing::PrintSettings> settings) override;

 private:
  void StartPrinting(scoped_refptr<printing::PrintJob> job,
                     const std::string& extension_id,
                     std::unique_ptr<printing::PrintSettings> settings);

  bool fail_ = false;
  // Current job id.
  int job_id_ = 0;

  base::WeakPtrFactory<FakePrintJobController> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PRINTING_FAKE_PRINT_JOB_CONTROLLER_H_
