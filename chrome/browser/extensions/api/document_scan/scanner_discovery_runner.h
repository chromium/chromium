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
#include "chrome/common/extensions/api/document_scan.h"
#include "chromeos/ash/components/dbus/lorgnette/lorgnette_service.pb.h"
#include "extensions/common/extension_id.h"
#include "ui/gfx/native_ui_types.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace gfx {
class Image;
}  // namespace gfx

namespace ui {
class NativeWindowTracker;
}  // namespace ui

namespace extensions {

class Extension;

// Handles API requests from chrome.documentScan.getScannerList including
// prompting for permission and collecting the returned results.
class ScannerDiscoveryRunner {
 public:
  using GetScannerListCallback =
      base::OnceCallback<void(api::document_scan::GetScannerListResponse)>;

  ScannerDiscoveryRunner(gfx::NativeWindow native_window,
                         content::BrowserContext* browser_context,
                         scoped_refptr<const Extension> extension);
  ~ScannerDiscoveryRunner();

  static void SetDiscoveryConfirmationResultForTesting(bool result);

  // Prompts the user for confirmation if needed, then sends lorgnette a
  // `GetScannerList` request with `filter` and sends the response to
  // `callback`. If `approved` is true, this request is already approved and any
  // confirmation dialogs are skipped.
  void Start(bool approved,
             api::document_scan::DeviceFilter filter,
             GetScannerListCallback callback);

  const ExtensionId& extension_id() const;

 private:
  void ShowScanDiscoveryDialog(const gfx::Image& icon);
  void SendGetScannerListRequest();
  void OnConfirmationDialogClosed(bool approved);
  void OnScannerListReceived(
      const std::optional<lorgnette::ListScannersResponse>& response);

  gfx::NativeWindow native_window_;
  const raw_ptr<content::BrowserContext> browser_context_;

  // Tracks whether |native_window_| got destroyed.
  std::unique_ptr<ui::NativeWindowTracker> native_window_tracker_;

  scoped_refptr<const Extension> extension_;

  // Parameters for the in-progress call.
  std::optional<api::document_scan::DeviceFilter> filter_;
  GetScannerListCallback callback_;

  base::WeakPtrFactory<ScannerDiscoveryRunner> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_SCANNER_DISCOVERY_RUNNER_H_
