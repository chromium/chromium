// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/extensions/api/document_scan/simple_scan_runner.h"

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/common/pref_names.h"
#include "chromeos/crosapi/mojom/document_scan.mojom.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/extension.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/native_window_tracker.h"

namespace extensions {

namespace {

// Error messages that can be included in a response when scanning fails.
constexpr char kNoScannersAvailableError[] = "No scanners available";
constexpr char kScanImageError[] = "Failed to scan image";
constexpr char kUnsupportedMimeTypesError[] = "Unsupported MIME types";
constexpr char kVirtualPrinterUnavailableError[] =
    "Virtual USB printer unavailable";

// Special MIME type that triggers use of virtual-usb-printer for scanning.
constexpr char kTestingMimeType[] = "testing";

// The name of the virtual USB printer used for testing.
constexpr char kVirtualUSBPrinter[] = "DavieV Virtual USB Printer (USB)";

// The PNG MIME type.
constexpr char kScannerImageMimeTypePng[] = "image/png";

// The PNG image data URL prefix of a scanned image.
constexpr char kPngImageDataUrlPrefix[] = "data:image/png;base64,";

}  // namespace

SimpleScanRunner::SimpleScanRunner(scoped_refptr<const Extension> extension,
                                   crosapi::mojom::DocumentScan* document_scan)
    : extension_(std::move(extension)), document_scan_(document_scan) {
  CHECK(extension_);
}

SimpleScanRunner::~SimpleScanRunner() = default;

void SimpleScanRunner::Start(std::vector<std::string> mime_types,
                             SimpleScanCallback callback) {
  CHECK(!callback_) << "scan call already in progress";
  callback_ = std::move(callback);
  mime_types_ = std::move(mime_types);

  bool should_use_virtual_usb_printer = false;
  if (base::Contains(mime_types_, kTestingMimeType)) {
    should_use_virtual_usb_printer = true;
  } else if (!base::Contains(mime_types_, kScannerImageMimeTypePng)) {
    std::move(callback_).Run(std::nullopt, kUnsupportedMimeTypesError);
    return;
  }

  document_scan_->GetScannerNames(base::BindOnce(
      &SimpleScanRunner::OnSimpleScanNamesReceived,
      weak_ptr_factory_.GetWeakPtr(), should_use_virtual_usb_printer));
}

const ExtensionId& SimpleScanRunner::extension_id() const {
  return extension_->id();
}

void SimpleScanRunner::OnSimpleScanNamesReceived(
    bool force_virtual_usb_printer,
    const std::vector<std::string>& scanner_names) {
  if (scanner_names.empty()) {
    std::move(callback_).Run(std::nullopt, kNoScannersAvailableError);
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
      std::move(callback_).Run(std::nullopt, kVirtualPrinterUnavailableError);
      return;
    }

    scanner_name = kVirtualUSBPrinter;
  } else {
    scanner_name = scanner_names[0];
  }

  document_scan_->ScanFirstPage(
      scanner_name, base::BindOnce(&SimpleScanRunner::OnSimpleScanCompleted,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void SimpleScanRunner::OnSimpleScanCompleted(
    crosapi::mojom::ScanFailureMode failure_mode,
    const std::optional<std::string>& scan_data) {
  // TODO(pstew): Enlist a delegate to display received scan in the UI and
  // confirm that this scan should be sent to the caller. If this is a
  // multi-page scan, provide a means for adding additional scanned images up to
  // the requested limit.
  if (!scan_data.has_value() ||
      failure_mode != crosapi::mojom::ScanFailureMode::kNoFailure) {
    std::move(callback_).Run(std::nullopt, kScanImageError);
    return;
  }

  std::string image_base64 = base::Base64Encode(scan_data.value());
  api::document_scan::ScanResults scan_results;
  scan_results.data_urls.push_back(kPngImageDataUrlPrefix +
                                   std::move(image_base64));
  scan_results.mime_type = kScannerImageMimeTypePng;

  std::move(callback_).Run(std::move(scan_results), std::nullopt);
}

}  // namespace extensions
