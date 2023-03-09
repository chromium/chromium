// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_APP_SESSION_BROWSER_WINDOW_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_APP_SESSION_BROWSER_WINDOW_HANDLER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/app_mode/app_session_policies.h"
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
  kMaxValue = kOpenedTroubleshootingNormalBrowser,
};

// This class monitors for the addition and removal of new browser windows
// during the kiosk session. On construction for web kiosk sessions, it gets a
// wab app name stored as `web_app_name_`.
//
//
// If a new browser window is opened, this gets closed immediately, unless it's
// an allowed Settings window or `CanOpenNewBrowserWindow` method returns true.
//
// If the last browser window gets closed, the session gets ended.
//
// It also manages showing required settings pages in a consistent browser.
class AppSessionBrowserWindowHandler : public BrowserListObserver {
 public:
  AppSessionBrowserWindowHandler(
      Profile* profile,
      const absl::optional<std::string>& web_app_name,
      base::RepeatingCallback<void(bool is_closing)>
          on_browser_window_added_callback,
      base::OnceClosure shutdown_app_session_callback);
  AppSessionBrowserWindowHandler(const AppSessionBrowserWindowHandler&) =
      delete;
  AppSessionBrowserWindowHandler& operator=(
      const AppSessionBrowserWindowHandler&) = delete;
  ~AppSessionBrowserWindowHandler() override;

  Browser* GetSettingsBrowserForTesting() { return settings_browser_; }

 private:
  void HandleNewBrowserWindow(Browser* browser);
  void HandleNewSettingsWindow(Browser* browser, const std::string& url_string);

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

  // Calls `shutdown_app_session_callback_` once.
  void ShutdownAppSession();

  // Owned by `ProfileManager`.
  const raw_ptr<Profile, DanglingUntriaged> profile_;
  // `web_app_name_` is set only when we have the initial browser in the web
  // kiosk session.
  const absl::optional<std::string> web_app_name_;
  base::RepeatingCallback<void(bool is_closing)>
      on_browser_window_added_callback_;
  base::OnceClosure shutdown_app_session_callback_;

  std::unique_ptr<KioskTroubleshootingController>
      kiosk_troubleshooting_controller_;

  // Browser in which settings are shown, restricted by
  // KioskSettingsNavigationThrottle.
  raw_ptr<Browser> settings_browser_ = nullptr;

  // Provides access to app session related policies.
  AppSessionPolicies app_session_policies_;

  base::WeakPtrFactory<AppSessionBrowserWindowHandler> weak_ptr_factory_{this};
};

}  // namespace chromeos
#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_APP_SESSION_BROWSER_WINDOW_HANDLER_H_
