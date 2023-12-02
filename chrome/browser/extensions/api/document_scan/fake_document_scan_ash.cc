// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/document_scan/fake_document_scan_ash.h"

#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/notreached.h"
#include "chrome/browser/extensions/api/document_scan/document_scan_test_utils.h"

namespace extensions {

FakeDocumentScanAsh::FakeDocumentScanAsh() = default;
FakeDocumentScanAsh::~FakeDocumentScanAsh() = default;

void FakeDocumentScanAsh::GetScannerNames(GetScannerNamesCallback callback) {
  std::move(callback).Run(scanner_names_);
}

void FakeDocumentScanAsh::ScanFirstPage(const std::string& scanner_name,
                                        ScanFirstPageCallback callback) {
  if (scan_data_.has_value()) {
    std::move(callback).Run(crosapi::mojom::ScanFailureMode::kNoFailure,
                            scan_data_.value()[0]);
  } else {
    std::move(callback).Run(crosapi::mojom::ScanFailureMode::kDeviceBusy,
                            absl::nullopt);
  }
}

void FakeDocumentScanAsh::GetScannerList(
    const std::string& client_id,
    crosapi::mojom::ScannerEnumFilterPtr filter,
    GetScannerListCallback callback) {
  auto response = crosapi::mojom::GetScannerListResponse::New();
  response->result = crosapi::mojom::ScannerOperationResult::kSuccess;
  for (const auto& scanner : scanners_) {
    response->scanners.emplace_back(scanner.Clone());

    // Since this scanner will be listed, also create an entry that allows
    // callers to open it.
    auto open_response = crosapi::mojom::OpenScannerResponse::New();
    open_response->result = crosapi::mojom::ScannerOperationResult::kSuccess;
    open_response->scanner_id = scanner->id;
    open_response->scanner_handle = scanner->id + "-handle-" + client_id;
    open_response->options.emplace();
    open_response->options.value()["option1"] =
        CreateTestScannerOption("option1", 5);
    open_responses_[scanner->id] = std::move(open_response);
  }
  std::move(callback).Run(std::move(response));
}

void FakeDocumentScanAsh::OpenScanner(const std::string& client_id,
                                      const std::string& scanner_id,
                                      OpenScannerCallback callback) {
  // If a response for scanner_id hasn't been set, this is the equivalent
  // of trying to open a device that has been unplugged or disappeared off the
  // network.
  if (!base::Contains(open_responses_, scanner_id)) {
    auto response = crosapi::mojom::OpenScannerResponse::New();
    response->scanner_id = scanner_id;
    response->result = crosapi::mojom::ScannerOperationResult::kDeviceMissing;
    std::move(callback).Run(std::move(response));
    return;
  }
  // If the scanner is already open by a different client, the real backend will
  // report DEVICE_BUSY to any other clients trying to open it.  Do the same
  // here.
  for (const auto& [handle, original] : open_scanners_) {
    if (original.connection_string == scanner_id &&
        original.client_id != client_id) {
      auto response = crosapi::mojom::OpenScannerResponse::New();
      response->scanner_id = scanner_id;
      response->result = crosapi::mojom::ScannerOperationResult::kDeviceBusy;
      std::move(callback).Run(std::move(response));
      return;
    }
  }

  crosapi::mojom::OpenScannerResponsePtr response =
      open_responses_[scanner_id].Clone();
  open_scanners_[response->scanner_handle.value_or(scanner_id + "-handle")] =
      OpenScannerState{
          .client_id = client_id,
          .connection_string = scanner_id,
      };
  std::move(callback).Run(std::move(response));
}

void FakeDocumentScanAsh::CloseScanner(const std::string& scanner_handle,
                                       CloseScannerCallback callback) {
  auto response = crosapi::mojom::CloseScannerResponse::New();
  response->scanner_handle = scanner_handle;
  if (base::Contains(open_scanners_, scanner_handle)) {
    response->result = crosapi::mojom::ScannerOperationResult::kSuccess;
  } else {
    response->result = crosapi::mojom::ScannerOperationResult::kInvalid;
  }
  open_scanners_.erase(scanner_handle);
  std::move(callback).Run(std::move(response));
}

void FakeDocumentScanAsh::StartPreparedScan(
    const std::string& scanner_handle,
    crosapi::mojom::StartScanOptionsPtr options,
    StartPreparedScanCallback callback) {
  // TODO(b/299489635): Implement this when adding the extension handler.
  NOTIMPLEMENTED();
}

void FakeDocumentScanAsh::ReadScanData(const std::string& job_handle,
                                       ReadScanDataCallback callback) {
  // TODO(b/299489635): Implement this when adding the extension handler.
  NOTIMPLEMENTED();
}

void FakeDocumentScanAsh::SetGetScannerNamesResponse(
    std::vector<std::string> scanner_names) {
  scanner_names_ = std::move(scanner_names);
}

void FakeDocumentScanAsh::SetScanResponse(
    const absl::optional<std::vector<std::string>>& scan_data) {
  if (scan_data.has_value()) {
    DCHECK(!scan_data.value().empty());
  }
  scan_data_ = scan_data;
}

void FakeDocumentScanAsh::AddScanner(crosapi::mojom::ScannerInfoPtr scanner) {
  scanners_.emplace_back(std::move(scanner));
}

void FakeDocumentScanAsh::SetOpenScannerResponse(
    const std::string& connection_string,
    crosapi::mojom::OpenScannerResponsePtr response) {
  open_responses_[connection_string] = std::move(response);
}

}  // namespace extensions
