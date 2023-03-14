// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_DOCUMENT_SCAN_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_DOCUMENT_SCAN_API_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/common/extensions/api/document_scan.h"
#include "chromeos/crosapi/mojom/document_scan.mojom-forward.h"
#include "extensions/browser/extension_function.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace crosapi::mojom {
class DocumentScan;
}  // namespace crosapi::mojom

namespace extensions {

namespace api {

class DocumentScanScanFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("documentScan.scan", DOCUMENT_SCAN_SCAN)
  DocumentScanScanFunction();
  DocumentScanScanFunction(const DocumentScanScanFunction&) = delete;
  DocumentScanScanFunction& operator=(const DocumentScanScanFunction&) = delete;

  void SetMojoInterfaceForTesting(crosapi::mojom::DocumentScan* document_scan);

 protected:
  ~DocumentScanScanFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void MaybeInitializeMojoInterface();
  void OnNamesReceived(const std::vector<std::string>& scanner_names);
  void OnScanCompleted(crosapi::mojom::ScanFailureMode failure_mode,
                       const absl::optional<std::string>& scan_data);

  absl::optional<document_scan::Scan::Params> params_;

  // Used to transmit mojo interface method calls to ash chrome.
  // Null if the interface is unavailable.
  // The pointer is constant - if Ash crashes and the mojo connection is lost,
  // Lacros will automatically be restarted.
  raw_ptr<crosapi::mojom::DocumentScan> document_scan_ = nullptr;
};

}  // namespace api

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_DOCUMENT_SCAN_API_H_
