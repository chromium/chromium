// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/document_scan/fake_document_scan_ash.h"

#include <utility>

#include "base/check.h"
#include "base/notreached.h"

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

}  // namespace extensions
