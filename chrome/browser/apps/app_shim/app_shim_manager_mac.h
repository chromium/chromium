// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_MANAGER_MAC_H_
#define CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_MANAGER_MAC_H_

#include <Security/Security.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "apps/app_lifetime_monitor.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_shim/app_shim_host_bootstrap_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_host_mac.h"
#include "chrome/browser/badging/badge_manager.h"
#include "chrome/browser/profiles/avatar_menu_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/common/mac/app_shim.mojom.h"
#include "chrome/services/mac_notifications/public/mojom/mac_notifications.mojom.h"
#include "components/webapps/common/web_app_id.h"

class Profile;
class ProfileManager;

namespace base {
class FilePath;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace apps {

// The passed in `callback` will be called when all launches for the next app
// shim launch have completed (all profiles the app will launch in, as well
// as possibly multiple windows within profiles).
void SetMacShimStartupDoneCallbackForTesting(base::OnceClosure callback);

// Returns the callback set with SetMacShimStartupDoneCallbackForTesting;
base::OnceClosure TakeShimStartupDoneCallbackForTesting();

// This app shim handler that handles events for app shims that correspond to an
// extension.
class AppShimManager
    : public AppShimHostBootstrap::Client,
      public AppShimHost::Client,
      public AppLifetimeMonitor::Observer,
      public BrowserListObserver,
      public AvatarMenuObserver,
      public ProfileManagerObserver,
      public ProfileObserver,
      public mac_notifications::mojom::MacNotificationProvider,
      public mac_notifications::mojom::MacNotificationActionHandler {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Show all app windows (for non-PWA apps). Return true if there existed any
    // windows.
    virtual bool ShowAppWindows(Profile* profile,
                                const webapps::AppId& app_id) = 0;

    // Close all app windows (for non-PWA apps).
    virtual void CloseAppWindows(Profile* profile,
                                 const webapps::AppId& app_id) = 0;

    // Return true iff |app_id| corresponds to an app that is installed for
    // |profile|. Note that |profile| may be nullptr (in which case it should
    // always return false).
    virtual bool AppIsInstalled(Profile* profile,
                                const webapps::AppId& app_id) = 0;

    // Return true iff the specified app can create an AppShimHost, which will
    // keep the app shim process connected (as opposed to, e.g, a bookmark app
    // that opens in a tab, which will immediately close).
    virtual bool AppCanCreateHost(Profile* profile,
                                  const webapps::AppId& app_id) = 0;

    // Return true if Cocoa windows for this app should be hosted in the app
    // shim process.
    virtual bool AppUsesRemoteCocoa(Profile* profile,
                                    const webapps::AppId& app_id) = 0;

    // Return true if a single app shim is used for all profiles (as opposed to
    // one shim per profile).
    virtual bool AppIsMultiProfile(Profile* profile,
                                   const webapps::AppId& app_id) = 0;

    // Open a dialog to enable the specified extension. Call |callback| after
    // the dialog is executed.
    virtual void EnableExtension(Profile* profile,
                                 const std::string& extension_id,
                                 base::OnceCallback<void()> callback) = 0;

    // Launch the app in Chrome. This will (often) create a new window. It is
    // guaranteed that |app_id| is installed for |profile| when this method
    // is called.
    virtual void LaunchApp(
        Profile* profile,
        const webapps::AppId& app_id,
        const std::vector<base::FilePath>& files,
        const std::vector<GURL>& urls,
        const GURL& override_url,
        chrome::mojom::AppShimLoginItemRestoreState login_item_restore_state,
        base::OnceClosure launch_finished_callback) = 0;

    // Launch the shim process for an app. It is guaranteed that |app_id| is
    // installed for |profile| when this method is called.
    virtual void LaunchShim(Profile* profile,
                            const webapps::AppId& app_id,
                            web_app::LaunchShimUpdateBehavior update_behavior,
                            web_app::ShimLaunchMode launch_mode,
                            ShimLaunchedCallback launched_callback,
                            ShimTerminatedCallback terminated_callback) = 0;

    // Return true if any app windows are open. This is eventually invoked
    // by MaybeTerminate. It does not apply to bookmark apps.
    virtual bool HasNonBookmarkAppWindowsOpen() = 0;

    virtual std::vector<chrome::mojom::ApplicationDockMenuItemPtr>
    GetAppShortcutsMenuItemInfos(Profile* profile,
                                 const webapps::AppId& app_id) = 0;
  };

  // Helper function to get the instance on the browser process. This will be
  // non-null except for tests.
  static AppShimManager* Get();

  explicit AppShimManager(std::unique_ptr<Delegate> delegate);
  AppShimManager(const AppShimManager&) = delete;
  AppShimManager& operator=(const AppShimManager&) = delete;
  ~AppShimManager() override;

