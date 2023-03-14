// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process_platform_part_chromeos.h"

#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_service_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

BrowserProcessPlatformPartChromeOS::BrowserProcessPlatformPartChromeOS()
    : browser_restore_observer_(this) {}

BrowserProcessPlatformPartChromeOS::~BrowserProcessPlatformPartChromeOS() =
    default;

bool BrowserProcessPlatformPartChromeOS::CanRestoreUrlsForProfile(
    const Profile* profile) const {
  return profile->IsRegularProfile();
}

BrowserProcessPlatformPartChromeOS::BrowserRestoreObserver::
    BrowserRestoreObserver(
        const BrowserProcessPlatformPartChromeOS* browser_process_platform_part)
    : browser_process_platform_part_(browser_process_platform_part) {
  BrowserList::AddObserver(this);
}

BrowserProcessPlatformPartChromeOS::BrowserRestoreObserver::
    ~BrowserRestoreObserver() {
  BrowserList::RemoveObserver(this);
}

void BrowserProcessPlatformPartChromeOS::BrowserRestoreObserver::OnBrowserAdded(
    Browser* browser) {
  // If |browser| is the only browser, restores urls based on the on startup
  // setting.
  if (chrome::GetBrowserCount(browser->profile()) == 1 &&
      ShouldRestoreUrls(browser)) {
    if (ShouldOpenUrlsInNewBrowser(browser)) {
      // Delay creating a new browser until |browser| is activated.
      on_session_restored_callback_subscription_ =
          SessionRestore::RegisterOnSessionRestoredCallback(base::BindRepeating(
              &BrowserProcessPlatformPartChromeOS::BrowserRestoreObserver::
                  OnSessionRestoreDone,
              base::Unretained(this)));
    } else {
      RestoreUrls(browser);
    }
  }

  // If the startup urls from LAST_AND_URLS pref are already opened in a new
  // browser, skip opening the same browser.
  if (browser->creation_source() ==
      Browser::CreationSource::kLastAndUrlsStartupPref) {
    DCHECK(on_session_restored_callback_subscription_);
    on_session_restored_callback_subscription_ = {};
  }
}

void BrowserProcessPlatformPartChromeOS::BrowserRestoreObserver::
    OnSessionRestoreDone(Profile* profile, int num_tabs_restored) {
  // Ensure this callback to be called exactly once.
  on_session_restored_callback_subscription_ = {};

  // All browser windows are created. Open startup urls in a new browser.
  auto create_params = Browser::CreateParams(profile, /*user_gesture*/ false);
  Browser* browser = Browser::Create(create_params);
  RestoreUrls(browser);
  browser->window()->Show();
  browser->window()->Activate();
}

bool BrowserProcessPlatformPartChromeOS::BrowserRestoreObserver::
    ShouldRestoreUrls(Browser* browser) const {
  Profile* profile = browser->profile();

  // Only open urls for regular sign in users.
  DCHECK(profile);
  if (!browser_process_platform_part_->CanRestoreUrlsForProfile(profile))
    return false;

  // If during the restore process, or restore from a crash, don't launch urls.
  // However, in case of LAST_AND_URLS startup setting, urls should be opened
  // even when the restore session is in progress.
  SessionStartupPref pref =
      SessionStartupPref::GetStartupPref(browser->profile()->GetPrefs());
  if ((SessionRestore::IsRestoring(profile) &&
       pref.type != SessionStartupPref::LAST_AND_URLS) ||
      HasPendingUncleanExit(profile)) {
    return false;
  }

  // App windows should not be restored.
  auto window_type = WindowTypeForBrowserType(browser->type());
  if (window_type == sessions::SessionWindow::TYPE_APP ||
      window_type == sessions::SessionWindow::TYPE_APP_POPUP) {
    return false;
  }

  // If the browser is created by StartupBrowserCreator,
  // StartupBrowserCreatorImpl::OpenTabsInBrowser can open tabs, so don't
  // restore urls here.
  if (browser->creation_source() == Browser::CreationSource::kStartupCreator)
    return false;

  // If the startup setting is not open urls, don't launch urls.
  if (!pref.ShouldOpenUrls() || pref.urls.empty())
    return false;

  return true;
}

// If the startup setting is both the restore last session and the open urls,
// those should be opened in a new browser.
bool BrowserProcessPlatformPartChromeOS::BrowserRestoreObserver::
    ShouldOpenUrlsInNewBrowser(Browser* browser) const {
  SessionStartupPref pref =
      SessionStartupPref::GetStartupPref(browser->profile()->GetPrefs());
  return pref.type == SessionStartupPref::LAST_AND_URLS;
}

void BrowserProcessPlatformPartChromeOS::BrowserRestoreObserver::RestoreUrls(
    Browser* browser) {
  DCHECK(browser);

  SessionStartupPref pref =
      SessionStartupPref::GetStartupPref(browser->profile()->GetPrefs());
  std::vector<GURL> urls;
  for (const auto& url : pref.urls)
    urls.push_back(url);

  custom_handlers::ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(browser->profile());
  for (const GURL& url : urls) {
    // We skip URLs that we'd have to launch an external protocol handler for.
    // This avoids us getting into an infinite loop asking ourselves to open
    // a URL, should the handler be (incorrectly) configured to be us. Anyone
    // asking us to open such a URL should really ask the handler directly.
    bool handled_by_chrome =
        ProfileIOData::IsHandledURL(url) ||
        (registry && registry->IsHandledProtocol(url.scheme()));
    if (!handled_by_chrome)
      continue;

    int add_types = AddTabTypes::ADD_NONE | AddTabTypes::ADD_FORCE_INDEX;
    NavigateParams params(browser, url, ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
    params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
    params.tabstrip_add_types = add_types;
    Navigate(&params);
  }
}
