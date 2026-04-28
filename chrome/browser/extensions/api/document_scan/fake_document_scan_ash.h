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

  void CloseScanner(const std::string& scanner_handle);

  void SetOpenScannerResponse(const std::string& connection_string,
                              crosapi::mojom::OpenScannerResponsePtr response);

 private:
  struct OpenScannerState {
    OpenScannerState();
    OpenScannerState(const std::string& client_id,
                     const std::string& connection_string);
    ~OpenScannerState();

    std::string client_id;
    std::string connection_string;
  };

  // Map from connection strings to the OpenScannerResponsePtr that should be
  // returned.
  std::map<std::string, crosapi::mojom::OpenScannerResponsePtr> open_responses_;

  // Map from scanner handles to the original client and scanner used to create
  // the handle.
  std::map<std::string, OpenScannerState> open_scanners_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_FAKE_DOCUMENT_SCAN_ASH_H_
