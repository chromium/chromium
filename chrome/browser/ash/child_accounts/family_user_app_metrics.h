// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_FAMILY_USER_APP_METRICS_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_FAMILY_USER_APP_METRICS_H_

#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/child_accounts/family_user_metrics_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"

class Profile;

namespace extensions {
class ExtensionRegistry;
}  // namespace extensions

namespace apps {
class InstanceRegistry;
}  // namespace apps

namespace ash {

class FamilyUserAppMetrics : public FamilyUserMetricsService::Observer,
                             public apps::AppRegistryCache::Observer {
 public:
  FamilyUserAppMetrics(const FamilyUserAppMetrics&) = delete;
  FamilyUserAppMetrics& operator=(const FamilyUserAppMetrics&) = delete;
  ~FamilyUserAppMetrics() override;

  static std::unique_ptr<FamilyUserAppMetrics> Create(Profile* profile);

  // UMA metrics for a snapshot count of installed and enabled extensions for a
  // given family user.
  static const char* GetInstalledExtensionsCountHistogramNameForTest();
  static const char* GetEnabledExtensionsCountHistogramNameForTest();

  // UMA metrics for a snapshot count of recently used apps for a given family
  // user.
  static const char* GetAppsCountHistogramNameForTest(apps::AppType app_type);

 protected:
  // These methods are marked protected for visibility to derived test class.
  explicit FamilyUserAppMetrics(Profile* profile);

  void Init();

  // FamilyUserMetricsService::Observer:
  void OnNewDay() override;

  // AppRegistryCache::Observer:
  void OnAppTypeInitialized(apps::AppType app_type) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;
  void OnAppUpdate(const apps::AppUpdate& update) override;

  bool IsAppTypeReady(apps::AppType app_type) const;

 private:
  // Records the number of non-component extensions that the family user has
  // installed. This count is a superset of the enabled extensions count.
  void RecordInstalledExtensionsCount();

  // Records the number of non-component extensions that the family user has
  // enabled. This count is a subset of the installed extensions count.
  void RecordEnabledExtensionsCount();

  // Records the number of apps of the given `app_type` that the family user has
  // recently used.
  void RecordRecentlyUsedAppsCount(apps::AppType app_type);

  // Returns true if the app is currently open.
  bool IsAppWindowOpen(const std::string& app_id);

  const raw_ptr<const extensions::ExtensionRegistry> extension_registry_;
  const raw_ptr<apps::AppRegistryCache> app_registry_;
  const raw_ptr<apps::InstanceRegistry> instance_registry_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};

  bool should_record_metrics_on_new_day_ = false;
  bool first_report_on_current_device_ = false;
  std::set<apps::AppType> ready_app_types_;
};
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_FAMILY_USER_APP_METRICS_H_
