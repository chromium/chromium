// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_START_SCAN_RUNNER_H_
#define CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_START_SCAN_RUNNER_H_

#include <string>

#include "base/auto_reset.h"
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

// Handles API requests from chrome.documentScan.startScan including prompting
// for permission and collecting the returned results.
class StartScanRunner {
 public:
  using StartScanCallback =
      base::OnceCallback<void(crosapi::mojom::StartPreparedScanResponsePtr)>;

  StartScanRunner(gfx::NativeWindow native_window,
                  content::BrowserContext* browser_context,
                  scoped_refptr<const Extension> extension,
                  crosapi::mojom::DocumentScan* document_scan);

  ~StartScanRunner();

  static base::AutoReset<std::optional<bool>>
  SetStartScanConfirmationResultForTesting(bool val);

  // Does any work needed to send the start scan request.  If `is_approved` is
  // true, no dialog prompt is displayed and the request is sent right away.
  // Otherwise, the dialog is displayed and the request is sent depending on the
  // user's response to the dialog.  `scanner_name` is used in the dialog so the
  // user knows which scanner the request is for.  `scanner_handle`, `options`
  // and `callback` are used in the start scan request.
  void Start(bool is_approved,
             const std::string& scanner_name,
             const std::string& scanner_handle,
             crosapi::mojom::StartScanOptionsPtr options,
             StartScanCallback callback);

  const ExtensionId& extension_id() const;

  bool approved() const { return approved_; }

 private:
  // Shows the dialog.  `scanner_name` is the name of the scanner that is
  // requesting to start a scan.
  void ShowStartScanDialog(const std::string& scanner_name,
                           const gfx::Image& icon);

  // Used `document_scan_` to actually send the start scan request.
  void SendStartScanRequest();

  // When the dialog is closed, depending on if the user approved the request or
  // not, the scan is requested or `callback_` is called with a denied result.
  void OnConfirmationDialogClosed(bool approved);

  // Processes the result from requesting the scan and runs `callback_`.
  void OnStartScanResponse(
      crosapi::mojom::StartPreparedScanResponsePtr response);

  gfx::NativeWindow native_window_;
  const raw_ptr<content::BrowserContext> browser_context_;

  // Tracks whether `native_window_` got destroyed.
  std::unique_ptr<views::NativeWindowTracker> native_window_tracker_;

  scoped_refptr<const Extension> extension_;

  const raw_ptr<crosapi::mojom::DocumentScan> document_scan_;

  // Keep track of whether the user approved the request or not.
  bool approved_;

  // Parameters for the in-progress call.
  std::string scanner_handle_;
  crosapi::mojom::StartScanOptionsPtr options_;
  StartScanCallback callback_;

  base::WeakPtrFactory<StartScanRunner> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_START_SCAN_RUNNER_H_
