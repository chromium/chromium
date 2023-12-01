// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/web_api/web_printing_service_chromeos.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/printing/local_printer_utils_chromeos.h"
#include "chrome/browser/printing/pdf_blob_data_flattener.h"
#include "chrome/browser/printing/print_job_controller.h"
#include "chrome/browser/printing/web_api/web_printing_type_converters.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_frame_host.h"
#include "printing/backend/print_backend.h"
#include "printing/metafile_skia.h"
#include "printing/printed_document.h"

namespace printing {

namespace {

blink::mojom::WebPrinterAttributesPtr ConvertResponse(
    crosapi::mojom::CapabilitiesResponsePtr response) {
  if (!response || !response->capabilities) {
    return nullptr;
  }
  return blink::mojom::WebPrinterAttributes::From(*response->capabilities);
}

std::optional<PrinterSemanticCapsAndDefaults> ExtractCapsAndDefaults(
    crosapi::mojom::CapabilitiesResponsePtr response) {
  return response ? response->capabilities : std::nullopt;
}

bool ValidatePrintJobTemplateAttributesAgainstPrinterAttributes(
    const PrintSettings& pjt_attributes,
    const PrinterSemanticCapsAndDefaults& printer_attributes) {
  if (pjt_attributes.copies() < 1 ||
      pjt_attributes.copies() > printer_attributes.copies_max) {
    return false;
  }
  if (pjt_attributes.collate() && !printer_attributes.collate_capable) {
    return false;
  }
  if (pjt_attributes.duplex_mode() != mojom::DuplexMode::kUnknownDuplexMode &&
      !base::Contains(printer_attributes.duplex_modes,
                      pjt_attributes.duplex_mode())) {
    return false;
  }
  return true;
}

}  // namespace

WebPrintingServiceChromeOS::WebPrintingServiceChromeOS(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::WebPrintingService> receiver)
    : DocumentService(*render_frame_host, std::move(receiver)),
      pdf_flattener_(std::make_unique<PdfBlobDataFlattener>(
          Profile::FromBrowserContext(render_frame_host->GetBrowserContext()))),
      print_job_controller_(std::make_unique<PrintJobController>()) {}

WebPrintingServiceChromeOS::~WebPrintingServiceChromeOS() = default;

void WebPrintingServiceChromeOS::GetPrinters(GetPrintersCallback callback) {
  GetLocalPrinterInterface()->GetPrinters(
      base::BindOnce(&WebPrintingServiceChromeOS::OnPrintersRetrieved,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void WebPrintingServiceChromeOS::FetchAttributes(
    FetchAttributesCallback callback) {
  GetLocalPrinterInterface()->GetCapability(
      *printers_.current_context(),
      base::BindOnce(&ConvertResponse).Then(std::move(callback)));
}

void WebPrintingServiceChromeOS::Print(
    mojo::PendingRemote<blink::mojom::Blob> document,
    std::unique_ptr<PrintSettings> attributes,
    PrintCallback callback) {
  auto printer_id = *printers_.current_context();
  attributes->set_device_name(base::UTF8ToUTF16(printer_id));
  GetLocalPrinterInterface()->GetCapability(
      printer_id,
      base::BindOnce(&ExtractCapsAndDefaults)
          .Then(base::BindOnce(
              &WebPrintingServiceChromeOS::OnPrinterAttributesRetrievedForPrint,
              weak_factory_.GetWeakPtr(), std::move(document),
              std::move(attributes), std::move(callback), printer_id)));
}

void WebPrintingServiceChromeOS::OnPrintersRetrieved(
    GetPrintersCallback callback,
    std::vector<crosapi::mojom::LocalDestinationInfoPtr> printers) {
  // TODO(b/302505962): Figure out the correct permissions UX.
  std::vector<blink::mojom::WebPrinterInfoPtr> web_printers;
  for (const auto& printer : printers) {
    mojo::PendingRemote<blink::mojom::WebPrinter> printer_remote;
    printers_.Add(this, printer_remote.InitWithNewPipeAndPassReceiver(),
                  PrinterId(printer->id));

    auto printer_info = blink::mojom::WebPrinterInfo::New();
    printer_info->printer_name = printer->name;
    printer_info->printer_remote = std::move(printer_remote);
    web_printers.push_back(std::move(printer_info));
  }
  std::move(callback).Run(std::move(web_printers));
}

void WebPrintingServiceChromeOS::OnPrinterAttributesRetrievedForPrint(
    mojo::PendingRemote<blink::mojom::Blob> document,
    std::unique_ptr<PrintSettings> pjt_attributes,
    PrintCallback callback,
    const std::string& printer_id,
    std::optional<PrinterSemanticCapsAndDefaults> printer_attributes) {
  if (!printer_attributes) {
    std::move(callback).Run(blink::mojom::WebPrintResult::NewError(
        blink::mojom::WebPrintError::kPrinterUnreachable));
    return;
  }

  if (!ValidatePrintJobTemplateAttributesAgainstPrinterAttributes(
          *pjt_attributes, *printer_attributes)) {
    std::move(callback).Run(blink::mojom::WebPrintResult::NewError(
        blink::mojom::WebPrintError::kPrintJobTemplateAttributesMismatch));
    return;
  }

  pdf_flattener_->ReadAndFlattenPdf(
      std::move(document),
      base::BindOnce(&WebPrintingServiceChromeOS::OnPdfReadAndFlattened,
                     weak_factory_.GetWeakPtr(), std::move(pjt_attributes),
                     std::move(callback)));
}

void WebPrintingServiceChromeOS::OnPdfReadAndFlattened(
    std::unique_ptr<PrintSettings> settings,
    PrintCallback callback,
    std::unique_ptr<MetafileSkia> flattened_pdf) {
  if (!flattened_pdf) {
    std::move(callback).Run(blink::mojom::WebPrintResult::NewError(
        blink::mojom::WebPrintError::kDocumentMalformed));
    return;
  }

  auto job_info = blink::mojom::WebPrintJobInfo::New();
  job_info->job_name = base::UTF16ToUTF8(settings->title());

  // TODO(b/302505962): Run this callback directly after calling
  // CreatePrintJob() on the controller without waiting for its own callback.
  // At the moment there's no signal that could allow us to keep the browser
  // test running until the printing pipeline completes; for this reason the
  // callback is currently invoked after CreatePrintJob()'s own callback to
  // account for this.
  auto cb = base::BindOnce(
      std::move(callback),
      blink::mojom::WebPrintResult::NewPrintJobInfo(std::move(job_info)));

  // TODO(b/302505962): Figure out the correct value to pass as `source_id`.
  print_job_controller_->CreatePrintJob(
      std::move(flattened_pdf), std::move(settings),
      /*source=*/crosapi::mojom::PrintJob::Source::kIsolatedWebApp,
      /*source_id=*/"",
      base::BindOnce(&WebPrintingServiceChromeOS::OnPrintJobCreated,
                     weak_factory_.GetWeakPtr())
          .Then(std::move(cb)));
}

void WebPrintingServiceChromeOS::OnPrintJobCreated(
    std::optional<PrintJobCreatedInfo> creation_info) {
  if (!creation_info) {
    // TODO(b/302505962): Propagate error via remote.
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(b/302505962): Figure out the correct value to pass as `source_id`.
  NotifyAshJobCreated(
      creation_info->job_id, *creation_info->document,
      /*source=*/crosapi::mojom::PrintJob::Source::kIsolatedWebApp,
      /*source_id=*/"", GetLocalPrinterInterface());
#endif
}

}  // namespace printing
