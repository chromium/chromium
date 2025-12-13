// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_SIMPLE_SCAN_RUNNER_H_
#define CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_SIMPLE_SCAN_RUNNER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/extensions/api/document_scan.h"
#include "chromeos/ash/components/dbus/lorgnette/lorgnette_service.pb.h"
#include "chromeos/crosapi/mojom/document_scan.mojom.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

class Extension;

// Handles API requests from chrome.documentScan.scan including selecting a
// scanner and collecting the returned results.
class SimpleScanRunner {
 public:
  using SimpleScanCallback = base::OnceCallback<void(
      std::optional<api::document_scan::ScanResults> scan_results,
      std::optional<std::string> error)>;
  using SimpleScanRunnerCallback =
      base::OnceCallback<void(crosapi::mojom::ScanFailureMode,
                              const std::optional<std::string>&)>;

  SimpleScanRunner(content::BrowserContext* browser_context,
                   scoped_refptr<const Extension> extension,
                   crosapi::mojom::DocumentScan* document_scan);

  ~SimpleScanRunner();

  void Start(std::vector<std::string> mime_types, SimpleScanCallback callback);

  const ExtensionId& extension_id() const;

 private:
  void OnSimpleScanListReceived(
      bool force_virtual_usb_printer,
      const std::optional<lorgnette::ListScannersResponse>& response);
  void OnOpenScannerResponse(crosapi::mojom::OpenScannerResponsePtr response);
  void OnStartPreparedScanResponse(
      crosapi::mojom::StartPreparedScanResponsePtr response);
  void OnReadScanDataResponse(crosapi::mojom::ReadScanDataResponsePtr response);
  void OnCloseScannerResponse(crosapi::mojom::CloseScannerResponsePtr response);
  void OnSimpleScanCompleted(crosapi::mojom::ScanFailureMode failure_mode);

  void OpenFirstScanner();
  void ReadScanData();

  const raw_ptr<content::BrowserContext> browser_context_;
  scoped_refptr<const Extension> extension_;
  const raw_ptr<crosapi::mojom::DocumentScan> document_scan_;

  // List of potential scanners to open.
  std::vector<std::string> scanner_ids_;

  // Parameters for the in-progress call.
  std::vector<std::string> mime_types_;
  SimpleScanCallback callback_;

  // State for the in-progress scan.
  std::string scanner_handle_;
  std::string job_handle_;
  std::vector<uint8_t> scan_data_;
  crosapi::mojom::ScanFailureMode scan_result_;

  base::WeakPtrFactory<SimpleScanRunner> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_SIMPLE_SCAN_RUNNER_H_
