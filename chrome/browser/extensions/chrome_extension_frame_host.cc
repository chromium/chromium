// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extension_frame_host.h"

#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_set.h"
#include "url/gurl.h"

namespace extensions {

ChromeExtensionFrameHost::ChromeExtensionFrameHost(
    content::WebContents* web_contents)
    : ExtensionFrameHost(web_contents) {}

ChromeExtensionFrameHost::~ChromeExtensionFrameHost() = default;

void ChromeExtensionFrameHost::RequestScriptInjectionPermission(
    const std::string& extension_id,
    mojom::InjectionType script_type,
    mojom::RunLocation run_location,
    RequestScriptInjectionPermissionCallback callback) {
  ExtensionActionRunner* runner =
      ExtensionActionRunner::GetForWebContents(web_contents());
  if (!runner) {
    std::move(callback).Run(false);
    return;
  }
  runner->OnRequestScriptInjectionPermission(extension_id, script_type,
                                             run_location, std::move(callback));
}

void ChromeExtensionFrameHost::GetAppInstallState(
    const GURL& requestor_url,
    GetAppInstallStateCallback callback) {
  ExtensionRegistry* registry =
      ExtensionRegistry::Get(web_contents()->GetBrowserContext());
  const ExtensionSet& extensions = registry->enabled_extensions();
  const ExtensionSet& disabled_extensions = registry->disabled_extensions();

  std::string state;
  if (extensions.GetHostedAppByURL(requestor_url))
    state = extension_misc::kAppStateInstalled;
  else if (disabled_extensions.GetHostedAppByURL(requestor_url))
    state = extension_misc::kAppStateDisabled;
  else
    state = extension_misc::kAppStateNotInstalled;

  std::move(callback).Run(state);
}

void ChromeExtensionFrameHost::WatchedPageChange(
    const std::vector<std::string>& css_selectors) {
  TabHelper* tab_helper = TabHelper::FromWebContents(web_contents());
  if (!tab_helper)
    return;
  tab_helper->OnWatchedPageChanged(css_selectors);
}

}  // namespace extensions
