// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/management/chrome_management_api_delegate.h"

#include "base/notimplemented.h"
#include "base/notreached.h"

namespace extensions {

bool ChromeManagementAPIDelegate::LaunchAppFunctionDelegate(
    const Extension* extension,
    content::BrowserContext* context) const {
  // chrome.management.launchApp() is disabled at the API impl level.
  NOTREACHED();
}

bool ChromeManagementAPIDelegate::CreateAppShortcutFunctionDelegate(
    ManagementCreateAppShortcutFunction* function,
    const Extension* extension,
    std::string* error) const {
  // chrome.management.createAppShortcut() is disabled at the API impl level.
  NOTREACHED();
}

std::unique_ptr<AppForLinkDelegate>
ChromeManagementAPIDelegate::GenerateAppForLinkFunctionDelegate(
    ManagementGenerateAppForLinkFunction* function,
    content::BrowserContext* context,
    const std::string& title,
    const GURL& launch_url) const {
  // chrome.management.generateAppForLink() is disabled at the API impl level.
  NOTREACHED();
}

bool ChromeManagementAPIDelegate::CanContextInstallWebApps(
    content::BrowserContext* context) const {
  // chrome.management.installReplacementWebApp() is disabled on desktop Android
  // via _api_features.json. http://crbug.com/371332103
  NOTREACHED();
}

void ChromeManagementAPIDelegate::InstallOrLaunchReplacementWebApp(
    content::BrowserContext* context,
    const GURL& web_app_url,
    InstallOrLaunchWebAppCallback callback) const {
  // chrome.management.installReplacementWebApp() is disabled on desktop Android
  // via _api_features.json. http://crbug.com/371332103
  NOTREACHED();
}

void ChromeManagementAPIDelegate::ShowMv2DeprecationReEnableDialog(
    content::BrowserContext* context,
    content::WebContents* web_contents,
    const Extension& extension,
    base::OnceCallback<void(bool)> done_callback) const {
  NOTIMPLEMENTED();
  std::move(done_callback).Run(/*enable_allowed=*/false);
}

}  // namespace extensions
