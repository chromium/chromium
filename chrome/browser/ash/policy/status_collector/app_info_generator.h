// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_STATUS_COLLECTOR_APP_INFO_GENERATOR_H_
#define CHROME_BROWSER_ASH_POLICY_STATUS_COLLECTOR_APP_INFO_GENERATOR_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/ash/policy/status_collector/activity_storage.h"
#include "chrome/browser/ash/policy/status_collector/managed_session_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/services/app_service/public/cpp/instance_registry.h"

class Profile;

namespace apps {
class AppUpdate;
}

namespace enterprise_management {
class AppInfo;
}  // namespace enterprise_management

namespace policy {

// A class that is responsible for collecting application inventory and usage
// information.
class AppInfoGenerator : public apps::InstanceRegistry::Observer,
                         public ManagedSessionService::Observer {
 public:
  using Result = std::optional<std::vector<enterprise_management::AppInfo>>;

  explicit AppInfoGenerator(
      ManagedSessionService* managed_session_service,
      base::TimeDelta max_stored_past_activity_interval,
      base::Clock* clock = base::DefaultClock::GetInstance());
  AppInfoGenerator(const AppInfoGenerator&) = delete;
  AppInfoGenerator& operator=(const AppInfoGenerator&) = delete;
  ~AppInfoGenerator() override;

  // If reporting is enabled and there is an active affiliated user session,
  // generates an app info report with usage per app for up to the last
  // |max_stored_past_activity_interval| days for the current user, not
  // including the current day, otherwise returns a null optional.
  const Result Generate() const;

  // When not reporting usage is no longer recorded and app information is not
  // reported when generating the report.
  void OnReportingChanged(bool should_report);

  // Occurs when usage has been reported successfully. This removes reported
  // usage before |report_time| so it will not be reported again.
  void OnReportedSuccessfully(base::Time report_time);

  // Occurs when usage will be reported, which records active usage in the pref
  // up until the current time, so it may be reported.
  void OnWillReport();

  // ManagedSessionService::Observer
  void OnLogin(Profile* profile) override;
  void OnLogout(Profile* profile) override;
  void OnLocked() override;
  void OnUnlocked() override;
  void OnResumeActive(base::Time suspend_time) override;

  // InstanceRegistry::Observer
  void OnInstanceUpdate(const apps::InstanceUpdate& update) override;
  void OnInstanceRegistryWillBeDestroyed(
      apps::InstanceRegistry* cache) override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  struct AppInstances {
    explicit AppInstances(const base::Time start_time_);
    AppInstances(const AppInstances&) = delete;
    AppInstances& operator=(const AppInstances&) = delete;
    ~AppInstances();

    const base::Time start_time;
    std::unordered_set<base::UnguessableToken, base::UnguessableTokenHash>
        running_instances;
  };
  struct AppInfoProvider {
    explicit AppInfoProvider(Profile* profile);
    AppInfoProvider(const AppInfoProvider&) = delete;
    AppInfoProvider& operator=(const AppInfoProvider&) = delete;
    ~AppInfoProvider();

    ActivityStorage activity_storage;
    const raw_ref<apps::AppServiceProxy> app_service_proxy;
  };

  const enterprise_management::AppInfo ConvertToAppInfo(
      const apps::AppUpdate& update,
      const std::vector<enterprise_management::TimePeriod>& app_activity) const;

  void SetOpenDurationsToClosed(base::Time end_time);

  void SetIdleDurationsToOpen();

  void OpenUsageInterval(const std::string& app_id,
                         const base::UnguessableToken& instance_key,
                         const base::Time start_time);

  void CloseUsageInterval(const std::string& app_id,
                          const base::UnguessableToken& instance_key,
                          const base::Time end_time);

  std::unique_ptr<AppInfoProvider> provider_;

  bool should_report_ = false;

  bool device_locked_ = false;

  std::map<std::string, std::unique_ptr<AppInstances>> app_instances_by_id_;

  // The timeout in the past to store activity.
  // This is kept in case status uploads fail for a number of days.
  base::TimeDelta max_stored_past_activity_interval_;

  const raw_ref<const base::Clock> clock_;

  base::ScopedObservation<ManagedSessionService,
                          ManagedSessionService::Observer>
      managed_session_observation_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_STATUS_COLLECTOR_APP_INFO_GENERATOR_H_
