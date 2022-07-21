// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_MANAGER_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/one_shot_event.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_background_task.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate_map.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_type.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_url_loader.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace base {
class Version;
}

namespace content {
class NavigationHandle;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace web_app {
class WebAppUiManager;
class WebAppSyncBridge;
class WebAppPolicyManager;
class WebAppProvider;
}  // namespace web_app

class PrefService;
class Profile;

namespace ash {

// Installs, uninstalls, and updates System Web Apps.
// System Web Apps are built-in, highly-privileged Web Apps for Chrome OS. They
// have access to more APIs and are part of the Chrome OS image. All clients
// should await `on_apps_synchronized()` event to start working with SWAs.
class SystemWebAppManager : public KeyedService,
                            public web_app::WebAppUiManagerObserver {
 public:
  // Policy for when the SystemWebAppManager will update apps/install new apps.
  enum class UpdatePolicy {
    // Update every system start.
    kAlwaysUpdate,
    // Update when the Chrome version number changes.
    kOnVersionChange,
  };

  static constexpr char kInstallResultHistogramName[] =
      "Webapp.InstallResult.System";
  static constexpr char kInstallDurationHistogramName[] =
      "Webapp.SystemApps.FreshInstallDuration";

  // Returns whether the given app type is enabled.
  bool IsAppEnabled(SystemWebAppType type) const;

  explicit SystemWebAppManager(Profile* profile);
  SystemWebAppManager(const SystemWebAppManager&) = delete;
  SystemWebAppManager& operator=(const SystemWebAppManager&) = delete;
  ~SystemWebAppManager() override;

  // On Chrome OS: returns the SystemWebAppManager that hosts System Web Apps in
  // Ash; In Lacros, returns nullptr (unless
  // EnableSystemWebAppInLacrosForTesting). On other platforms, always returns a
  // SystemWebAppManager.
  static SystemWebAppManager* Get(Profile* profile);
  // Gets the associated WebAppProvider for system web apps. `WebAppProvider` is
  // always presented in the `profile` if the `Get` above returns non-nullptr.
  static web_app::WebAppProvider* GetWebAppProvider(Profile* profile);

  // Returns the SystemWebAppManager object for the current process.
  // Avoid using this function where possible and prefer `Get` which guarantees
  // it is being called from the correct process. Only use
  // `GetForLocalAppsUnchecked` if the calling code is shared between Ash/Lacros
  // and expects that some SystemWebAppManager always exists. In Lacros, this
  // function returns an empty SWA manager with no concrete apps.
  static SystemWebAppManager* GetForLocalAppsUnchecked(Profile* profile);

  // Returns the SystemWebAppManager for tests, regardless of whether this is
  // running in Lacros/Ash. Blocks if the web app registry is not yet ready.
  static SystemWebAppManager* GetForTest(Profile* profile);

  void SetSubsystems(
      web_app::ExternallyManagedAppManager* externally_managed_app_manager,
      web_app::WebAppRegistrar* registrar,
      web_app::WebAppSyncBridge* sync_bridge,
      web_app::WebAppUiManager* ui_manager,
      web_app::WebAppPolicyManager* web_app_policy_manager);
  void ConnectSubsystems(web_app::WebAppProvider* provider);
  void ScheduleStart();

  // Gets called when `WebAppProvider` is ready.
  void Start();

  // KeyedService:
  void Shutdown() override;

  // The SystemWebAppManager is disabled in browser tests by default because it
  // pollutes the startup state (several tests expect the Extensions state to be
  // clean).
  //
  // Call this to install apps for SystemWebApp specific tests, e.g if a test
  // needs to open OS Settings.
  //
  // This can also be called multiple times to simulate reinstallation from
  // system restart, e.g.
  void InstallSystemAppsForTesting();

  // Returns the app id for the given System App |type|.
  absl::optional<web_app::AppId> GetAppIdForSystemApp(
      SystemWebAppType type) const;

  // Returns the System App Type for the given |app_id|.
  absl::optional<SystemWebAppType> GetSystemAppTypeForAppId(
      const web_app::AppId& app_id) const;

  // Returns the System App Delegate for the given App |type|.
  const SystemWebAppDelegate* GetSystemApp(SystemWebAppType type) const;

  // Returns the App Ids for all installed System Web Apps.
  std::vector<web_app::AppId> GetAppIds() const;

  // Returns whether |app_id| points to an installed System App.
  bool IsSystemWebApp(const web_app::AppId& app_id) const;

  // Returns the SystemWebAppType that should capture the navigation to
  // |url|.
  absl::optional<SystemWebAppType> GetCapturingSystemAppForURL(
      const GURL& url) const;

  const base::OneShotEvent& on_apps_synchronized() const {
    return *on_apps_synchronized_;
  }

  // Return the OneShotEvent that is fired after all of the background tasks
  // have started and their timers become active.
  const base::OneShotEvent& on_tasks_started() const {
    return *on_tasks_started_;
  }

  // Returns a map of registered system app types and infos, these apps will be
  // installed on the system.
  const SystemWebAppDelegateMap& system_app_delegates() const {
    return system_app_delegates_;
  }

  base::WeakPtr<SystemWebAppManager> GetWeakPtr();

  // This call will override default System Apps configuration. You should call
  // Start() after this call to install |system_apps|.
  void SetSystemAppsForTesting(SystemWebAppDelegateMap system_apps);

  // Overrides the update policy. If AlwaysReinstallSystemWebApps feature is
  // enabled, this method does nothing, and system apps will be reinstalled.
  void SetUpdatePolicyForTesting(UpdatePolicy policy);

  void ResetOnAppsSynchronizedForTesting();

  // Get the timers. Only use this for testing.
  const std::vector<std::unique_ptr<SystemWebAppBackgroundTask>>&
  GetBackgroundTasksForTesting();

  const Profile* profile() const { return profile_; }

 protected:
  virtual const base::Version& CurrentVersion() const;
  virtual const std::string& CurrentLocale() const;

 private:
  // Returns the list of origin trials to enable for |url| loaded in System
  // App |type|. Returns an empty vector if the App does not specify origin
  // trials for |url|.
  const std::vector<std::string>* GetEnabledOriginTrials(
      const SystemWebAppDelegate* system_app,
      const GURL& url) const;

  void StopBackgroundTasks();

  void OnAppsSynchronized(
      bool did_force_install_apps,
      const base::TimeTicks& install_start_time,
      std::map<GURL, web_app::ExternallyManagedAppManager::InstallResult>
          install_results,
      std::map<GURL, bool> uninstall_results);
  bool ShouldForceInstallApps() const;
  void UpdateLastAttemptedInfo();
  // Returns if we have exceeded the number of retry attempts allowed for this
  // version.
  bool CheckAndIncrementRetryAttempts();

  void RecordSystemWebAppInstallResults(
      const std::map<GURL, web_app::ExternallyManagedAppManager::InstallResult>&
          install_results) const;

  void RecordSystemWebAppInstallDuration(
      const base::TimeDelta& time_duration) const;

  void StartBackgroundTasks() const;

  // web_app::WebAppUiManagerObserver:
  void OnReadyToCommitNavigation(
      const web_app::AppId& app_id,
      content::NavigationHandle* navigation_handle) override;
  void OnWebAppUiManagerDestroyed() override;

  void CheckIsConnected() const;
  void ConnectProviderToSystemWebAppDelegateMap(
      const SystemWebAppDelegateMap* system_web_apps_delegate_map) const;

  raw_ptr<Profile> profile_;
  // SystemWebAppManager KeyedService depends on WebAppProvider KeyedService,
  // therefore this pointer is always valid once connected.
  raw_ptr<web_app::WebAppProvider> provider_ = nullptr;

  std::unique_ptr<base::OneShotEvent> on_apps_synchronized_;
  std::unique_ptr<base::OneShotEvent> on_tasks_started_;

  bool shutting_down_ = false;

  std::string install_result_per_profile_histogram_name_;

  UpdatePolicy update_policy_;

  // We skip app installation in tests by default. Tests can trigger
  // installation by calling `InstallSystemAppsForTesting()` or
  // `SetSystemAppsForTesting()`.
  bool skip_app_installation_in_test_ = true;

  SystemWebAppDelegateMap system_app_delegates_;

  const raw_ptr<PrefService> pref_service_;

  // Used to install, uninstall, and update apps. Should outlive this class.
  raw_ptr<web_app::ExternallyManagedAppManager>
      externally_managed_app_manager_ = nullptr;

  raw_ptr<web_app::WebAppRegistrar> registrar_ = nullptr;

  raw_ptr<web_app::WebAppSyncBridge> sync_bridge_ = nullptr;

  raw_ptr<web_app::WebAppPolicyManager> web_app_policy_manager_ = nullptr;

  std::vector<std::unique_ptr<SystemWebAppBackgroundTask>> tasks_;

  base::ScopedObservation<web_app::WebAppUiManager,
                          web_app::WebAppUiManagerObserver>
      ui_manager_observation_{this};

  base::WeakPtrFactory<SystemWebAppManager> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_MANAGER_H_
