// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Download code which handles CRX files (extensions, themes, apps, ...).

#include "chrome/browser/download/download_crx_util.h"

#include <memory>

#include "base/auto_reset.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/browser_window_util.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "extensions/browser/crx_installer.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/user_script.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/browser.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using content::BrowserThread;
using download::DownloadItem;
using extensions::WebstoreInstaller;

namespace download_crx_util {

namespace {

bool g_allow_offstore_install_for_testing = false;

// Hold a mock ExtensionInstallPrompt object that will be used when the
// download system opens a CRX.
ExtensionInstallPrompt* mock_install_prompt_for_testing = nullptr;

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
    mock_install_prompt_for_testing = nullptr;
    return std::unique_ptr<ExtensionInstallPrompt>(result);
  }
  content::WebContents* web_contents =
      content::DownloadItemUtils::GetWebContents(
          const_cast<DownloadItem*>(&download_item));
  if (!web_contents) {
    BrowserWindowInterface* browser =
        extensions::browser_window_util::GetLastActiveBrowserWithProfile(
            *profile, false);
    if (!browser) {
#if BUILDFLAG(IS_ANDROID)
      // TODO(crbug.com/474161414): Implement fallback if no browser is found.
      // Android does not have Browser implementation yet, but we are okay with
      // not showing an installed dialog if no window is open. The caller
      // handles having an empty ExtensionInstallPrompt.
      return nullptr;
#else
      browser = Browser::Create(
          Browser::CreateParams(Browser::TYPE_NORMAL, profile, true));
#endif
    }
    TabListInterface* tab_list = TabListInterface::From(browser);
    web_contents = tab_list->GetActiveTab()->GetContents();
  }
  return std::make_unique<ExtensionInstallPrompt>(web_contents);
}
}  // namespace

bool OffStoreInstallAllowedByPrefs(Profile* profile, const DownloadItem& item) {
  return g_allow_offstore_install_for_testing ||
         extensions::ExtensionManagementFactory::GetForBrowserContext(profile)
             ->IsOffstoreInstallAllowed(item.GetURL(), item.GetReferrerUrl());
}

// Tests can call this method to inject a mock ExtensionInstallPrompt
// to be used to confirm permissions on a downloaded CRX.
void SetMockInstallPromptForTesting(
    std::unique_ptr<ExtensionInstallPrompt> mock_prompt) {
  mock_install_prompt_for_testing = mock_prompt.release();
}

scoped_refptr<extensions::CrxInstaller> CreateCrxInstaller(
    Profile* profile,
    const download::DownloadItem& download_item) {
  scoped_refptr<extensions::CrxInstaller> installer(
      extensions::CrxInstaller::Create(
          profile, CreateExtensionInstallPrompt(profile, download_item),
          WebstoreInstaller::GetAssociatedApproval(download_item)));

  installer->set_error_on_unsupported_requirements(true);
  installer->set_delete_source(true);
  installer->set_was_triggered_by_user_download();
  installer->set_original_mime_type(download_item.GetOriginalMimeType());
  installer->set_apps_require_extension_mime_type(true);

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
         (OffStoreInstallAllowedByPrefs(profile, item) ||
          extension_urls::IsWebstoreUpdateUrl(item.GetOriginalUrl()) ||
          extension_urls::IsWebstoreDomain(item.GetOriginalUrl()));
}

std::unique_ptr<base::AutoReset<bool>> OverrideOffstoreInstallAllowedForTesting(
    bool allowed) {
  return std::make_unique<base::AutoReset<bool>>(
      &g_allow_offstore_install_for_testing, allowed);
}

}  // namespace download_crx_util
