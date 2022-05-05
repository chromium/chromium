// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_APP_SESSION_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_APP_SESSION_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/app_mode/kiosk_session_plugin_handler_delegate.h"

class PrefRegistrySimple;
class PrefService;
class Profile;
class Browser;

namespace content {
class WebContents;
}

namespace extensions {
class AppWindow;
}

namespace chromeos {

// Kiosk histogram metrics-related constants.
extern const char kKioskMetrics[];
extern const char kKioskSessionStateHistogram[];
extern const char kKioskSessionCountPerDayHistogram[];
extern const char kKioskSessionDurationNormalHistogram[];
extern const char kKioskSessionDurationInDaysNormalHistogram[];
extern const char kKioskSessionDurationCrashedHistogram[];
extern const char kKioskSessionDurationInDaysCrashedHistogram[];

extern const char kKioskSessionLastDayList[];
extern const char kKioskSessionStartTime[];

extern const base::TimeDelta kKioskSessionDurationHistogramLimit;

class KioskSessionPluginHandler;
class AppSessionMetricsService;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Keep in sync with respective enum in tools/metrics/histograms/enums.xml
enum class KioskSessionState {
  kStarted = 0,
  kWebStarted = 1,
  kCrashed = 2,
  kStopped = 3,
  kPluginCrashed = 4,
  kPluginHung = 5,
  kMaxValue = kPluginHung,
};

// AppSession maintains a kiosk session and handles its lifetime.
class AppSession : public KioskSessionPluginHandlerDelegate {
 public:
  AppSession();
  explicit AppSession(base::OnceClosure attempt_user_exit,
                      PrefService* local_state);
  AppSession(const AppSession&) = delete;
  AppSession& operator=(const AppSession&) = delete;
  ~AppSession() override;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Initializes an app session.
  virtual void Init(Profile* profile, const std::string& app_id);

  // Initializes an app session for Web kiosk.
  virtual void InitForWebKiosk(Browser* browser);

  // Invoked when GuestViewManager adds a guest web contents.
  void OnGuestAdded(content::WebContents* guest_web_contents);

  // Replaces chrome::AttemptUserExit() by |closure|.
  void SetAttemptUserExitForTesting(base::OnceClosure closure);

  Browser* GetSettingsBrowserForTesting() { return settings_browser_; }
  void SetOnHandleBrowserCallbackForTesting(base::RepeatingClosure closure);

 protected:
  // Set the |profile_| object.
  void SetProfile(Profile* profile);

  // Create a |browser_window_handler_| object.
  void CreateBrowserWindowHandler(Browser* browser);

 private:
  // AppWindowHandler watches for app window and exits the session when the
  // last window of a given app is closed.
  class AppWindowHandler;

  // BrowserWindowHandler monitors Browser object being created during
  // a kiosk session, log info such as URL so that the code path could be
  // fixed and closes the just opened browser window.
  class BrowserWindowHandler;

  void OnAppWindowAdded(extensions::AppWindow* app_window);
  void OnLastAppWindowClosed();

  // KioskSessionPluginHandlerDelegate
  bool ShouldHandlePlugin(const base::FilePath& plugin_path) const override;
  void OnPluginCrashed(const base::FilePath& plugin_path) override;
  void OnPluginHung(const std::set<int>& hung_plugins) override;

  bool is_shutting_down_ = false;

  std::unique_ptr<AppWindowHandler> app_window_handler_;
  std::unique_ptr<BrowserWindowHandler> browser_window_handler_;
  std::unique_ptr<KioskSessionPluginHandler> plugin_handler_;

  // Browser in which settings are shown, restricted by
  // KioskSettingsNavigationThrottle.
  Browser* settings_browser_ = nullptr;

  Profile* profile_ = nullptr;

  base::OnceClosure attempt_user_exit_;
  const std::unique_ptr<AppSessionMetricsService> metrics_service_;

  // Is called whenever a new browser creation was handled by the
  // BrowserWindowHandler.
  base::RepeatingClosure on_handle_browser_callback_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_APP_SESSION_H_
