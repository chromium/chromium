// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PRINTING_PRINT_JOB_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_API_PRINTING_PRINT_JOB_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"

namespace printing {
class MetafileSkia;
class PrintedDocument;
class PrintJob;
class PrintSettings;

struct PrintJobCreatedInfo {
  const int32_t job_id;
  const raw_ref<PrintedDocument> document;
};

}  // namespace printing

namespace extensions {

// This class is responsible for sending print jobs in the printing pipeline.
// It should be used by API handler as the entry point of actual printing
// pipeline.
class PrintJobController {
 public:
  static std::unique_ptr<PrintJobController> Create();

  PrintJobController() = default;
  PrintJobController(const PrintJobController&) = delete;
  PrintJobController& operator=(const PrintJobController&) = delete;
  virtual ~PrintJobController() = default;

  // Returns an uninitialized print job and starts printing.
  // Do not call Initialize() on the returned print job. StartPrintJob() will
  // initialize it internally.
  virtual scoped_refptr<printing::PrintJob> StartPrintJob(
      const std::string& extension_id,
      std::unique_ptr<printing::MetafileSkia> metafile,
      std::unique_ptr<printing::PrintSettings> settings) = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PRINTING_PRINT_JOB_CONTROLLER_H_
