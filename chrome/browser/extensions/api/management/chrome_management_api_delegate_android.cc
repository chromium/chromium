// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/management/chrome_management_api_delegate.h"

#include "base/notimplemented.h"
#include "extensions/browser/api/management/management_api.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/uninstall_reason.h"

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
  // the user accepted it.
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

void ChromeManagementAPIDelegate::EnableExtension(
    content::BrowserContext* context,
    const ExtensionId& extension_id) const {
  const Extension* extension =
      ExtensionRegistry::Get(context)->GetExtensionById(
          extension_id, ExtensionRegistry::EVERYTHING);
  // The extension must exist as this method is invoked on enabling an extension
  // from the extensions management page (see `ManagementSetEnabledFunction`).
  CHECK(extension);

  // TODO(crbug.com/crbug.com/410612887): Support supervised user metrics here.
  ExtensionRegistrar::Get(context)->GrantPermissionsAndEnableExtension(
      *extension);
}

void ChromeManagementAPIDelegate::DisableExtension(
    content::BrowserContext* context,
    const Extension* source_extension,
    const ExtensionId& extension_id,
    disable_reason::DisableReason disable_reason) const {
  // TODO(crbug.com/crbug.com/410612887): Support supervised user metrics here.
  ExtensionRegistrar::Get(context)->DisableExtensionWithSource(
      source_extension, extension_id, disable_reason);
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
