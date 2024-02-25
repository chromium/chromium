// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_FRAME_HOST_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_FRAME_HOST_H_

#include "extensions/browser/extension_frame_host.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/frame.mojom.h"
#include "extensions/common/mojom/injection_type.mojom-shared.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"

namespace content {
class WebContents;
}

class GURL;

namespace extensions {

class ChromeExtensionFrameHost : public ExtensionFrameHost {
 public:
  explicit ChromeExtensionFrameHost(content::WebContents* web_contents);
  ChromeExtensionFrameHost(const ChromeExtensionFrameHost&) = delete;
  ChromeExtensionFrameHost& operator=(const ChromeExtensionFrameHost&) = delete;
  ~ChromeExtensionFrameHost() override;

  // mojom::LocalFrameHost:
  void RequestScriptInjectionPermission(
      const ExtensionId& extension_id,
      mojom::InjectionType script_type,
      mojom::RunLocation run_location,
      RequestScriptInjectionPermissionCallback callback) override;
  void GetAppInstallState(const GURL& url,
                          GetAppInstallStateCallback callback) override;
  void WatchedPageChange(
      const std::vector<std::string>& css_selectors) override;
  void DetailedConsoleMessageAdded(
      const std::u16string& message,
      const std::u16string& source,
      const StackTrace& stack_trace,
      blink::mojom::ConsoleMessageLevel level) override;
  void ContentScriptsExecuting(
      const base::flat_map<ExtensionId, std::vector<std::string>>&
          extension_id_to_scripts,
      const GURL& frame_url) override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_FRAME_HOST_H_
