// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_browser_window_handler.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/function_ref.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/app_mode/kiosk_policies.h"
#include "chrome/browser/chromeos/app_mode/kiosk_settings_navigation_throttle.h"
#include "chrome/browser/chromeos/app_mode/kiosk_troubleshooting_controller_ash.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

namespace chromeos {

namespace {

constexpr base::TimeDelta kCloseBrowserTimeout = base::Seconds(2);

#define WINDOW_ALLOWED true

void MakeWindowResizable(BrowserWindow* window) {
  views::Widget* widget =
      views::Widget::GetWidgetForNativeWindow(window->GetNativeWindow());
  if (widget) {
    widget->widget_delegate()->SetCanResize(true);
  }
}

content::WebContents* GetActiveWebContents(
    const BrowserWindowInterface* browser_window_interface) {
  return browser_window_interface->GetTabStripModel()->GetActiveWebContents();
}

std::string GetUrlOfActiveTab(
    const BrowserWindowInterface* browser_window_interface) {
  content::WebContents* active_tab =
      GetActiveWebContents(browser_window_interface);
  return active_tab ? active_tab->GetVisibleURL().spec() : std::string();
}

void CloseBrowser(BrowserWindowInterface* browser_window_interface) {
  // We prefer to use `CloseAllTabs`, because
  // `GetWindow()->Close()` can silently fail if the window is currently
  // being dragged. However, `CloseAllTabs` becomes a no-op if no tabs are
  // present, so we fall back to `GetWindow()->Close()` for that case.
  if (!browser_window_interface->GetTabStripModel()->empty()) {
    browser_window_interface->GetTabStripModel()->CloseAllTabs();
  } else {
    browser_window_interface->GetWindow()->Close();
  }
}

size_t GetBrowserCount() {
  size_t browser_count = 0;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        browser_count++;
        return true;
      });
  return browser_count;
}

}  // namespace

const char kKioskNewBrowserWindowHistogram[] = "Kiosk.NewBrowserWindow";

class NavigationWaiter : public content::WebContentsObserver,
                         public views::WidgetObserver {
 public:
  NavigationWaiter(Browser* browser, base::OnceClosure callback)
      : browser_(browser), callback_(std::move(callback)) {
    content::WebContents* web_contents = GetActiveWebContents(browser);
    if (!web_contents) {
      // If no WebContents is present, we'll continue to triaging without a url.
      // This is more restrictive and will likely result in the browser window
      // being closed.
      // One known case of this is a picture-in-picture browser.
      LOG(WARNING) << "New browser without WebContents detected.";
      RunCallback();
      return;
    }

    if (web_contents->GetVisibleURL().is_empty()) {
      LOG(WARNING)
          << "New browser with empty url detected, Waiting for navigation.";
      Observe(GetActiveWebContents(browser));
      // Observe the browser's widget visibility changes if someone wants to
      // show it in between.
      widget_observation_.Observe(browser->GetBrowserView().GetWidget());
    } else {
      RunCallback();
    }
  }

  NavigationWaiter(const NavigationWaiter&) = delete;
  NavigationWaiter& operator=(const NavigationWaiter&) = delete;
 private:
  // content::WebContentsObserver
  void DidStartNavigation(content::NavigationHandle* navigation) override {
    RunCallback();
  }

  void RunCallback() {
    // The callback should be called only once.
    if (callback_.is_null()) {
      return;
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback_));
  }

  // views::WidgetObserver:
  void OnWidgetVisibilityChanged(views::Widget* widget,
                                 bool visibility) override {
    // If the browser's widget visibility is changing before the
    // navigation is started, no need to wait for navigation,
    // proceed to the triaging of browser window immediately.
    // This prevents other non-navigation events, such as
    // subsequent tabs, from showing the window before a URL-based
    // decision can be made.
    RunCallback();
  }

  void OnWidgetDestroying(views::Widget* widget) override {
    widget_observation_.Reset();
  }

  raw_ptr<Browser> browser_;
  base::ScopedObservation<views::Widget, WidgetObserver> widget_observation_{
      this};
  base::OnceClosure callback_;
};

KioskBrowserWindowHandler::KioskBrowserWindowHandler(
    Profile* profile,
    const std::optional<std::string>& web_app_name,
    base::RepeatingCallback<void(bool is_closing)>
        on_browser_window_added_callback,
    base::OnceClosure shutdown_kiosk_browser_session_callback)
    : profile_(profile),
      web_app_name_(web_app_name),
      on_browser_window_added_callback_(on_browser_window_added_callback),
      shutdown_kiosk_browser_session_callback_(
          std::move(shutdown_kiosk_browser_session_callback)),
      kiosk_policies_(profile_->GetPrefs()) {
  kiosk_troubleshooting_controller_ =
      std::make_unique<ash::KioskTroubleshootingControllerAsh>(
          profile_->GetPrefs(),
          base::BindOnce(&KioskBrowserWindowHandler::Shutdown,
                         weak_ptr_factory_.GetWeakPtr()));

  CloseAllUnexpectedBrowserWindows();

  BrowserList::AddObserver(this);
}

