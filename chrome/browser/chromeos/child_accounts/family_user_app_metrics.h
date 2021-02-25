// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_FAMILY_USER_APP_METRICS_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_FAMILY_USER_APP_METRICS_H_

#include <set>
#include <string>

#include "chrome/browser/chromeos/child_accounts/family_user_metrics_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

class Profile;

namespace extensions {
class ExtensionRegistry;
}  // namespace extensions

namespace apps {
class InstanceRegistry;
}  // namespace apps

namespace chromeos {

class FamilyUserAppMetrics : public FamilyUserMetricsService::Observer,
                             public apps::AppRegistryCache::Observer {
 public:
  explicit FamilyUserAppMetrics(Profile* profile);
  FamilyUserAppMetrics(const FamilyUserAppMetrics&) = delete;
  FamilyUserAppMetrics& operator=(const FamilyUserAppMetrics&) = delete;
  ~FamilyUserAppMetrics() override;

  // UMA metrics for a snapshot count of installed and enabled extensions for a
  // given family user.
  static const char* GetInstalledExtensionsCountHistogramNameForTest();
  static const char* GetEnabledExtensionsCountHistogramNameForTest();

  // UMA metrics for a snapshot count of recently used apps for a given family
  // user.
  static const char* GetAppsCountHistogramNameForTest(
      apps::mojom::AppType app_type);

 protected:
  // These methods are marked protected for visibility to derived test class.

  // FamilyUserMetricsService::Observer:
  void OnNewDay() override;

  // AppRegistryCache::Observer:
  void OnAppTypeInitialized(apps::mojom::AppType app_type) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;
  void OnAppUpdate(const apps::AppUpdate& update) override;

  bool IsAppTypeReady(apps::mojom::AppType app_type) const;

 private:
  // Records the number of non-component extensions that the family user has
  // installed. This count is a superset of the enabled extensions count.
  void RecordInstalledExtensionsCount();

  // Records the number of non-component extensions that the family user has
  // enabled. This count is a subset of the installed extensions count.
  void RecordEnabledExtensionsCount();

  // Records the number of apps of the given `app_type` that the family user has
  // recently used.
  void RecordRecentlyUsedAppsCount(apps::mojom::AppType app_type);

  // Returns true if the app is currently open.
  bool IsAppWindowOpen(const std::string& app_id);

  const extensions::ExtensionRegistry* const extension_registry_;
  apps::AppRegistryCache* const app_registry_;
  apps::InstanceRegistry* const instance_registry_;

  bool should_record_metrics_on_new_day_ = false;
  bool first_report_on_current_device_ = false;
  std::set<apps::mojom::AppType> ready_app_types_;
};
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_FAMILY_USER_APP_METRICS_H_
