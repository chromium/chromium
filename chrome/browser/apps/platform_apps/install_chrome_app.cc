// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/install_chrome_app.h"

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/webstore_install_with_prompt.h"
#include "chrome/browser/extensions/webstore_standalone_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/extensions/webstore_install_result.h"
#include "components/crx_file/id_util.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/manifest_constants.h"

using extensions::ExtensionRegistry;

namespace {

// The URL to the webstore page for a specific app.
const char kWebstoreUrlFormat[] =
    "https://chrome.google.com/webstore/detail/%s";

// Error given when the extension is not an app.
const char kInstallChromeAppErrorNotAnApp[] =
    "--install-chrome-app can only be used to install apps.";

// Returns the webstore URL for an app.
GURL GetAppInstallUrl(const std::string& app_id) {
  return GURL(base::StringPrintf(kWebstoreUrlFormat, app_id.c_str()));
}

// Checks the manifest and completes the installation with NOT_PERMITTED if the
// extension is not an app.
class WebstoreInstallWithPromptAppsOnly
    : public extensions::WebstoreInstallWithPrompt {
 public:
  WebstoreInstallWithPromptAppsOnly(const std::string& app_id,
                                    Profile* profile,
                                    gfx::NativeWindow parent_window)
      : WebstoreInstallWithPrompt(
            app_id,
            profile,
            parent_window,
            extensions::WebstoreStandaloneInstaller::Callback()) {}
  WebstoreInstallWithPromptAppsOnly(const WebstoreInstallWithPromptAppsOnly&) =
      delete;
  WebstoreInstallWithPromptAppsOnly& operator=(
      const WebstoreInstallWithPromptAppsOnly&) = delete;

 private:
  ~WebstoreInstallWithPromptAppsOnly() override {}

  // extensions::WebstoreStandaloneInstaller overrides:
  void OnManifestParsed() override;
};

void WebstoreInstallWithPromptAppsOnly::OnManifestParsed() {
  if (!base::Contains(manifest(), extensions::manifest_keys::kApp)) {
    CompleteInstall(extensions::webstore_install::NOT_PERMITTED,
                    kInstallChromeAppErrorNotAnApp);
    return;
  }

  ProceedWithInstallPrompt();
}

}  // namespace

namespace install_chrome_app {

void InstallChromeApp(const std::string& app_id) {
  if (!crx_file::id_util::IdIsValid(app_id))
    return;

  // At the moment InstallChromeApp() is called immediately after handling
  // startup URLs, so a browser is guaranteed to be created. If that changes we
  // may need to start a browser or browser session here.
  Browser* browser = BrowserList::GetInstance()->get(0);
  DCHECK(browser);

  content::OpenURLParams params(GetAppInstallUrl(app_id), content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false);
  browser->OpenURL(params, /*navigation_handle_callback=*/{});

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser->profile());
  // Skip if this app is already installed or blocklisted. For disabled or
  // or terminated apps, going through the installation flow should re-enable
  // them.
  const extensions::Extension* installed_extension = registry->GetExtensionById(
      app_id, ExtensionRegistry::ENABLED | ExtensionRegistry::BLOCKLISTED);
  // TODO(jackhou): For installed apps, maybe we should do something better,
  // e.g. show the app list (and re-add it to the taskbar).
  if (installed_extension)
    return;

  WebstoreInstallWithPromptAppsOnly* installer =
      new WebstoreInstallWithPromptAppsOnly(
          app_id, browser->profile(), browser->window()->GetNativeWindow());
  installer->BeginInstall();
}

}  // namespace install_chrome_app