KioskBrowserWindowHandler::~KioskBrowserWindowHandler() {
  BrowserList::RemoveObserver(this);
}

bool KioskBrowserWindowHandler::TriageNewSettingsBrowserWindow(
    Browser* browser) {
  url_waiters_.erase(browser);
  // It is safe to assume that no other tabs are present in `browser`, because
  // creating a second tab causes the browser window to be shown, which would
  // have caused the window to be closed before getting here.
  std::string url_string = GetUrlOfActiveTab(browser);

  if (KioskSettingsNavigationThrottle::IsSettingsPage(url_string)) {
    base::UmaHistogramEnumeration(kKioskNewBrowserWindowHistogram,
                                  KioskBrowserWindowType::kSettingsPage);
    HandleNewSettingsWindow(browser, url_string);
    on_browser_window_added_callback_.Run(/*is_closing=*/false);
    return WINDOW_ALLOWED;
  }

  base::UmaHistogramEnumeration(kKioskNewBrowserWindowHistogram,
                                KioskBrowserWindowType::kClosedRegularBrowser);
  LOG(WARNING) << "Force close browser opened in kiosk session"
               << ", url=" << url_string;
  CloseBrowserAndSetTimer(browser);
  on_browser_window_added_callback_.Run(/*is_closing=*/true);
  return !WINDOW_ALLOWED;
}

bool KioskBrowserWindowHandler::PreTriageNewBrowserWindowWithoutUrl(
    Browser* browser) {
  if (IsNewBrowserWindowAllowed(browser)) {
    base::UmaHistogramEnumeration(
        kKioskNewBrowserWindowHistogram,
        KioskBrowserWindowType::kOpenedRegularBrowser);
    LOG(WARNING)
        << "Open additional fullscreen browser window in kiosk session";
    chrome::ToggleFullscreenMode(browser, /*user_initiated=*/false);
    on_browser_window_added_callback_.Run(/*is_closing=*/false);
    return WINDOW_ALLOWED;
  }

  if (IsDevToolsAllowedBrowser(browser)) {
    MakeWindowResizable(browser->window());
    base::UmaHistogramEnumeration(
        kKioskNewBrowserWindowHistogram,
        KioskBrowserWindowType::kOpenedDevToolsBrowser);
    on_browser_window_added_callback_.Run(/*is_closing=*/false);
    return WINDOW_ALLOWED;
  }

  if (IsNormalTroubleshootingBrowserAllowed(browser)) {
    MakeWindowResizable(browser->window());
    base::UmaHistogramEnumeration(
        kKioskNewBrowserWindowHistogram,
        KioskBrowserWindowType::kOpenedTroubleshootingNormalBrowser);
    on_browser_window_added_callback_.Run(/*is_closing=*/false);
    return WINDOW_ALLOWED;
  }

  return !WINDOW_ALLOWED;
}

void KioskBrowserWindowHandler::HandleNewSettingsWindow(
    Browser* browser,
    const std::string& url_string) {
  if (settings_browser_) {
    // If another settings browser exist, navigate to `url_string` in the
    // existing browser.
    CloseBrowserAndSetTimer(browser);
    // Navigate in the existing browser.
    NavigateParams nav_params(
        settings_browser_, GURL(url_string),
        ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL);
    nav_params.window_action = NavigateParams::WindowAction::kShowWindow;
    Navigate(&nav_params);
    return;
  }

  bool app_browser = browser->is_type_app() || browser->is_type_app_popup() ||
                     browser->is_type_popup();
  if (!app_browser) {
    // If this browser is not an app browser, create a new app browser if none
    // yet exists.
    CloseBrowserAndSetTimer(browser);
    // Create a new app browser.
    NavigateParams nav_params(
        profile_, GURL(url_string),
        ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL);
    nav_params.disposition = WindowOpenDisposition::NEW_POPUP;
    Navigate(&nav_params);

    // We do not save the newly created browser to the `settings_browser_` here
    // because this new browser will be handled by this function after creation,
    // and it will be saved there.
    return;
  }

  settings_browser_ = browser;
  // We have to first call Restore() because the window was created as a
  // fullscreen window, having no prior bounds.
  // TODO(crbug.com/40103687): Figure out how to do it more cleanly.
  browser->window()->Restore();
  browser->window()->Maximize();
}

void KioskBrowserWindowHandler::CloseAllUnexpectedBrowserWindows() {
  CloseBrowserWindowsIf(
      [&web_app_name = web_app_name_](
          const BrowserWindowInterface& browser_window_interface) {
        // Do not close the main web app window (if any).
        bool is_web_app = web_app_name.has_value();
        bool is_web_app_window =
            is_web_app &&
            (browser_window_interface.GetBrowserForMigrationOnly()
                 ->app_name() == web_app_name);
        return !is_web_app_window;
      });
}

