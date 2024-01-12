// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_APP_ACTIVITY_REGISTRY_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_APP_ACTIVITY_REGISTRY_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_activity_report_interface.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_service_wrapper.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_types.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace enterprise_management {
class ChildStatusReportRequest;
}  // namespace enterprise_management

class PrefRegistrySimple;
class PrefService;

namespace ash {
namespace app_time {

class AppTimeLimitsAllowlistPolicyWrapper;
class AppTimeNotificationDelegate;
class PersistedAppInfo;

// Keeps track of app activity and time limits information.
// Stores app activity between user session. Information about uninstalled apps
// are removed from the registry after activity was uploaded to server or after
// 30 days if upload did not happen.
class AppActivityRegistry : public AppServiceWrapper::EventListener {
 public:
  // Used for tests to get internal implementation details.
  class TestApi {
   public:
    explicit TestApi(AppActivityRegistry* registry);
    ~TestApi();

    const std::optional<AppLimit>& GetAppLimit(const AppId& app_id) const;
    std::optional<base::TimeDelta> GetTimeLeft(const AppId& app_id) const;
    void SaveAppActivity();

   private:
    const raw_ptr<AppActivityRegistry, DanglingUntriaged> registry_;
  };

  // Interface for the observers interested in the changes of apps state.
  class AppStateObserver : public base::CheckedObserver {
   public:
    AppStateObserver() = default;
    AppStateObserver(const AppStateObserver&) = delete;
    AppStateObserver& operator=(const AppStateObserver&) = delete;
    ~AppStateObserver() override = default;

    // Called when state of the app with |app_id| changed to |kLimitReached|.
    // |was_active| indicates whether the app was active before reaching the
    // limit.
    virtual void OnAppLimitReached(const AppId& app_id,
                                   base::TimeDelta time_limit,
                                   bool was_active) = 0;

    // Called when state of the app with |app_id| is no longer |kLimitReached|.
    virtual void OnAppLimitRemoved(const AppId& app_id) = 0;

    // Called when new app was installed.
    virtual void OnAppInstalled(const AppId& app_id) = 0;
  };

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  AppActivityRegistry(AppServiceWrapper* app_service_wrapper,
                      AppTimeNotificationDelegate* notification_delegate,
                      PrefService* pref_service);
  AppActivityRegistry(const AppActivityRegistry&) = delete;
  AppActivityRegistry& operator=(const AppActivityRegistry&) = delete;
  ~AppActivityRegistry() override;

  // AppServiceWrapper::EventListener:
  void OnAppInstalled(const AppId& app_id) override;
  void OnAppUninstalled(const AppId& app_id) override;
  void OnAppAvailable(const AppId& app_id) override;
  void OnAppBlocked(const AppId& app_id) override;
  void OnAppActive(const AppId& app_id,
                   const base::UnguessableToken& instance_id,
                   base::Time timestamp) override;
  void OnAppInactive(const AppId& app_id,
                     const base::UnguessableToken& instance_id,
                     base::Time timestamp) override;
  void OnAppDestroyed(const AppId& app_id,
                      const base::UnguessableToken& instance_id,
                      base::Time timestamp) override;

  bool IsAppInstalled(const AppId& app_id) const;
  bool IsAppAvailable(const AppId& app_id) const;
  bool IsAppBlocked(const AppId& app_id) const;
  bool IsAppTimeLimitReached(const AppId& app_id) const;
  bool IsAppActive(const AppId& app_id) const;
  bool IsAllowlistedApp(const AppId& app_id) const;

  // Manages AppStateObservers.
  void AddAppStateObserver(AppStateObserver* observer);
  void RemoveAppStateObserver(AppStateObserver* observer);

  // Called from AppTimeController to notify AppActivityRegistry about installed
  // apps which AppActivityRegistry may have missed.
  void SetInstalledApps(const std::vector<AppId>& installed_apps);

  // Returns the total active time for the application since the last time limit
  // reset.
  base::TimeDelta GetActiveTime(const AppId& app_id) const;

