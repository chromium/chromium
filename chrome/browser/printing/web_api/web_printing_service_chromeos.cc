// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/web_api/web_printing_service_chromeos.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/containers/map_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/printing/cups_wrapper.h"
#include "chrome/browser/printing/local_printer_utils_chromeos.h"
#include "chrome/browser/printing/pdf_blob_data_flattener.h"
#include "chrome/browser/printing/print_job_controller.h"
#include "chrome/browser/printing/web_api/web_printing_type_converters.h"
#include "chrome/browser/printing/web_api/web_printing_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/permissions/permission_request_data.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "printing/backend/cups_ipp_constants.h"
#include "printing/backend/print_backend.h"
#include "printing/metafile_skia.h"
#include "printing/print_settings.h"
#include "printing/printed_document.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-shared.h"

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

bool IsDuplexModeKnown(mojom::DuplexMode duplex_mode) {
  return duplex_mode != mojom::DuplexMode::kUnknownDuplexMode;
}

bool IsColorModelKnown(mojom::ColorModel color_model) {
  return color_model != mojom::ColorModel::kUnknownColorModel;
}

bool ValidateMediaCol(
    const PrintSettings& pjt_attributes,
    const PrinterSemanticCapsAndDefaults& printer_attributes) {
  // media-size / media-size-name:
  const auto& media = pjt_attributes.requested_media();
  if (media.IsDefault()) {
    // Means nothing has actually been requested.
    return true;
  }
  const auto& papers = printer_attributes.papers;
  // Validate that the requested paper is supported by the printer.
  if (!base::ranges::any_of(papers, [&](const auto& paper) {
        return paper.IsSizeWithinBounds(media.size_microns);
      })) {
    return false;
  }
  return true;
}

void UpdatePrintJobTemplateAttributesWithPrinterDefaults(
    PrintSettings& pjt_attributes,
    const PrinterSemanticCapsAndDefaults& printer_attributes) {
  if (!IsDuplexModeKnown(pjt_attributes.duplex_mode())) {
    pjt_attributes.set_duplex_mode(printer_attributes.duplex_default);
  }
  if (!IsColorModelKnown(pjt_attributes.color())) {
    pjt_attributes.set_color(printer_attributes.color_default
                                 ? mojom::ColorModel::kColorModeColor
                                 : mojom::ColorModel::kColorModeMonochrome);
  }
}

bool ValidateAdvancedCapability(
    const PrintSettings& pjt_attributes,
    const PrinterSemanticCapsAndDefaults& printer_attributes,
    const std::string& capability_name) {
  auto* requested_capability =
      base::FindOrNull(pjt_attributes.advanced_settings(), capability_name);
  if (!requested_capability) {
    // If the capability has not been actually requested, we're good.
    return true;
  }
  auto* printer_capability =
      internal::FindAdvancedCapability(printer_attributes, capability_name);
  if (!printer_capability) {
    // If the capability has been requested but the printer doesn't support it,
    // reject.
    return false;
  }
  // `requested_capability` is guaranteed to be a string -- it's set this way in
  // StructTraits<>.
  return base::Contains(printer_capability->values,
                        requested_capability->GetString(),
                        &AdvancedCapabilityValue::name);
}

bool ValidateAttributesAndUpdateIfNecessary(
    PrintSettings& pjt_attributes,
    const PrinterSemanticCapsAndDefaults& printer_attributes) {
  if (pjt_attributes.copies() < 1 ||
      pjt_attributes.copies() > printer_attributes.copies_max) {
    return false;
  }
  if (pjt_attributes.collate() && !printer_attributes.collate_capable) {
    return false;
  }
  // Checks that printer supports color printing if requested so.
  if (IsColorModelKnown(pjt_attributes.color()) &&
      ::printing::IsColorModelSelected(pjt_attributes.color()).value() &&
      !IsColorModelKnown(printer_attributes.color_model)) {
    return false;
  }
  if (IsDuplexModeKnown(pjt_attributes.duplex_mode()) &&
      !base::Contains(printer_attributes.duplex_modes,
                      pjt_attributes.duplex_mode())) {
    return false;
  }
  if (!IsDuplexModeKnown(pjt_attributes.duplex_mode()) &&
      !IsDuplexModeKnown(printer_attributes.duplex_default)) {
    return false;
  }
  if (!pjt_attributes.dpi_size().IsZero() &&
      !base::Contains(printer_attributes.dpis, pjt_attributes.dpi_size())) {
    return false;
  }
  if (!ValidateMediaCol(pjt_attributes, printer_attributes)) {
    return false;
  }
  if (!ValidateAdvancedCapability(pjt_attributes, printer_attributes,
                                  kIppMediaSource)) {
    return false;
  }
  // Update selected fields to printer defaults if they're not specified.
  UpdatePrintJobTemplateAttributesWithPrinterDefaults(pjt_attributes,
                                                      printer_attributes);
  return true;
}

