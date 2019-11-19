// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SHIM_EXTENSION_APP_SHIM_HANDLER_MAC_H_
#define CHROME_BROWSER_APPS_APP_SHIM_EXTENSION_APP_SHIM_HANDLER_MAC_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "apps/app_lifetime_monitor.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_shim/app_shim_host_bootstrap_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_host_mac.h"
#include "chrome/browser/profiles/avatar_menu_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/app_window/app_window_registry.h"

class Profile;

namespace base {
class FilePath;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {
class AppWindow;
class Extension;
}  // namespace extensions

namespace apps {

// This app shim handler that handles events for app shims that correspond to an
// extension.
class ExtensionAppShimHandler : public AppShimHostBootstrap::Client,
                                public AppShimHost::Client,
                                public content::NotificationObserver,
                                public AppLifetimeMonitor::Observer,
                                public BrowserListObserver,
                                public AvatarMenuObserver {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Return the profile for |path|, only if it is already loaded.
    virtual Profile* ProfileForPath(const base::FilePath& path);

    // Call |callback| with the list of profiles for which this app is
    // installed.
    virtual void GetProfilesForAppAsync(
        const std::string& app_id,
        const std::vector<base::FilePath>& profile_paths_to_check,
        base::OnceCallback<void(const std::vector<base::FilePath>&)>);

    // Load a profile and call |callback| when completed or failed.
    virtual void LoadProfileAsync(const base::FilePath& path,
                                  base::OnceCallback<void(Profile*)> callback);

    // Return true if the specified path is for a valid profile that is also
    // locked.
    virtual bool IsProfileLockedForPath(const base::FilePath& path);

    // Return the app windows (not browser windows) for a legacy app.
    virtual extensions::AppWindowRegistry::AppWindowList GetWindows(
        Profile* profile,
        const std::string& extension_id);

    // Look up an extension from its id.
    virtual const extensions::Extension* MaybeGetAppExtension(
        content::BrowserContext* context,
        const std::string& extension_id);

    // Return true if the specified app should use an app shim (false, e.g, for
    // bookmark apps that open in tabs).
    virtual bool AllowShimToConnect(Profile* profile,
                                    const extensions::Extension* extension);

    // Create an AppShimHost for the specified parameters (intercept-able for
    // tests).
    virtual std::unique_ptr<AppShimHost> CreateHost(
        AppShimHost::Client* client,
        const base::FilePath& profile_path,
        const std::string& app_id,
        bool use_remote_cocoa);

    // Open a dialog to enable the specified extension. Call |callback| after
    // the dialog is executed.
    virtual void EnableExtension(Profile* profile,
                                 const std::string& extension_id,
                                 base::OnceCallback<void()> callback);

    // Launch the app in Chrome. This will (often) create a new window.
    virtual void LaunchApp(Profile* profile,
                           const extensions::Extension* extension,
                           const std::vector<base::FilePath>& files);

    // Launch the shim process for an app.
    virtual void LaunchShim(Profile* profile,
                            const extensions::Extension* extension,
                            bool recreate_shims,
                            ShimLaunchedCallback launched_callback,
                            ShimTerminatedCallback terminated_callback);

    // Launch the user manager (in response to attempting to access a locked
    // profile).
    virtual void LaunchUserManager();

    // Terminate Chrome if Chrome attempted to quit, but was prevented from
    // quitting due to apps being open.
    virtual void MaybeTerminate();
  };

  // Helper function to get the instance on the browser process. This will be
  // non-null except for tests.
  static ExtensionAppShimHandler* Get();

  ExtensionAppShimHandler();
  ~ExtensionAppShimHandler() override;

  // Get the host corresponding to a profile and app id, or null if there is
  // none.
  AppShimHost* FindHost(Profile* profile, const std::string& app_id);

  // Get the AppShimHost corresponding to a browser instance, returning nullptr
  // if none should exist. If no AppShimHost exists, but one should exist
  // (e.g, because the app process is still launching), create one, which will
  // bind to the app process when it finishes launching.
  AppShimHost* GetHostForBrowser(Browser* browser);

  static const extensions::Extension* MaybeGetAppExtension(
      content::BrowserContext* context,
      const std::string& extension_id);

  static const extensions::Extension* MaybeGetAppForBrowser(Browser* browser);

  // Instructs the shim to request user attention. Returns false if there is no
  // shim for this window.
  void RequestUserAttentionForWindow(extensions::AppWindow* app_window,
                                     AppShimAttentionType attention_type);

  // AppShimHostBootstrap::Client:
  void OnShimProcessConnected(
      std::unique_ptr<AppShimHostBootstrap> bootstrap) override;