void KioskBrowserWindowHandler::OnBrowserAdded(Browser* browser) {
  // At this point no WebContents has been added to the browser, so we need to
  // post once to ensure we have a WebContents.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&KioskBrowserWindowHandler::OnCompleteBrowserAdded,
                     weak_ptr_factory_.GetWeakPtr(), browser));
}

void KioskBrowserWindowHandler::OnCompleteBrowserAdded(Browser* browser) {
  // URL may not be properly loaded yet. Pre-triage without it.
  // If the window is allowed to be shown, do not wait for navigation.
  if (PreTriageNewBrowserWindowWithoutUrl(browser)) {
    return;
  }

  // Hide the window until it is triaged.
  browser->window()->Hide();

  // At this point the URL being opened might still be unknown.
  // This URL is required for our triaging, so we'll wait for it.
  url_waiters_[browser] = std::make_unique<NavigationWaiter>(
      browser, base::BindOnce(
                   &KioskBrowserWindowHandler::OnBrowserNavigationWatchEnded,
                   weak_ptr_factory_.GetWeakPtr(), base::Unretained(browser)));
}

void KioskBrowserWindowHandler::OnBrowserNavigationWatchEnded(
    Browser* browser) {
  if (TriageNewSettingsBrowserWindow(browser)) {
    browser->window()->Show();
  }
}

void KioskBrowserWindowHandler::OnBrowserRemoved(Browser* browser) {
  url_waiters_.erase(browser);
  closing_browsers_.erase(browser);

  // Exit the kiosk session if the last browser was closed.
  if (ShouldExitKioskWhenLastBrowserRemoved() && GetBrowserCount() == 0) {
    LOG(WARNING) << "Last browser window closed, ending kiosk session.";
    Shutdown();
  }

  if (browser == settings_browser_) {
    settings_browser_ = nullptr;
  } else if (ShouldExitKioskWhenLastBrowserRemoved() &&
             IsOnlySettingsBrowserRemainOpen()) {
    // Only `settings_browser_` is opened and there are no app browsers anymore.
    // So we should close `settings_browser_` and it will end the kiosk session.
    CloseBrowserAndSetTimer(settings_browser_);
  }
}

bool KioskBrowserWindowHandler::IsNewBrowserWindowAllowed(
    Browser* browser) const {
  return kiosk_policies_.IsWindowCreationAllowed() &&
         browser->is_type_app_popup() && web_app_name_.has_value() &&
         browser->app_name() == web_app_name_.value();
}

bool KioskBrowserWindowHandler::IsDevToolsAllowedBrowser(
    Browser* browser) const {
  return browser->is_type_devtools() &&
         kiosk_troubleshooting_controller_
             ->AreKioskTroubleshootingToolsEnabled();
}

bool KioskBrowserWindowHandler::IsNormalTroubleshootingBrowserAllowed(
    Browser* browser) const {
  return browser->is_type_normal() &&
         kiosk_troubleshooting_controller_
             ->AreKioskTroubleshootingToolsEnabled();
}

bool KioskBrowserWindowHandler::ShouldExitKioskWhenLastBrowserRemoved() const {
  return web_app_name_.has_value();
}

bool KioskBrowserWindowHandler::IsOnlySettingsBrowserRemainOpen() const {
  return settings_browser_ && GetBrowserCount() == 1 &&
         GetLastActiveBrowserWindowInterfaceWithAnyProfile() ==
             settings_browser_;
}

void KioskBrowserWindowHandler::Shutdown() {
  if (!shutdown_kiosk_browser_session_callback_.is_null()) {
    std::move(shutdown_kiosk_browser_session_callback_).Run();
  }
}

void KioskBrowserWindowHandler::CloseBrowserWindowsIf(
    base::FunctionRef<bool(const BrowserWindowInterface&)> filter) {
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&filter, this](BrowserWindowInterface* browser_window_interface) {
        if (filter(*browser_window_interface)) {
          LOG(WARNING) << "kiosk: Closing unexpected browser window with url "
                       << GetUrlOfActiveTab(browser_window_interface)
                       << " of app "
                       << browser_window_interface->GetBrowserForMigrationOnly()
                              ->app_name();
          CloseBrowserAndSetTimer(browser_window_interface);
        }
        return true;
      });
}

void KioskBrowserWindowHandler::CloseBrowserAndSetTimer(
    BrowserWindowInterface* browser_window_interface) {
  closing_browsers_.emplace(std::piecewise_construct,
                            std::make_tuple(browser_window_interface),
                            std::make_tuple());
  closing_browsers_[browser_window_interface].Start(
      FROM_HERE, kCloseBrowserTimeout,
      base::BindOnce(&KioskBrowserWindowHandler::OnCloseBrowserTimeout,
                     weak_ptr_factory_.GetWeakPtr()));
  CloseBrowser(browser_window_interface);
}

void KioskBrowserWindowHandler::OnCloseBrowserTimeout() {
  NOTREACHED() << "Failed to close unexpected browser window.";
}

}  // namespace chromeos
