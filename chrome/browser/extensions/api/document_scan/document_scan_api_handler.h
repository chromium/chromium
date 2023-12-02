// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_DOCUMENT_SCAN_API_HANDLER_H_
#define CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_DOCUMENT_SCAN_API_HANDLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/api/document_scan/scanner_discovery_runner.h"
#include "chrome/common/extensions/api/document_scan.h"
#include "chromeos/crosapi/mojom/document_scan.mojom-forward.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/common/extension_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/native_widget_types.h"

class PrefRegistrySimple;

namespace content {
class BrowserContext;
}  // namespace content

namespace gfx {
class Image;
}  // namespace gfx

namespace extensions {

class Extension;

// Handles chrome.documentScan API function calls.
class DocumentScanAPIHandler : public BrowserContextKeyedAPI {
 public:
  using SimpleScanCallback = base::OnceCallback<void(
      absl::optional<api::document_scan::ScanResults> scan_results,
      absl::optional<std::string> error)>;
  using GetScannerListCallback =
      base::OnceCallback<void(api::document_scan::GetScannerListResponse)>;
  using OpenScannerCallback =
      base::OnceCallback<void(api::document_scan::OpenScannerResponse)>;
  using CloseScannerCallback =
      base::OnceCallback<void(api::document_scan::CloseScannerResponse)>;

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

  // Registers the documentScan API preference with the |registry|.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Replaces the DocumentScan service with a mock.
  void SetDocumentScanForTesting(crosapi::mojom::DocumentScan* document_scan);

  // Scans one page from the first available scanner on the system and passes
  // the result to `callback`.  `mime_types` is a list of MIME types the caller
  // is willing to receive back as the image format.
  void SimpleScan(const std::vector<std::string>& mime_types,
                  SimpleScanCallback callback);

  // If the user approves, gets a list of available scanners that match
  // `filter`.  Explicit approval is obtained through a Chrome dialog or by
  // adding the extension ID to the list of trusted document scan extensions.
  // The result of the denial or the backend call will be passed to `callback`.
  void GetScannerList(gfx::NativeWindow native_window,
                      scoped_refptr<const Extension> extension,
                      api::document_scan::DeviceFilter filter,
                      GetScannerListCallback callback);

  // Given `scanner_id` previously returned from `GetScannerList`, opens the
  // device for exclusive access.  The result containing a handle and the set of
  // current device options will be passed to `callback`.
  void OpenScanner(scoped_refptr<const Extension> extension,
                   const std::string& scanner_id,
                   OpenScannerCallback callback);

  // Given `scanner_handle` previously returned from `OpenScanner`, closes the
  // handle.  No further operations on this handle can be performed even if the
  // result code does not indicate success.  The result of closing the handle on
  // the backend will be passed to `callback`.
  void CloseScanner(scoped_refptr<const Extension> extension,
                    const std::string& scanner_handle,
                    CloseScannerCallback callback);

 private:
  // Needed for BrowserContextKeyedAPI implementation.
  friend class BrowserContextKeyedAPIFactory<DocumentScanAPIHandler>;

  // Tracks open handles and scanner IDs that have been given out to a
  // particular extension.
  struct ExtensionState {
    ExtensionState();
    ~ExtensionState();

    // Map from unguessable token scanner IDs given out by `getScannerList` back
    // to the internal connection strings needed by the backend.
    std::map<std::string, std::string> scanner_ids;

    // Map from scanner handles that have been returned by `openScanner` back to
    // the original connection string used to open them.
    std::map<std::string, std::string> scanner_handles;
  };

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

  void SendGetScannerListRequest(const api::document_scan::DeviceFilter& filter,
                                 GetScannerListCallback callback);
  void ShowScanDiscoveryDialog(const api::document_scan::DeviceFilter& filter,
                               GetScannerListCallback callback,
                               const gfx::Image& icon);
  void OnScannerListReceived(
      std::unique_ptr<ScannerDiscoveryRunner> discovery_runner,
      GetScannerListCallback callback,
      crosapi::mojom::GetScannerListResponsePtr response);
  void OnOpenScannerResponse(const ExtensionId& extension_id,
                             const std::string& scanner_id,
                             OpenScannerCallback callback,
                             crosapi::mojom::OpenScannerResponsePtr response);
  void OnCloseScannerResponse(CloseScannerCallback callback,
                              crosapi::mojom::CloseScannerResponsePtr response);
  bool IsValidScannerHandle(const ExtensionId& extension_id,
                            const std::string& scanner_handle);

  raw_ptr<content::BrowserContext> browser_context_;
  raw_ptr<crosapi::mojom::DocumentScan> document_scan_;
  std::map<ExtensionId, ExtensionState> extension_state_;
  base::WeakPtrFactory<DocumentScanAPIHandler> weak_ptr_factory_{this};
};

template <>
KeyedService*
BrowserContextKeyedAPIFactory<DocumentScanAPIHandler>::BuildServiceInstanceFor(
    content::BrowserContext* context) const;

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_DOCUMENT_SCAN_API_HANDLER_H_
