// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extension_frame_host.h"

#include "chrome/browser/extensions/extension_action_runner.h"

namespace extensions {

ChromeExtensionFrameHost::ChromeExtensionFrameHost(
    content::WebContents* web_contents)
    : ExtensionFrameHost(web_contents), web_contents_(web_contents) {}

ChromeExtensionFrameHost::~ChromeExtensionFrameHost() = default;

void ChromeExtensionFrameHost::RequestScriptInjectionPermission(
    const std::string& extension_id,
    mojom::InjectionType script_type,
    mojom::RunLocation run_location,
    RequestScriptInjectionPermissionCallback callback) {
  ExtensionActionRunner* runner =
      ExtensionActionRunner::GetForWebContents(web_contents_);
  if (!runner) {
    std::move(callback).Run(false);
    return;
  }
  runner->OnRequestScriptInjectionPermission(extension_id, script_type,
                                             run_location, std::move(callback));
}

}  // namespace extensions
