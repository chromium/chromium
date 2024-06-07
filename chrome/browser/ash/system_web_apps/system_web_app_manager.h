// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_MANAGER_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_MANAGER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/one_shot_event.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_background_task.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_icon_checker.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate_map.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/webapps/common/web_app_id.h"
#include "url/gurl.h"

namespace base {
class Version;
}

namespace content {
class NavigationHandle;
}

namespace web_app {
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

  // Number of attempts to install a given version & locale of the SWAs before
  // bailing out.
  static constexpr int kInstallFailureAttempts = 3;

  static constexpr char kSystemWebAppSessionHasBrokenIconsPrefName[] =
      "web_apps.system_web_app_has_broken_icons_in_session";

  static constexpr char kInstallResultHistogramName[] =
      "Webapp.InstallResult.System";
  static constexpr char kInstallDurationHistogramName[] =
      "Webapp.SystemApps.FreshInstallDuration";
  static constexpr char kIconsFixedOnReinstallHistogramName[] =
      "Webapp.SystemApps.IconsFixedOnReinstall";

  // Returns whether the given app type is enabled.
  bool IsAppEnabled(SystemWebAppType type) const;

  explicit SystemWebAppManager(Profile* profile);
  SystemWebAppManager(const SystemWebAppManager&) = delete;
  SystemWebAppManager& operator=(const SystemWebAppManager&) = delete;
  ~SystemWebAppManager() override;

  // Return the SystemWebAppManager that hosts system web apps in profile.
  // Returns nullptr if the profile doesn't support system web apps (e.g. Kiosk,
  // lock-screen, system profile).
  static SystemWebAppManager* Get(Profile* profile);
  // Gets the associated WebAppProvider for system web apps. `WebAppProvider` is
  // always present in the `profile` if the `Get` above returns non-nullptr.
  static web_app::WebAppProvider* GetWebAppProvider(Profile* profile);

  // Returns the SystemWebAppManager for tests. Blocks if the web app registry
  // is not yet ready.
  static SystemWebAppManager* GetForTest(Profile* profile);

  // Calls `Start` when `WebAppProvider` is ready.
  void ScheduleStart();

  // Initialize the SystemWebAppManager.
  void Start();

  // KeyedService:
  void Shutdown() override;

  // By default, we don't install system web apps in browser tests to avoid
  // running installation tasks (inefficient because most browser tests don't
  // need SWAs).
  //
  // Call this to install default enabled system apps if the test needs them.
  // (e.g. test opening OS Settings from an Ash views button).
  //
  // This can be called multiple times to simulate reinstallation from system
  // restart.
  void InstallSystemAppsForTesting();

  // Returns the app id for the given System App |type|.
  std::optional<webapps::AppId> GetAppIdForSystemApp(
      SystemWebAppType type) const;

  // Returns the System App Type for the given |app_id|.
  std::optional<SystemWebAppType> GetSystemAppTypeForAppId(
      const webapps::AppId& app_id) const;

  // Returns the System App Delegate for the given App |type|.
  const SystemWebAppDelegate* GetSystemApp(SystemWebAppType type) const;

  // Returns the App Ids for all installed System Web Apps.
  std::vector<webapps::AppId> GetAppIds() const;

  // Returns whether |app_id| points to an installed System App.
  bool IsSystemWebApp(const webapps::AppId& app_id) const;

  // Returns the SystemWebAppType that should handle |url|.
  //
  // Under the hood, it returns the system web app whose `start_url` shares
  // the same origin with the given |url|. It does not take
  // `SystemWebAppDelegate::IsURLInSystemAppScope` into account.
  std::optional<SystemWebAppType> GetSystemAppForURL(const GURL& url) const;

  // Returns the SystemWebAppType that should capture the navigation to |url|.
  std::optional<SystemWebAppType> GetCapturingSystemAppForURL(
      const GURL& url) const;

  const base::OneShotEvent& on_apps_synchronized() const {
    return *on_apps_synchronized_;
  }

