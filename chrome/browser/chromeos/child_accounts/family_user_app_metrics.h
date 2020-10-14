// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_FAMILY_USER_APP_METRICS_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_FAMILY_USER_APP_METRICS_H_

#include "chrome/browser/chromeos/child_accounts/family_user_metrics_service.h"

class Profile;

namespace extensions {
class ExtensionRegistry;
}  // namespace extensions

namespace apps {
class AppRegistryCache;
}  // namespace apps

namespace chromeos {

class FamilyUserAppMetrics : public FamilyUserMetricsService::Observer {
 public:
  // UMA metrics for a snapshot count of installed and enabled extensions for a
  // given family user.
  static const char kInstalledExtensionsCountHistogramName[];
  static const char kEnabledExtensionsCountHistogramName[];

  // UMA metrics for a snapshot count of recently used apps for a given family
  // user.
  static const char kOtherAppsCountHistogramName[];
  static const char kArcAppsCountHistogramName[];
  static const char kBorealisAppsCountHistogramName[];
  static const char kCrostiniAppsCountHistogramName[];
  static const char kExtensionAppsCountHistogramName[];
  static const char kWebAppsCountHistogramName[];
  // Sum of the above metrics for a given snapshot.
  static const char kTotalAppsCountHistogramName[];

  explicit FamilyUserAppMetrics(Profile* profile);
  FamilyUserAppMetrics(const FamilyUserAppMetrics&) = delete;
  FamilyUserAppMetrics& operator=(const FamilyUserAppMetrics&) = delete;
  ~FamilyUserAppMetrics() override;

 private:
  // FamilyUserMetricsService::Observer:
  void OnNewDay() override;

  // Records the number of non-component extensions that the family user has
  // installed. This count is a superset of the enabled extensions count.
  void RecordInstalledExtensionsCount();

  // Records the number of non-component extensions that the family user has
  // enabled. This count is a subset of the installed extensions count.
  void RecordEnabledExtensionsCount();

  // Records the number of apps that the family user has recently used.
  void RecordRecentlyUsedAppsCount();

  const extensions::ExtensionRegistry* const extension_registry_;
  apps::AppRegistryCache* const app_registry_;

  bool on_new_day_ = false;
};
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_FAMILY_USER_APP_METRICS_H_
