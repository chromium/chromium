// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/document_scan_ash.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/scanning/lorgnette_scanner_manager.h"
#include "chrome/browser/ash/scanning/lorgnette_scanner_manager_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/dbus/lorgnette/lorgnette_service.pb.h"
#include "components/user_manager/user_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace crosapi {

namespace {

Profile* GetProfile() {
  if (!user_manager::UserManager::IsInitialized() ||
      !user_manager::UserManager::Get()->IsUserLoggedIn()) {
    return nullptr;
  }
  return ProfileManager::GetPrimaryUserProfile();
}

void GetScannerNamesAdapter(DocumentScanAsh::GetScannerNamesCallback callback,
                            std::vector<std::string> scanner_names) {
  std::move(callback).Run(scanner_names);
}

// Supports the static_cast() in ProtobufResultToMojoResult() below.
static_assert(lorgnette::SCAN_FAILURE_MODE_NO_FAILURE ==
              static_cast<int>(mojom::ScanFailureMode::kNoFailure));
static_assert(lorgnette::SCAN_FAILURE_MODE_UNKNOWN ==
              static_cast<int>(mojom::ScanFailureMode::kUnknown));
static_assert(lorgnette::SCAN_FAILURE_MODE_DEVICE_BUSY ==
              static_cast<int>(mojom::ScanFailureMode::kDeviceBusy));
static_assert(lorgnette::SCAN_FAILURE_MODE_ADF_JAMMED ==
              static_cast<int>(mojom::ScanFailureMode::kAdfJammed));
static_assert(lorgnette::SCAN_FAILURE_MODE_ADF_EMPTY ==
              static_cast<int>(mojom::ScanFailureMode::kAdfEmpty));
static_assert(lorgnette::SCAN_FAILURE_MODE_FLATBED_OPEN ==
              static_cast<int>(mojom::ScanFailureMode::kFlatbedOpen));
static_assert(lorgnette::SCAN_FAILURE_MODE_IO_ERROR ==
              static_cast<int>(mojom::ScanFailureMode::kIoError));

mojom::ScanFailureMode ProtobufResultToMojoResult(
    lorgnette::ScanFailureMode failure_mode) {
  // The static_assert() checks above make this cast safe.
  return static_cast<mojom::ScanFailureMode>(failure_mode);
}

// Wrapper around `data` that allows this to be a WeakPtr.
struct ScanResult : public base::SupportsWeakPtr<ScanResult> {
 public:
  ScanResult() = default;
  ScanResult(const ScanResult&) = delete;
  ScanResult& operator=(const ScanResult&) = delete;
  ~ScanResult() = default;

  absl::optional<std::string> data;
};

void OnPageReceived(base::WeakPtr<ScanResult> scan_result,
                    std::string scanned_image,
                    uint32_t /*page_number*/) {
  if (!scan_result)
    return;

  // Take only the first page of the scan.
  if (scan_result->data.has_value())
    return;

  scan_result->data = std::move(scanned_image);
}

// As a standalone function, this will always run `callback`. If this was a
// DocumentScanAsh method instead, then that method bound to a
// base::WeakPtr<DocumentScanAsh> may sometimes not run `callback`.
void OnScanCompleted(DocumentScanAsh::ScanFirstPageCallback callback,
                     std::unique_ptr<ScanResult> scan_result,
                     lorgnette::ScanFailureMode failure_mode) {
  std::move(callback).Run(ProtobufResultToMojoResult(failure_mode),
                          std::move(scan_result->data));
}

}  // namespace

DocumentScanAsh::DocumentScanAsh() = default;

DocumentScanAsh::~DocumentScanAsh() = default;

void DocumentScanAsh::BindReceiver(
    mojo::PendingReceiver<mojom::DocumentScan> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void DocumentScanAsh::GetScannerNames(GetScannerNamesCallback callback) {
  ash::LorgnetteScannerManagerFactory::GetForBrowserContext(GetProfile())
      ->GetScannerNames(
          base::BindOnce(GetScannerNamesAdapter, std::move(callback)));
}

void DocumentScanAsh::ScanFirstPage(const std::string& scanner_name,
                                    ScanFirstPageCallback callback) {
  lorgnette::ScanSettings settings;
  settings.set_color_mode(lorgnette::MODE_COLOR);  // Hardcoded for now.

  auto scan_result = std::make_unique<ScanResult>();
  auto scan_result_weak_ptr = scan_result->AsWeakPtr();
  ash::LorgnetteScannerManagerFactory::GetForBrowserContext(GetProfile())
      ->Scan(scanner_name, settings, base::NullCallback(),
             base::BindRepeating(&OnPageReceived, scan_result_weak_ptr),
             base::BindOnce(&OnScanCompleted, std::move(callback),
                            std::move(scan_result)));
}

}  // namespace crosapi
