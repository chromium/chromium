// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PRINTING_PRINT_JOB_SUBMITTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_PRINTING_PRINT_JOB_SUBMITTER_H_

#include <memory>
#include <string>

#include "base/auto_reset.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/common/extensions/api/printing.h"
#include "chromeos/crosapi/mojom/local_printer.mojom-forward.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class ReadOnlySharedMemoryRegion;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace gfx {
class Image;
}

namespace views {
class NativeWindowTracker;
}

namespace printing {
namespace mojom {
class PdfFlattener;
}  // namespace mojom
class PrintedDocument;
class PrintSettings;
}  // namespace printing

namespace extensions {

class Extension;
class PrintJobController;

// Handles chrome.printing.submitJob() API request including parsing job
// arguments, sending errors and submitting print job to the printer.
class PrintJobSubmitter : public printing::PrintJob::Observer {
 public:
  // At most one of `job_id` and `error` are set.
  // Either `job_id`, `print_job`, and `document` are all set or none of them
  // are set.
  using SubmitJobCallback =
      base::OnceCallback<void(absl::optional<int> job_id,
                              printing::PrintJob* print_job,
                              printing::PrintedDocument* document,
                              absl::optional<std::string> error)>;

  PrintJobSubmitter(gfx::NativeWindow native_window,
                    content::BrowserContext* browser_context,
                    PrintJobController* print_job_controller,
                    mojo::Remote<printing::mojom::PdfFlattener>* pdf_flattener,
                    scoped_refptr<const extensions::Extension> extension,
                    api::printing::SubmitJobRequest request,
#if BUILDFLAG(IS_CHROMEOS_LACROS)
                    int local_printer_version,
#endif
                    crosapi::mojom::LocalPrinter* local_printer,
                    SubmitJobCallback callback);

  ~PrintJobSubmitter() override;

  static void Run(std::unique_ptr<PrintJobSubmitter> submitter);

  static base::AutoReset<bool> DisablePdfFlatteningForTesting();

  static base::AutoReset<bool> SkipConfirmationDialogForTesting();

  // PrintJob::Observer:
  void OnDocDone(int job_id, printing::PrintedDocument* document) override;
  void OnFailed() override;

 private:
  friend class PrintingAPIHandler;

  void Start();

  bool CheckContentType() const;

  bool CheckPrintTicket();

  void CheckPrinter();

  void CheckCapabilitiesCompatibility(
      crosapi::mojom::CapabilitiesResponsePtr capabilities);

  void ReadDocumentData();

  void OnDocumentDataRead(std::unique_ptr<std::string> data,
                          int64_t total_blob_length);

  void OnPdfFlattenerDisconnected();

  void OnPdfFlattened(base::ReadOnlySharedMemoryRegion flattened_pdf);

  void ShowPrintJobConfirmationDialog(const gfx::Image& extension_icon);

  void OnPrintJobConfirmationDialogClosed(bool accepted);

  void StartPrintJob();

  void FireErrorCallback(const std::string& error);

  gfx::NativeWindow native_window_;
  const raw_ptr<content::BrowserContext> browser_context_;

  // Tracks whether |native_window_| got destroyed.
  std::unique_ptr<views::NativeWindowTracker> native_window_tracker_;

  // These objects are owned by PrintingAPIHandler.
  const raw_ptr<PrintJobController> print_job_controller_;
  const raw_ptr<mojo::Remote<printing::mojom::PdfFlattener>> pdf_flattener_;

  // TODO(crbug.com/996785): Consider tracking extension being unloaded instead
  // of storing scoped_refptr.
  scoped_refptr<const extensions::Extension> extension_;
  scoped_refptr<printing::PrintJob> print_job_;

  api::printing::SubmitJobRequest request_;
  std::unique_ptr<printing::PrintSettings> settings_;
  std::u16string printer_name_;
  base::ReadOnlySharedMemoryMapping flattened_pdf_mapping_;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  const int local_printer_version_;
#endif
  const raw_ptr<crosapi::mojom::LocalPrinter> local_printer_;
  SubmitJobCallback callback_;
  base::WeakPtrFactory<PrintJobSubmitter> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PRINTING_PRINT_JOB_SUBMITTER_H_
