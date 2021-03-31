// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_STATUS_COLLECTOR_APP_INFO_GENERATOR_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_STATUS_COLLECTOR_APP_INFO_GENERATOR_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/optional.h"
#include "base/time/default_clock.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/chromeos/policy/status_collector/activity_storage.h"
#include "chrome/browser/chromeos/policy/status_collector/affiliated_session_service.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/prefs/pref_registry_simple.h"

class Profile;

namespace aura {
class Window;
}  // namespace aura

namespace enterprise_management {
class AppInfo;
}  // namespace enterprise_management

namespace policy {

// A class that is responsible for collecting application inventory and usage
// information.
class AppInfoGenerator : public apps::InstanceRegistry::Observer,
                         public AffiliatedSessionService::Observer {
 public:
  using Result = base::Optional<std::vector<enterprise_management::AppInfo>>;

  explicit AppInfoGenerator(
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

  // AffiliatedSessionManager::Observer
  void OnAffiliatedLogin(Profile* profile) override;
  void OnAffiliatedLogout(Profile* profile) override;
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
    std::unordered_set<aura::Window*> running_instances;
  };
  struct AppInfoProvider {
    explicit AppInfoProvider(Profile* profile);
    AppInfoProvider(const AppInfoProvider&) = delete;
    AppInfoProvider& operator=(const AppInfoProvider&) = delete;
    ~AppInfoProvider();

    ActivityStorage activity_storage;
    apps::AppServiceProxy& app_service_proxy;
    web_app::WebAppProvider& web_app_provider;
  };

  const enterprise_management::AppInfo ConvertToAppInfo(
      const apps::AppUpdate& update,
      const std::vector<enterprise_management::TimePeriod>& app_activity) const;

  void SetOpenDurationsToClosed(base::Time end_time);

  void SetIdleDurationsToOpen();

  void OpenUsageInterval(const std::string& app_id,
                         aura::Window* window,
                         const base::Time start_time);

  void CloseUsageInterval(const std::string& app_id,
                          aura::Window* window,
                          const base::Time end_time);

  std::unique_ptr<AppInfoProvider> provider_ = nullptr;

  bool should_report_ = false;

  std::map<std::string, std::unique_ptr<AppInstances>> app_instances_by_id_;

  // The timeout in the past to store activity.
  // This is kept in case status uploads fail for a number of days.
  base::TimeDelta max_stored_past_activity_interval_;

  const base::Clock& clock_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_STATUS_COLLECTOR_APP_INFO_GENERATOR_H_
