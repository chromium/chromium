// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Download code which handles CRX files (extensions, themes, apps, ...).

#include "chrome/browser/download/download_crx_util.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/user_script.h"

using content::BrowserThread;
using download::DownloadItem;
using extensions::WebstoreInstaller;

namespace download_crx_util {

namespace {

bool g_allow_offstore_install_for_testing = false;

// Hold a mock ExtensionInstallPrompt object that will be used when the
// download system opens a CRX.
ExtensionInstallPrompt* mock_install_prompt_for_testing = NULL;

// Called to get an extension install UI object.  In tests, will return
// a mock if the test calls download_util::SetMockInstallPromptForTesting()
// to set one.
std::unique_ptr<ExtensionInstallPrompt> CreateExtensionInstallPrompt(
    Profile* profile,
    const DownloadItem& download_item) {
  // Use a mock if one is present.  Otherwise, create a real extensions
  // install UI.
  if (mock_install_prompt_for_testing) {
    ExtensionInstallPrompt* result = mock_install_prompt_for_testing;
    mock_install_prompt_for_testing = NULL;
    return std::unique_ptr<ExtensionInstallPrompt>(result);
  } else {
    content::WebContents* web_contents =
        content::DownloadItemUtils::GetWebContents(
            const_cast<DownloadItem*>(&download_item));
    if (!web_contents) {
      Browser* browser = chrome::FindLastActiveWithProfile(profile);
      if (!browser) {
        browser = new Browser(
            Browser::CreateParams(Browser::TYPE_NORMAL, profile, true));
      }
      web_contents = browser->tab_strip_model()->GetActiveWebContents();
    }
    return std::unique_ptr<ExtensionInstallPrompt>(
        new ExtensionInstallPrompt(web_contents));
  }
}

bool OffStoreInstallAllowedByPrefs(Profile* profile, const DownloadItem& item) {
  return g_allow_offstore_install_for_testing ||
         extensions::ExtensionManagementFactory::GetForBrowserContext(profile)
             ->IsOffstoreInstallAllowed(item.GetURL(), item.GetReferrerUrl());
}

}  // namespace

// Tests can call this method to inject a mock ExtensionInstallPrompt
// to be used to confirm permissions on a downloaded CRX.
void SetMockInstallPromptForTesting(
    std::unique_ptr<ExtensionInstallPrompt> mock_prompt) {
  mock_install_prompt_for_testing = mock_prompt.release();
}

scoped_refptr<extensions::CrxInstaller> CreateCrxInstaller(
    Profile* profile,
    const download::DownloadItem& download_item) {
  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  CHECK(service);

  scoped_refptr<extensions::CrxInstaller> installer(
      extensions::CrxInstaller::Create(
          service,
          CreateExtensionInstallPrompt(profile, download_item),
          WebstoreInstaller::GetAssociatedApproval(download_item)));

  installer->set_error_on_unsupported_requirements(true);
  installer->set_delete_source(true);
  installer->set_install_cause(extension_misc::INSTALL_CAUSE_USER_DOWNLOAD);
  installer->set_original_mime_type(download_item.GetOriginalMimeType());
  installer->set_apps_require_extension_mime_type(true);

  return installer;
}

scoped_refptr<extensions::CrxInstaller> OpenChromeExtension(
    Profile* profile,
    const DownloadItem& download_item) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  scoped_refptr<extensions::CrxInstaller> installer(
      CreateCrxInstaller(profile, download_item));

  if (OffStoreInstallAllowedByPrefs(profile, download_item)) {
    installer->set_off_store_install_allow_reason(
        extensions::CrxInstaller::OffStoreInstallAllowedBecausePref);
  }

  if (extensions::UserScript::IsURLUserScript(download_item.GetURL(),
                                              download_item.GetMimeType())) {
    installer->InstallUserScript(download_item.GetFullPath(),
                                 download_item.GetURL());
  } else {
    DCHECK(!WebstoreInstaller::GetAssociatedApproval(download_item));
    installer->InstallCrx(download_item.GetFullPath());
  }

  return installer;
}

bool IsExtensionDownload(const DownloadItem& download_item) {
  if (download_item.GetTargetDisposition() ==
      DownloadItem::TARGET_DISPOSITION_PROMPT)
    return false;

  if (download_item.GetMimeType() == extensions::Extension::kMimeType ||
      extensions::UserScript::IsURLUserScript(download_item.GetURL(),
                                              download_item.GetMimeType())) {
    return true;
  } else {
    return false;
  }
}

bool IsTrustedExtensionDownload(Profile* profile, const DownloadItem& item) {
  return IsExtensionDownload(item) &&
         OffStoreInstallAllowedByPrefs(profile, item);
}

std::unique_ptr<base::AutoReset<bool>> OverrideOffstoreInstallAllowedForTesting(
    bool allowed) {
  return std::make_unique<base::AutoReset<bool>>(
      &g_allow_offstore_install_for_testing, allowed);
}

}  // namespace download_crx_util
