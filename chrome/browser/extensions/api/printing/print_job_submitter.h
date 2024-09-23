// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PRINTING_PRINT_JOB_SUBMITTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_PRINTING_PRINT_JOB_SUBMITTER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/auto_reset.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/common/extensions/api/printing.h"
#include "chromeos/crosapi/mojom/local_printer.mojom-forward.h"
#include "ui/gfx/native_widget_types.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace gfx {
class Image;
}  // namespace gfx

namespace views {
class NativeWindowTracker;
}  // namespace views

namespace printing {
class PdfBlobDataFlattener;
class PrintedDocument;
class PrintJobController;
class PrintSettings;
struct FlattenPdfResult;
struct PrintJobCreatedInfo;
}  // namespace printing

namespace extensions {

class Extension;

// Handles chrome.printing.submitJob() API request including parsing job
// arguments, sending errors and submitting print job to the printer.
class PrintJobSubmitter {
 public:
  // The error field in `PrintJobCreationResult` is std::nullopt when a print
  // job is rejected by the user. In all other possible failure cases the error
  // is well-formed.
  using PrintJobCreationResult =
      base::expected<printing::PrintJobCreatedInfo, std::optional<std::string>>;

  using SubmitJobCallback = base::OnceCallback<void(PrintJobCreationResult)>;

  PrintJobSubmitter(gfx::NativeWindow native_window,
                    content::BrowserContext* browser_context,
                    printing::PrintJobController* print_job_controller,
                    printing::PdfBlobDataFlattener* pdf_blob_data_flattener,
                    scoped_refptr<const extensions::Extension> extension,
                    api::printing::SubmitJobRequest request,
                    crosapi::mojom::LocalPrinter* local_printer,
                    SubmitJobCallback callback);

  ~PrintJobSubmitter();

  static void Run(std::unique_ptr<PrintJobSubmitter> submitter);

  static base::AutoReset<bool> DisablePdfFlatteningForTesting();

  static void SkipConfirmationDialogForTesting();

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

  void OnPdfReadAndFlattened(
      std::unique_ptr<printing::FlattenPdfResult> result);

  void OnImageDataRead(std::string data, int64_t);

  void ShowPrintJobConfirmationDialog(const gfx::Image& extension_icon);

  void OnPrintJobConfirmationDialogClosed(bool accepted);

  void StartPrintJob();

  void OnPrintJobCreated(std::optional<printing::PrintJobCreatedInfo> info);

  void FireErrorCallback(const std::string& error);

  gfx::NativeWindow native_window_;
  const raw_ptr<content::BrowserContext> browser_context_;

  // Tracks whether |native_window_| got destroyed.
  std::unique_ptr<views::NativeWindowTracker> native_window_tracker_;

  // These objects are owned by PrintingAPIHandler.
  const raw_ptr<printing::PrintJobController> print_job_controller_;
  const raw_ref<printing::PdfBlobDataFlattener> pdf_blob_data_flattener_;

  // TODO(crbug.com/40641692): Consider tracking extension being unloaded
  // instead of storing scoped_refptr.
  scoped_refptr<const extensions::Extension> extension_;

  api::printing::SubmitJobRequest request_;
  std::unique_ptr<printing::PrintSettings> settings_;
  std::u16string printer_name_;

  std::unique_ptr<printing::FlattenPdfResult> flatten_pdf_result_;

  const raw_ptr<crosapi::mojom::LocalPrinter> local_printer_;
  SubmitJobCallback callback_;
  base::WeakPtrFactory<PrintJobSubmitter> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PRINTING_PRINT_JOB_SUBMITTER_H_
