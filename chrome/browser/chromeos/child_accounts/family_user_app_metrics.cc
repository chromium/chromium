// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/family_user_app_metrics.h"

#include <memory>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"

namespace chromeos {

namespace {
constexpr base::TimeDelta k28Days = base::TimeDelta::FromDays(28);
}  // namespace

// static
// UMA metrics for a snapshot count of installed and enabled extensions for a
// given family user.
const char FamilyUserAppMetrics::kInstalledExtensionsCountHistogramName[] =
    "FamilyUser.InstalledExtensionsCount";
const char FamilyUserAppMetrics::kEnabledExtensionsCountHistogramName[] =
    "FamilyUser.EnabledExtensionsCount";

// UMA metrics for a snapshot count of installed apps for a given family user.
const char FamilyUserAppMetrics::kOtherAppsCountHistogramName[] =
    "FamilyUser.OtherAppsCount";
const char FamilyUserAppMetrics::kArcAppsCountHistogramName[] =
    "FamilyUser.ArcAppsCount";
const char FamilyUserAppMetrics::kBorealisAppsCountHistogramName[] =
    "FamilyUser.BorealisAppsCount";
const char FamilyUserAppMetrics::kCrostiniAppsCountHistogramName[] =
    "FamilyUser.CrostiniAppsCount";
const char FamilyUserAppMetrics::kExtensionAppsCountHistogramName[] =
    "FamilyUser.ExtensionAppsCount";
const char FamilyUserAppMetrics::kWebAppsCountHistogramName[] =
    "FamilyUser.WebAppsCount";
// Sum of the above.
const char FamilyUserAppMetrics::kTotalAppsCountHistogramName[] =
    "FamilyUser.TotalAppsCount";

FamilyUserAppMetrics::FamilyUserAppMetrics(Profile* profile)
    : extension_registry_(extensions::ExtensionRegistry::Get(profile)),
      app_registry_(&apps::AppServiceProxyFactory::GetForProfile(profile)
                         ->AppRegistryCache()) {
  DCHECK(extension_registry_);
  DCHECK(app_registry_);
}

FamilyUserAppMetrics::~FamilyUserAppMetrics() {
  if (on_new_day_) {
    RecordInstalledExtensionsCount();
    RecordEnabledExtensionsCount();
    RecordRecentlyUsedAppsCount();
  }
}

void FamilyUserAppMetrics::OnNewDay() {
  on_new_day_ = true;
}

void FamilyUserAppMetrics::RecordInstalledExtensionsCount() {
  int counter = 0;
  std::unique_ptr<extensions::ExtensionSet> all_installed_extensions =
      extension_registry_->GenerateInstalledExtensionsSet();
  for (const auto& extension : *all_installed_extensions) {
    if (extensions::Manifest::IsComponentLocation(extension->location()))
      continue;
    if (extension->is_extension() || extension->is_theme())
      counter++;
  }
  // If a family user has more than a thousand extensions installed, then that
  // count is going into an overflow bucket. We don't expect this scenario to
  // happen often.
  base::UmaHistogramCounts1000(kInstalledExtensionsCountHistogramName, counter);
}

void FamilyUserAppMetrics::RecordEnabledExtensionsCount() {
  int counter = 0;
  for (const auto& extension : extension_registry_->enabled_extensions()) {
    if (extensions::Manifest::IsComponentLocation(extension->location()))
      continue;
    if (extension->is_extension() || extension->is_theme())
      counter++;
  }
  // If a family user has more than a thousand extensions enabled, then that
  // count is going into an overflow bucket. We don't expect this scenario to
  // happen often.
  base::UmaHistogramCounts1000(kEnabledExtensionsCountHistogramName, counter);
}

void FamilyUserAppMetrics::RecordRecentlyUsedAppsCount() {
  int other_counter, arc_counter, borealis_counter, crostini_counter,
      extension_counter, web_counter, total_counter;
  other_counter = arc_counter = borealis_counter = crostini_counter =
      extension_counter = web_counter = total_counter = 0;
  app_registry_->ForEachApp([&other_counter, &arc_counter, &borealis_counter,
                             &crostini_counter, &extension_counter,
                             &web_counter,
                             &total_counter](const apps::AppUpdate& update) {
    // Only count apps that have been used recently.
    if (base::Time::Now() - update.LastLaunchTime() > k28Days)
      return;
    switch (update.AppType()) {
      case apps::mojom::AppType::kArc:
        arc_counter++;
        break;
      case apps::mojom::AppType::kBorealis:
        borealis_counter++;
        break;
      case apps::mojom::AppType::kCrostini:
        crostini_counter++;
        break;
      case apps::mojom::AppType::kExtension:
        // The InstalledExtensionsCount only includes regular browser
        // extensions and themes. This counter only includes apps. The two
        // counters are mutually exclusive.
        extension_counter++;
        break;
      case apps::mojom::AppType::kWeb:
        web_counter++;
        break;
      default:
        // We're not interested in tracking other app types in detail.
        other_counter++;
        break;
    }
    total_counter++;
  });
  // If a family user has more than a thousand apps installed, then that count
  // is going into an overflow bucket. We don't expect this scenario to happen
  // often.
  base::UmaHistogramCounts1000(kOtherAppsCountHistogramName, other_counter);
  base::UmaHistogramCounts1000(kArcAppsCountHistogramName, arc_counter);
  base::UmaHistogramCounts1000(kBorealisAppsCountHistogramName,
                               borealis_counter);
  base::UmaHistogramCounts1000(kCrostiniAppsCountHistogramName,
                               crostini_counter);
  base::UmaHistogramCounts1000(kExtensionAppsCountHistogramName,
                               extension_counter);
  base::UmaHistogramCounts1000(kWebAppsCountHistogramName, web_counter);
  base::UmaHistogramCounts1000(kTotalAppsCountHistogramName, total_counter);
}

}  // namespace chromeos
