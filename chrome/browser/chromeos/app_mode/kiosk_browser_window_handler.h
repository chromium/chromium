// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_BROWSER_WINDOW_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_BROWSER_WINDOW_HANDLER_H_

#include <map>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/app_mode/kiosk_policies.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list_observer.h"

namespace chromeos {

class KioskTroubleshootingController;

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
  kClosedAshBrowserWithLacrosEnabled = 6,
  kMaxValue = kClosedAshBrowserWithLacrosEnabled,
};

// This class monitors for the addition and removal of new browser windows
// during the kiosk session. On construction for web kiosk sessions, it gets a
// web app name stored as `web_app_name_`.
//
//
// If a new browser window is opened, this gets closed immediately, unless it's
// an allowed Settings window or `CanOpenNewBrowserWindow` method returns true.
//
// If the last browser window gets closed, the session gets ended.
//
// It also manages showing required settings pages in a consistent browser.
class KioskBrowserWindowHandler : public BrowserListObserver {
 public:
  KioskBrowserWindowHandler(
      Profile* profile,
      const std::optional<std::string>& web_app_name,
      base::RepeatingCallback<void(bool is_closing)>
          on_browser_window_added_callback,
      base::OnceClosure shutdown_kiosk_browser_session_callback);
  KioskBrowserWindowHandler(const KioskBrowserWindowHandler&) = delete;
  KioskBrowserWindowHandler& operator=(const KioskBrowserWindowHandler&) =
      delete;
  ~KioskBrowserWindowHandler() override;

  Browser* GetSettingsBrowserForTesting() { return settings_browser_; }

 private:
  void HandleNewBrowserWindow(Browser* browser);
  void HandleNewSettingsWindow(Browser* browser, const std::string& url_string);

  void CloseBrowserWindowsIf(base::FunctionRef<bool(const Browser&)> filter);
  void CloseBrowserAndSetTimer(Browser* browser);
  void OnCloseBrowserTimeout();
  void CloseAllUnexpectedBrowserWindows();

  // BrowserListObserver
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // Returns true if open by web application and allowed by policy.
  bool IsNewBrowserWindowAllowed(Browser* browser) const;

  // Returns true if open devtools browser and it is allowed by policy.
  bool IsDevToolsAllowedBrowser(Browser* browser) const;

  // Returns true if open normal browser and it is allowed by troubleshooting
  // policy.
  bool IsNormalTroubleshootingBrowserAllowed(Browser* browser) const;

  // Returns true in case of the initial browser window existed for web kiosks.
  bool ShouldExitKioskWhenLastBrowserRemoved() const;

  // Checks that there is no app browser and only `settings_browser_` remains
  // open.
  bool IsOnlySettingsBrowserRemainOpen() const;

  // Calls `shutdown_kiosk_browser_session_callback_` once.
  void Shutdown();

  // Owned by `ProfileManager`.
  const raw_ptr<Profile, DanglingUntriaged> profile_;
  // `web_app_name_` is set only for web kiosk sessions.
  const std::optional<std::string> web_app_name_;
  base::RepeatingCallback<void(bool is_closing)>
      on_browser_window_added_callback_;
  base::OnceClosure shutdown_kiosk_browser_session_callback_;

  std::unique_ptr<KioskTroubleshootingController>
      kiosk_troubleshooting_controller_;

  // Browser in which settings are shown, restricted by
  // KioskSettingsNavigationThrottle.
  raw_ptr<Browser> settings_browser_ = nullptr;

  // Provides access to app session related policies.
  KioskPolicies kiosk_policies_;

  // Map that keeps track of all unexpected browser windows until they are
  // confirmed to be closed via `OnBrowserRemoved`. If they did not get closed
  // before the timer fires, we will crash as we consider the kiosk session
  // compromised.
  std::map<Browser*, base::OneShotTimer> closing_browsers_;

  base::WeakPtrFactory<KioskBrowserWindowHandler> weak_ptr_factory_{this};
};

}  // namespace chromeos
#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_BROWSER_WINDOW_HANDLER_H_
