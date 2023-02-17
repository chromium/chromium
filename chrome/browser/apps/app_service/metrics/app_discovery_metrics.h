// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_METRICS_APP_DISCOVERY_METRICS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_METRICS_APP_DISCOVERY_METRICS_H_

#include <map>
#include <set>

#include "base/unguessable_token.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace apps {

// Represents the different state changes of interest for app-discovery. Keep
// in-sync with definition in structured.xml.
enum class AppStateChange {
  kInactive = 0,
  kActive = 1,
  kClosed = 2,
};

// Records metrics related to app discovery and app usage.
//
// No metrics should be recorded if app-sync is off.
class AppDiscoveryMetrics : public AppPlatformMetrics::Observer,
                            InstanceRegistry::Observer {
 public:
  AppDiscoveryMetrics(Profile* profile,
                      InstanceRegistry& instance_registry,
                      AppPlatformMetrics* app_platform_metrics);
  ~AppDiscoveryMetrics() override;

  // AppPlatformMetrics::Observer
  void OnAppInstalled(const std::string& app_id,
                      AppType app_type,
                      InstallSource app_install_source,
                      InstallReason app_install_reason,
                      InstallTime app_install_time) override;
  void OnAppLaunched(const std::string& app_id,
                     AppType app_type,
                     LaunchSource launch_source) override;
  void OnAppUninstalled(const std::string& app_id,
                        AppType app_type,
                        UninstallSource app_uninstall_source) override;
  void OnAppPlatformMetricsDestroyed() override;

  // InstanceRegistry::Observer
  void OnInstanceUpdate(const InstanceUpdate& instance_update) override;
  void OnInstanceRegistryWillBeDestroyed(InstanceRegistry* cache) override;

 private:
  // Returns whether app sync is enabled for |profile_|.
  bool IsAppSyncEnabled();

  // Returns true if there is an active instance of an app other than
  // |exclude_instance_id|. If |exclude_instance_id| is nullopt, then all
  // instances will be checked.
  bool IsAnyAppInstanceActive(const std::string& app_id,
                              absl::optional<base::UnguessableToken>
                                  exclude_instance_id = absl::nullopt);

  // Records app state metrics if there has been a change.
  void RecordAppState(const InstanceUpdate& instance_update);

  void RecordFromInactiveState(const InstanceUpdate& instance_update);
  void RecordFromActiveState(const InstanceUpdate& instance_update);
  void RecordFromStartState(const InstanceUpdate& instance_update);
  void RecordAppClosed(const InstanceUpdate& instance_update);

  // Profile for which apps discovery metrics are being recorded for.
  Profile* profile_;

  // Instance of AppPlatformMetrics |this| is observing.
  AppPlatformMetrics* app_platform_metrics_ = nullptr;

  // Map associating instance_ids to current state.
  std::map<base::UnguessableToken, InstanceState> instance_to_state_;

  // Map associating app_ids to instance_ids.
  std::map<std::string, std::set<base::UnguessableToken>>
      app_id_to_instance_ids_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_METRICS_APP_DISCOVERY_METRICS_H_
