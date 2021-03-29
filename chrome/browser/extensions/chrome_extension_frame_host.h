// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_FRAME_HOST_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_FRAME_HOST_H_

#include "extensions/browser/extension_frame_host.h"
#include "extensions/common/mojom/frame.mojom.h"
#include "extensions/common/mojom/injection_type.mojom-shared.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"

namespace content {
class WebContents;
}

namespace extensions {

class ChromeExtensionFrameHost : public ExtensionFrameHost {
 public:
  explicit ChromeExtensionFrameHost(content::WebContents* web_contents);
  ChromeExtensionFrameHost(const ChromeExtensionFrameHost&) = delete;
  ChromeExtensionFrameHost& operator=(const ChromeExtensionFrameHost&) = delete;
  ~ChromeExtensionFrameHost() override;

  // mojom::LocalFrameHost:
  void RequestScriptInjectionPermission(
      const std::string& extension_id,
      mojom::InjectionType script_type,
      mojom::RunLocation run_location,
      RequestScriptInjectionPermissionCallback callback) override;

 private:
  // This raw pointer is safe to use because ExtensionWebContentsObserver whose
  // lifetime is tied to the WebContents owns this instance.
  // The parent class ExtensionFrameHost uses WebContentsFrameReceiverSet with
  // |web_contents_| for mojom::LocalFrameHost.
  content::WebContents* web_contents_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_FRAME_HOST_H_
