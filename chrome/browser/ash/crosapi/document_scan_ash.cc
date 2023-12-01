// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/document_scan_ash.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/crosapi/document_scan_ash_type_converters.h"
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
struct ScanResult {
 public:
  ScanResult() = default;
  ScanResult(const ScanResult&) = delete;
  ScanResult& operator=(const ScanResult&) = delete;
  ~ScanResult() = default;

  base::WeakPtr<ScanResult> AsWeakPtr() {
    return weak_ptr_factory.GetWeakPtr();
  }

  absl::optional<std::string> data;

 private:
  base::WeakPtrFactory<ScanResult> weak_ptr_factory{this};
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

void GetScannerListAdapter(
    DocumentScanAsh::GetScannerListCallback callback,
    const absl::optional<lorgnette::ListScannersResponse>& response_in) {
  if (!response_in) {
    auto response_out = mojom::GetScannerListResponse::New();
    response_out->result = mojom::ScannerOperationResult::kInternalError;
    std::move(callback).Run(std::move(response_out));
    return;
  }
  std::move(callback).Run(
      mojom::GetScannerListResponse::From(response_in.value()));
}

void OpenScannerAdapter(
    const std::string& scanner_id,
    DocumentScanAsh::OpenScannerCallback callback,
    const absl::optional<lorgnette::OpenScannerResponse>& response_in) {
  if (!response_in) {
    auto response_out = mojom::OpenScannerResponse::New();
    response_out->scanner_id = scanner_id;
    response_out->result = mojom::ScannerOperationResult::kInternalError;
    std::move(callback).Run(std::move(response_out));
    return;
  }
  std::move(callback).Run(
      mojom::OpenScannerResponse::From(response_in.value()));
}

void CloseScannerAdapter(
    const std::string& scanner_handle,
    DocumentScanAsh::CloseScannerCallback callback,
    const absl::optional<lorgnette::CloseScannerResponse>& response_in) {
  if (!response_in) {
    auto response_out = mojom::CloseScannerResponse::New();
    response_out->scanner_handle = scanner_handle;
    response_out->result = mojom::ScannerOperationResult::kInternalError;
    std::move(callback).Run(std::move(response_out));
    return;
  }
  std::move(callback).Run(
      mojom::CloseScannerResponse::From(response_in.value()));
}

void StartPreparedScanAdapter(
    const std::string& scanner_handle,
    DocumentScanAsh::StartPreparedScanCallback callback,
    const absl::optional<lorgnette::StartPreparedScanResponse>& response_in) {
  if (!response_in) {
    auto response = mojom::StartPreparedScanResponse::New();
    response->result = mojom::ScannerOperationResult::kInternalError;
    response->scanner_handle = scanner_handle;
    std::move(callback).Run(std::move(response));
    return;
  }
  std::move(callback).Run(
      mojom::StartPreparedScanResponse::From(response_in.value()));
}

void ReadScanDataAdapter(
    const std::string& job_handle,
    DocumentScanAsh::ReadScanDataCallback callback,
    const absl::optional<lorgnette::ReadScanDataResponse>& response_in) {
  if (!response_in) {
    auto response = mojom::ReadScanDataResponse::New();
    response->result = mojom::ScannerOperationResult::kInternalError;
    response->job_handle = job_handle;
    std::move(callback).Run(std::move(response));
    return;
  }
  std::move(callback).Run(
      mojom::ReadScanDataResponse::From(response_in.value()));
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

void DocumentScanAsh::GetScannerList(const std::string& client_id,
                                     mojom::ScannerEnumFilterPtr filter,
                                     GetScannerListCallback callback) {
  using LocalScannerFilter = ash::LorgnetteScannerManager::LocalScannerFilter;
  using SecureScannerFilter = ash::LorgnetteScannerManager::SecureScannerFilter;

  if (!ash::features::IsAdvancedDocumentScanAPIEnabled()) {
    auto response = crosapi::mojom::GetScannerListResponse::New();
    response->result = crosapi::mojom::ScannerOperationResult::kUnsupported;
    std::move(callback).Run(std::move(response));
    return;
  }

  ash::LorgnetteScannerManagerFactory::GetForBrowserContext(GetProfile())
      ->GetScannerInfoList(
          client_id,
          filter->local ? LocalScannerFilter::kLocalScannersOnly
                        : LocalScannerFilter::kIncludeNetworkScanners,
          filter->secure ? SecureScannerFilter::kSecureScannersOnly
                         : SecureScannerFilter::kIncludeUnsecureScanners,
          base::BindOnce(&GetScannerListAdapter, std::move(callback)));
}

void DocumentScanAsh::OpenScanner(const std::string& client_id,
                                  const std::string& scanner_id,
                                  OpenScannerCallback callback) {
  if (!ash::features::IsAdvancedDocumentScanAPIEnabled()) {
    auto response = crosapi::mojom::OpenScannerResponse::New();
    response->scanner_id = scanner_id;
    response->result = crosapi::mojom::ScannerOperationResult::kUnsupported;
    std::move(callback).Run(std::move(response));
    return;
  }

  lorgnette::OpenScannerRequest request;
  request.mutable_scanner_id()->set_connection_string(scanner_id);
  request.set_client_id(client_id);
  ash::LorgnetteScannerManagerFactory::GetForBrowserContext(GetProfile())
      ->OpenScanner(
          std::move(request),
          base::BindOnce(&OpenScannerAdapter, scanner_id, std::move(callback)));
}

void DocumentScanAsh::CloseScanner(const std::string& scanner_handle,
                                   CloseScannerCallback callback) {
  if (!ash::features::IsAdvancedDocumentScanAPIEnabled()) {
    auto response = crosapi::mojom::CloseScannerResponse::New();
    response->scanner_handle = scanner_handle;
    response->result = crosapi::mojom::ScannerOperationResult::kUnsupported;
    std::move(callback).Run(std::move(response));
    return;
  }

  lorgnette::CloseScannerRequest request;
  request.mutable_scanner()->set_token(scanner_handle);
  ash::LorgnetteScannerManagerFactory::GetForBrowserContext(GetProfile())
      ->CloseScanner(std::move(request),
                     base::BindOnce(&CloseScannerAdapter, scanner_handle,
                                    std::move(callback)));
}

void DocumentScanAsh::StartPreparedScan(const std::string& scanner_handle,
                                        mojom::StartScanOptionsPtr options,
                                        StartPreparedScanCallback callback) {
  if (!ash::features::IsAdvancedDocumentScanAPIEnabled()) {
    auto response = mojom::StartPreparedScanResponse::New();
    response->result = mojom::ScannerOperationResult::kUnsupported;
    response->scanner_handle = scanner_handle;
    std::move(callback).Run(std::move(response));
    return;
  }

  lorgnette::StartPreparedScanRequest request;
  request.mutable_scanner()->set_token(scanner_handle);
  request.set_image_format(options->format);

  ash::LorgnetteScannerManagerFactory::GetForBrowserContext(GetProfile())
      ->StartPreparedScan(
          request, base::BindOnce(&StartPreparedScanAdapter, scanner_handle,
                                  std::move(callback)));
}

void DocumentScanAsh::ReadScanData(const std::string& job_handle,
                                   ReadScanDataCallback callback) {
  if (!ash::features::IsAdvancedDocumentScanAPIEnabled()) {
    auto response = mojom::ReadScanDataResponse::New();
    response->result = mojom::ScannerOperationResult::kUnsupported;
    response->job_handle = job_handle;
    std::move(callback).Run(std::move(response));
    return;
  }

  lorgnette::ReadScanDataRequest request;
  request.mutable_job_handle()->set_token(job_handle);

  ash::LorgnetteScannerManagerFactory::GetForBrowserContext(GetProfile())
      ->ReadScanData(request, base::BindOnce(&ReadScanDataAdapter, job_handle,
                                             std::move(callback)));
}

}  // namespace crosapi