  // AppShimHost::Client:
  void OnShimLaunchRequested(
      AppShimHost* host,
      bool recreate_shims,
      base::OnceCallback<void(base::Process)> launched_callback,
      base::OnceClosure terminated_callback) override;
  void OnShimProcessDisconnected(AppShimHost* host) override;
  void OnShimFocus(AppShimHost* host,
                   AppShimFocusType focus_type,
                   const std::vector<base::FilePath>& files) override;
  void OnShimSelectedProfile(AppShimHost* host,
                             const base::FilePath& profile_path) override;

  // AppLifetimeMonitor::Observer overrides:
  void OnAppStart(content::BrowserContext* context,
                  const std::string& app_id) override;
  void OnAppActivated(content::BrowserContext* context,
                      const std::string& app_id) override;
  void OnAppDeactivated(content::BrowserContext* context,
                        const std::string& app_id) override;
  void OnAppStop(content::BrowserContext* context,
                 const std::string& app_id) override;

  // content::NotificationObserver overrides:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // BrowserListObserver overrides;
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;
  void OnBrowserSetLastActive(Browser* browser) override;

  // AvatarMenuObserver:
  void OnAvatarMenuChanged(AvatarMenu* menu) override;

 protected:
  typedef std::set<Browser*> BrowserSet;

  // Virtual for tests.
  virtual bool IsAcceptablyCodeSigned(pid_t pid) const;

  // Exposed for testing.
  void set_delegate(Delegate* delegate);
  content::NotificationRegistrar& registrar() { return registrar_; }

  // Update |profile_menu_items_| from |avatar_menu_|. Virtual for tests.
  virtual void UpdateProfileMenuItems();

  // The list of all profiles that might appear in the menu.
  std::vector<chrome::mojom::ProfileMenuItemPtr> profile_menu_items_;

 private:
  // The state for an individual app, and for the profile-scoped app info.
  struct ProfileState;
  struct AppState;

  // Close all app shims associated with the specified profile.
  void CloseShimsForProfile(Profile* profile);

  // Close one specified app.
  void CloseShimForApp(Profile* profile, const std::string& app_id);

  // Return the profile that should be opened for |app_id|, preferring
  // |specified_profile_path| if is valid, otherwise prefering the most recently
  // used of |profile_paths|.
  base::FilePath SelectProfileForApp(
      const std::string& app_id,
      const base::FilePath& specified_profile_path,
      const std::vector<base::FilePath>& profile_paths) const;

  // Continuation of OnShimProcessConnected, once the query for all profiles
  // with the app installed has returned.profiles
  void OnShimProcessConnectedAndProfilesRetrieved(
      std::unique_ptr<AppShimHostBootstrap> bootstrap,
      const std::vector<base::FilePath>& profiles);

  // Continuation of OnShimProcessConnectedAndProfilesRetrieved, once the
  // decided profile has loaded.
  void OnShimProcessConnectedAndAppLoaded(
      std::unique_ptr<AppShimHostBootstrap> bootstrap,
      Profile* profile,
      const extensions::Extension* extension);

  // Continuation of OnShimSelectedProfile, once the profile has loaded.
  void OnShimSelectedProfileAndAppLoaded(
      Profile* profile,
      const extensions::Extension* extension);

  // Load the specified profile and extension, and run |callback| with
  // the result. The callback's arguments may be nullptr on failure.
  using LoadProfileAppCallback =
      base::OnceCallback<void(Profile*, const extensions::Extension*)>;
  void LoadProfileAndApp(const base::FilePath& profile_path,
                         const std::string& app_id,
                         LoadProfileAppCallback callback);
  void OnProfileLoaded(const base::FilePath& profile_path,
                       const std::string& app_id,
                       LoadProfileAppCallback callback,
                       Profile* profile);
  void OnAppEnabled(const base::FilePath& profile_path,
                    const std::string& app_id,
                    LoadProfileAppCallback callback);

  // Check to see for which profile paths in |profile_menu_items_| the app with
  // |app_id| is installed, and return them as the argument to |callback|.
  void GetProfilesForAppAsync(
      const std::string& app_id,
      base::OnceCallback<void(const std::vector<base::FilePath>&)> callback);

  // Callback for the call to asynchronously query which profiles have an app
  // installed.
  void OnGotProfilesForApp(const std::string& app_id,
                           const std::vector<base::FilePath>& profiles);

  // Update the profiles menu for the specified host.
  void UpdateAppProfileMenu(AppState* app_state);

  std::unique_ptr<Delegate> delegate_;

  // Retrieve the ProfileState for a given (Profile, Extension) pair. If one
  // does not exist, create one.
  ProfileState* GetOrCreateProfileState(Profile* profile,
                                        const extensions::Extension* extension);

  // Map from extension id to the state for that app.
  std::map<std::string, std::unique_ptr<AppState>> apps_;

  content::NotificationRegistrar registrar_;

  // The avatar menu instance used by all app shims.
  std::unique_ptr<AvatarMenu> avatar_menu_;

  base::WeakPtrFactory<ExtensionAppShimHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionAppShimHandler);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SHIM_EXTENSION_APP_SHIM_HANDLER_MAC_H_
