// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_SCANNER_DISCOVERY_RUNNER_H_
#define CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_SCANNER_DISCOVERY_RUNNER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chromeos/crosapi/mojom/document_scan.mojom.h"
#include "extensions/common/extension_id.h"
#include "ui/gfx/native_widget_types.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace gfx {
class Image;
}  // namespace gfx

namespace views {
class NativeWindowTracker;
}  // namespace views

namespace extensions {

class Extension;

// Handles API requests from chrome.documentScan.getScannerList including
// prompting for permission and collecting the returned results.
class ScannerDiscoveryRunner {
 public:
  using GetScannerListCallback =
      base::OnceCallback<void(crosapi::mojom::GetScannerListResponsePtr)>;

  ScannerDiscoveryRunner(gfx::NativeWindow native_window,
                         content::BrowserContext* browser_context,
                         scoped_refptr<const Extension> extension,
                         crosapi::mojom::DocumentScan* document_scan);

  ~ScannerDiscoveryRunner();

  static void SetDiscoveryConfirmationResultForTesting(bool result);

  // Prompts the user for confirmation if needed, then sends lorgnette a
  // `GetScannerList` request with `filter` and sends the response to
  // `callback`. If `approved` is true, this request is already approved and any
  // confirmation dialogs are skipped.
  void Start(bool approved,
             crosapi::mojom::ScannerEnumFilterPtr filter,
             GetScannerListCallback callback);

  const ExtensionId& extension_id() const;

 private:
  void ShowScanDiscoveryDialog(const gfx::Image& icon);
  void SendGetScannerListRequest();
  void OnConfirmationDialogClosed(bool approved);
  void OnScannerListReceived(
      crosapi::mojom::GetScannerListResponsePtr response);

  gfx::NativeWindow native_window_;
  const raw_ptr<content::BrowserContext> browser_context_;

  // Tracks whether |native_window_| got destroyed.
  std::unique_ptr<views::NativeWindowTracker> native_window_tracker_;

  scoped_refptr<const Extension> extension_;

  const raw_ptr<crosapi::mojom::DocumentScan> document_scan_;

  // Parameters for the in-progress call.
  crosapi::mojom::ScannerEnumFilterPtr filter_;
  GetScannerListCallback callback_;

  base::WeakPtrFactory<ScannerDiscoveryRunner> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_SCANNER_DISCOVERY_RUNNER_H_
