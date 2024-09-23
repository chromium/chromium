// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_FAKE_DOCUMENT_SCAN_ASH_H_
#define CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_FAKE_DOCUMENT_SCAN_ASH_H_

#include <map>
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
  void OpenScanner(const std::string& client_id,
                   const std::string& scanner_id,
                   OpenScannerCallback callback) override;
  void GetOptionGroups(const std::string& scanner_handle,
                       GetOptionGroupsCallback callback) override;
  void CloseScanner(const std::string& scanner_handle,
                    CloseScannerCallback callback) override;
  void StartPreparedScan(const std::string& scanner_handle,
                         crosapi::mojom::StartScanOptionsPtr options,
                         StartPreparedScanCallback callback) override;
  void ReadScanData(const std::string& job_handle,
                    ReadScanDataCallback callback) override;
  void SetOptions(const std::string& scanner_handle,
                  std::vector<crosapi::mojom::OptionSettingPtr> options,
                  SetOptionsCallback callback) override;
  void CancelScan(const std::string& job_handle,
                  CancelScanCallback callback) override;

  void AddScanner(crosapi::mojom::ScannerInfoPtr scanner);
  void SetGetScannerNamesResponse(std::vector<std::string> scanner_names);
  void SetScanResponse(
      const std::optional<std::vector<std::string>>& scan_data);
  void SetOpenScannerResponse(const std::string& connection_string,
                              crosapi::mojom::OpenScannerResponsePtr response);
  void SetSmallestMaxReadSize(size_t max_size);

 private:
  struct OpenScannerState {
    OpenScannerState();
    OpenScannerState(const std::string& client_id,
                     const std::string& connection_string);
    ~OpenScannerState();

    std::string client_id;
    std::string connection_string;
    std::optional<std::string> job_handle;
  };

  size_t handle_count_ = 0;  // How many times a handle has been issued.
  std::vector<std::string> scanner_names_;
  std::optional<std::vector<std::string>> scan_data_;
  std::vector<crosapi::mojom::ScannerInfoPtr> scanners_;
  size_t smallest_max_read_ = 0;

  // Map from connection strings to the OpenScannerResponsePtr that should be
  // returned.
  std::map<std::string, crosapi::mojom::OpenScannerResponsePtr> open_responses_;

  // Map from scanner handles to the original client and scanner used to create
  // the handle.
  std::map<std::string, OpenScannerState> open_scanners_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_FAKE_DOCUMENT_SCAN_ASH_H_