  // Get the host corresponding to a profile and app id, or null if there is
  // none.
  AppShimHost* FindHost(Profile* profile, const webapps::AppId& app_id);

  // If the specified |browser| should be using RemoteCocoa (because it is a
  // bookmark app), then get or create an AppShimHost for it, and return
  // it. If an AppShimHost had to be created (e.g, because the app process is
  // still launching), create one, which will bind to the app process when it
  // finishes launching.
  AppShimHost* GetHostForRemoteCocoaBrowser(Browser* browser);

  // Returns true if the specified `browser` should be using RemoteCocoa. This
  // is equivalent to `GetHostForRemoteCocoaBrowser` return a non-null value,
  // except that this method does not cause an AppShimHost to be created.
  bool BrowserUsesRemoteCocoa(Browser* browser);

  // Return true if any non-bookmark app windows open.
  bool HasNonBookmarkAppWindowsOpen();

  // Called when the launch of the app was cancelled by the user. For example,
  // if the user clicks cancel during a protocol launch.
  void OnAppLaunchCancelled(content::BrowserContext* context,
                            const std::string& app_id);

  void UpdateAppBadge(
      Profile* profile,
      const webapps::AppId& app_id,
      const std::optional<badging::BadgeManager::BadgeValue>& badge);

  // Called to connect to a MacNotificationProvider instance in the app shim
  // process for the given app_id. This is only supported for multi-profile
  // app shims; but only legacy platform apps would use single-profile shims
  // anyway.
  // If there is no running app shim matching `app_id`, currently this method
  // instead returns a remote connected to a dummy notification provider. In
  // the future this will instead launch an app shim for `app_id` and connect
  // to that.
  mojo::Remote<mac_notifications::mojom::MacNotificationProvider>
  LaunchNotificationProvider(const webapps::AppId& app_id);

  // Triggers an OS-level notification permission request prompt to be shown by
  // the app shim corresponding to `app_id`. Returns the current state without
  // showing a prompt if permission has already been granted and/or denied to
  // the app shim.
  using RequestNotificationPermissionCallback =
      chrome::mojom::AppShim::RequestNotificationPermissionCallback;
  void ShowNotificationPermissionRequest(
      const webapps::AppId& app_id,
      RequestNotificationPermissionCallback callback);

  // Causes ShowNotificationPermissionRequest() to immediately call its callback
  // with the given `result`, rather than trying to request permission from the
  // app shim.
  void SetNotificationPermissionResponseForTesting(
      mac_notifications::mojom::RequestPermissionResult result) {
    notification_permission_result_for_testing_ = result;
  }

  // Opens the given app in the given profile in response to the user picking
  // said profile in the Profiles menu.
  void LaunchAppInProfile(const webapps::AppId& app_id,
                          const base::FilePath& profile_path);

  // AppShimHostBootstrap::Client:
  void OnShimProcessConnected(
      std::unique_ptr<AppShimHostBootstrap> bootstrap) override;

  // AppShimHost::Client:
  void OnShimLaunchRequested(
      AppShimHost* host,
      web_app::LaunchShimUpdateBehavior update_behavior,
      web_app::ShimLaunchMode launch_mode,
      base::OnceCallback<void(base::Process)> launched_callback,
      base::OnceClosure terminated_callback) override;
  void OnShimProcessDisconnected(AppShimHost* host) override;
  void OnShimFocus(AppShimHost* host) override;
  void OnShimReopen(AppShimHost* host) override;
  void OnShimOpenedFiles(AppShimHost* host,
                         const std::vector<base::FilePath>& files) override;
  void OnShimSelectedProfile(AppShimHost* host,
                             const base::FilePath& profile_path) override;
  void OnShimOpenedAppSettings(AppShimHost* host) override;
  void OnShimOpenedUrls(AppShimHost* host,
                        const std::vector<GURL>& urls) override;
  void OnShimOpenAppWithOverrideUrl(AppShimHost* host,
                                    const GURL& override_url) override;
  void OnShimWillTerminate(AppShimHost* host) override;
  void OnNotificationPermissionStatusChanged(
      AppShimHost* host,
      mac_notifications::mojom::PermissionStatus status) override;

  // AppLifetimeMonitor::Observer overrides:
  void OnAppStart(content::BrowserContext* context,
                  const std::string& app_id) override;
  void OnAppActivated(content::BrowserContext* context,
                      const std::string& app_id) override;
  void OnAppDeactivated(content::BrowserContext* context,
                        const std::string& app_id) override;
  void OnAppStop(content::BrowserContext* context,
                 const std::string& app_id) override;

  // ProfileManagerObserver overrides:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileMarkedForPermanentDeletion(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  // BrowserListObserver overrides:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;
  void OnBrowserSetLastActive(Browser* browser) override;

