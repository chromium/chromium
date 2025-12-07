// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/extensions/api/document_scan/simple_scan_runner.h"

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/scanning/lorgnette_scanner_manager.h"
#include "chrome/browser/ash/scanning/lorgnette_scanner_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/common/pref_names.h"
#include "chromeos/crosapi/mojom/document_scan.mojom.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/extension.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/native_window_tracker/native_window_tracker.h"

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

// The delay between reads from the scanner when data isn't expected to be ready
// immediately.
constexpr base::TimeDelta kSlowReadInterval = base::Milliseconds(500);

// The delay between reads from the scanner when data might be ready.
constexpr base::TimeDelta kReadInterval = base::Milliseconds(100);

// The connection type name for Mopria eSCL scanners.
constexpr char kMopriaProtocolName[] = "Mopria";

}  // namespace

SimpleScanRunner::SimpleScanRunner(content::BrowserContext* browser_context,
                                   scoped_refptr<const Extension> extension,
                                   crosapi::mojom::DocumentScan* document_scan)
    : browser_context_(browser_context),
      extension_(std::move(extension)),
      document_scan_(document_scan) {
  CHECK(browser_context_);
  CHECK(extension_);
}

SimpleScanRunner::~SimpleScanRunner() = default;

void SimpleScanRunner::Start(std::vector<std::string> mime_types,
                             SimpleScanCallback callback) {
  CHECK(!callback_) << "scan call already in progress";
  callback_ = std::move(callback);
  mime_types_ = std::move(mime_types);

  // Clear any leftover state from a previous scan.
  scanner_ids_.clear();
  scanner_handle_ = "";
  job_handle_ = "";
  scan_data_.clear();
  scan_result_ = crosapi::mojom::ScanFailureMode::kUnknown;

  bool should_use_virtual_usb_printer = false;
  if (base::Contains(mime_types_, kTestingMimeType)) {
    should_use_virtual_usb_printer = true;
  } else if (!base::Contains(mime_types_, kScannerImageMimeTypePng)) {
    std::move(callback_).Run(std::nullopt, kUnsupportedMimeTypesError);
    return;
  }

  ash::LorgnetteScannerManagerFactory::GetForBrowserContext(browser_context_)
      ->GetScannerInfoList(
          extension_id(),
          ash::LorgnetteScannerManager::LocalScannerFilter::
              kIncludeNetworkScanners,
          ash::LorgnetteScannerManager::SecureScannerFilter::
              kIncludeUnsecureScanners,
          base::BindOnce(&SimpleScanRunner::OnSimpleScanListReceived,
                         weak_ptr_factory_.GetWeakPtr(),
                         should_use_virtual_usb_printer));
}

const ExtensionId& SimpleScanRunner::extension_id() const {
  return extension_->id();
}

void SimpleScanRunner::OnSimpleScanListReceived(
    bool force_virtual_usb_printer,
    const std::optional<lorgnette::ListScannersResponse>& response) {
  if (!response.has_value() || response->scanners().empty()) {
    std::move(callback_).Run(std::nullopt, kNoScannersAvailableError);
    return;
  }

  std::vector<const lorgnette::ScannerInfo*> scanners;
  scanners.reserve(response->scanners().size());
  for (const auto& scanner : response->scanners()) {
    scanners.push_back(&scanner);
  }

  // A scanner source needs to be chosen.  Since the choice is unspecified, sort
  // the list with these heuristics and take the first one that can be
  // successfully opened:
  //   1.  If force_virtual_usb_printer is true, always pick the virtual USB
  //       printer.
  //   2.  USB scanners come first, since they are both local and secure.
  //   3.  Secure network scanners come next.
  //   4.  Insecure network scanners come last.
  // Within each grouping, prefer Mopria eSCL to legacy protocols, since the
  // backend is known to work consistently.
  std::ranges::stable_sort(
      scanners, std::less<>{}, [](const lorgnette::ScannerInfo* info) {
        return std::tuple(
            // Virtual USB printer always comes first.
            info->display_name() != kVirtualUSBPrinter,

            // USB devices come first.
            info->connection_type() !=
                lorgnette::ConnectionType::CONNECTION_USB,

            // Secure devices come before insecure.
            !info->secure(),

            // Mopria/eSCL devices come before legacy devices.
            info->protocol_type() != kMopriaProtocolName,

            // Sort by display name if all else is equal.
            info->display_name());
      });

  if (force_virtual_usb_printer &&
      scanners[0]->display_name() != kVirtualUSBPrinter) {
    std::move(callback_).Run(std::nullopt, kVirtualPrinterUnavailableError);
    return;
  }

  // Store the list of IDs in reverse so it can be processed more efficiently in
  // the callbacks.  The rest of the ScannerInfo fields aren't needed.
  scanner_ids_.reserve(scanners.size());
  for (const lorgnette::ScannerInfo* info : scanners) {
    if (force_virtual_usb_printer &&
        info->display_name() != kVirtualUSBPrinter) {
      continue;
    }
    scanner_ids_.push_back(std::move(info->name()));
  }
  std::ranges::reverse(scanner_ids_);

  OpenFirstScanner();
}

