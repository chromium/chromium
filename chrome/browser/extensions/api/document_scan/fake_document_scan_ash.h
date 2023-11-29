// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_FAKE_DOCUMENT_SCAN_ASH_H_
#define CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_FAKE_DOCUMENT_SCAN_ASH_H_

#include <optional>
#include <string>
#include <vector>

#include "chromeos/crosapi/mojom/document_scan.mojom.h"

namespace extensions {

// Fake implementation of DocumentScan that doesn't send D-Bus calls to
// lorgnette.
class FakeDocumentScanAsh : public crosapi::mojom::DocumentScan {
 public:
  FakeDocumentScanAsh();
  FakeDocumentScanAsh(const FakeDocumentScanAsh&) = delete;
  FakeDocumentScanAsh(const FakeDocumentScanAsh&&) = delete;
  ~FakeDocumentScanAsh() override;

  // crosapi::mojom::DocumentScan overrides:
  void GetScannerNames(GetScannerNamesCallback callback) override;
  void ScanFirstPage(const std::string& scanner_name,
                     ScanFirstPageCallback callback) override;
  void GetScannerList(const std::string& client_id,
                      crosapi::mojom::ScannerEnumFilterPtr filter,
                      GetScannerListCallback callback) override;

  void AddScanner(crosapi::mojom::ScannerInfoPtr scanner);
  void SetGetScannerNamesResponse(std::vector<std::string> scanner_names);
  void SetScanResponse(
      const std::optional<std::vector<std::string>>& scan_data);

 private:
  std::vector<std::string> scanner_names_;
  std::optional<std::vector<std::string>> scan_data_;
  std::vector<crosapi::mojom::ScannerInfoPtr> scanners_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_FAKE_DOCUMENT_SCAN_ASH_H_
