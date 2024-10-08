// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_MANAGER_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_MANAGER_H_

#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ash/crosapi/browser_action_queue.h"
#include "chrome/browser/ash/crosapi/browser_launcher.h"
#include "chrome/browser/ash/crosapi/browser_manager_feature.h"
#include "chrome/browser/ash/crosapi/browser_manager_observer.h"
#include "chrome/browser/ash/crosapi/browser_manager_scoped_keep_alive.h"
#include "chrome/browser/ash/crosapi/browser_service_host_observer.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/browser_version_service_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_id.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/standalone_browser/lacros_selection.h"
#include "chromeos/crosapi/mojom/browser_service.mojom.h"
#include "chromeos/crosapi/mojom/desk_template.mojom.h"
#include "components/component_updater/component_updater_service.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler_observer.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/component_cloud_policy_service_observer.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/values_util.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/tab_groups/tab_group_info.h"
#include "components/user_manager/user_manager.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/base/ui_base_types.h"

namespace component_updater {
class ComponentManagerAsh;
}  // namespace component_updater

namespace apps {
class AppServiceProxyAsh;
class StandaloneBrowserExtensionApps;
}  // namespace apps

namespace ash {
class ApkWebAppService;
namespace login {
class SecurityTokenSessionController;
}
}  // namespace ash

namespace drive {
class DriveIntegrationService;
}

namespace extensions {
class AutotestPrivateGetLacrosInfoFunction;
}

namespace policy {
class CloudPolicyCore;
}

namespace user_manager {
class DeviceOwnershipWaiter;
}  // namespace user_manager

