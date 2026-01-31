// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/browser_restore_observer.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/browser_delegate/browser_controller.h"
#include "chrome/browser/ash/browser_delegate/browser_delegate.h"
#include "chrome/browser/ash/browser_delegate/browser_type.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_service_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/sessions/core/session_types.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace ash {

namespace {

// Returns true if we can restore URLs for `profile`. Restoring URLs should
// only be allowed for regular signed-in users.
bool CanRestoreUrlsForProfile(const Profile* profile) {
  return profile->IsRegularProfile() && !profile->IsSystemProfile() &&
         ash::ProfileHelper::IsUserProfile(profile) &&
         !ash::ProfileHelper::IsEphemeralUserProfile(profile);
}

// Returns true, if the url defined in the on startup setting should be
// opened. Otherwise, returns false.
bool ShouldRestoreUrls(BrowserDelegate* browser) {
  Profile* profile = browser->GetBrowser().profile();

  // Only open urls for regular sign in users.
  CHECK(profile);
  if (!CanRestoreUrlsForProfile(profile)) {
    return false;
  }

  // If during the restore process, or restore from a crash, don't launch urls.
  // However, in case of LAST_AND_URLS startup setting, urls should be opened
  // even when the restore session is in progress.
  SessionStartupPref pref = SessionStartupPref::GetStartupPref(
      browser->GetBrowser().profile()->GetPrefs());
  if ((SessionRestore::IsRestoring(profile) &&
       pref.type != SessionStartupPref::LAST_AND_URLS) ||
      HasPendingUncleanExit(profile)) {
    return false;
  }

  // App windows should not be restored.
  if (browser->GetType() == BrowserType::kApp ||
      browser->GetType() == BrowserType::kAppPopup) {
    return false;
  }

  // If the browser is created by StartupBrowserCreator,
  // StartupBrowserCreatorImpl::OpenTabsInBrowser can open tabs, so don't
  // restore urls here.
  if (browser->GetBrowser().creation_source() ==
      Browser::CreationSource::kStartupCreator) {
    return false;
  }

  // If the startup setting is not open urls, don't launch urls.
  if (!pref.ShouldOpenUrls() || pref.urls.empty()) {
    return false;
  }

  return true;
}

// Returns true, if the url defined in the on startup setting should be
// opened in a new browser. Otherwise, returns false.
bool ShouldOpenUrlsInNewBrowser(BrowserDelegate* browser) {
  SessionStartupPref pref = SessionStartupPref::GetStartupPref(
      browser->GetBrowser().profile()->GetPrefs());
  return pref.type == SessionStartupPref::LAST_AND_URLS;
}

}  // namespace

BrowserRestoreObserver::BrowserRestoreObserver() {
  browser_controller_observation_.Observe(BrowserController::GetInstance());
}

BrowserRestoreObserver::~BrowserRestoreObserver() = default;

void BrowserRestoreObserver::OnBrowserCreated(BrowserDelegate* browser) {
  // If |browser| is the only browser, restores urls based on the on startup
  // setting.
  bool is_the_only_browser_for_profile = true;
  BrowserController::GetInstance()->ForEachBrowser(
      BrowserController::kAscendingCreationTime,
      [browser, &is_the_only_browser_for_profile](BrowserDelegate& b) {
        if (b.GetBrowser().profile() == browser->GetBrowser().profile() &&
            &b != browser) {
          is_the_only_browser_for_profile = false;
        }
        return is_the_only_browser_for_profile
                   ? BrowserController::kContinueIteration
                   : BrowserController::kBreakIteration;
      });
  if (is_the_only_browser_for_profile && ShouldRestoreUrls(browser)) {
    if (ShouldOpenUrlsInNewBrowser(browser)) {
      // Delay creating a new browser until |browser| is activated.
      on_session_restored_callback_subscription_ =
          SessionRestore::RegisterOnSessionRestoredCallback(
              base::BindRepeating(&BrowserRestoreObserver::OnSessionRestoreDone,
                                  base::Unretained(this)));
    } else {
      RestoreUrls(browser);
    }
  }

  // If the startup urls from LAST_AND_URLS pref are already opened in a new
  // browser, skip opening the same browser.
  if (browser->GetBrowser().creation_source() ==
      Browser::CreationSource::kLastAndUrlsStartupPref) {
    on_session_restored_callback_subscription_ = {};
  }
}

// static
bool BrowserRestoreObserver::CanRestoreUrlsForProfileForTesting(
    const Profile* profile) {
  return CanRestoreUrlsForProfile(profile);
}

void BrowserRestoreObserver::OnSessionRestoreDone(Profile* profile,
                                                  int num_tabs_restored) {
  // Ensure this callback to be called exactly once.
  on_session_restored_callback_subscription_ = {};

  // All browser windows are created. Open startup urls in a new browser.
  auto create_params = Browser::CreateParams(profile, /*user_gesture*/ false);
  BrowserDelegate* browser = BrowserController::GetInstance()->GetDelegate(
      Browser::Create(create_params));
  RestoreUrls(browser);
  browser->Show();
  browser->Activate();
}

void BrowserRestoreObserver::RestoreUrls(BrowserDelegate* browser) {
  CHECK(browser);

  SessionStartupPref pref = SessionStartupPref::GetStartupPref(
      browser->GetBrowser().profile()->GetPrefs());

  custom_handlers::ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(
          browser->GetBrowser().profile());
  for (const GURL& url : pref.urls) {
    // We skip URLs that we'd have to launch an external protocol handler for.
    // This avoids us getting into an infinite loop asking ourselves to open
    // a URL, should the handler be (incorrectly) configured to be us. Anyone
    // asking us to open such a URL should really ask the handler directly.
    bool handled_by_chrome =
        ProfileIOData::IsHandledURL(url) ||
        (registry && registry->IsHandledProtocol(url.GetScheme()));
    if (!handled_by_chrome) {
      continue;
    }

    int add_types = AddTabTypes::ADD_NONE | AddTabTypes::ADD_FORCE_INDEX;
    NavigateParams params(&browser->GetBrowser(), url,
                          ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
    params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
    params.tabstrip_add_types = add_types;
    Navigate(&params);
  }
}

}  // namespace ash