  // Web time limit is the time limit set for Chrome browser. It is shared
  // between Chrome and Web apps.
  const std::optional<AppLimit>& GetWebTimeLimit() const;

  AppState GetAppState(const AppId& app_id) const;

  // Returns current time limit for the app identified by |app_id|.
  // Will return nullopt if there is no limit set.
  std::optional<base::TimeDelta> GetTimeLimit(const AppId& app_id) const;

  // Reporting enablement is set if |enabled| has value.
  void SetReportingEnabled(std::optional<bool> enabled);

  void GenerateHiddenApps(
      enterprise_management::ChildStatusReportRequest* report);

  // Populates |report| with collected app activity. Returns whether any data
  // were reported.
  AppActivityReportInterface::ReportParams GenerateAppActivityReport(
      enterprise_management::ChildStatusReportRequest* report);

  // Application activities earlier than |timestamp| have been reported. Clear
  // entries earlier than |timestamp|.
  void OnSuccessfullyReported(base::Time timestamp);

  // Updates time limits for all installed apps.
  // Apps not present in |app_limits| are treated as they do not have limit set.
  // Returns true if a new app limit is observed in any of the applications.
  bool UpdateAppLimits(const std::map<AppId, AppLimit>& app_limits);

  // Sets time limit for app identified with |app_id|.
  // Does not affect limits of any other app. Not specified |app_limit| means
  // that app does not have limit set. Does not affect limits of any other app.
  // Returns true if a new app limit is observed.
  bool SetAppLimit(const AppId& app_id,
                   const std::optional<AppLimit>& app_limit);

  // Sets the app identified with |app_id| as being always available.
  void SetAppAllowlisted(const AppId& app_id);

  // Reset time has been reached at |timestamp|.
  void OnResetTimeReached(base::Time timestamp);

  // Called from WebTimeActivityProvider to update chrome app state.
  void OnChromeAppActivityChanged(ChromeAppActivityState state,
                                  base::Time timestamp);

  // Allowlisted applications changed. Called by AppTimeController.
  void OnTimeLimitAllowlistChanged(
      const AppTimeLimitsAllowlistPolicyWrapper& wrapper);

  // Saves app activity into user preference.
  void SaveAppActivity();

  std::vector<AppId> GetAppsWithAppRestriction(
      AppRestriction restriction) const;

 private:
  struct SystemNotification {
    SystemNotification(std::optional<base::TimeDelta> app_time_limit,
                       AppNotification app_notification);
    SystemNotification(const SystemNotification&);
    SystemNotification& operator=(const SystemNotification&);
    std::optional<base::TimeDelta> time_limit = std::nullopt;
    AppNotification notification = AppNotification::kUnknown;
  };

  // Bundles detailed data stored for a specific app.
  struct AppDetails {
    AppDetails();
    explicit AppDetails(const AppActivity& activity);
    AppDetails(const AppDetails&) = delete;
    AppDetails& operator=(const AppDetails&) = delete;
    ~AppDetails();

    // Resets the time limit check timer.
    void ResetTimeCheck();

    // Checks |limit| and |activity| to determine if the limit was reached.
    bool IsLimitReached() const;

    // Checks if |limit| is equal to |another_limit| with exception for the
    // timestamp (that does not indicate that limit changed).
    bool IsLimitEqual(const std::optional<AppLimit>& another_limit) const;

    // Contains information about current app state and logged activity.
    AppActivity activity{AppState::kAvailable};

    // Contains the set of active instances for the application.
    std::set<base::UnguessableToken> active_instances;

    // The set of instances AppActivityRegistry has requested to be paused, but
    // which have not been paused yet.
    std::set<base::UnguessableToken> paused_instances;

    // Contains information about restriction set for the app.
    std::optional<AppLimit> limit;

    // Timer set up for when the app time limit is expected to be reached and
    // preceding notifications.
    std::unique_ptr<base::OneShotTimer> app_limit_timer;

    // Boolean to specify if OnAppInstalled call has been received for this
    // particular application.
    bool received_app_installed_ = false;

