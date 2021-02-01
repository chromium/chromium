// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_APP_SESSION_H_
#define CHROME_BROWSER_ASH_APP_MODE_APP_SESSION_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/ash/app_mode/kiosk_session_plugin_handler_delegate.h"

class Profile;
class Browser;

namespace content {
class WebContents;
}

namespace extensions {
class AppWindow;
}

namespace ash {

class KioskSessionPluginHandler;

// AppSession maintains a kiosk session and handles its lifetime.
class AppSession : public KioskSessionPluginHandlerDelegate {
 public:
  AppSession();
  ~AppSession() override;

  // Initializes an app session.
  void Init(Profile* profile, const std::string& app_id);

  // Initializes an app session for Web kiosk.
  void InitForWebKiosk(Browser* browser);

  // Invoked when GuestViewManager adds a guest web contents.
  void OnGuestAdded(content::WebContents* guest_web_contents);

  // Replaces chrome::AttemptUserExit() by |closure|.
  void SetAttemptUserExitForTesting(base::OnceClosure closure);

  Browser* GetSettingsBrowserForTesting() { return settings_browser_; }
  void SetOnHandleBrowserCallbackForTesting(base::RepeatingClosure closure);

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
  // Is called whenever a new browser creation was handled by the
  // BrowserWindowHandler.
  base::RepeatingClosure on_handle_browser_callback_;

  DISALLOW_COPY_AND_ASSIGN(AppSession);
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when the migration of
// //chrome/browser/chromeos is finished.
namespace chromeos {
using ::ash::AppSession;
}

#endif  // CHROME_BROWSER_ASH_APP_MODE_APP_SESSION_H_
