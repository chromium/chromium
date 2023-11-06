// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_DOCUMENT_SCAN_API_HANDLER_H_
#define CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_DOCUMENT_SCAN_API_HANDLER_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/common/extensions/api/document_scan.h"
#include "chromeos/crosapi/mojom/document_scan.mojom-forward.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace extensions {

// Handles chrome.documentScan API function calls.
class DocumentScanAPIHandler : public BrowserContextKeyedAPI {
 public:
  using SimpleScanCallback = base::OnceCallback<void(
      absl::optional<api::document_scan::ScanResults> scan_results,
      absl::optional<std::string> error)>;

  static std::unique_ptr<DocumentScanAPIHandler> CreateForTesting(
      content::BrowserContext* browser_context,
      crosapi::mojom::DocumentScan* document_scan);

  explicit DocumentScanAPIHandler(content::BrowserContext* browser_context);
  DocumentScanAPIHandler(const DocumentScanAPIHandler&) = delete;
  DocumentScanAPIHandler& operator=(const DocumentScanAPIHandler&) = delete;
  ~DocumentScanAPIHandler() override;

  // BrowserContextKeyedAPI:
  static BrowserContextKeyedAPIFactory<DocumentScanAPIHandler>*
  GetFactoryInstance();

  // Returns the current instance for `browser_context`.
  static DocumentScanAPIHandler* Get(content::BrowserContext* browser_context);

  // Scans one page from the first available scanner on the system and passes
  // the result to `callback`.  `mime_types` is a list of MIME types the caller
  // is willing to receive back as the image format.
  void SimpleScan(const std::vector<std::string>& mime_types,
                  SimpleScanCallback callback);

 private:
  // Needed for BrowserContextKeyedAPI implementation.
  friend class BrowserContextKeyedAPIFactory<DocumentScanAPIHandler>;

  // BrowserContextKeyedAPI:
  static const char* service_name() { return "DocumentScanAPIHandler"; }
  static const bool kServiceIsCreatedWithBrowserContext = false;
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceHasOwnInstanceInIncognito = true;

  // Used by CreateForTesting:
  DocumentScanAPIHandler(content::BrowserContext* browser_context,
                         crosapi::mojom::DocumentScan* document_scan);

  void OnSimpleScanNamesReceived(bool force_virtual_usb_printer,
                                 SimpleScanCallback callback,
                                 const std::vector<std::string>& scanner_names);
  void OnSimpleScanCompleted(SimpleScanCallback callback,
                             crosapi::mojom::ScanFailureMode failure_mode,
                             const absl::optional<std::string>& scan_data);

  raw_ptr<crosapi::mojom::DocumentScan> document_scan_;
  base::WeakPtrFactory<DocumentScanAPIHandler> weak_ptr_factory_{this};
};

template <>
KeyedService*
BrowserContextKeyedAPIFactory<DocumentScanAPIHandler>::BuildServiceInstanceFor(
    content::BrowserContext* context) const;

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_DOCUMENT_SCAN_API_HANDLER_H_
