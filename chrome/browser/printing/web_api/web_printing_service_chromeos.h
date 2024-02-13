// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_WEB_API_WEB_PRINTING_SERVICE_CHROMEOS_H_
#define CHROME_BROWSER_PRINTING_WEB_API_WEB_PRINTING_SERVICE_CHROMEOS_H_

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/types/strong_alias.h"
#include "chrome/browser/printing/web_api/in_progress_jobs_storage_chromeos.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-forward.h"
#include "third_party/blink/public/mojom/printing/web_printing.mojom.h"

namespace chromeos {
class CupsWrapper;
}  // namespace chromeos

namespace content {
class RenderFrameHost;
}  // namespace content

namespace printing {

class PdfBlobDataFlattener;
class PrintJobController;
class PrintSettings;
struct FlattenPdfResult;
struct PrinterSemanticCapsAndDefaults;
struct PrintJobCreatedInfo;

class WebPrintingServiceChromeOS
    : public content::DocumentService<blink::mojom::WebPrintingService>,
      public blink::mojom::WebPrinter {
 public:
  WebPrintingServiceChromeOS(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::WebPrintingService> receiver,
      const std::string& app_id);
  ~WebPrintingServiceChromeOS() override;

  // blink::mojom::WebPrintingService:
  void GetPrinters(GetPrintersCallback callback) override;

  // blink::mojom::WebPrinter:
  void FetchAttributes(FetchAttributesCallback callback) override;
  void Print(mojo::PendingRemote<blink::mojom::Blob> document,
             std::unique_ptr<PrintSettings> attributes,
             PrintCallback callback) override;

 private:
  using PrinterId = base::StrongAlias<class PrinterId, std::string>;

  void OnPermissionDecidedForGetPrinters(
      GetPrintersCallback,
      blink::mojom::PermissionStatus permission_status);

  void OnPrintersRetrieved(
      GetPrintersCallback callback,
      std::vector<crosapi::mojom::LocalDestinationInfoPtr> printers);

  void OnPrinterAttributesRetrieved(
      const std::string& printer_id,
      FetchAttributesCallback callback,
      blink::mojom::WebPrinterAttributesPtr printer_attributes);

  void OnPrinterAttributesRetrievedForPrint(
      mojo::PendingRemote<blink::mojom::Blob> document,
      std::unique_ptr<PrintSettings> pjt_attributes,
      PrintCallback callback,
      const std::string& printer_id,
      std::optional<PrinterSemanticCapsAndDefaults> printer_attributes);

  void OnPdfReadAndFlattened(
      std::unique_ptr<PrintSettings> settings,
      PrintCallback callback,
      std::unique_ptr<FlattenPdfResult> flatten_pdf_result);

  void OnPrintJobCreated(
      mojo::PendingRemote<blink::mojom::WebPrintJobStateObserver> observer,
      mojo::PendingReceiver<blink::mojom::WebPrintJobController> controller,
      std::optional<PrintJobCreatedInfo> creation_info);

  // Stores browser-side endpoints for blink-side Printer objects.
  mojo::ReceiverSet<blink::mojom::WebPrinter, PrinterId> printers_;

  // The id of the app that owns this service.
  const std::string app_id_;

  std::unique_ptr<chromeos::CupsWrapper> cups_wrapper_;
  std::unique_ptr<PdfBlobDataFlattener> pdf_flattener_;
  std::unique_ptr<PrintJobController> print_job_controller_;
  InProgressJobsStorageChromeOS in_progress_jobs_storage_;

  base::WeakPtrFactory<WebPrintingServiceChromeOS> weak_factory_{this};
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_WEB_API_WEB_PRINTING_SERVICE_CHROMEOS_H_
