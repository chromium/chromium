// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/document_scan/document_scan_api_handler.h"

#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/crosapi/mojom/document_scan.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/document_scan_ash.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif

namespace extensions {

namespace {

// Error messages that can be included in a response when scanning fails.
constexpr char kNoScannersAvailableError[] = "No scanners available";
constexpr char kUnsupportedMimeTypesError[] = "Unsupported MIME types";
constexpr char kScanImageError[] = "Failed to scan image";
constexpr char kVirtualPrinterUnavailableError[] =
    "Virtual USB printer unavailable";

// The name of the virtual USB printer used for testing.
constexpr char kVirtualUSBPrinter[] = "DavieV Virtual USB Printer (USB)";

// The testing MIME type.
constexpr char kTestingMimeType[] = "testing";

// The PNG MIME type.
constexpr char kScannerImageMimeTypePng[] = "image/png";

// The PNG image data URL prefix of a scanned image.
constexpr char kPngImageDataUrlPrefix[] = "data:image/png;base64,";

crosapi::mojom::DocumentScan* GetDocumentScanInterface() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // CrosapiManager is not always initialized in tests.
  if (!crosapi::CrosapiManager::IsInitialized()) {
    CHECK_IS_TEST();
    return nullptr;
  }
  return crosapi::CrosapiManager::Get()->crosapi_ash()->document_scan_ash();
#else
  auto* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::DocumentScan>()) {
    LOG(ERROR) << "DocumentScan service not available";
    return nullptr;
  }
  return service->GetRemote<crosapi::mojom::DocumentScan>().get();
#endif
}

}  // namespace

// static
std::unique_ptr<DocumentScanAPIHandler>
DocumentScanAPIHandler::CreateForTesting(
    content::BrowserContext* browser_context,
    crosapi::mojom::DocumentScan* document_scan) {
  return base::WrapUnique(
      new DocumentScanAPIHandler(browser_context, document_scan));
}

DocumentScanAPIHandler::DocumentScanAPIHandler(
    content::BrowserContext* browser_context)
    : DocumentScanAPIHandler(browser_context, GetDocumentScanInterface()) {}

DocumentScanAPIHandler::DocumentScanAPIHandler(
    content::BrowserContext* browser_context,
    crosapi::mojom::DocumentScan* document_scan)
    : document_scan_(document_scan) {
  CHECK(document_scan_);
}

DocumentScanAPIHandler::~DocumentScanAPIHandler() = default;

// static
BrowserContextKeyedAPIFactory<DocumentScanAPIHandler>*
DocumentScanAPIHandler::GetFactoryInstance() {
  static base::NoDestructor<
      BrowserContextKeyedAPIFactory<DocumentScanAPIHandler>>
      instance;
  return instance.get();
}

// static
DocumentScanAPIHandler* DocumentScanAPIHandler::Get(
    content::BrowserContext* browser_context) {
  return BrowserContextKeyedAPIFactory<DocumentScanAPIHandler>::Get(
      browser_context);
}

void DocumentScanAPIHandler::SimpleScan(
    const std::vector<std::string>& mime_types,
    SimpleScanCallback callback) {
  bool should_use_virtual_usb_printer = false;
  if (base::Contains(mime_types, kTestingMimeType)) {
    should_use_virtual_usb_printer = true;
  } else if (!base::Contains(mime_types, kScannerImageMimeTypePng)) {
    std::move(callback).Run(absl::nullopt, kUnsupportedMimeTypesError);
    return;
  }

  document_scan_->GetScannerNames(
      base::BindOnce(&DocumentScanAPIHandler::OnSimpleScanNamesReceived,
                     weak_ptr_factory_.GetWeakPtr(),
                     should_use_virtual_usb_printer, std::move(callback)));
}

void DocumentScanAPIHandler::OnSimpleScanNamesReceived(
    bool force_virtual_usb_printer,
    SimpleScanCallback callback,
    const std::vector<std::string>& scanner_names) {
  if (scanner_names.empty()) {
    std::move(callback).Run(absl::nullopt, kNoScannersAvailableError);
    return;
  }

  // TODO(pstew): Call a delegate method here to select a scanner and options.
  // The first scanner supporting one of the requested MIME types used to be
  // selected. The testing MIME type dictates that the virtual USB printer
  // should be used if available. Otherwise, since all of the scanners always
  // support PNG, select the first scanner in the list.

  std::string scanner_name;
  if (force_virtual_usb_printer) {
    if (!base::Contains(scanner_names, kVirtualUSBPrinter)) {
      std::move(callback).Run(absl::nullopt, kVirtualPrinterUnavailableError);
      return;
    }

    scanner_name = kVirtualUSBPrinter;
  } else {
    scanner_name = scanner_names[0];
  }

  document_scan_->ScanFirstPage(
      scanner_name,
      base::BindOnce(&DocumentScanAPIHandler::OnSimpleScanCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DocumentScanAPIHandler::OnSimpleScanCompleted(
    SimpleScanCallback callback,
    crosapi::mojom::ScanFailureMode failure_mode,
    const absl::optional<std::string>& scan_data) {
  // TODO(pstew): Enlist a delegate to display received scan in the UI and
  // confirm that this scan should be sent to the caller. If this is a
  // multi-page scan, provide a means for adding additional scanned images up to
  // the requested limit.
  if (!scan_data.has_value() ||
      failure_mode != crosapi::mojom::ScanFailureMode::kNoFailure) {
    std::move(callback).Run(absl::nullopt, kScanImageError);
    return;
  }

  std::string image_base64;
  base::Base64Encode(scan_data.value(), &image_base64);
  api::document_scan::ScanResults scan_results;
  scan_results.data_urls.push_back(kPngImageDataUrlPrefix +
                                   std::move(image_base64));
  scan_results.mime_type = kScannerImageMimeTypePng;

  std::move(callback).Run(std::move(scan_results), absl::nullopt);
}

template <>
KeyedService*
BrowserContextKeyedAPIFactory<DocumentScanAPIHandler>::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Profile* profile = Profile::FromBrowserContext(context);
  // We do not want an instance of DocumentScanAPIHandler on the lock screen.
  if (!profile->IsRegularProfile()) {
    return nullptr;
  }
  return new DocumentScanAPIHandler(context);
}

}  // namespace extensions
