// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/browser_close_manager.h"

#include <algorithm>
#include <iterator>
#include <ranges>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/background/extensions/background_mode_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/buildflags.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(ENABLE_CHROME_NOTIFICATIONS)
#include "chrome/browser/notifications/notification_ui_manager.h"
#endif

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/background/glic/glic_background_mode_manager.h"
#endif

namespace {

// Make a copy of the BrowserList and watch for any calls to AddBrowser or
// RemoveBrowser. This class allows a safe iteration over the list assuming
// that removing some Browser instance may remove another pending Browser
// instance.
class BrowserListIterator : public BrowserListObserver {
 public:
  BrowserListIterator()
      : browsers_(BrowserList::GetInstance()->begin(),
                  BrowserList::GetInstance()->end()) {
    BrowserList::GetInstance()->AddObserver(this);
  }
  BrowserListIterator(const BrowserListIterator&) = delete;
  BrowserListIterator(BrowserListIterator&&) = delete;
  ~BrowserListIterator() override {
    BrowserList::GetInstance()->RemoveObserver(this);
  }

  void OnBrowserAdded(Browser* browser) override {
    browsers_.push_back(browser);
  }
  void OnBrowserRemoved(Browser* browser) override {
    auto it = std::ranges::find(browsers_.begin(), browsers_.end(), browser);
    if (it != browsers_.end()) {
      browsers_.erase(it);
    }
  }
  bool IsEmpty() const { return browsers_.empty(); }

  Browser* Pop() {
    Browser* browser = browsers_.front();
    browsers_.erase(browsers_.begin());
    DCHECK(base::Contains(*BrowserList::GetInstance(), browser));
    return browser;
  }

 private:
  BrowserList::BrowserVector browsers_;
};

// Navigates a browser window for |profile|, creating one if necessary, to the
// downloads page if there are downloads in progress for |profile|.
void ShowInProgressDownloads(Profile* profile) {
  DownloadCoreService* download_core_service =
      DownloadCoreServiceFactory::GetForBrowserContext(profile);
  if (download_core_service &&
      download_core_service->BlockingShutdownCount() > 0) {
    chrome::ScopedTabbedBrowserDisplayer displayer(profile);
    chrome::ShowDownloads(displayer.browser());
  }
}

}  // namespace

BrowserCloseManager::BrowserCloseManager() : current_browser_(nullptr) {}
BrowserCloseManager::~BrowserCloseManager() = default;

void BrowserCloseManager::StartClosingBrowsers() {
  // If the session is ending or a silent exit was requested, skip straight to
  // closing the browsers without waiting for beforeunload dialogs.
  if (browser_shutdown::ShouldIgnoreUnloadHandlers()) {
    // Tell everyone that we are shutting down.
    browser_shutdown::SetTryingToQuit(true);
    CloseBrowsers();
    return;
  }
  TryToCloseBrowsers();
}

void BrowserCloseManager::CancelBrowserClose() {
  browser_shutdown::SetTryingToQuit(false);
  for (Browser* browser : *BrowserList::GetInstance()) {
    browser->ResetTryToCloseWindow();
  }
}

void BrowserCloseManager::TryToCloseBrowsers() {
  // If all browser windows can immediately be closed, fall out of this loop and
  // close the browsers. If any browser window cannot be closed, temporarily
  // stop closing. CallBeforeUnloadHandlers prompts the user and calls
  // OnBrowserReportCloseable with the result. If the user confirms the close,
  // this will trigger TryToCloseBrowsers to try again.
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->TryToCloseWindow(
            false, base::BindRepeating(
                       &BrowserCloseManager::OnBrowserReportCloseable, this))) {
      current_browser_ = browser;
      return;
    }
  }
  CheckForDownloadsInProgress();
}

void BrowserCloseManager::OnBrowserReportCloseable(bool proceed) {
  if (!current_browser_) {
    return;
  }

  current_browser_ = nullptr;

  if (proceed) {
    TryToCloseBrowsers();
  } else {
    CancelBrowserClose();
  }
}