namespace crosapi {

namespace mojom {
enum class CreationResult;
class Crosapi;
}  // namespace mojom

class BrowserAction;
class BrowserLoader;
class FilesAppLauncher;
class PersistentForcedExtensionKeepAlive;
class TestMojoConnectionManager;

using ash::standalone_browser::LacrosSelection;
using component_updater::ComponentUpdateService;

// Manages the lifetime of lacros-chrome, and its loading status. Observes the
// component updater for future updates. This class is a part of ash-chrome.
class BrowserManager : public session_manager::SessionManagerObserver,
                       public BrowserServiceHostObserver,
                       public policy::CloudPolicyCore::Observer,
                       public policy::CloudPolicyStore::Observer,
                       public policy::ComponentCloudPolicyServiceObserver,
                       public policy::CloudPolicyRefreshSchedulerObserver,
                       public user_manager::UserManager::Observer {
 public:
  // Static getter of BrowserManager instance. In real use cases,
  // BrowserManager instance should be unique in the process.
  static BrowserManager* Get();

  explicit BrowserManager(
      scoped_refptr<component_updater::ComponentManagerAsh> manager);
  // Constructor for testing.
  BrowserManager(std::unique_ptr<BrowserLoader> browser_loader,
                 ComponentUpdateService* update_service);

  BrowserManager(const BrowserManager&) = delete;
  BrowserManager& operator=(const BrowserManager&) = delete;

  ~BrowserManager() override;

  // Returns true if Lacros is in running state.
  // Virtual for testing.
  virtual bool IsRunning() const;

  // Return true if Lacros is running, launching or terminating.
  // We do not want the multi-signin to be available when Lacros is
  // running; therefore, we also have to exclude other states (e.g. if
  // Lacros is launched and multi-signin is enabled, we would have
  // Lacros running and multiple users signed in simultaneously).
  // Virtual for testing.
  virtual bool IsRunningOrWillRun() const;

  bool IsInitialized() const;

  // Returns true if Lacros is terminated.
  bool IsTerminated() const { return is_terminated_; }

  // Opens the browser window in lacros-chrome.
  // If lacros-chrome is not yet launched, it triggers to launch.
  // This needs to be called after loading.
  // TODO(crbug.com/40703695): Notify callers the result of opening window
  // request. Because of asynchronous operations crossing processes,
  // there's no guarantee that the opening window request succeeds.
  // Currently, its condition and result are completely hidden behind this
  // class, so there's no way for callers to handle such error cases properly.
  // This design often leads the flakiness behavior of the product and testing,
  // so should be avoided.
  // If `should_trigger_session_restore` is true, a new window opening should be
  // treated like the start of a new session (with potential session restore,
  // startup URLs, etc). Otherwise, don't restore the session and instead open a
  // new window with the default blank tab.
  void NewWindow(bool incognito, bool should_trigger_session_restore);

  // Performs a full restore of the lacros browser. This must be done after
  // Lacros has been launched from a background state. If `skip_crash_restore`
  // is true lacros will perform a full restore and skip any restore prompts.
  void OpenForFullRestore(bool skip_crash_restore);

  // Returns true if crosapi interface supports NewWindowForDetachingTab API.
  bool NewWindowForDetachingTabSupported() const;

  // NOTE on callbacks:
  // An action's callback (e.g. the last parameter to NewWindowForDetachingTab
  // below) will never be invoked with a CreationResult value of
  // kBrowserShutdown. In the case of a Lacros shutdown (rather than system
  // shutdown), BrowserManager will try to perform the action again later.

  using NewWindowForDetachingTabCallback =
      base::OnceCallback<void(crosapi::mojom::CreationResult,
                              const std::string&)>;
  // Opens a new window in the browser and transfers the given tab (or group)
  // to it.
  // NOTE: This method is used by Chrome OS WebUI in tablet mode as a response
  // to a drag'n drop operation from the user.
  void NewWindowForDetachingTab(const std::u16string& tab_id,
                                const std::u16string& group_id,
                                NewWindowForDetachingTabCallback closure);

  // Returns true if crosapi interface supports NewFullscreenWindow API.
  bool NewFullscreenWindowSupported() const;

  using NewFullscreenWindowCallback =
      base::OnceCallback<void(crosapi::mojom::CreationResult)>;
  // Open a fullscreen browser window in lacros-chrome. The only tab will be
  // navigated to the given `url` once the window is launched.
  // NOTE: This method is used by Chrome OS web Kiosk session only. The behavior
  // may change and it shouldn't be used by anybody else.
  // Virtual for testing.
  virtual void NewFullscreenWindow(const GURL& url,
                                   NewFullscreenWindowCallback callback);

  // Opens a new window in lacros-chrome with the Guest profile if the Guest
  // mode is enabled.
  void NewGuestWindow();

  // Similar to NewWindow(), but opens a tab instead if there already is a
  // window. See crosapi::mojom::BrowserService::NewTab for more details.
  void NewTab();

  // Similar to NewWindow and NewTab. If a suitable window exists, a new tab is
  // added. Otherwise a new window is created with session restore (no new tab
  // is added to that).
  void Launch();

  // Opens the specified URL in lacros-chrome. If it is not running,
  // it launches lacros-chrome with the given URL.
  // See crosapi::mojom::BrowserService::OpenUrl for more details.
  void OpenUrl(
      const GURL& url,
      crosapi::mojom::OpenUrlFrom from,
      crosapi::mojom::OpenUrlParams::WindowOpenDisposition disposition,
      NavigateParams::PathBehavior path_behavior = NavigateParams::RESPECT);

  // Opens the captive portal signin window in lacros-chrome.
  void OpenCaptivePortalSignin(const GURL& url);

  // If there's already a tab opening the URL in lacros-chrome, in some window
  // of the primary profile, activate the tab. Otherwise, opens a tab for
  // the given URL. `path_behavior` will be assigned to the variable of the same
  // name in the `NavigateParams` struct that's used to perform the actual
  // navigation downstream.
  void SwitchToTab(const GURL& url, NavigateParams::PathBehavior path_behavior);

  // Similar to NewWindow(), but restores a tab recently closed.
  // See crosapi::mojom::BrowserService::RestoreTab for more details
  void RestoreTab();

  // Triggers tab switching in Lacros via horizontal 3-finger swipes.
  //
  // |x_offset| is in DIP coordinates.
  void HandleTabScrubbing(float x_offset, bool is_fling_scroll_event);

  // Create a browser with the restored data containing `urls`,
  // `bounds`,`tab_group_infos`, `show_state`, `active_tab_index`,
  // `first_non_pinned_tab_index`, and `app_name`. Note an non-empty `app_name`
  // indicates that the browser window is an app type browser window.  Also
  // note that` first_non_pinned_tab_indexes` with negative values are ignored
  // type constraints for the `first_non_pinned_tab_index` and are enforced on
  // the browser side and are dropped if they don't comply with said restraints.
  void CreateBrowserWithRestoredData(
      const std::vector<GURL>& urls,
      const gfx::Rect& bounds,
      const std::vector<tab_groups::TabGroupInfo>& tab_group_infos,
      const ui::mojom::WindowShowState show_state,
      int32_t active_tab_index,
      int32_t first_non_pinned_tab_index,
      const std::string& app_name,
      int32_t restore_window_id,
      uint64_t lacros_profile_id);

  // Opens the profile manager window in lacros-chrome.
  void OpenProfileManager();

  // Ensures Lacros launches.
  // Returns true if Lacros could be launched, or is already in the
  // process of launching. Returns false if Lacros could not be launched.
  // NOTE: this method requires the user profile to be already initialized.
  bool EnsureLaunch();

  // Initialize resources and start Lacros.
  //
  // NOTE: If InitializeAndStartIfNeeded finds Lacros disabled, it unloads
  // Lacros via BrowserLoader::Unload, which also deletes the user data
  // directory.
  virtual void InitializeAndStartIfNeeded();

  // Returns true if keep-alive is enabled.
  bool IsKeepAliveEnabled() const;

  using GetFeedbackDataCallback = base::OnceCallback<void(base::Value::Dict)>;
  // Gathers Lacros feedback data.
  // Virtual for testing.
  virtual void GetFeedbackData(GetFeedbackDataCallback callback);

  // Returns true if crosapi interface supports GetHistograms API.
  bool GetHistogramsSupported() const;

  using GetHistogramsCallback = base::OnceCallback<void(const std::string&)>;
  // Gets Lacros histograms.
  void GetHistograms(GetHistogramsCallback callback);

  // Returns true if crosapi interface supports GetActiveTabUrl API.
  bool GetActiveTabUrlSupported() const;

  using GetActiveTabUrlCallback =
      base::OnceCallback<void(const std::optional<GURL>&)>;
  // Gets Url of the active tab from lacros if there is any.
  void GetActiveTabUrl(GetActiveTabUrlCallback callback);

  using GetBrowserInformationCallback =
      base::OnceCallback<void(crosapi::mojom::DeskTemplateStatePtr)>;
  // Gets URLs and active indices of the tab strip models from the Lacros
  // browser window.
  // Virtual for testing.
  virtual void GetBrowserInformation(const std::string& window_unique_id,
                                     GetBrowserInformationCallback callback);

  void AddObserver(BrowserManagerObserver* observer);
  void RemoveObserver(BrowserManagerObserver* observer);

  const std::string& browser_version() const { return browser_version_; }
  void set_browser_version(const std::string& version) {
    browser_version_ = version;
  }

  const base::FilePath& lacros_path() const { return lacros_path_; }

  // Set the data of device account policy. It is the serialized blob of
  // PolicyFetchResponse received from the server, or parsed from the file after
  // is was validated by Ash.
  void SetDeviceAccountPolicy(const std::string& policy_blob);

  // Notifies the BrowserManager that it should prepare for shutdown. This is
  // called in the early stages of ash shutdown to give Lacros sufficient time
  // for a graceful exit.
  void Shutdown();

  const BrowserVersionServiceAsh::Delegate* version_service_delegate() const {
    return version_service_delegate_.get();
  }
  void set_version_service_delegate_for_testing(
      std::unique_ptr<BrowserVersionServiceAsh::Delegate>
          version_service_delegate) {
    version_service_delegate_ = std::move(version_service_delegate);
  }

  // TODO(crbug.com/40275396): Remove this once we refactored to use the
  // constructor.
  void set_device_ownership_waiter_for_testing(
      std::unique_ptr<user_manager::DeviceOwnershipWaiter>
          device_ownership_waiter);

  void set_relaunch_requested_for_testing(bool relaunch_requested);

  // Disable most of BrowserManager's functionality such that it never tries to
  // launch Lacros. This is used e.g. by test_ash_chrome.
  static void DisableForTesting();
  static void EnableForTesting();

  void KillLacrosForTesting();

 protected:
  // The actual Lacros launch mode.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class LacrosLaunchMode {
    // Indicates that Lacros is disabled.
    kLacrosDisabled = 0,
    // Lacros is the only browser and Ash is disabled.
    kLacrosOnly = 3,

    kMaxValue = kLacrosOnly
  };

