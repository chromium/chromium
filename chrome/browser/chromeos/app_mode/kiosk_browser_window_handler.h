// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_BROWSER_WINDOW_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_BROWSER_WINDOW_HANDLER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/app_mode/kiosk_policies.h"
#include "chromeos/ash/components/browser_delegate/browser_controller.h"
#include "components/webapps/common/web_app_id.h"

class Profile;

namespace ash {
class BrowserDelegate;
}

namespace chromeos {

class KioskTroubleshootingController;
class NavigationWaiter;

extern const char kKioskNewBrowserWindowHistogram[];

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Keep in sync with respective enum in tools/metrics/histograms/enums.xml
enum class KioskBrowserWindowType {
  kSettingsPage = 0,
  kClosedRegularBrowser = 1,
  kOpenedRegularBrowser = 2,
  kOpenedDevToolsBrowser = 3,
  kOpenedTroubleshootingNormalBrowser = 4,
  kOpenedSystemWebApp = 5,
  // Removed: kClosedAshBrowserWithLacrosEnabled = 6,
  kMaxValue = kOpenedSystemWebApp
};

// This class monitors for the addition and removal of new browser windows
// during the kiosk session. On construction for web kiosk sessions, it gets a
// web app id stored as `web_app_id_`.
//
//
// If a new browser window is opened, this gets closed immediately, unless it's
// an allowed Settings window or `CanOpenNewBrowserWindow` method returns true.
//
// If the last browser window gets closed, the session gets ended.
//
// It also manages showing required settings pages in a consistent browser.
class KioskBrowserWindowHandler : public ash::BrowserController::Observer {
 public:
  KioskBrowserWindowHandler(
      Profile* profile,
      const std::optional<webapps::AppId>& web_app_id,
      base::RepeatingCallback<void(bool is_closing)>
          on_browser_window_added_callback,
      base::OnceClosure shutdown_kiosk_browser_session_callback);
  KioskBrowserWindowHandler(const KioskBrowserWindowHandler&) = delete;
  KioskBrowserWindowHandler& operator=(const KioskBrowserWindowHandler&) =
      delete;
  ~KioskBrowserWindowHandler() override;

  ash::BrowserDelegate* GetSettingsBrowserForTesting() {
    return settings_browser_;
  }

 private:
  void OnCompleteBrowserAdded(ash::BrowserDelegate* browser);

  // Signals the end of the navigation monitoring phase.
  // Invoked in one of the two scenarios:
  // 1. The browser navigation has successfully started.
  // 2. An unexpected event changed the window visibility (e.g. new tab being
  // opened).
  void OnBrowserNavigationWatchEnded(ash::BrowserDelegate* browser,
                                     const std::string& url = std::string());
  // Returns true if the browser window is allowed to be opened in kiosk mode
  // independent of the navigation URL with no need to wait for navigation to
  // happen.
  bool PreTriageNewBrowserWindowWithoutUrl(ash::BrowserDelegate* browser);
  // Returns true if it's a valid settings window and closes the browser window
  // otherwise.
  // Once the navigation has started or is considered not necessary to wait for,
  // triage the settings browser window, since all other cases have been triaged
  // in scope of `PreTriageNewBrowserWindowWithoutUrl`.
  bool TriageNewSettingsBrowserWindow(ash::BrowserDelegate* browser,
                                      const std::string& url = std::string());
  void HandleNewSettingsWindow(ash::BrowserDelegate* browser,
                               const std::string& url_string);

  void CloseBrowserWindowsIf(
      base::FunctionRef<bool(const ash::BrowserDelegate&)> filter);
  void CloseBrowserAndSetTimer(ash::BrowserDelegate* browser);
  void OnCloseBrowserTimeout();
  void CloseAllUnexpectedBrowserWindows();

  // ash::BrowserController::Observer
  void OnBrowserCreated(ash::BrowserDelegate* browser) override;
  void OnBrowserClosed(ash::BrowserDelegate* browser) override;

  // Returns true if open by web application and allowed by policy.
  bool IsNewBrowserWindowAllowed(ash::BrowserDelegate* browser) const;

  // Returns true if open devtools browser and it is allowed by policy.
  bool IsDevToolsAllowedBrowser(ash::BrowserDelegate* browser) const;

  // Returns true if open normal browser and it is allowed by troubleshooting
  // policy.
  bool IsNormalTroubleshootingBrowserAllowed(
      ash::BrowserDelegate* browser) const;

  // Returns true in case of the initial browser window existed for web kiosks.
  bool ShouldExitKioskWhenLastBrowserRemoved() const;

  // Checks that there is no app browser and only `settings_browser_` remains
  // open.
  bool IsOnlySettingsBrowserRemainOpen() const;

  void CloseSettingsBrowser();

  // Calls `shutdown_kiosk_browser_session_callback_` once.
  void Shutdown();

  // Owned by `ProfileManager`.
  const raw_ptr<Profile, DanglingUntriaged> profile_;
  // `web_app_id_` is set only for web kiosk sessions.
  const std::optional<webapps::AppId> web_app_id_;
  base::RepeatingCallback<void(bool is_closing)>
      on_browser_window_added_callback_;
  base::OnceClosure shutdown_kiosk_browser_session_callback_;

  std::unique_ptr<KioskTroubleshootingController>
      kiosk_troubleshooting_controller_;

  // Browser in which settings are shown, restricted by
  // KioskSettingsNavigationThrottle.
  raw_ptr<ash::BrowserDelegate> settings_browser_ = nullptr;

  // Provides access to app session related policies.
  KioskPolicies kiosk_policies_;

  // Map that keeps track of all unexpected browser windows until they are
  // confirmed to be closed via `OnBrowserClosed`. If they did not get closed
  // before the timer fires, we will crash as we consider the kiosk session
  // compromised.
  std::map<ash::BrowserDelegate*, base::OneShotTimer> closing_browsers_;

  std::map<ash::BrowserDelegate*, std::unique_ptr<NavigationWaiter>>
      url_waiters_;

  base::ScopedObservation<ash::BrowserController,
                          ash::BrowserController::Observer>
      browser_controller_observation_{this};

  base::WeakPtrFactory<KioskBrowserWindowHandler> weak_ptr_factory_{this};
};

}  // namespace chromeos
#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_BROWSER_WINDOW_HANDLER_H_
