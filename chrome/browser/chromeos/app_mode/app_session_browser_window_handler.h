// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_APP_SESSION_BROWSER_WINDOW_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_APP_SESSION_BROWSER_WINDOW_HANDLER_H_

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/app_mode/app_session_policies.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list_observer.h"

namespace chromeos {

extern const char kKioskNewBrowserWindowHistogram[];

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Keep in sync with respective enum in tools/metrics/histograms/enums.xml
enum class KioskBrowserWindowType {
  kSettingsPage = 0,
  kOther = 1,
  kMaxValue = kOther,
};

// This class monitors for the addition and removal of new browser windows
// during the kiosk session. On construction it gets a main browser handle
// stored as |browser_|.
//
// If a new browser window is opened, this gets closed immediately, unless it's
// an allowed Settings window.
//
// If the main browser window |browser_| of the session gets closed, the session
// gets ended.
//
// It also manages showing required settings pages in a consistent browser.
class AppSessionBrowserWindowHandler : public BrowserListObserver {
 public:
  AppSessionBrowserWindowHandler(
      Profile* profile,
      Browser* browser,
      base::RepeatingClosure on_browser_window_added_callback,
      base::RepeatingClosure on_last_browser_window_closed_callback);
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

  const raw_ptr<Profile> profile_;
  const raw_ptr<Browser> browser_;
  base::RepeatingClosure on_browser_window_added_callback_;
  base::RepeatingClosure on_last_browser_window_closed_callback_;

  // Browser in which settings are shown, restricted by
  // KioskSettingsNavigationThrottle.
  raw_ptr<Browser> settings_browser_ = nullptr;

  // Provides access to app session related policies.
  std::unique_ptr<AppSessionPolicies> app_session_policies_;

  base::WeakPtrFactory<AppSessionBrowserWindowHandler> weak_ptr_factory_{this};
};

}  // namespace chromeos
#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_APP_SESSION_BROWSER_WINDOW_HANDLER_H_
