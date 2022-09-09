// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/app_session_browser_window_handler.h"
#include <memory>

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/chromeos/app_mode/app_session_policies.h"
#include "chrome/browser/chromeos/app_mode/kiosk_settings_navigation_throttle.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"

namespace chromeos {

const char kKioskNewBrowserWindowHistogram[] = "Kiosk.NewBrowserWindow";

AppSessionBrowserWindowHandler::AppSessionBrowserWindowHandler(
    Profile* profile,
    Browser* browser,
    base::RepeatingClosure on_browser_window_added_callback,
    base::RepeatingClosure on_last_browser_window_closed_callback)
    : profile_(profile),
      browser_(browser),
      on_browser_window_added_callback_(on_browser_window_added_callback),
      on_last_browser_window_closed_callback_(
          on_last_browser_window_closed_callback) {
  BrowserList::AddObserver(this);
  app_session_policies_ =
      std::make_unique<AppSessionPolicies>(profile_->GetPrefs());
}

AppSessionBrowserWindowHandler::~AppSessionBrowserWindowHandler() {
  BrowserList::RemoveObserver(this);
}

void AppSessionBrowserWindowHandler::HandleNewBrowserWindow(Browser* browser) {
  content::WebContents* active_tab =
      browser->tab_strip_model()->GetActiveWebContents();
  std::string url_string =
      active_tab ? active_tab->GetURL().spec() : std::string();

  if (KioskSettingsNavigationThrottle::IsSettingsPage(url_string)) {
    base::UmaHistogramEnumeration(kKioskNewBrowserWindowHistogram,
                                  KioskBrowserWindowType::kSettingsPage);
    HandleNewSettingsWindow(browser, url_string);
  } else {
    base::UmaHistogramEnumeration(kKioskNewBrowserWindowHistogram,
                                  KioskBrowserWindowType::kOther);
    LOG(WARNING) << "Browser opened in kiosk session"
                 << ", url=" << url_string;
    browser->window()->Close();
  }

  on_browser_window_added_callback_.Run();
}

void AppSessionBrowserWindowHandler::HandleNewSettingsWindow(
    Browser* browser,
    const std::string& url_string) {
  if (settings_browser_) {
    // If another settings browser exist, navigate to |url_string| in the
    // existing browser.
    browser->window()->Close();
    // Navigate in the existing browser.
    NavigateParams nav_params(
        settings_browser_, GURL(url_string),
        ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL);
    nav_params.window_action = NavigateParams::SHOW_WINDOW;
    Navigate(&nav_params);
    return;
  }

  bool app_browser = browser->is_type_app() || browser->is_type_app_popup() ||
                     browser->is_type_popup();
  if (!app_browser) {
    // If this browser is not an app browser, create a new app browser if none
    // yet exists.
    browser->window()->Close();
    // Create a new app browser.
    NavigateParams nav_params(
        profile_, GURL(url_string),
        ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL);
    nav_params.disposition = WindowOpenDisposition::NEW_POPUP;
    Navigate(&nav_params);

    // We do not save the newly created browser to the |settings_browser_| here
    // because this new browser will be handled by this function after creation,
    // and it will be saved there.
    return;
  }

  settings_browser_ = browser;
  // We have to first call Restore() because the window was created as a
  // fullscreen window, having no prior bounds.
  // TODO(crbug.com/1015383): Figure out how to do it more cleanly.
  browser->window()->Restore();
  browser->window()->Maximize();
}

void AppSessionBrowserWindowHandler::OnBrowserAdded(Browser* browser) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&AppSessionBrowserWindowHandler::HandleNewBrowserWindow,
                     weak_ptr_factory_.GetWeakPtr(), browser));
}

void AppSessionBrowserWindowHandler::OnBrowserRemoved(Browser* browser) {
  // The app browser was removed.
  if (browser == browser_) {
    on_last_browser_window_closed_callback_.Run();
  }

  if (browser == settings_browser_) {
    settings_browser_ = nullptr;
  }
}

}  // namespace chromeos
