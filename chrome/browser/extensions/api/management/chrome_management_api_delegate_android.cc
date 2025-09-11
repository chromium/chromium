// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/management/chrome_management_api_delegate.h"

#include "base/notimplemented.h"

namespace extensions {

bool ChromeManagementAPIDelegate::LaunchAppFunctionDelegate(
    const Extension* extension,
    content::BrowserContext* context) const {
  // Return false to to cause the chrome.management API call to return an error.
  // This is similar to how we behave with Chrome Apps on Win/Mac/Linux.
  return false;
}

std::unique_ptr<InstallPromptDelegate>
ChromeManagementAPIDelegate::SetEnabledFunctionDelegate(
    content::WebContents* web_contents,
    content::BrowserContext* browser_context,
    const Extension* extension,
    base::OnceCallback<void(bool)> callback) const {
  // TODO(crbug.com/410932770): Show a permission dialog. For now, pretend that
  // the user accepted it. When this dialog is built, also enable the test
  // ManagementApiUnitTest.SetEnabled_IncreasedPermissions.
  NOTIMPLEMENTED() << "Skipping enable extension dialog";
  std::move(callback).Run(true);
  return nullptr;
}

bool ChromeManagementAPIDelegate::CreateAppShortcutFunctionDelegate(
    ManagementCreateAppShortcutFunction* function,
    const Extension* extension,
    std::string* error) const {
  NOTIMPLEMENTED();
  return false;
}

std::unique_ptr<AppForLinkDelegate>
ChromeManagementAPIDelegate::GenerateAppForLinkFunctionDelegate(
    ManagementGenerateAppForLinkFunction* function,
    content::BrowserContext* context,
    const std::string& title,
    const GURL& launch_url) const {
  NOTIMPLEMENTED();
  return nullptr;
}

bool ChromeManagementAPIDelegate::CanContextInstallWebApps(
    content::BrowserContext* context) const {
  NOTIMPLEMENTED();
  return false;
}

void ChromeManagementAPIDelegate::InstallOrLaunchReplacementWebApp(
    content::BrowserContext* context,
    const GURL& web_app_url,
    InstallOrLaunchWebAppCallback callback) const {
  NOTIMPLEMENTED();
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