void SimpleScanRunner::OpenFirstScanner() {
  if (scanner_ids_.empty()) {
    std::move(callback_).Run(std::nullopt, kNoScannersAvailableError);
    return;
  }

  std::string scanner_id = std::move(scanner_ids_.back());
  scanner_ids_.pop_back();
  document_scan_->OpenScanner(
      extension_id(), std::move(scanner_id),
      base::BindOnce(&SimpleScanRunner::OnOpenScannerResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SimpleScanRunner::OnOpenScannerResponse(
    crosapi::mojom::OpenScannerResponsePtr response) {
  if (response->result != crosapi::mojom::ScannerOperationResult::kSuccess ||
      !response->scanner_handle.has_value()) {
    OpenFirstScanner();
    return;
  }
  scanner_handle_ = std::move(response->scanner_handle.value());

  auto options = crosapi::mojom::StartScanOptions::New();
  options->format = kScannerImageMimeTypePng;

  document_scan_->StartPreparedScan(
      scanner_handle_, std::move(options),
      base::BindOnce(&SimpleScanRunner::OnStartPreparedScanResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SimpleScanRunner::OnStartPreparedScanResponse(
    crosapi::mojom::StartPreparedScanResponsePtr response) {
  if (response->result != crosapi::mojom::ScannerOperationResult::kSuccess ||
      !response->job_handle.has_value()) {
    // Closing the scanner will also return the response to the caller.
    document_scan_->CloseScanner(
        scanner_handle_,
        base::BindOnce(&SimpleScanRunner::OnCloseScannerResponse,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // Scanners normally don't produce bytes right away, so start the read loop
  // after a delay.
  job_handle_ = std::move(response->job_handle.value());
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SimpleScanRunner::ReadScanData,
                     weak_ptr_factory_.GetWeakPtr()),
      kSlowReadInterval);
}

void SimpleScanRunner::ReadScanData() {
  document_scan_->ReadScanData(
      job_handle_, base::BindOnce(&SimpleScanRunner::OnReadScanDataResponse,
                                  weak_ptr_factory_.GetWeakPtr()));
}

void SimpleScanRunner::OnReadScanDataResponse(
    crosapi::mojom::ReadScanDataResponsePtr response) {
  // Success means to keep going.  If data was ready, append it to what we got
  // so far.
  if (response->result == crosapi::mojom::ScannerOperationResult::kSuccess) {
    if (response->data.has_value() && response->data->size() > 0) {
      scan_data_.insert(scan_data_.end(), response->data->begin(),
                        response->data->end());
    }

    // Once the first byte after the image headers is received, poll the scanner
    // more quickly because data usually streams consistently.
    base::TimeDelta delay =
        (scan_data_.size() > 100) ? kReadInterval : kSlowReadInterval;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&SimpleScanRunner::ReadScanData,
                       weak_ptr_factory_.GetWeakPtr()),
        delay);
    return;
  }

  // EOF means no more data is available.  There might be a final data chunk.
  if (response->result == crosapi::mojom::ScannerOperationResult::kEndOfData) {
    if (response->data.has_value() && response->data->size() > 0) {
      scan_data_.insert(scan_data_.end(), response->data->begin(),
                        response->data->end());
    }

    scan_result_ = crosapi::mojom::ScanFailureMode::kNoFailure;
  }

  document_scan_->CloseScanner(
      scanner_handle_, base::BindOnce(&SimpleScanRunner::OnCloseScannerResponse,
                                      weak_ptr_factory_.GetWeakPtr()));
}

void SimpleScanRunner::OnCloseScannerResponse(
    crosapi::mojom::CloseScannerResponsePtr) {
  // Intentionally ignore the response.  The result to return to the caller has
  // already been determined at the end of the read loop.
  OnSimpleScanCompleted(scan_result_);
}

void SimpleScanRunner::OnSimpleScanCompleted(
    crosapi::mojom::ScanFailureMode failure_mode) {
  if (!scan_data_.size() ||
      failure_mode != crosapi::mojom::ScanFailureMode::kNoFailure) {
    std::move(callback_).Run(std::nullopt, kScanImageError);
    return;
  }

  std::string image_base64 = base::Base64Encode(scan_data_);
  api::document_scan::ScanResults scan_results;
  scan_results.data_urls.push_back(kPngImageDataUrlPrefix +
                                   std::move(image_base64));
  scan_results.mime_type = kScannerImageMimeTypePng;

  std::move(callback_).Run(std::move(scan_results), std::nullopt);
}

}  // namespace extensions
