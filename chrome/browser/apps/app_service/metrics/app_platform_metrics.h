// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_METRICS_APP_PLATFORM_METRICS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_METRICS_APP_PLATFORM_METRICS_H_

#include <map>
#include <set>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_utils.h"
#include "chrome/browser/apps/app_service/metrics/browser_to_tab_list.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/services/app_service/public/protos/app_types.pb.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class Profile;

namespace apps {

class AppUpdate;

// This is used for logging, so do not remove or reorder existing entries. Also
// needs to be kept in sync with the ApplicationInstallTime in
// //components/services/app_service/public/protos/app_types.proto.
enum class InstallTime {
  kInit = 0,
  kRunning = 1,

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kRunning,
};

struct CrostiniAppId {
  std::string desktop_id;
  std::string registration_name;
};

extern const char kAppRunningDuration[];
extern const char kAppActivatedCount[];
extern const char kAppUsageTime[];

extern const char kAppLaunchPerAppTypeHistogramName[];
extern const char kAppLaunchPerAppTypeV2HistogramName[];

extern const char kChromeAppTabHistogramName[];
extern const char kChromeAppWindowHistogramName[];
extern const char kWebAppTabHistogramName[];
extern const char kWebAppWindowHistogramName[];

extern const char kUsageTimeAppIdKey[];
extern const char kUsageTimeAppPublisherIdKey[];
extern const char kUsageTimeAppTypeKey[];
extern const char kUsageTimeDurationKey[];
extern const char kReportingUsageTimeDurationKey[];

std::string GetAppTypeHistogramNameV2(apps::AppTypeNameV2 app_type_name);

ApplicationInstallTime ConvertInstallTimeToProtoApplicationInstallTime(
    InstallTime install_time);

// Records metrics when launching apps.
void RecordAppLaunchMetrics(Profile* profile,
                            AppType app_type,
                            const std::string& app_id,
                            apps::LaunchSource launch_source,
                            apps::LaunchContainer container);

class AppPlatformMetrics : public apps::AppRegistryCache::Observer,
                           public apps::InstanceRegistry::Observer,
                           public ukm::UkmRecorder::Observer {
 public:
  // Observer that is notified on certain app related events like install,
  // launch, uninstall, etc.
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;
    ~Observer() override = default;

    // Invoked when app install metrics are being reported.
    virtual void OnAppInstalled(const std::string& app_id,
                                AppType app_type,
                                InstallSource app_install_source,
                                InstallReason app_install_reason,
                                InstallTime app_install_time) {}

    // Invoked when app launch metrics are being reported.
    virtual void OnAppLaunched(const std::string& app_id,
                               AppType app_type,
                               apps::LaunchSource launch_source) {}

    // Invoked when app uninstall metrics are being reported.
    virtual void OnAppUninstalled(const std::string& app_id,
                                  AppType app_type,
                                  UninstallSource app_uninstall_source) {}

    // Invoked when app usage metrics are being recorded (every 5 mins). Since
    // apps can have multiple instances, we also include the instance id here.
    virtual void OnAppUsage(const std::string& app_id,
                            AppType app_type,
                            const base::UnguessableToken& instance_id,
                            base::TimeDelta running_time) {}

    // Invoked when the `AppPlatformMetrics` component (being observed) is being
    // destroyed.
    virtual void OnAppPlatformMetricsDestroyed() {}
  };

  // Usage time representation for the data that is persisted in the pref store.
  // Includes helpers for serialization/deserialization.
  struct UsageTime {
    UsageTime();
    explicit UsageTime(const base::Value& value);
    UsageTime(const UsageTime&) = delete;
    UsageTime& operator=(const UsageTime&) = delete;
    ~UsageTime();

    base::TimeDelta running_time;
    ukm::SourceId source_id = ukm::kInvalidSourceId;
    std::string app_id;

    // App publisher id tracked for commercial insights reporting. This
    // facilitates external components to report the publisher id that includes
    // the package name for android apps, web app url for web apps, etc. which
    // are public app identifiers. We use an empty string if there is no
    // publisher id associated with the app.
    std::string app_publisher_id;
    AppTypeName app_type_name = AppTypeName::kUnknown;
    bool window_is_closed = false;

    // Usage time tracked for Chrome OS commercial insights reporting. Because
    // we have two independent attributes that track usage time now, the pref
    // store data retention period will depend on both of these attributes being
    // reset, ideally after the corresponding snapshot has been reported.
    base::TimeDelta reporting_usage_time;