  // The actual Lacros launch mode.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class LacrosLaunchModeAndSource {
    // Either set by user or system/flags, indicates that Lacros is disabled.
    kPossiblySetByUserLacrosDisabled = 0,
    // Either set by user or system/flags, Lacros is the only browser and Ash is
    // disabled.
    kPossiblySetByUserLacrosOnly = 3,
    // Enforced by the user, indicates that Lacros is disabled.
    kForcedByUserLacrosDisabled = 4 + kPossiblySetByUserLacrosDisabled,
    // Enforced by the user, Lacros is the only browser and Ash is disabled.
    kForcedByUserLacrosOnly = 4 + kPossiblySetByUserLacrosOnly,
    // Enforced by policy, indicates that Lacros is disabled.
    kForcedByPolicyLacrosDisabled = 8 + kPossiblySetByUserLacrosDisabled,
    // Enforced by policy, Lacros is the only browser and Ash is disabled.
    kForcedByPolicyLacrosOnly = 8 + kPossiblySetByUserLacrosOnly,

    kMaxValue = kForcedByPolicyLacrosOnly
  };

  // NOTE: You may have to update tests if you make changes to State, as state_
  // is exposed via autotest_private.
  enum class State {
    // Lacros is not initialized yet.
    // Lacros-chrome loading depends on user type, so it needs to wait
    // for user session.
    NOT_INITIALIZED,

    // User session started, and now it's mounting lacros-chrome.
    MOUNTING,

    // Lacros-chrome is unavailable. I.e., failed to load for some reason
    // or disabled.
    UNAVAILABLE,

    // Lacros-chrome is loaded and ready for launching.
    STOPPED,

    // Params for lacros-chrome are parepared on a background thread.
    PREPARING_FOR_LAUNCH,

    // Lacros-chrome is launching.
    STARTING,

    // Mojo connection to lacros-chrome is established so, it's in
    // the running state.
    RUNNING,

    // Following two states represent the Lacros-chrome termination related
    // state. There are two types of Lacros-chrome termination. It proceeds in
    // the following ways from Ash perspective.
    //
    // Ash initiated termination:
    // 1. Ash requests Lacros to terminate.
    // 2. Wait for mojo disconnection. (NOTE: If Ash is shutdown, we skip this
    // step since the main message loop may be stopped before receiving mojo
    // disconnection.)
    // 3. Wait for the process to be terminated.
    // The state should be set to WAITING_FOR_MOJO_DISCONNECTED on 1, set to
    // WAITING_FOR_PROCESS_TERMINATED on 2 completed and then move forward to
    // the next task scheduled after the termination on 3 completed.
    // If Ash is shutdown, we skip the step 2, so we immediately enter
    // WAITING_FOR_PROCESS_TERMINATED state on 1.
    //
    // Lacros initiated termination:
    // 1. Ash receives mojo disconnection.
    // 2. Wait for the process to be terminated
    //
    // Lacros-chrome is requested to terminate from Ash.
    WAITING_FOR_MOJO_DISCONNECTED,

    // Mojo connection is disconnected and Lacros-chrome is being terminated
    // soon.
    // Waiting for the process to be terminated. This is usually set after
    // WAITING_FOR_MOJO_DISCONNECTED on mojo disconnected except for the
    // scenario when Ash is shutdown, in other word, when Ash skips
    // WAITING_FOR_MOJO_DISCONNECTED phase.
    WAITING_FOR_PROCESS_TERMINATED,
  };
  // Changes |state| value and potentially notify observers of the change.
  void SetState(State state);

  // Posts CreateLogFile() and StartWithLogFile() to the thread pool.
  // Also takes care of loading an update first, if available.
  // Virtual for tests.
  virtual void Start();

  // BrowserServiceHostObserver:
  void OnBrowserServiceConnected(CrosapiId id,
                                 mojo::RemoteSetElementId mojo_id,
                                 mojom::BrowserService* browser_service,
                                 uint32_t browser_service_version) override;
  void OnBrowserServiceDisconnected(CrosapiId id,
                                    mojo::RemoteSetElementId mojo_id) override;

  // Called when the Mojo connection to lacros-chrome is disconnected. It may be
  // "just a Mojo error" or "lacros-chrome crash". This method posts a
  // shutdown-blocking async task that waits lacros-chrome to exit, giving it a
  // chance to gracefully exit. The task will send a terminate signal to
  // lacros-chrome if the process has not terminated within the graceful
  // shutdown window.
  void OnMojoDisconnected();

  // Called when lacros-chrome is terminated and successfully wait(2)ed.
  void OnLacrosChromeTerminated();

  // ID for the current Crosapi connection.
  // Available only when lacros-chrome is running.
  std::optional<CrosapiId> crosapi_id_;

  // Proxy to BrowserService mojo service in lacros-chrome.
  // Available only when lacros-chrome is running.
  struct BrowserServiceInfo {
    BrowserServiceInfo(mojo::RemoteSetElementId mojo_id,
                       mojom::BrowserService* service,
                       uint32_t interface_version);
    BrowserServiceInfo(const BrowserServiceInfo&);
    BrowserServiceInfo& operator=(const BrowserServiceInfo&);
    ~BrowserServiceInfo();

    // ID managed in BrowserServiceHostAsh, which is tied to the |service|.
    mojo::RemoteSetElementId mojo_id;
    // BrowserService proxy connected to lacros-chrome.
    raw_ptr<mojom::BrowserService, DanglingUntriaged> service;
    // Supported interface version of the BrowserService in Lacros-chrome.
    uint32_t interface_version;
  };
  std::optional<BrowserServiceInfo> browser_service_;

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserManagerTest, LacrosKeepAlive);
  FRIEND_TEST_ALL_PREFIXES(BrowserManagerTest,
                           LacrosKeepAliveReloadsWhenUpdateAvailable);
  FRIEND_TEST_ALL_PREFIXES(BrowserManagerTest,
                           LacrosKeepAliveDoesNotBlockRestart);
  FRIEND_TEST_ALL_PREFIXES(BrowserManagerTest,
                           NewWindowReloadsWhenUpdateAvailable);
  FRIEND_TEST_ALL_PREFIXES(BrowserManagerTest, OnLacrosUserDataDirRemoved);
  friend class apps::StandaloneBrowserExtensionApps;
  friend class BrowserManagerScopedKeepAlive;
  // App service require the lacros-chrome to keep alive for web apps to:
  // 1. Have lacros-chrome running before user open the browser so we can
  //    have web apps info showing on the app list, shelf, etc..
  // 2. Able to interact with web apps (e.g. uninstall) at any time.
  // 3. Have notifications.
  // TODO(crbug.com/40167449): This is a short term solution to integrate
  // web apps in Lacros. Need to decouple the App Platform systems from
  // needing lacros-chrome running all the time.
  friend class apps::AppServiceProxyAsh;
  // TODO(crbug.com/40220252): ApkWebAppService does not yet support app
  // installation when lacros-chrome starts at arbitrary points of time, so it
  // needs to be kept alive.
  friend class ash::ApkWebAppService;
  // Only for exposing state_ to Tast tests.
  friend class extensions::AutotestPrivateGetLacrosInfoFunction;
  // In LacrosOnly mode, certificate provider and smart card connector
  // extensions will be running in Lacros, but policy implementation stays in
  // Ash. Thus, session controller needs to keep Lacros alive to keep track of
  // smart card status.
  friend class ash::login::SecurityTokenSessionController;
  // Registers a KeepAlive if there is a force-installed extension that should
  // always be running.
  friend class PersistentForcedExtensionKeepAlive;
  friend class PersistentForcedExtensionKeepAliveTest;
  // DriveFS requires Lacros to be alive so it can connect to the Docs Offline
  // extension. This allows Files App to make Docs files available offline.
  friend class drive::DriveIntegrationService;

  // Processes the action depending on the current state.
  // Ignoring a few exceptional cases, the logic is as follows:
  // - If Lacros is ready, the action is performed.
  // - If Lacros is not ready and the action is queueable, the action is queued
  //   (and Lacros started if necessary).
  // - Otherwise, the action is cancelled.
  void PerformOrEnqueue(std::unique_ptr<BrowserAction> action);

  void OnActionPerformed(std::unique_ptr<BrowserAction> action, bool retry);

  // Remembers lacros launch mode and migration status by calling
  // `SetLacrosMigrationStatus()` and `SetLacrosLaunchMode()`, then kicks off
  // the daily reporting for the metrics.
  void RecordLacrosLaunchModeAndMigrationStatus();
  // Sets `migration_mode_`.
  void SetLacrosMigrationStatus();
  // Sets `lacros_mode_` and `lacros_mode_and_source_`.
  void SetLacrosLaunchMode();

  using Feature = BrowserManagerFeature;

  // De-registers any already existing KeepAlive features for testing.
  class ScopedUnsetAllKeepAliveForTesting {
   public:
    explicit ScopedUnsetAllKeepAliveForTesting(BrowserManager* manager);
    ~ScopedUnsetAllKeepAliveForTesting();

   private:
    raw_ptr<BrowserManager> manager_;
    std::set<BrowserManager::Feature> previous_keep_alive_features_;
  };

  // Ash features that want Lacros to stay running in the background must be
  // marked as friends of this class so that lacros owners can audit usage.
  std::unique_ptr<BrowserManagerScopedKeepAlive> KeepAlive(Feature feature);

  void StartIfNeeded();

  // This may be called synchronously by the BrowserManager following a
  // Terminate() signal during shutdown, or following a call to
  // OnMojoDisconnected(). This posts a shutdown blocking task that waits for
  // lacros-chrome to cleanly exit for `timeout` duration before forcefully
  // killing the process.
  void EnsureLacrosChromeTermination(base::TimeDelta timeout);

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  // Called on launch process completed.
  void OnLaunchComplete(
      base::expected<BrowserLauncher::LaunchResults,
                     BrowserLauncher::LaunchFailureReason> launch_results);

  // Launch "Go to files" if the migration error page was clicked.
  void HandleGoToFiles();

  // BrowserServiceHostObserver:
  void OnBrowserRelaunchRequested(CrosapiId id) override;

  // CloudPolicyCore::Observer:
  void OnCoreConnected(policy::CloudPolicyCore* core) override;
  void OnRefreshSchedulerStarted(policy::CloudPolicyCore* core) override;
  void OnCoreDisconnecting(policy::CloudPolicyCore* core) override;
  void OnCoreDestruction(policy::CloudPolicyCore* core) override;

  // policy::CloudPolicyStore::Observer:
  void OnStoreLoaded(policy::CloudPolicyStore* store) override;
  void OnStoreError(policy::CloudPolicyStore* store) override;
  void OnStoreDestruction(policy::CloudPolicyStore* store) override;

  // policy::ComponentCloudPolicyService::Observer:
  // Updates the component policy for given namespace. The policy blob is JSON
  // value received from the server, or parsed from the file after is was
  // validated.
  void OnComponentPolicyUpdated(
      const policy::ComponentPolicyMap& component_policy) override;
  void OnComponentPolicyServiceDestruction(
      policy::ComponentCloudPolicyService* service) override;

  // policy::CloudPolicyRefreshScheduler::Observer:
  void OnFetchAttempt(policy::CloudPolicyRefreshScheduler* scheduler) override;
  void OnRefreshSchedulerDestruction(
      policy::CloudPolicyRefreshScheduler* scheduler) override;

  // user_manager::UserManager::Observer:
  void OnUserProfileCreated(const user_manager::User& user) override;

  // crosapi::BrowserManagerObserver:
  void OnLoadComplete(const base::FilePath& path,
                      LacrosSelection selection,
                      base::Version version);

  // Methods for features to register and de-register for needing to keep Lacros
  // alive.
  void StartKeepAlive(Feature feature);
  void StopKeepAlive(Feature feature);

  // Notifies browser to update its keep-alive status.
  // Disabling keep-alive here may shut down the browser in background.
  // (i.e., if there's no browser window opened, it may be shut down).
  void UpdateKeepAliveInBrowserIfNecessary(bool enabled);

  // Shared implementation of OpenUrl and SwitchToTab.
  void OpenUrlImpl(
      const GURL& url,
      crosapi::mojom::OpenUrlParams::WindowOpenDisposition disposition,
      crosapi::mojom::OpenUrlFrom from,
      NavigateParams::PathBehavior path_behavior);

  // Creates windows from template data.
  void RestoreWindowsFromTemplate();

  // Sending the LaunchMode and MigrationStatus state at least once a day.
  // multiple events will get de-duped on the server side.
  void OnDailyLaunchModeAndMigrationStatusTimer();

  void PerformAction(std::unique_ptr<BrowserAction> action);

  // Start a sequence to clear Lacros related data. It posts a task to remove
  // Lacros user data directory and if that is successful, calls
  // `OnLacrosUserDataDirRemoved()` to clear some prefs set by Lacros in Ash.
  // Call if Lacros is disabled and not running.
  void ClearLacrosData();

  // Called as a callback to `RemoveLacrosUserDataDir()`. `cleared` is set to
  // true if the directory existed and was removed successfully.
  void OnLacrosUserDataDirRemoved(bool cleared);

  base::ObserverList<BrowserManagerObserver> observers_;

  // NOTE: The state is exposed to tests via autotest_private.
  State state_ = State::NOT_INITIALIZED;

  crosapi::BrowserLauncher browser_launcher_;

  std::unique_ptr<crosapi::BrowserLoader> browser_loader_;

  // Delegate handling various concerns regarding the version service.
  std::unique_ptr<BrowserVersionServiceAsh::Delegate> version_service_delegate_;

  // Path to the lacros-chrome disk image directory.
  base::FilePath lacros_path_;

  // Whether we are starting "rootfs" or "stateful" lacros.
  std::optional<LacrosSelection> lacros_selection_;

  // Version of the browser (e.g. lacros-chrome) displayed to user in feedback
  // report, etc. It includes both browser version and channel in the format of:
  // {browser version} {channel}
  // For example, "87.0.0.1 dev", "86.0.4240.38 beta".
  std::string browser_version_;

  // Time when the lacros process was launched.
  base::TimeTicks lacros_launch_time_;

  // Remembers the request from Lacros-chrome whether it needs to be
  // relaunched. Reset on new process start in any cases.
  bool relaunch_requested_ = false;

  // Tracks whether Shutdown() has been signalled by ash. This flag ensures any
  // new or existing lacros startup tasks are not executed during shutdown.
  bool shutdown_requested_ = false;

  // Tracks whether BrowserManager should attempt to load a newer lacros-chrome
  // browser version (if an update is possible and a new version is available).
  // This helps to avoid re-trying an update multiple times should lacros-chrome
  // fail to uprev on a reload.
  bool should_attempt_update_ = true;

  // Tracks whether lacros-chrome is terminated.
  bool is_terminated_ = false;

  // Helps set up and manage the mojo connections between lacros-chrome and
  // ash-chrome in testing environment. Only applicable when
  // '--lacros-mojo-socket-for-testing' is present in the command line.
  std::unique_ptr<TestMojoConnectionManager> test_mojo_connection_manager_;

  // The features that are currently registered to keep Lacros alive.
  std::set<Feature> keep_alive_features_;

  const bool disabled_for_testing_;

  // Used to launch files.app when user clicked "Go to files" on the migration
  // error screen.
  std::unique_ptr<FilesAppLauncher> files_app_launcher_;

  // The queue of actions to be performed when Lacros becomes ready.
  BrowserActionQueue pending_actions_;

  // The timer used to periodically check if the daily event should be
  // triggered.
  base::RepeatingTimer daily_event_timer_;

  // The launch mode and the launch mode with source which were used after
  // deciding if Lacros should be used or not.
  std::optional<LacrosLaunchMode> lacros_mode_;
  std::optional<LacrosLaunchModeAndSource> lacros_mode_and_source_;
  // The migration status used to emit UMA reports.
  std::optional<browser_util::MigrationStatus> migration_status_;

  base::ScopedObservation<user_manager::UserManager,
                          user_manager::UserManager::Observer>
      user_manager_observation_{this};

  base::WeakPtrFactory<BrowserManager> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_MANAGER_H_
