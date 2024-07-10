// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_BROWSER_SESSION_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_BROWSER_SESSION_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/chromeos/app_mode/kiosk_browser_window_handler.h"
#include "chrome/browser/chromeos/app_mode/kiosk_metrics_service.h"
#include "ppapi/buildflags/buildflags.h"

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

class KioskSessionPluginHandler;
class KioskSessionPluginHandlerDelegate;

// KioskBrowserSession maintains a kiosk session and handles its lifetime.
class KioskBrowserSession {
 public:
  explicit KioskBrowserSession(Profile* profile);
  KioskBrowserSession(Profile* profile,
                      base::OnceClosure attempt_user_exit,
                      PrefService* local_state);
  KioskBrowserSession(const KioskBrowserSession&) = delete;
  KioskBrowserSession& operator=(const KioskBrowserSession&) = delete;
  virtual ~KioskBrowserSession();

  static std::unique_ptr<KioskBrowserSession> CreateForTesting(
      Profile* profile,
      base::OnceClosure attempt_user_exit,
      PrefService* local_state,
      const std::vector<std::string>& crash_dirs);

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Initializes an app session for Chrome App Kiosk.
  void InitForChromeAppKiosk(const std::string& app_id);

  // Initializes an app session for Web kiosk.
  // `web_app_name` is std::nullopt for ash-side of the web kiosk with Lacros.
  void InitForWebKiosk(const std::optional<std::string>& web_app_name);

  // Invoked when GuestViewManager adds a guest web contents.
  void OnGuestAdded(content::WebContents* guest_web_contents);

  Browser* GetSettingsBrowserForTesting();
  void SetOnHandleBrowserCallbackForTesting(
      base::RepeatingCallback<void(bool is_closing)> callback);

  KioskSessionPluginHandlerDelegate* GetPluginHandlerDelegateForTesting();

  bool is_shutting_down() const { return is_shutting_down_; }

 private:
  // AppWindowHandler watches for app window and exits the session when the
  // last window of a given app is closed. This class is only used for Chrome
  // App Kiosk.
  class AppWindowHandler;

  // PluginHandlerDelegateImpl handles callbacks from `plugin_handler_`.
  class PluginHandlerDelegateImpl;

  KioskBrowserSession(Profile* profile,
                      base::OnceClosure attempt_user_exit,
                      std::unique_ptr<KioskMetricsService> metrics_service);

  // Create a `browser_window_handler_` object.
  void CreateBrowserWindowHandler(
      const std::optional<std::string>& web_app_name);

  Profile* profile() const { return profile_; }

  void OnHandledNewBrowserWindow(bool is_closing);
  void OnAppWindowAdded(extensions::AppWindow* app_window);
  void Shutdown();

  bool is_shutting_down_ = false;

  // Owned by `ProfileManager`.
  raw_ptr<Profile, DanglingUntriaged> profile_ = nullptr;

  std::unique_ptr<AppWindowHandler> app_window_handler_;
  std::unique_ptr<KioskBrowserWindowHandler> browser_window_handler_;
#if BUILDFLAG(ENABLE_PLUGINS)
  std::unique_ptr<PluginHandlerDelegateImpl> plugin_handler_delegate_;
  // Initialized only for Chrome app kiosks in `InitForChromeAppKiosk`.
  std::unique_ptr<KioskSessionPluginHandler> plugin_handler_;
#endif

  base::OnceClosure attempt_user_exit_;
  const std::unique_ptr<KioskMetricsService> metrics_service_;

  // Is called whenever a new browser creation was handled by the
  // BrowserWindowHandler.
  base::RepeatingCallback<void(bool is_closing)> on_handle_browser_callback_;

  base::WeakPtrFactory<KioskBrowserSession> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_BROWSER_SESSION_H_