    // Converts the struct UsageTime to base::Value::Dict, e.g.:
    // {
    //    "app_id": "hhsosodfjlsjdflkjsdlfksdf",
    //    "app_type": "SystemWebApp",
    //    "time": 3600,
    //    "reporting_usage_time": 1800,
    // }
    base::Value::Dict ConvertToDict() const;
  };

  explicit AppPlatformMetrics(Profile* profile,
                              apps::AppRegistryCache& app_registry_cache,
                              InstanceRegistry& instance_registry);
  AppPlatformMetrics(const AppPlatformMetrics&) = delete;
  AppPlatformMetrics& operator=(const AppPlatformMetrics&) = delete;
  ~AppPlatformMetrics() override;

  // Returns the SourceId of UKM for `app_id`.
  static ukm::SourceId GetSourceId(Profile* profile, const std::string& app_id);

  // Returns the URL used to create the SourceId for UKM. The URL will be empty
  // if nothing should be recorded for |app_id|.
  //
  // This is used to retrieve an app identifier that is used in UKM where UKM is
  // not the logger.
  static GURL GetURLForApp(Profile* profile, const std::string& app_id);

  // Returns a publisher id fetched from |profile| for a given |app_id|.
  static std::string GetPublisherId(Profile* profile,
                                    const std::string& app_id);

  // Returns the URL for a Borealis app_id.
  static GURL GetURLForBorealis(Profile* profile, const std::string& app_id);

  // Returns a crostini id struct for an app_id.
  static CrostiniAppId GetIdForCrostini(Profile* profile,
                                        const std::string& app_id);

  // Informs UKM service that the source_id is no longer needed and can be
  // deleted later.
  static void RemoveSourceId(ukm::SourceId source_id);

  // UMA metrics name for installed apps count in Chrome OS.
  static std::string GetAppsCountHistogramNameForTest(
      AppTypeName app_type_name);

  // UMA metrics name for installed apps count per InstallReason in Chrome OS.
  static std::string GetAppsCountPerInstallReasonHistogramNameForTest(
      AppTypeName app_type_name,
      apps::InstallReason install_reason);

  // UMA metrics name for apps running duration in Chrome OS.
  static std::string GetAppsRunningDurationHistogramNameForTest(
      AppTypeName app_type_name);

  // UMA metrics name for apps running percentage in Chrome OS.
  static std::string GetAppsRunningPercentageHistogramNameForTest(
      AppTypeName app_type_name);

  // UMA metrics name for app window activated count in Chrome OS.
  static std::string GetAppsActivatedCountHistogramNameForTest(
      AppTypeName app_type_name);

  // UMA metrics name for apps usage time in Chrome OS for AppTypeName.
  static std::string GetAppsUsageTimeHistogramNameForTest(
      AppTypeName app_type_name);

  // UMA metrics name for apps usage time in Chrome OS for AppTypeNameV2.
  static std::string GetAppsUsageTimeHistogramNameForTest(
      AppTypeNameV2 app_type_name);

  void OnNewDay();
  void OnTenMinutes();
  void OnFiveMinutes();

  // Records the app usage time AppKM each 2 hours.
  void OnTwoHours();

  // Records UKM when launching an app.
  void RecordAppLaunchUkm(AppType app_type,
                          const std::string& app_id,
                          apps::LaunchSource launch_source,
                          apps::LaunchContainer container);

  // Records UKM when uninstalling an app.
  void RecordAppUninstallUkm(AppType app_type,
                             const std::string& app_id,
                             UninstallSource uninstall_source);

  void AddObserver(Observer* observer);

  void RemoveObserver(Observer* observer);

 private:
  struct RunningStartTime {
    base::TimeTicks start_time;
    AppTypeName app_type_name;
    AppTypeNameV2 app_type_name_v2;
    std::string app_id;
  };

  // AppRegistryCache::Observer:
  void OnAppTypeInitialized(AppType app_type) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;
  void OnAppUpdate(const apps::AppUpdate& update) override;

  // apps::InstanceRegistry::Observer:
  void OnInstanceUpdate(const apps::InstanceUpdate& update) override;
  void OnInstanceRegistryWillBeDestroyed(
      apps::InstanceRegistry* cache) override;

  // ukm::UkmRecorder::Observer:
  // Called only in Managed Guest Session since the observation is started only
  // in Managed Guest Session.
  void OnStartingShutdown() override;

  // Returns the browser instance app id, instance id and state for
  // `browser_window`. If there is no browser instance, the returned token of
  // the browser id and app id will be empty, and the state will be unknown.
  void GetBrowserInstanceInfo(const aura::Window* browser_window,
                              base::UnguessableToken& browser_id,
                              std::string& browser_app_id,
                              InstanceState& state) const;

