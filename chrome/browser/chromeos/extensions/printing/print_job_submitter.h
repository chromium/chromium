// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_PRINT_JOB_SUBMITTER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_PRINT_JOB_SUBMITTER_H_

#include <memory>
#include <string>

#include "base/auto_reset.h"
#include "base/callback.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/common/extensions/api/printing.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class ReadOnlySharedMemoryRegion;
}  // namespace base

namespace chromeos {
class CupsPrintersManager;
}  // namespace chromeos

namespace content {
class BrowserContext;
}  // namespace content

namespace gfx {
class Image;
}

namespace printing {
namespace mojom {
class PdfFlattener;
}  // namespace mojom
struct PrinterSemanticCapsAndDefaults;
class PrintSettings;
}  // namespace printing

class NativeWindowTracker;

namespace extensions {

class Extension;
class PrinterCapabilitiesProvider;
class PrintJobController;

// Handles chrome.printing.submitJob() API request including parsing job
// arguments, sending errors and submitting print job to the printer.
class PrintJobSubmitter {
 public:
  // In case of success |job_id| will contain unique job identifier returned
  // by CUPS. In case of failure |job_id| is nullptr.
  // We could use base::Optional but to be consistent with auto-generated API
  // wrappers we use std::unique_ptr.
  using SubmitJobCallback = base::OnceCallback<void(
      base::Optional<api::printing::SubmitJobStatus> status,
      std::unique_ptr<std::string> job_id,
      base::Optional<std::string> error)>;

  PrintJobSubmitter(gfx::NativeWindow native_window,
                    content::BrowserContext* browser_context,
                    chromeos::CupsPrintersManager* printers_manager,
                    PrinterCapabilitiesProvider* printer_capabilities_provider,
                    PrintJobController* print_job_controller,
                    mojo::Remote<printing::mojom::PdfFlattener>* pdf_flattener,
                    scoped_refptr<const extensions::Extension> extension,
                    api::printing::SubmitJobRequest request);
  ~PrintJobSubmitter();

  // Only one call to Start() should happen at a time.
  // |callback| is called asynchronously with the success or failure of the
  // process.
  void Start(SubmitJobCallback callback);

  static base::AutoReset<bool> DisablePdfFlatteningForTesting();

  static base::AutoReset<bool> SkipConfirmationDialogForTesting();

 private:
  bool CheckContentType() const;

  bool CheckPrintTicket();

  void CheckPrinter();

  void CheckCapabilitiesCompatibility(
      base::Optional<printing::PrinterSemanticCapsAndDefaults> capabilities);

  void ReadDocumentData();

  void OnDocumentDataRead(std::unique_ptr<std::string> data,
                          int64_t total_blob_length);

  void OnPdfFlattenerDisconnected();

  void OnPdfFlattened(base::ReadOnlySharedMemoryRegion flattened_pdf);

  void ShowPrintJobConfirmationDialog(const gfx::Image& extension_icon);

  void OnPrintJobConfirmationDialogClosed(bool accepted);

  void StartPrintJob();

  void OnPrintJobRejected();

  void OnPrintJobSubmitted(std::unique_ptr<std::string> job_id);

  void FireErrorCallback(const std::string& error);

  gfx::NativeWindow native_window_;
  content::BrowserContext* const browser_context_;

  // Tracks whether |native_window_| got destroyed.
  std::unique_ptr<NativeWindowTracker> native_window_tracker_;

  // These objects are owned by PrintingAPIHandler.
  chromeos::CupsPrintersManager* const printers_manager_;
  PrinterCapabilitiesProvider* const printer_capabilities_provider_;
  PrintJobController* const print_job_controller_;
  mojo::Remote<printing::mojom::PdfFlattener>* const pdf_flattener_;

  // TODO(crbug.com/996785): Consider tracking extension being unloaded instead
  // of storing scoped_refptr.
  scoped_refptr<const extensions::Extension> extension_;
  api::printing::SubmitJobRequest request_;
  std::unique_ptr<printing::PrintSettings> settings_;
  base::string16 printer_name_;
  base::ReadOnlySharedMemoryMapping flattened_pdf_mapping_;
  // This is cleared after the request is handled (successfully or not).
  SubmitJobCallback callback_;

  base::WeakPtrFactory<PrintJobSubmitter> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_PRINT_JOB_SUBMITTER_H_