blink::mojom::WebPrinterAttributesPtr MergePrinterAttributesAndStatus(
    blink::mojom::WebPrinterAttributesPtr printer_attributes,
    std::unique_ptr<PrinterStatus> printer_status) {
  if (!printer_status) {
    // Even though `printer_attributes` were successfully fetched, it's better
    // to play safe here and pretend that the entire request has failed.
    return nullptr;
  }
  printer_attributes->printer_state = printer_status->state;
  auto& printer_state_reasons = printer_attributes->printer_state_reasons;
  for (const auto& reason : printer_status->reasons) {
    printer_state_reasons.push_back(reason.reason);
  }
  base::ranges::sort(printer_state_reasons);
  printer_state_reasons.erase(base::ranges::unique(printer_state_reasons),
                              printer_state_reasons.end());
  printer_attributes->printer_state_message = printer_status->message;
  return printer_attributes;
}

bool HasPrintingPermission(content::RenderFrameHost& rfh) {
  return rfh.GetBrowserContext()
             ->GetPermissionController()
             ->GetPermissionStatusForCurrentDocument(
                 blink::PermissionType::WEB_PRINTING, &rfh) ==
         blink::mojom::PermissionStatus::GRANTED;
}

void InvokeFetchAttributesCallback(
    WebPrintingServiceChromeOS::FetchAttributesCallback callback,
    blink::mojom::WebPrinterAttributesPtr printer_attributes) {
  if (!printer_attributes) {
    std::move(callback).Run(blink::mojom::WebPrinterFetchResult::NewError(
        blink::mojom::WebPrinterFetchError::kPrinterUnreachable));
    return;
  }
  std::move(callback).Run(
      blink::mojom::WebPrinterFetchResult::NewPrinterAttributes(
          std::move(printer_attributes)));
}

}  // namespace

WebPrintingServiceChromeOS::WebPrintingServiceChromeOS(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::WebPrintingService> receiver,
    const std::string& app_id)
    : DocumentService(*render_frame_host, std::move(receiver)),
      app_id_(app_id),
      cups_wrapper_(chromeos::CupsWrapper::Create()),
      pdf_flattener_(std::make_unique<PdfBlobDataFlattener>(
          Profile::FromBrowserContext(render_frame_host->GetBrowserContext()))),
      print_job_controller_(std::make_unique<PrintJobController>()) {}

WebPrintingServiceChromeOS::~WebPrintingServiceChromeOS() = default;

void WebPrintingServiceChromeOS::GetPrinters(GetPrintersCallback callback) {
  render_frame_host()
      .GetBrowserContext()
      ->GetPermissionController()
      ->RequestPermissionFromCurrentDocument(
          &render_frame_host(),
          content::PermissionRequestDescription(
              blink::PermissionType::WEB_PRINTING),
          base::BindOnce(
              &WebPrintingServiceChromeOS::OnPermissionDecidedForGetPrinters,
              weak_factory_.GetWeakPtr(), std::move(callback)));
}

void WebPrintingServiceChromeOS::FetchAttributes(
    FetchAttributesCallback callback) {
  if (!HasPrintingPermission(render_frame_host())) {
    std::move(callback).Run(blink::mojom::WebPrinterFetchResult::NewError(
        blink::mojom::WebPrinterFetchError::kUserPermissionDenied));
    return;
  }

  const std::string& printer_id = *printers_.current_context();
  GetLocalPrinterInterface()->GetCapability(
      printer_id,
      base::BindOnce(&ConvertResponse)
          .Then(base::BindOnce(
              &WebPrintingServiceChromeOS::OnPrinterAttributesRetrieved,
              weak_factory_.GetWeakPtr(), printer_id, std::move(callback))));
}

void WebPrintingServiceChromeOS::Print(
    mojo::PendingRemote<blink::mojom::Blob> document,
    std::unique_ptr<PrintSettings> attributes,
    PrintCallback callback) {
  if (!HasPrintingPermission(render_frame_host())) {
    std::move(callback).Run(blink::mojom::WebPrintResult::NewError(
        blink::mojom::WebPrintError::kUserPermissionDenied));
    return;
  }

  const std::string& printer_id = *printers_.current_context();
  attributes->set_device_name(base::UTF8ToUTF16(printer_id));
  GetLocalPrinterInterface()->GetCapability(
      printer_id,
      base::BindOnce(&ExtractCapsAndDefaults)
          .Then(base::BindOnce(
              &WebPrintingServiceChromeOS::OnPrinterAttributesRetrievedForPrint,
              weak_factory_.GetWeakPtr(), std::move(document),
              std::move(attributes), std::move(callback), printer_id)));
}