  // Updates the browser window status when the web app tab `update` is
  // inactivated.
  void UpdateBrowserWindowStatus(const InstanceUpdate& update);

  void SetWindowActivated(AppType app_type,
                          AppTypeName app_type_name,
                          AppTypeNameV2 app_type_name_v2,
                          const std::string& app_id,
                          const base::UnguessableToken& instance_id);
  void SetWindowInActivated(const std::string& app_id,
                            const base::UnguessableToken& instance_id,
                            apps::InstanceState state);

  void InitRunningDuration();
  void ClearRunningDuration();

  // Reads the installed apps from AppRegistryCache before AppPlatformMetrics is
  // created to record the install AppKM.
  void ReadInstalledApps();

  // Records the number of apps of the given `app_type` that the family user has
  // recently used.
  void RecordAppsCount(AppType app_type);

  // Records the app running duration.
  void RecordAppsRunningDuration();

  // Saves the app usage time metrics UKM to the user preferences and records
  // UMA.
  void RecordAppsUsageTime();

  // Sends the app usage time UKM to `ukm::UkmRecorder`.
  void RecordAppsUsageTimeUkm();

  // Records the installed app in Chrome OS.
  void RecordAppsInstallUkm(const apps::AppUpdate& update,
                            InstallTime install_time);

  // Updates `usage_time_per_two_hours_` each 5 minutes or when the app window
  // is inactivated.
  void UpdateUsageTime(const base::UnguessableToken& instance_id,
                       const std::string& app_id,
                       AppTypeName app_type_name,
                       const base::TimeDelta& running_time);

  // Saves the app window usage time in `usage_time_per_two_hours_` to the user
  // pref each 5 minutes.
  void SaveUsageTime();

  // Reads the app platform metrics saved in the user pref to
  // `usage_times_from_pref_`.
  void LoadAppsUsageTimeUkmFromPref();

  // Sends the app usage time UKM to `ukm::UkmRecorder` based on the usage time
  // saved in `usage_times_from_pref_`.
  void RecordAppsUsageTimeUkmFromPref();

  // Attempts to clear app usage info entries in the pref store for instances if
  // and only if both the UKM usage and reporting usage time snapshots have been
  // reset.
  void CleanUpAppsUsageInfoInPrefStore();

  // Clears UKM usage tracked for a given app instance in the pref store.
  // Normally triggered after corresponding usage snapshot has been reported to
  // UKM for the app instance.
  void ClearAppsUsageTimeForInstance(std::string_view instance_id);

  void UpdateMetricsBeforeShutdown();

  const raw_ptr<Profile> profile_ = nullptr;

  const raw_ref<AppRegistryCache> app_registry_cache_;

  bool should_record_metrics_on_new_day_ = false;

  bool should_refresh_duration_pref = false;
  bool should_refresh_activated_count_pref = false;

  int user_type_by_device_type_;

  BrowserToTabList browser_to_tab_list_;

  // |running_start_time_| and |running_duration_| are used for accumulating app
  // running duration per each day interval.
  std::map<const base::UnguessableToken, RunningStartTime> running_start_time_;
  std::map<AppTypeName, base::TimeDelta> running_duration_;
  std::map<AppTypeName, int> activated_count_;

  // |start_time_per_five_minutes_|, |app_type_running_time_per_five_minutes_|,
  // |app_type_v2_running_time_per_five_minutes_| are used for accumulating app
  // running duration per 5 minutes interval.
  std::map<const base::UnguessableToken, RunningStartTime>
      start_time_per_five_minutes_;
  std::map<AppTypeName, base::TimeDelta>
      app_type_running_time_per_five_minutes_;
  std::map<AppTypeNameV2, base::TimeDelta>
      app_type_v2_running_time_per_five_minutes_;

  // Records the app window running duration for the app usage AppKM.
  std::map<const base::UnguessableToken, UsageTime> usage_time_per_two_hours_;

  // The app usage time loaded from the user pref during the init phase.
  std::vector<std::unique_ptr<UsageTime>> usage_times_from_pref_;

  base::ObserverList<Observer> observers_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};

  base::ScopedObservation<InstanceRegistry, InstanceRegistry::Observer>
      instance_registry_observation_{this};

  // Observes `UkmRecorder` only in Managed Guest Session.
  base::ScopedObservation<ukm::UkmRecorder, ukm::UkmRecorder::Observer>
      ukm_recorder_observer_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_METRICS_APP_PLATFORM_METRICS_H_