void BrowserCloseManager::CheckForDownloadsInProgress() {
#if BUILDFLAG(IS_MAC)
  // Mac has its own in-progress downloads prompt in app_controller_mac.mm.
  CloseBrowsers();
#else
  int download_count = DownloadCoreService::BlockingShutdownCountAllProfiles();
  if (download_count == 0) {
    CloseBrowsers();
    return;
  }

  ConfirmCloseWithPendingDownloads(
      download_count,
      base::BindOnce(&BrowserCloseManager::OnReportDownloadsCancellable, this));
#endif
}

void BrowserCloseManager::ConfirmCloseWithPendingDownloads(
    int download_count,
    base::OnceCallback<void(bool)> callback) {
  Browser* browser = BrowserList::GetInstance()->GetLastActive();
  if (browser == nullptr) {
    // Background may call CloseAllBrowsers() with no Browsers. In this
    // case immediately continue with shutting down.
    std::move(callback).Run(/* proceed= */ true);
    return;
  }
  browser->window()->ConfirmBrowserCloseWithPendingDownloads(
      download_count, Browser::DownloadCloseType::kBrowserShutdown,
      std::move(callback));
}

void BrowserCloseManager::OnReportDownloadsCancellable(bool proceed) {
  if (proceed) {
    CloseBrowsers();
    return;
  }

  CancelBrowserClose();

  // Open the downloads page for each profile with downloads in progress.
  std::vector<Profile*> profiles(
      g_browser_process->profile_manager()->GetLoadedProfiles());
  for (Profile* profile : profiles) {
    ShowInProgressDownloads(profile);
    std::vector<Profile*> otr_profiles = profile->GetAllOffTheRecordProfiles();
    for (Profile* otr : otr_profiles) {
      ShowInProgressDownloads(otr);
    }
  }
}

void BrowserCloseManager::CloseBrowsers() {
#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  // Before we close the browsers shutdown all session services. That way an
  // exit can restore all browsers open before exiting.
  ProfileManager::ShutdownSessionServices();
#endif
#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  if (!browser_shutdown::IsTryingToQuit()) {
    BackgroundModeManager* background_mode_manager =
        g_browser_process->background_mode_manager();
    if (background_mode_manager) {
      background_mode_manager->SuspendBackgroundMode();
    }
  }
#endif

#if BUILDFLAG(ENABLE_GLIC)
  auto* glic_background_mode_manager =
      glic::GlicBackgroundModeManager::GetInstance();
  if (glic_background_mode_manager) {
    glic_background_mode_manager->ExitBackgroundMode();
  }
#endif

  // Make a copy of the BrowserList to simplify the case where we need to
  // destroy a Browser during the loop.
  BrowserListIterator browser_list_copy;

  bool ignore_unload_handlers = browser_shutdown::ShouldIgnoreUnloadHandlers();

  while (!browser_list_copy.IsEmpty()) {
    Browser* browser = browser_list_copy.Pop();
    browser->set_force_skip_warning_user_on_close(ignore_unload_handlers);
    browser->window()->Close();
    if (ignore_unload_handlers) {
      // This path is hit during logoff/power-down. It could be the case that
      // there are some tabs which would have prevented the browser from closing
      // (Ex: A form with an open dialog asking for permission to leave the
      // current site). Since we are attempting to end the session, we will
      // force skip these warnings and manually close all the tabs to make sure
      // the browser is destroyed and cleanup can happen.
      browser->tab_strip_model()->CloseAllTabs();
      browser->window()->DestroyBrowser();
      // Destroying the browser should have removed it from the browser list.
      DCHECK(!base::Contains(*BrowserList::GetInstance(), browser));
    }
  }

#if BUILDFLAG(ENABLE_CHROME_NOTIFICATIONS)
  NotificationUIManager* notification_manager =
      g_browser_process->notification_ui_manager();
  if (notification_manager) {
    notification_manager->CancelAll();
  }
#endif
}