    // At the beginning of a session, we may want to send system notifications
    // for applications. This may happen if there is an update in
    // PerAppTimeLimits policy while the user was logged out. In these
    // scenarios, we have to wait until the application is installed.
    std::vector<SystemNotification> pending_notifications_;
  };

  // OnAppReinstalled is called when an application has been uninstalled and
  // then installed again before being removed from app registry.
  void OnAppReinstalled(const AppId& app_id);

  // Removes data older than |timestamp| from the registry.
  // Removes entries for uninstalled apps if there is no more relevant activity
  // data left.
  void CleanRegistry(base::Time timestamp);

  // Adds an ap to the registry if it does not exist.
  void Add(const AppId& app_id);

  // Convenience methods to access state of the app identified by |app_id|.
  // Should only be called if app exists in the registry.
  void SetAppState(const AppId& app_id, AppState app_state);

  // Notifies state observers the application identified by |app_id| has reached
  // its set time limit.
  void NotifyLimitReached(const AppId& app_id, bool was_active);

  // Methods to set the application as active and inactive respectively.
  void SetAppActive(const AppId& app_id, base::Time timestamp);
  void SetAppInactive(const AppId& app_id, base::Time timestamp);

  std::optional<base::TimeDelta> GetTimeLeftForApp(const AppId& app_id) const;

  // Schedules a time limit check for application when it becomes active.
  void ScheduleTimeLimitCheckForApp(const AppId& app_id);

  // Checks the limit and shows notification if needed.
  void CheckTimeLimitForApp(const AppId& app_id);

  // Shows notification about time limit updates for the app if there were
  // relevant changes between |old_limit| and |new_limit|. Returns true if a
  // notification has been made.
  bool ShowLimitUpdatedNotificationIfNeeded(
      const AppId& app_id,
      const std::optional<AppLimit>& old_limit,
      const std::optional<AppLimit>& new_limit);

  base::TimeDelta GetWebActiveRunningTime() const;

  void WebTimeLimitReached(base::Time timestamp);

  // Initializes |activity_registry_| from the stored values in user pref.
  // Installed applications, their AppStates and their running active times will
  // be restored.
  void InitializeRegistryFromPref();
  void InitializeAppActivities();

  // Updates |AppActivity::active_times_| to include the current activity up to
  // |timestamp| then creates the most up to date instance of PersistedAppInfo.
  PersistedAppInfo GetPersistedAppInfoForApp(const AppId& app_id,
                                             base::Time timestamp);

  // Returns true if the last successfully reported time is earlier than 30 days
  // from base::Time::Now();
  bool ShouldCleanUpStoredPref();

  // Sends system notification for the application.
  void SendSystemNotificationsForApp(const AppId& app_id);

  // Shows notification or queues it to be shown later.
  void MaybeShowSystemNotification(const AppId& app_id,
                                   const SystemNotification& notification);

  // Called by AppActivityRegistry::SetAppLimit after the application's limit
  // has been updated.
  void AppLimitUpdated(const AppId& app_id);

  const raw_ptr<PrefService> pref_service_;

  // Owned by AppTimeController.
  const raw_ptr<AppServiceWrapper> app_service_wrapper_;

  // Notification delegate.
  const raw_ptr<AppTimeNotificationDelegate> notification_delegate_;

  // Observers to be notified about app state changes.
  base::ObserverList<AppStateObserver> app_state_observers_;

  std::map<AppId, AppDetails> activity_registry_;

  // Newly installed applications which have not yet been added to the user
  // pref.
  std::vector<AppId> newly_installed_apps_;

  // Repeating timer to trigger saving app activity to pref service.
  base::RepeatingTimer save_data_to_pref_service_;

  // This records the timestamp of the latest set app limit.
  base::Time latest_app_limit_update_;

  // Boolean to capture if app activity data reporting is enabled.
  bool activity_reporting_enabled_ = true;
};

}  // namespace app_time
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_APP_ACTIVITY_REGISTRY_H_
