// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/family_user_app_metrics.h"

#include <memory>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"

namespace chromeos {

namespace {
// Recently launched apps this many days ago in the past will be recorded.
constexpr base::TimeDelta k28Days = base::TimeDelta::FromDays(28);
}  // namespace

// static
// UMA metrics for a snapshot count of installed and enabled extensions for a
// given family user.
const char FamilyUserAppMetrics::kInstalledExtensionsCountHistogramName[] =
    "FamilyUser.InstalledExtensionsCount2";
const char FamilyUserAppMetrics::kEnabledExtensionsCountHistogramName[] =
    "FamilyUser.EnabledExtensionsCount2";

// UMA metrics for a snapshot count of installed apps for a given family user.
const char FamilyUserAppMetrics::kArcAppsCountHistogramName[] =
    "FamilyUser.ArcAppsCount2";
const char FamilyUserAppMetrics::kBorealisAppsCountHistogramName[] =
    "FamilyUser.BorealisAppsCount2";
const char FamilyUserAppMetrics::kCrostiniAppsCountHistogramName[] =
    "FamilyUser.CrostiniAppsCount2";
const char FamilyUserAppMetrics::kExtensionAppsCountHistogramName[] =
    "FamilyUser.ExtensionAppsCount2";
const char FamilyUserAppMetrics::kWebAppsCountHistogramName[] =
    "FamilyUser.WebAppsCount2";

FamilyUserAppMetrics::FamilyUserAppMetrics(Profile* profile)
    : extension_registry_(extensions::ExtensionRegistry::Get(profile)),
      app_registry_(&apps::AppServiceProxyFactory::GetForProfile(profile)
                         ->AppRegistryCache()),
      first_report_on_current_device_(
          user_manager::UserManager::Get()->IsCurrentUserNew()) {
  DCHECK(extension_registry_);
  DCHECK(app_registry_);
  Observe(app_registry_);
}

FamilyUserAppMetrics::~FamilyUserAppMetrics() = default;

void FamilyUserAppMetrics::OnNewDay() {
  // Ignores the first report during OOBE. Apps and extensions may sync slowly
  // after the OOBE process, biasing the metrics downwards toward zero.
  if (first_report_on_current_device_) {
    first_report_on_current_device_ = false;
    return;
  }

  should_record_metrics_on_new_day_ = true;
  RecordInstalledExtensionsCount();
  RecordEnabledExtensionsCount();
  for (const auto& app_type : ready_app_types_)
    RecordRecentlyUsedAppsCount(app_type);
}

void FamilyUserAppMetrics::OnAppTypeInitialized(apps::mojom::AppType app_type) {
  DCHECK(!base::Contains(ready_app_types_, app_type));
  ready_app_types_.insert(app_type);
  if (should_record_metrics_on_new_day_)
    RecordRecentlyUsedAppsCount(app_type);
}

void FamilyUserAppMetrics::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  DCHECK_EQ(cache, app_registry_);
  Observe(nullptr);
}

void FamilyUserAppMetrics::OnAppUpdate(const apps::AppUpdate& update) {}

bool FamilyUserAppMetrics::IsAppTypeReady(apps::mojom::AppType app_type) const {
  return base::Contains(ready_app_types_, app_type);
}

void FamilyUserAppMetrics::RecordInstalledExtensionsCount() {
  int extensions_count = 0;
  std::unique_ptr<extensions::ExtensionSet> all_installed_extensions =
      extension_registry_->GenerateInstalledExtensionsSet();
  for (const auto& extension : *all_installed_extensions) {
    if (extensions::Manifest::IsComponentLocation(extension->location()))
      continue;
    if (extension->is_extension() || extension->is_theme())
      extensions_count++;
  }
  // If a family user has more than a thousand extensions installed, then that
  // count is going into an overflow bucket. We don't expect this scenario to
  // happen often.
  base::UmaHistogramCounts1000(kInstalledExtensionsCountHistogramName,
                               extensions_count);
}

void FamilyUserAppMetrics::RecordEnabledExtensionsCount() {
  int extensions_count = 0;
  for (const auto& extension : extension_registry_->enabled_extensions()) {
    if (extensions::Manifest::IsComponentLocation(extension->location()))
      continue;
    if (extension->is_extension() || extension->is_theme())
      extensions_count++;
  }
  // If a family user has more than a thousand extensions enabled, then that
  // count is going into an overflow bucket. We don't expect this scenario to
  // happen often.
  base::UmaHistogramCounts1000(kEnabledExtensionsCountHistogramName,
                               extensions_count);
}

void FamilyUserAppMetrics::RecordRecentlyUsedAppsCount(
    apps::mojom::AppType app_type) {
  int app_count = 0;
  base::Time now = base::Time::Now();
  // The below will execute synchronously.
  app_registry_->ForEachApp(
      [app_type, now, &app_count](const apps::AppUpdate& update) {
        if (update.AppType() != app_type)
          return;
        // Only count apps that have been used recently.
        if (now - update.LastLaunchTime() > k28Days)
          return;
        app_count++;
      });
  // If a family user has more than a thousand apps installed, then that count
  // is going into an overflow bucket. We don't expect this scenario to happen
  // often.
  switch (app_type) {
    case apps::mojom::AppType::kArc:
      base::UmaHistogramCounts1000(kArcAppsCountHistogramName, app_count);
      break;
    case apps::mojom::AppType::kBorealis:
      base::UmaHistogramCounts1000(kBorealisAppsCountHistogramName, app_count);
      break;
    case apps::mojom::AppType::kCrostini:
      base::UmaHistogramCounts1000(kCrostiniAppsCountHistogramName, app_count);
      break;
    case apps::mojom::AppType::kExtension:
      // The InstalledExtensionsCount only includes regular browser
      // extensions and themes. This counter only includes apps. The two
      // counters are mutually exclusive.
      base::UmaHistogramCounts1000(kExtensionAppsCountHistogramName, app_count);
      break;
    case apps::mojom::AppType::kWeb:
      base::UmaHistogramCounts1000(kWebAppsCountHistogramName, app_count);
      break;
    default:
      // We're not interested in tracking other app types in detail.
      break;
  }
}

}  // namespace chromeos
