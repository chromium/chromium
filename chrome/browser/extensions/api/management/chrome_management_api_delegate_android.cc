// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/management/chrome_management_api_delegate.h"

#include "base/notimplemented.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/launch_util.h"

namespace extensions {

ChromeManagementAPIDelegate::ChromeManagementAPIDelegate() = default;

ChromeManagementAPIDelegate::~ChromeManagementAPIDelegate() = default;

bool ChromeManagementAPIDelegate::LaunchAppFunctionDelegate(
    const Extension* extension,
    content::BrowserContext* context) const {
  NOTIMPLEMENTED();
  return true;
}

GURL ChromeManagementAPIDelegate::GetFullLaunchURL(
    const Extension* extension) const {
  return AppLaunchInfo::GetFullLaunchURL(extension);
}

LaunchType ChromeManagementAPIDelegate::GetLaunchType(
    const ExtensionPrefs* prefs,
    const Extension* extension) const {
  return ::extensions::GetLaunchType(prefs, extension);
}

std::unique_ptr<InstallPromptDelegate>
ChromeManagementAPIDelegate::SetEnabledFunctionDelegate(
    content::WebContents* web_contents,
    content::BrowserContext* browser_context,
    const Extension* extension,
    base::OnceCallback<void(bool)> callback) const {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<UninstallDialogDelegate>
ChromeManagementAPIDelegate::UninstallFunctionDelegate(
    ManagementUninstallFunctionBase* function,
    const Extension* target_extension,
    bool show_programmatic_uninstall_ui) const {
  NOTIMPLEMENTED();
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

bool ChromeManagementAPIDelegate::UninstallExtension(
    content::BrowserContext* context,
    const ExtensionId& transient_extension_id,
    UninstallReason reason,
    std::u16string* error) const {
  return ExtensionRegistrar::Get(context)->UninstallExtension(
      transient_extension_id, reason, error);
}

void ChromeManagementAPIDelegate::SetLaunchType(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    LaunchType launch_type) const {
  NOTIMPLEMENTED();
}

GURL ChromeManagementAPIDelegate::GetIconURL(const Extension* extension,
                                             int icon_size,
                                             ExtensionIconSet::Match match,
                                             bool grayscale) const {
  NOTIMPLEMENTED();
  return GURL();
}

GURL ChromeManagementAPIDelegate::GetEffectiveUpdateURL(
    const Extension& extension,
    content::BrowserContext* context) const {
  ExtensionManagement* extension_management =
      ExtensionManagementFactory::GetForBrowserContext(context);
  return extension_management->GetEffectiveUpdateURL(extension);
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
