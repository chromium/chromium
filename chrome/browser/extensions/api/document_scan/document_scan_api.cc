// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/document_scan/document_scan_api.h"

#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "chromeos/crosapi/mojom/document_scan.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/document_scan_ash.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif

namespace extensions::api {

namespace {

// Error messages that can be included in a response when scanning fails.
constexpr char kUserGestureRequiredError[] =
    "User gesture required to perform scan";
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

}  // namespace

DocumentScanScanFunction::DocumentScanScanFunction() = default;

DocumentScanScanFunction::~DocumentScanScanFunction() = default;

void DocumentScanScanFunction::SetMojoInterfaceForTesting(
    crosapi::mojom::DocumentScan* document_scan) {
  document_scan_ = document_scan;
}

ExtensionFunction::ResponseAction DocumentScanScanFunction::Run() {
  params_ = document_scan::Scan::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params_);

  if (!user_gesture())
    return RespondNow(Error(kUserGestureRequiredError));

  MaybeInitializeMojoInterface();
  if (!document_scan_)
    return RespondNow(Error(kScanImageError));

  document_scan_->GetScannerNames(
      base::BindOnce(&DocumentScanScanFunction::OnNamesReceived, this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void DocumentScanScanFunction::MaybeInitializeMojoInterface() {
  // Check if SetMojoInterfaceForTesting() already initialized `document_scan_`.
  if (document_scan_)
    return;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  DCHECK(crosapi::CrosapiManager::IsInitialized());
  document_scan_ =
      crosapi::CrosapiManager::Get()->crosapi_ash()->document_scan_ash();
#else
  auto* service = chromeos::LacrosService::Get();
  if (service->IsAvailable<crosapi::mojom::DocumentScan>()) {
    document_scan_ = service->GetRemote<crosapi::mojom::DocumentScan>().get();
  } else {
    LOG(ERROR) << "Document scan not available";
  }
#endif
}

void DocumentScanScanFunction::OnNamesReceived(
    const std::vector<std::string>& scanner_names) {
  if (scanner_names.empty()) {
    Respond(Error(kNoScannersAvailableError));
    return;
  }

  bool should_use_virtual_usb_printer = false;
  if (params_->options.mime_types) {
    std::vector<std::string>& mime_types = *params_->options.mime_types;
    if (base::Contains(mime_types, kTestingMimeType)) {
      should_use_virtual_usb_printer = true;
    } else if (!base::Contains(mime_types, kScannerImageMimeTypePng)) {
      Respond(Error(kUnsupportedMimeTypesError));
      return;
    }
  }

  // TODO(pstew): Call a delegate method here to select a scanner and options.
  // The first scanner supporting one of the requested MIME types used to be
  // selected. The testing MIME type dictates that the virtual USB printer
  // should be used if available. Otherwise, since all of the scanners only
  // support PNG, select the first scanner in the list.

  std::string scanner_name;
  if (should_use_virtual_usb_printer) {
    if (!base::Contains(scanner_names, kVirtualUSBPrinter)) {
      Respond(Error(kVirtualPrinterUnavailableError));
      return;
    }

    scanner_name = kVirtualUSBPrinter;
  } else {
    scanner_name = scanner_names[0];
  }

  document_scan_->ScanFirstPage(
      scanner_name,
      base::BindOnce(&DocumentScanScanFunction::OnScanCompleted, this));
}

void DocumentScanScanFunction::OnScanCompleted(
    crosapi::mojom::ScanFailureMode failure_mode,
    const absl::optional<std::string>& scan_data) {
  // TODO(pstew): Enlist a delegate to display received scan in the UI and
  // confirm that this scan should be sent to the caller. If this is a
  // multi-page scan, provide a means for adding additional scanned images up to
  // the requested limit.
  if (!scan_data.has_value() ||
      failure_mode != crosapi::mojom::ScanFailureMode::kNoFailure) {
    Respond(Error(kScanImageError));
    return;
  }

  std::string image_base64;
  base::Base64Encode(scan_data.value(), &image_base64);
  document_scan::ScanResults scan_results;
  scan_results.data_urls.push_back(kPngImageDataUrlPrefix + image_base64);
  scan_results.mime_type = kScannerImageMimeTypePng;
  Respond(ArgumentList(document_scan::Scan::Results::Create(scan_results)));
}

}  // namespace extensions::api