  // ProfileObserver overrides:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // AvatarMenuObserver:
  void OnAvatarMenuChanged(AvatarMenu* menu) override;

  static base::apple::ScopedCFTypeRef<CFStringRef>
      BuildAppShimRequirementStringFromFrameworkRequirementString(CFStringRef);

  class AppShimObserver {
   public:
    virtual void OnShimProcessConnected(base::ProcessId pid) {}
    virtual void OnShimProcessConnectedAndAllLaunchesDone(
        base::ProcessId pid,
        chrome::mojom::AppShimLaunchResult result) {}
    virtual void OnShimReopen(base::ProcessId pid) {}
    virtual void OnShimOpenedURLs(base::ProcessId pid) {}
    // If this is overridden to return false, the regular notification action
    // code path is bypassed.
    virtual bool OnNotificationAction(
        mac_notifications::mojom::NotificationActionInfoPtr& info);
  };
  void SetAppShimObserverForTesting(AppShimObserver* observer) {
    app_shim_observer_ = observer;
  }

  // Simulates a launch as triggered by an app shim for the specific `app_id`.
  void LoadAndLaunchAppForTesting(const webapps::AppId& app_id);

 protected:
  typedef std::set<Browser*> BrowserSet;

  // Virtual for tests.
  virtual bool IsAcceptablyCodeSigned(audit_token_t audit_token) const;

  // Return the profile for |path|, only if it is already loaded.
  virtual Profile* ProfileForPath(const base::FilePath& path);

  // Return a profile to use for a background shim launch, virtual for tests.
  virtual Profile* ProfileForBackgroundShimLaunch(const webapps::AppId& app_id);

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
      const webapps::AppId& app_id,
      bool use_remote_cocoa);

  // Open the specified URL in a new Chrome window. This is the fallback when
  // an app shim exists, but there is no profile or extension for it. If
  // |profile_path| is specified, then that profile is preferred, otherwise,
  // the last used profile is used.
  virtual void OpenAppURLInBrowserWindow(const base::FilePath& profile_path,
                                         const GURL& url);

  // Launch the user manager (in response to attempting to access a locked
  // profile).
  virtual void LaunchProfilePicker();

  // Terminate Chrome if Chrome attempted to quit, but was prevented from
  // quitting due to apps being open.
  virtual void MaybeTerminate();

  // Called when profile menu items may have changed. Rebuilds the profile
  // menu item list and sends updated lists to all apps.
  void UpdateAllProfileMenus();

  // Update |profile_menu_items_| from |avatar_menu_|. Virtual for tests.
  virtual void RebuildProfileMenuItemsFromAvatarMenu();

  // The list of all profiles that might appear in the menu.
  std::vector<chrome::mojom::ProfileMenuItemPtr> profile_menu_items_;

 private:
  friend class ScopedAppShimKeepAlive;

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
  // - If `params.files` is non-empty, then will always launch the app
  //   - If `profile_path` is non-empty, then use that profile.
  //   - In the most recently used profile, otherwise
  // - If `params.files` is empty, then launch the app only if:
  //   - If `profile_path` is non-empty, then launch if the app is not running
  //     in that profile.
  //   - Otherwise, launch the app only if it is not running any profile.
  using LoadAndLaunchAppCallback =
      base::OnceCallback<void(ProfileState* profile_state,
                              chrome::mojom::AppShimLaunchResult result)>;
  struct LoadAndLaunchAppParams {
    LoadAndLaunchAppParams();
    ~LoadAndLaunchAppParams();
    LoadAndLaunchAppParams(const LoadAndLaunchAppParams&);
    LoadAndLaunchAppParams& operator=(const LoadAndLaunchAppParams&);

    // Return true if `files` or `urls` is non-empty. If so, then this launch
    // will open exactly one window.
    bool HasFilesOrURLs() const;

