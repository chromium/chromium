// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_MANAGER_MAC_H_
#define CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_MANAGER_MAC_H_

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
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace base {
class FilePath;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace apps {

// This app shim handler that handles events for app shims that correspond to an
// extension.
class AppShimManager : public AppShimHostBootstrap::Client,
                       public AppShimHost::Client,
                       public content::NotificationObserver,
                       public AppLifetimeMonitor::Observer,
                       public BrowserListObserver,
                       public AvatarMenuObserver {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Show all app windows (for non-PWA apps). Return true if there existed any
    // windows.
    virtual bool ShowAppWindows(Profile* profile,
                                const web_app::AppId& app_id) = 0;

    // Close all app windows (for non-PWA apps).
    virtual void CloseAppWindows(Profile* profile,
                                 const web_app::AppId& app_id) = 0;

    // Return true iff |app_id| corresponds to an app that is installed for
    // |profile|. Note that |profile| may be nullptr (in which case it should
    // always return false).
    virtual bool AppIsInstalled(Profile* profile,
                                const web_app::AppId& app_id) = 0;

    // Return true iff the specified app can create an AppShimHost, which will
    // keep the app shim process connected (as opposed to, e.g, a bookmark app
    // that opens in a tab, which will immediately close).
    virtual bool AppCanCreateHost(Profile* profile,
                                  const web_app::AppId& app_id) = 0;

    // Return true if Cocoa windows for this app should be hosted in the app
    // shim process.
    virtual bool AppUsesRemoteCocoa(Profile* profile,
                                    const web_app::AppId& app_id) = 0;

    // Return true if a single app shim is used for all profiles (as opposed to
    // one shim per profile).
    virtual bool AppIsMultiProfile(Profile* profile,
                                   const web_app::AppId& app_id) = 0;

    // Open a dialog to enable the specified extension. Call |callback| after
    // the dialog is executed.
    virtual void EnableExtension(Profile* profile,
                                 const std::string& extension_id,
                                 base::OnceCallback<void()> callback) = 0;

    // Launch the app in Chrome. This will (often) create a new window. It is
    // guaranteed that |app_id| is installed for |profile| when this method
    // is called.
    virtual void LaunchApp(Profile* profile,
                           const web_app::AppId& app_id,
                           const std::vector<base::FilePath>& files) = 0;

    // Launch the shim process for an app. It is guaranteed that |app_id| is
    // installed for |profile| when this method is called.
    virtual void LaunchShim(Profile* profile,
                            const web_app::AppId& app_id,
                            bool recreate_shims,
                            ShimLaunchedCallback launched_callback,
                            ShimTerminatedCallback terminated_callback) = 0;

    // Return true if any app windows are open. This is eventually invoked
    // by MaybeTerminate. It does not apply to bookmark apps.
    virtual bool HasNonBookmarkAppWindowsOpen() = 0;
  };

  // Helper function to get the instance on the browser process. This will be
  // non-null except for tests.
  static AppShimManager* Get();

  explicit AppShimManager(std::unique_ptr<Delegate> delegate);
  ~AppShimManager() override;

  // Get the host corresponding to a profile and app id, or null if there is
  // none.
  AppShimHost* FindHost(Profile* profile, const web_app::AppId& app_id);

  // If the specified |browser| should be using RemoteCocoa (because it is a
  // bookmark app), then get or create an AppShimHost for it, and return
  // it. If an AppShimHost had to be created (e.g, because the app process is
  // still launching), create one, which will bind to the app process when it
  // finishes launching.
  AppShimHost* GetHostForRemoteCocoaBrowser(Browser* browser);

  // Return true if any non-bookmark app windows open.
  bool HasNonBookmarkAppWindowsOpen();

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
  void OnShimFocus(AppShimHost* host) override;
  void OnShimReopen(AppShimHost* host) override;
  void OnShimOpenedFiles(AppShimHost* host,
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

  // Return the profile for |path|, only if it is already loaded.
  virtual Profile* ProfileForPath(const base::FilePath& path);

  // Load a profile and call |callback| when completed or failed.
  virtual void LoadProfileAsync(const base::FilePath& path,
                                base::OnceCallback<void(Profile*)> callback);

  // Wait for |profile|'s WebAppProvider registry to be started.
  virtual void WaitForAppRegistryReadyAsync(
      Profile* profile,
      base::OnceCallback<void()> callback);

  // Return true if the specified path is for a valid profile that is also
  // locked.
  virtual bool IsProfileLockedForPath(const base::FilePath& path);

  // Create an AppShimHost for the specified parameters (intercept-able for
  // tests).
  virtual std::unique_ptr<AppShimHost> CreateHost(
      AppShimHost::Client* client,
      const base::FilePath& profile_path,
      const web_app::AppId& app_id,
      bool use_remote_cocoa);

  // Open the specified URL in a new Chrome window. This is the fallback when
  // an app shim exists, but there is no profile or extension for it. If
  // |profile_path| is specified, then that profile is preferred, otherwise,
  // the last used profile is used.
  virtual void OpenAppURLInBrowserWindow(const base::FilePath& profile_path,
                                         const GURL& url);

  // Launch the user manager (in response to attempting to access a locked
  // profile).
  virtual void LaunchUserManager();

  // Terminate Chrome if Chrome attempted to quit, but was prevented from
  // quitting due to apps being open.
  virtual void MaybeTerminate();

  // Exposed for testing.
  content::NotificationRegistrar& registrar() { return registrar_; }

  // Called when profile menu items may have changed. Rebuilds the profile
  // menu item list and sends updated lists to all apps.
  void UpdateAllProfileMenus();

  // Update |profile_menu_items_| from |avatar_menu_|. Virtual for tests.
  virtual void RebuildProfileMenuItemsFromAvatarMenu();

  // The list of all profiles that might appear in the menu.
  std::vector<chrome::mojom::ProfileMenuItemPtr> profile_menu_items_;

 private:
  // The state for an individual app, and for the profile-scoped app info.
  struct ProfileState;
  struct AppState;

  // Close all app shims associated with the specified profile.
  void CloseShimsForProfile(Profile* profile);

  // This is called by OnShimProcessConnected if the app shim was launched by
  // Chrome, and should connect to an already-existing AppShimHost.
  void OnShimProcessConnectedForRegisterOnly(
      std::unique_ptr<AppShimHostBootstrap> bootstrap);

  // The function LoadAndLaunchApp will:
  // - Find the appropriate profiles for which |app_id| should be launched.
  // - Load the profiles and ensure the app is enabled (using
  //   LoadProfileAndApp), if needed.
  // - Launch the app, if appropriate.
  // The "if appropriate" above is defined as:
  // - If |launch_files| is non-empty, then will always launch the app
  //   - If |profile_path| is non-empty, then use that profile.
  //   - In the most recently used profile, otherwise
  // - If |launch_files| is empty, then launch the app only if:
  //   - If |profile_path| is non-empty, then launch if the app is not running
  //     in that profile.
  //   - Otherwise, launch the app only if it is not running any profile.
  using LoadAndLaunchAppCallback =
      base::OnceCallback<void(ProfileState* profile_state,
                              chrome::mojom::AppShimLaunchResult result)>;
  void LoadAndLaunchApp(const web_app::AppId& app_id,
                        const base::FilePath& profile_path,
                        const std::vector<base::FilePath>& launch_files,
                        LoadAndLaunchAppCallback launch_callback);
  bool LoadAndLaunchApp_TryExistingProfileStates(
      const web_app::AppId& app_id,
      const base::FilePath& profile_path,
      const std::vector<base::FilePath>& launch_files,
      LoadAndLaunchAppCallback* launch_callback);
  void LoadAndLaunchApp_OnProfilesAndAppReady(
      const web_app::AppId& app_id,
      const std::vector<base::FilePath>& launch_files,
      const std::vector<base::FilePath>& profile_paths_to_launch,
      LoadAndLaunchAppCallback launch_callback);
  void LoadAndLaunchApp_LaunchIfAppropriate(
      Profile* profile,
      ProfileState* profile_state,
      const web_app::AppId& app_id,
      const std::vector<base::FilePath>& launch_files);

  // The final step of both paths for OnShimProcessConnected. This will connect
  // |bootstrap| to |profile_state|'s AppShimHost, if possible. The value of
  // |profile_state| is non-null if and only if |result| is success.
  void OnShimProcessConnectedAndAllLaunchesDone(
      std::unique_ptr<AppShimHostBootstrap> bootstrap,
      ProfileState* profile_state,
      chrome::mojom::AppShimLaunchResult result);

  // Load the specified profile and extension, and run |callback| with
  // the result. The callback's arguments may be nullptr on failure.
  using LoadProfileAndAppCallback = base::OnceCallback<void(Profile*)>;
  void LoadProfileAndApp(const base::FilePath& profile_path,
                         const web_app::AppId& app_id,
                         LoadProfileAndAppCallback callback);
  void LoadProfileAndApp_OnProfileLoaded(const base::FilePath& profile_path,
                                         const web_app::AppId& app_id,
                                         LoadProfileAndAppCallback callback,
                                         Profile* profile);
  void LoadProfileAndApp_OnProfileAppRegistryReady(
      const base::FilePath& profile_path,
      const web_app::AppId& app_id,
      LoadProfileAndAppCallback callback);
  void LoadProfileAndApp_OnAppEnabled(const base::FilePath& profile_path,
                                      const web_app::AppId& app_id,
                                      LoadProfileAndAppCallback callback);

  // Update the profiles menu for the specified host.
  void UpdateAppProfileMenu(AppState* app_state);

  std::unique_ptr<Delegate> delegate_;

  // Retrieve the ProfileState for a given (Profile, AppId) pair. If one
  // does not exist, create one.
  ProfileState* GetOrCreateProfileState(Profile* profile,
                                        const web_app::AppId& app_id);

  // Map from extension id to the state for that app.
  std::map<std::string, std::unique_ptr<AppState>> apps_;

  content::NotificationRegistrar registrar_;

  // The avatar menu instance used by all app shims.
  std::unique_ptr<AvatarMenu> avatar_menu_;

  base::WeakPtrFactory<AppShimManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppShimManager);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_MANAGER_MAC_H_