  // Return the OneShotEvent that is fired after all of the background tasks
  // have started and their timers become active.
  const base::OneShotEvent& on_tasks_started() const {
    return *on_tasks_started_;
  }

  // Returns the OneShotEvent that is fired after icon checks are complete.
  const base::OneShotEvent& on_icon_check_completed() const {
    return *on_icon_check_completed_;
  }

  // Returns a map of registered system app types and infos, these apps will be
  // installed on the system.
  const SystemWebAppDelegateMap& system_app_delegates() const {
    return system_app_delegates_;
  }

  // This call will override default System Apps configuration. You should call
  // Start() after this call to install |system_apps|.
  void SetSystemAppsForTesting(SystemWebAppDelegateMap system_apps);

  // Overrides the update policy. If AlwaysReinstallSystemWebApps feature is
  // enabled, this method does nothing, and system apps will be reinstalled.
  void SetUpdatePolicyForTesting(UpdatePolicy policy);

  void ResetForTesting();

  // Get the timers. Only use this for testing.
  const std::vector<std::unique_ptr<SystemWebAppBackgroundTask>>&
  GetBackgroundTasksForTesting();
  void StopBackgroundTasksForTesting();

  const Profile* profile() const { return profile_; }

 protected:
  virtual const base::Version& CurrentVersion() const;
  virtual const std::string& CurrentLocale() const;
  virtual bool PreviousSessionHadBrokenIcons() const;
  void StopBackgroundTasks();

 private:
  // Returns the list of origin trials to enable for |url| loaded in System
  // App |type|. Returns an empty vector if the App does not specify origin
  // trials for |url|.
  const std::vector<std::string>* GetEnabledOriginTrials(
      const SystemWebAppDelegate* system_app,
      const GURL& url) const;

  void OnAppsSynchronized(
      bool did_force_install_apps,
      const base::TimeTicks& install_start_time,
      std::map<GURL, web_app::ExternallyManagedAppManager::InstallResult>
          install_results,
      std::map<GURL, webapps::UninstallResultCode> uninstall_results);
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

  void OnIconCheckResult(SystemWebAppIconChecker::IconState result);

  // web_app::WebAppUiManagerObserver:
  void OnReadyToCommitNavigation(
      const webapps::AppId& app_id,
      content::NavigationHandle* navigation_handle) override;
  void OnWebAppUiManagerDestroyed() override;

  void ConnectProviderToSystemWebAppDelegateMap(
      const SystemWebAppDelegateMap* system_web_apps_delegate_map) const;

  raw_ptr<Profile> profile_;
  // SystemWebAppManager KeyedService depends on WebAppProvider KeyedService,
  // therefore this pointer is always valid.
  raw_ptr<web_app::WebAppProvider> provider_ = nullptr;

  std::unique_ptr<base::OneShotEvent> on_apps_synchronized_;
  std::unique_ptr<base::OneShotEvent> on_tasks_started_;
  std::unique_ptr<base::OneShotEvent> on_icon_check_completed_;

  bool shutting_down_ = false;

  bool previous_session_had_broken_icons_ = false;

  std::string install_result_per_profile_histogram_name_;

  UpdatePolicy update_policy_;

  // We skip app installation in tests by default. Tests can trigger
  // installation by calling `InstallSystemAppsForTesting()` or
  // `SetSystemAppsForTesting()`.
  bool skip_app_installation_in_test_ = true;

  SystemWebAppDelegateMap system_app_delegates_;

  const raw_ptr<PrefService> pref_service_;

  std::vector<std::unique_ptr<SystemWebAppBackgroundTask>> tasks_;

  base::ScopedObservation<web_app::WebAppUiManager,
                          web_app::WebAppUiManagerObserver>
      ui_manager_observation_{this};

  // Always a valid pointer, has the same lifecycle as `this` in production.
  // Might be reset in tests.
  std::unique_ptr<SystemWebAppIconChecker> icon_checker_;

  base::WeakPtrFactory<SystemWebAppManager> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_MANAGER_H_