void WebPrintingServiceChromeOS::OnPermissionDecidedForGetPrinters(
    GetPrintersCallback callback,
    blink::mojom::PermissionStatus permission_status) {
  if (permission_status != blink::mojom::PermissionStatus::GRANTED) {
    std::move(callback).Run(blink::mojom::GetPrintersResult::NewError(
        blink::mojom::GetPrintersError::kUserPermissionDenied));
    return;
  }
  GetLocalPrinterInterface()->GetPrinters(
      base::BindOnce(&WebPrintingServiceChromeOS::OnPrintersRetrieved,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
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
  std::move(callback).Run(
      blink::mojom::GetPrintersResult::NewPrinters(std::move(web_printers)));
}

void WebPrintingServiceChromeOS::OnPrinterAttributesRetrieved(
    const std::string& printer_id,
    FetchAttributesCallback callback,
    blink::mojom::WebPrinterAttributesPtr printer_attributes) {
  if (!printer_attributes) {
    InvokeFetchAttributesCallback(std::move(callback),
                                  /*printer_attributes=*/nullptr);
    return;
  }
  cups_wrapper_->QueryCupsPrinterStatus(
      printer_id, base::BindOnce(&MergePrinterAttributesAndStatus,
                                 std::move(printer_attributes))
                      .Then(base::BindOnce(&InvokeFetchAttributesCallback,
                                           std::move(callback))));
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

  if (!ValidateAttributesAndUpdateIfNecessary(*pjt_attributes,
                                              *printer_attributes)) {
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
    std::unique_ptr<FlattenPdfResult> flatten_pdf_result) {
  if (!flatten_pdf_result) {
    std::move(callback).Run(blink::mojom::WebPrintResult::NewError(
        blink::mojom::WebPrintError::kDocumentMalformed));
    return;
  }

  mojo::PendingRemote<blink::mojom::WebPrintJobStateObserver> observer;
  mojo::PendingReceiver<blink::mojom::WebPrintJobController> controller;
  auto job_info = blink::mojom::WebPrintJobInfo::New();
  job_info->job_name = base::UTF16ToUTF8(settings->title());
  // Total number of pages in all copies.
  job_info->job_pages = flatten_pdf_result->page_count * settings->copies();
  job_info->observer = observer.InitWithNewPipeAndPassReceiver();
  job_info->controller = controller.InitWithNewPipeAndPassRemote();

  print_job_controller_->CreatePrintJob(
      std::move(flatten_pdf_result->flattened_pdf), std::move(settings),
      flatten_pdf_result->page_count,
      /*source=*/crosapi::mojom::PrintJob::Source::kIsolatedWebApp,
      /*source_id=*/app_id_,
      base::BindOnce(&WebPrintingServiceChromeOS::OnPrintJobCreated,
                     weak_factory_.GetWeakPtr(), std::move(observer),
                     std::move(controller)));

  std::move(callback).Run(
      blink::mojom::WebPrintResult::NewPrintJobInfo(std::move(job_info)));
}

void WebPrintingServiceChromeOS::OnPrintJobCreated(
    mojo::PendingRemote<blink::mojom::WebPrintJobStateObserver> observer,
    mojo::PendingReceiver<blink::mojom::WebPrintJobController> controller,
    std::optional<PrintJobCreatedInfo> creation_info) {
  if (!creation_info) {
    // Dispatches a notification and deletes itself.
    auto update = blink::mojom::WebPrintJobUpdate::New();
    update->state = blink::mojom::WebPrintJobState::kAborted;
    mojo::Remote<blink::mojom::WebPrintJobStateObserver>(std::move(observer))
        ->OnWebPrintJobUpdate(std::move(update));
    return;
  }

  std::string printer_id =
      base::UTF16ToUTF8(creation_info->document->settings().device_name());
  in_progress_jobs_storage_.PrintJobAcknowledgedByThePrintSystem(
      printer_id, creation_info->job_id, std::move(observer),
      std::move(controller));

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  NotifyAshJobCreated(
      creation_info->job_id, *creation_info->document,
      /*source=*/crosapi::mojom::PrintJob::Source::kIsolatedWebApp,
      /*source_id=*/app_id_, GetLocalPrinterInterface());
#endif
}

}  // namespace printing
