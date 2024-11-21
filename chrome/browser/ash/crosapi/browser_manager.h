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
#include "chrome/browser/ash/crosapi/browser_manager_feature.h"
#include "chrome/browser/ash/crosapi/browser_manager_observer.h"
#include "chrome/browser/ash/crosapi/browser_service_host_observer.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
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

namespace policy {
class CloudPolicyCore;
}

namespace crosapi {

namespace mojom {
enum class CreationResult;
class Crosapi;
}  // namespace mojom

class BrowserAction;
class BrowserLoader;
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
                       public policy::CloudPolicyRefreshSchedulerObserver {
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

  // NOTE on callbacks:
  // An action's callback (e.g. the last parameter to NewWindowForDetachingTab
  // below) will never be invoked with a CreationResult value of
  // kBrowserShutdown. In the case of a Lacros shutdown (rather than system
  // shutdown), BrowserManager will try to perform the action again later.

  // Opens a new window in lacros-chrome with the Guest profile if the Guest
  // mode is enabled.
  void NewGuestWindow();

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

  // Initialize resources and start Lacros.
  //
  // NOTE: If InitializeAndStartIfNeeded finds Lacros disabled, it unloads
  // Lacros via BrowserLoader::Unload, which also deletes the user data
  // directory.
  virtual void InitializeAndStartIfNeeded();

  using GetBrowserInformationCallback =
      base::OnceCallback<void(crosapi::mojom::DeskTemplateStatePtr)>;
  // Gets URLs and active indices of the tab strip models from the Lacros
  // browser window.
  // Virtual for testing.
  virtual void GetBrowserInformation(const std::string& window_unique_id,
                                     GetBrowserInformationCallback callback);

  void AddObserver(BrowserManagerObserver* observer);
  void RemoveObserver(BrowserManagerObserver* observer);

  const base::FilePath& lacros_path() const { return lacros_path_; }

  // Set the data of device account policy. It is the serialized blob of
  // PolicyFetchResponse received from the server, or parsed from the file after
  // is was validated by Ash.
  void SetDeviceAccountPolicy(const std::string& policy_blob);

  // Notifies the BrowserManager that it should prepare for shutdown. This is
  // called in the early stages of ash shutdown to give Lacros sufficient time
  // for a graceful exit.
  void Shutdown();

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

    // Lacros-chrome is unavailable. I.e., failed to load for some reason
    // or disabled.
    UNAVAILABLE,
  };
  // Changes |state| value and potentially notify observers of the change.
  void SetState(State state);

  // BrowserServiceHostObserver:
  void OnBrowserServiceConnected(CrosapiId id,
                                 mojo::RemoteSetElementId mojo_id,
                                 mojom::BrowserService* browser_service,
                                 uint32_t browser_service_version) override;
  void OnBrowserServiceDisconnected(CrosapiId id,
                                    mojo::RemoteSetElementId mojo_id) override;

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

  // Processes the action depending on the current state.
  // Ignoring a few exceptional cases, the logic is as follows:
  // - If Lacros is ready, the action is performed.
  // - If Lacros is not ready and the action is queueable, the action is queued
  //   (and Lacros started if necessary).
  // - Otherwise, the action is cancelled.
  void PerformOrEnqueue(std::unique_ptr<BrowserAction> action);

  void OnActionPerformed(std::unique_ptr<BrowserAction> action, bool retry);

  // Remembers lacros launch mode by calling `SetLacrosLaunchMode()`, then kicks
  // off the daily reporting for the metrics.
  void RecordLacrosLaunchMode();
  // Sets `lacros_mode_` and `lacros_mode_and_source_`.
  void SetLacrosLaunchMode();

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

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

  // Shared implementation of OpenUrl and SwitchToTab.
  void OpenUrlImpl(
      const GURL& url,
      crosapi::mojom::OpenUrlParams::WindowOpenDisposition disposition,
      crosapi::mojom::OpenUrlFrom from,
      NavigateParams::PathBehavior path_behavior);

  // Sending the LaunchMode state at least once a day.
  // multiple events will get de-duped on the server side.
  void OnDailyLaunchModeTimer();

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

  std::unique_ptr<crosapi::BrowserLoader> browser_loader_;

  // Path to the lacros-chrome disk image directory.
  base::FilePath lacros_path_;

  // Time when the lacros process was launched.
  base::TimeTicks lacros_launch_time_;

  // Tracks whether Shutdown() has been signalled by ash. This flag ensures any
  // new or existing lacros startup tasks are not executed during shutdown.
  bool shutdown_requested_ = false;

  // Helps set up and manage the mojo connections between lacros-chrome and
  // ash-chrome in testing environment. Only applicable when
  // '--lacros-mojo-socket-for-testing' is present in the command line.
  std::unique_ptr<TestMojoConnectionManager> test_mojo_connection_manager_;

  // The queue of actions to be performed when Lacros becomes ready.
  BrowserActionQueue pending_actions_;

  // The timer used to periodically check if the daily event should be
  // triggered.
  base::RepeatingTimer daily_event_timer_;

  // The launch mode and the launch mode with source which were used after
  // deciding if Lacros should be used or not.
  std::optional<LacrosLaunchMode> lacros_mode_;
  std::optional<LacrosLaunchModeAndSource> lacros_mode_and_source_;

  base::WeakPtrFactory<BrowserManager> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_MANAGER_H_