    webapps::AppId app_id;
    std::vector<base::FilePath> files;
    std::vector<GURL> urls;
    GURL override_url;
    chrome::mojom::AppShimLoginItemRestoreState login_item_restore_state =
        chrome::mojom::AppShimLoginItemRestoreState::kNone;
  };
  void LoadAndLaunchApp(const base::FilePath& profile_path,
                        const LoadAndLaunchAppParams& params,
                        LoadAndLaunchAppCallback launch_callback);
  bool LoadAndLaunchApp_TryExistingProfileStates(
      const base::FilePath& profile_path,
      const LoadAndLaunchAppParams& params,
      const std::map<base::FilePath, int>& profiles_with_handlers,
      LoadAndLaunchAppCallback* launch_callback);
  void LoadAndLaunchApp_OnProfilesAndAppReady(
      const std::vector<base::FilePath>& profile_paths_to_launch,
      bool first_profile_is_from_bootstrap,
      const LoadAndLaunchAppParams& params,
      LoadAndLaunchAppCallback launch_callback);
  void LoadAndLaunchApp_LaunchIfAppropriate(
      Profile* profile,
      ProfileState* profile_state,
      const LoadAndLaunchAppParams& params,
      base::OnceClosure launch_finished_callback);

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
                         const webapps::AppId& app_id,
                         LoadProfileAndAppCallback callback);
  void LoadProfileAndApp_OnProfileLoaded(const base::FilePath& profile_path,
                                         const webapps::AppId& app_id,
                                         LoadProfileAndAppCallback callback,
                                         Profile* profile);
  void LoadProfileAndApp_OnProfileAppRegistryReady(
      const base::FilePath& profile_path,
      const webapps::AppId& app_id,
      LoadProfileAndAppCallback callback);
  void LoadProfileAndApp_OnAppEnabled(const base::FilePath& profile_path,
                                      const webapps::AppId& app_id,
                                      LoadProfileAndAppCallback callback);

  // Update the profiles menu for the specified host.
  void UpdateAppProfileMenu(AppState* app_state);

  // Update the application dock menu for the specified host.
  void UpdateApplicationDockMenu(Profile* profile, ProfileState* profile_state);

  // Updates the badge for the specified host.
  void UpdateApplicationBadge(ProfileState* profile_state);

  // Retrieve the ProfileState for a given (Profile, AppId) pair. If one
  // does not exist, create one.
  ProfileState* GetOrCreateProfileState(Profile* profile,
                                        const webapps::AppId& app_id);

  // Launches a shim for `app_id` in background mode (i.e. without being shown
  // in the Dock and other UI surfaces). Can call `callback` with nullptr if the
  // `app_id` is invalid (for example not installed locally in any profile). If
  // the launch itself fails, this will still call `callback` with a valid
  // AppShimHost, but a mojo connection to the app shim will never be
  // established (and any calls that were made to the remote app shim will be
  // dropped).
  void LaunchShimInBackgroundMode(
      const webapps::AppId& app_id,
      base::OnceCallback<void(AppShimHost*)> callback);

  // Returns a mapping of profile paths to how many of the files and urls passed
  // in in `params` each profile can handle.
  static std::map<base::FilePath, int> GetProfilesWithMatchingHandlers(
      const LoadAndLaunchAppParams& params);

  // mac_notifications::mojom::MacNotificationProvider:
  void BindNotificationService(
      mojo::PendingReceiver<mac_notifications::mojom::MacNotificationService>
          service,
      mojo::PendingRemote<
          mac_notifications::mojom::MacNotificationActionHandler> handler)
      override;

  // mac_notifications::mojom::MacNotificationActionHandler:
  void OnNotificationAction(
      mac_notifications::mojom::NotificationActionInfoPtr info) override;

  std::unique_ptr<Delegate> delegate_;

  // Weak, reset during OnProfileManagerDestroying.
  raw_ptr<ProfileManager> profile_manager_ = nullptr;

  // Map from extension id to the state for that app.
  std::map<std::string, std::unique_ptr<AppState>> apps_;

  // The avatar menu instance used by all app shims.
  std::unique_ptr<AvatarMenu> avatar_menu_;

  // Requests for MacNotificationProviders that can't be connected to the
  // correct app shim process right away get added to this receiver set
  // instead. This is needed because higher level notifications code currently
  // always expects to get a connected MacNotificationProvider remote.
  mojo::ReceiverSet<mac_notifications::mojom::MacNotificationProvider>
      dummy_notification_provider_receivers_;

  // Notification actions from all app shims are routed through these receivers
  // and this class to make sure notification actions can be handled even if the
  // browser process has never tried to connect to the notification service
  // in an app shim.
  mojo::ReceiverSet<mac_notifications::mojom::MacNotificationActionHandler,
                    webapps::AppId>
      notification_action_handler_receivers_;

  // This contains `AppShimHostBootstrap` instances, keyed by the `ReceiverId`
  // for the corresponding `MacNotificationActionHandler` receiver in
  // `notification_action_handler_receivers_`, for app shims that were launched
  // by the OS to handle notification actions.
  std::map<mojo::ReceiverId, std::unique_ptr<AppShimHostBootstrap>>
      bootstraps_pending_notification_actions_;

  // Set in some tests to short-circuit ShowNotificationPermissionRequest.
  std::optional<mac_notifications::mojom::RequestPermissionResult>
      notification_permission_result_for_testing_;

  raw_ptr<AppShimObserver> app_shim_observer_ = nullptr;

  base::ScopedMultiSourceObservation<Profile, ProfileObserver>
      profile_observation_{this};

  base::WeakPtrFactory<AppShimManager> weak_factory_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_MANAGER_MAC_H_
