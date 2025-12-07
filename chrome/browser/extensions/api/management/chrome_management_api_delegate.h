// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MANAGEMENT_CHROME_MANAGEMENT_API_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_MANAGEMENT_CHROME_MANAGEMENT_API_DELEGATE_H_

#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "extensions/browser/api/management/management_api_delegate.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

// Delegate for the //extensions layer implementation of chrome.management.
// Currently the implementation of this delegate is split by platform between
// chrome_management_api_delegate_android.cc and _nonandroid.cc.
// TODO(crbug.com/410612887): Merge the two implementations.
class ChromeManagementAPIDelegate : public ManagementAPIDelegate {
 public:
  ChromeManagementAPIDelegate();
  ~ChromeManagementAPIDelegate() override;

  // ManagementAPIDelegate.
  bool LaunchAppFunctionDelegate(
      const Extension* extension,
      content::BrowserContext* context) const override;
  GURL GetFullLaunchURL(const Extension* extension) const override;
  LaunchType GetLaunchType(const ExtensionPrefs* prefs,
                           const Extension* extension) const override;
  std::unique_ptr<InstallPromptDelegate> SetEnabledFunctionDelegate(
      content::WebContents* web_contents,
      content::BrowserContext* browser_context,
      const Extension* extension,
      base::OnceCallback<void(bool)> callback) const override;
  std::unique_ptr<UninstallDialogDelegate> UninstallFunctionDelegate(
      ManagementUninstallFunctionBase* function,
      const Extension* target_extension,
      bool show_programmatic_uninstall_ui) const override;
  bool CreateAppShortcutFunctionDelegate(
      ManagementCreateAppShortcutFunction* function,
      const Extension* extension,
      std::string* error) const override;
  std::unique_ptr<AppForLinkDelegate> GenerateAppForLinkFunctionDelegate(
      ManagementGenerateAppForLinkFunction* function,
      content::BrowserContext* context,
      const std::string& title,
      const GURL& launch_url) const override;
  bool CanContextInstallWebApps(
      content::BrowserContext* context) const override;
  void InstallOrLaunchReplacementWebApp(
      content::BrowserContext* context,
      const GURL& web_app_url,
      ManagementAPIDelegate::InstallOrLaunchWebAppCallback callback)
      const override;
  void EnableExtension(content::BrowserContext* context,
                       const ExtensionId& extension_id) const override;
  void DisableExtension(
      content::BrowserContext* context,
      const Extension* source_extension,
      const ExtensionId& extension_id,
      disable_reason::DisableReason disable_reason) const override;
  bool UninstallExtension(content::BrowserContext* context,
                          const ExtensionId& transient_extension_id,
                          UninstallReason reason,
                          std::u16string* error) const override;
  void SetLaunchType(content::BrowserContext* context,
                     const ExtensionId& extension_id,
                     LaunchType launch_type) const override;
  GURL GetIconURL(const Extension* extension,
                  int icon_size,
                  ExtensionIconSet::Match match,
                  bool grayscale) const override;
  GURL GetEffectiveUpdateURL(const Extension& extension,
                             content::BrowserContext* context) const override;
  void ShowMv2DeprecationReEnableDialog(
      content::BrowserContext* context,
      content::WebContents* web_contents,
      const Extension& extension,
      base::OnceCallback<void(bool)> done_callback) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MANAGEMENT_CHROME_MANAGEMENT_API_DELEGATE_H_
