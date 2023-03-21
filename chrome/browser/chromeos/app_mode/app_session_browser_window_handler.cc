// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/app_session_browser_window_handler.h"
#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/chromeos/app_mode/app_session_policies.h"
#include "chrome/browser/chromeos/app_mode/kiosk_settings_navigation_throttle.h"
#include "chrome/browser/chromeos/app_mode/kiosk_troubleshooting_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "kiosk_troubleshooting_controller_ash.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace chromeos {

namespace {

void MakeWindowResizable(BrowserWindow* window) {
  views::Widget* widget =
      views::Widget::GetWidgetForNativeWindow(window->GetNativeWindow());
  if (widget) {
    widget->widget_delegate()->SetCanResize(true);
  }
}

}  // namespace

const char kKioskNewBrowserWindowHistogram[] = "Kiosk.NewBrowserWindow";

AppSessionBrowserWindowHandler::AppSessionBrowserWindowHandler(
    Profile* profile,
    const absl::optional<std::string>& web_app_name,
    base::RepeatingCallback<void(bool is_closing)>
        on_browser_window_added_callback,
    base::OnceClosure shutdown_app_session_callback)
    : profile_(profile),
      web_app_name_(web_app_name),
      on_browser_window_added_callback_(on_browser_window_added_callback),
      shutdown_app_session_callback_(std::move(shutdown_app_session_callback)),
      app_session_policies_(profile_->GetPrefs()) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  kiosk_troubleshooting_controller_ =
      std::make_unique<ash::KioskTroubleshootingControllerAsh>(
          profile_->GetPrefs(),
          base::BindOnce(&AppSessionBrowserWindowHandler::ShutdownAppSession,
                         weak_ptr_factory_.GetWeakPtr()));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  kiosk_troubleshooting_controller_ =
      std::make_unique<KioskTroubleshootingController>(
          profile_->GetPrefs(),
          base::BindOnce(&AppSessionBrowserWindowHandler::ShutdownAppSession,
                         weak_ptr_factory_.GetWeakPtr()));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  BrowserList::AddObserver(this);
}

AppSessionBrowserWindowHandler::~AppSessionBrowserWindowHandler() {
  BrowserList::RemoveObserver(this);
}

void AppSessionBrowserWindowHandler::HandleNewBrowserWindow(Browser* browser) {
  content::WebContents* active_tab =
      browser->tab_strip_model()->GetActiveWebContents();
  std::string url_string =
      active_tab ? active_tab->GetVisibleURL().spec() : std::string();

  if (KioskSettingsNavigationThrottle::IsSettingsPage(url_string)) {
    base::UmaHistogramEnumeration(kKioskNewBrowserWindowHistogram,
                                  KioskBrowserWindowType::kSettingsPage);
    HandleNewSettingsWindow(browser, url_string);
    on_browser_window_added_callback_.Run(/*is_closing=*/false);
    return;
  }

  if (IsNewBrowserWindowAllowed(browser)) {
    base::UmaHistogramEnumeration(
        kKioskNewBrowserWindowHistogram,
        KioskBrowserWindowType::kOpenedRegularBrowser);
    LOG(WARNING) << "Open additional fullscreen browser window in kiosk session"
                 << ", url=" << url_string;
    chrome::ToggleFullscreenMode(browser);
    on_browser_window_added_callback_.Run(/*is_closing=*/false);
    return;
  }

  if (IsDevToolsAllowedBrowser(browser)) {
    MakeWindowResizable(browser->window());
    base::UmaHistogramEnumeration(
        kKioskNewBrowserWindowHistogram,
        KioskBrowserWindowType::kOpenedDevToolsBrowser);
    on_browser_window_added_callback_.Run(/*is_closing=*/false);
    return;
  }

  if (IsNormalTroubleshootingBrowserAllowed(browser)) {
    MakeWindowResizable(browser->window());
    base::UmaHistogramEnumeration(
        kKioskNewBrowserWindowHistogram,
        KioskBrowserWindowType::kOpenedTroubleshootingNormalBrowser);
    on_browser_window_added_callback_.Run(/*is_closing=*/false);
    return;
  }

  base::UmaHistogramEnumeration(kKioskNewBrowserWindowHistogram,
                                KioskBrowserWindowType::kClosedRegularBrowser);
  LOG(WARNING) << "Force close browser opened in kiosk session"
               << ", url=" << url_string;
  browser->window()->Close();
  on_browser_window_added_callback_.Run(/*is_closing=*/true);
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
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&AppSessionBrowserWindowHandler::HandleNewBrowserWindow,
                     weak_ptr_factory_.GetWeakPtr(), browser));
}

void AppSessionBrowserWindowHandler::OnBrowserRemoved(Browser* browser) {
  // Exit the kiosk session if the last browser was closed.
  if (ShouldExitKioskWhenLastBrowserRemoved() &&
      BrowserList::GetInstance()->empty()) {
    ShutdownAppSession();
  }

  if (browser == settings_browser_) {
    settings_browser_ = nullptr;
  } else if (ShouldExitKioskWhenLastBrowserRemoved() &&
             IsOnlySettingsBrowserRemainOpen()) {
    // Only |settings_browser_| is opened and there are no app browsers anymore.
    // So we should close |settings_browser_| and it will end the kiosk session.
    settings_browser_->window()->Close();
  }
}

bool AppSessionBrowserWindowHandler::IsNewBrowserWindowAllowed(
    Browser* browser) const {
  return app_session_policies_.IsWindowCreationAllowed() &&
         browser->is_type_app_popup() && web_app_name_.has_value() &&
         browser->app_name() == web_app_name_.value();
}

bool AppSessionBrowserWindowHandler::IsDevToolsAllowedBrowser(
    Browser* browser) const {
  return browser->is_type_devtools() &&
         kiosk_troubleshooting_controller_
             ->AreKioskTroubleshootingToolsEnabled();
}

bool AppSessionBrowserWindowHandler::IsNormalTroubleshootingBrowserAllowed(
    Browser* browser) const {
  return browser->is_type_normal() &&
         kiosk_troubleshooting_controller_
             ->AreKioskTroubleshootingToolsEnabled();
}

bool AppSessionBrowserWindowHandler::ShouldExitKioskWhenLastBrowserRemoved()
    const {
  return web_app_name_.has_value();
}

bool AppSessionBrowserWindowHandler::IsOnlySettingsBrowserRemainOpen() const {
  return settings_browser_ && BrowserList::GetInstance()->size() == 1 &&
         BrowserList::GetInstance()->get(0) == settings_browser_;
}

void AppSessionBrowserWindowHandler::ShutdownAppSession() {
  if (!shutdown_app_session_callback_.is_null()) {
    std::move(shutdown_app_session_callback_).Run();
  }
}

}  // namespace chromeos
