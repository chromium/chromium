// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SHIMLESS_RMA_BACKEND_SHIMLESS_RMA_DELEGATE_H_
#define ASH_WEBUI_SHIMLESS_RMA_BACKEND_SHIMLESS_RMA_DELEGATE_H_

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/web_contents.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
}

namespace ash::shimless_rma {

// A delegate which exposes browser functionality from //chrome to the Shimless
// RMA UI.
class ShimlessRmaDelegate {
 public:
  virtual ~ShimlessRmaDelegate() = default;

  // Exits the current RMA session then restarts the Chrome session without RMA.
  virtual void ExitRmaThenRestartChrome() = 0;

  // Starts the post-boot diagnostics app.
  virtual void ShowDiagnosticsDialog() = 0;

  // Sets the AccessibilityManager profile to the active profile to enable
  // accessibility features.
  virtual void RefreshAccessibilityManagerProfile() = 0;

  // Returns a string encoded QR code image of `url`.
  virtual void GenerateQrCode(
      const std::string& url,
      base::OnceCallback<void(const std::string& qr_code_image)> callback) = 0;

  // Prepare the browser context to show the 3p diagnostics app. A 3p
  // diagnostics app consists of a ChromeOS system extension and a isolated web
  // app (IWA).
  // This configures the browser context, installs the extension (crx file) and
  // the IWA (swbn file) from the specific path. The callback returns a result
  // object or an error message. This method is also responsible to check if the
  // extension and the IWA are allowed by the system.
  struct PrepareDiagnosticsAppBrowserContextResult {
    PrepareDiagnosticsAppBrowserContextResult(
        const raw_ptr<content::BrowserContext>& context,
        const std::string& extension_id,
        const web_package::SignedWebBundleId& iwa_id,
        const std::string& name,
        const std::optional<std::string>& permission_message);
    PrepareDiagnosticsAppBrowserContextResult(
        const PrepareDiagnosticsAppBrowserContextResult&);
    PrepareDiagnosticsAppBrowserContextResult& operator=(
        const PrepareDiagnosticsAppBrowserContextResult&);
    ~PrepareDiagnosticsAppBrowserContextResult();

    raw_ptr<content::BrowserContext> context;
    std::string extension_id;
    web_package::SignedWebBundleId iwa_id;
    std::string name;
    // Permission message to show. This is a multi-line string. Is omitted if no
    // permission is required.
    std::optional<std::string> permission_message;
  };
  using PrepareDiagnosticsAppBrowserContextCallback = base::OnceCallback<void(
      base::expected<PrepareDiagnosticsAppBrowserContextResult, std::string>)>;
  virtual void PrepareDiagnosticsAppBrowserContext(
      const base::FilePath& crx_path,
      const base::FilePath& swbn_path,
      PrepareDiagnosticsAppBrowserContextCallback callback) = 0;

  // Check if `manufacturer` provides any chromeos system extension.
  virtual bool IsChromeOSSystemExtensionProvider(
      const std::string& manufacturer) = 0;

  // Request for media device access. `extension` is set to NULL if request was
  // made from a webpage.
  virtual void ProcessMediaAccessRequest(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback,
      const extensions::Extension* extension) = 0;

  // Gets a weak ptr reference to this object.
  virtual base::WeakPtr<ShimlessRmaDelegate> GetWeakPtr() = 0;
};

}  // namespace ash::shimless_rma

#endif  // ASH_WEBUI_SHIMLESS_RMA_BACKEND_SHIMLESS_RMA_DELEGATE_H_
