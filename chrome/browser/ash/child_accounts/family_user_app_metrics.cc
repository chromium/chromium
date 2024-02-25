// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/family_user_app_metrics.h"

#include <memory>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"

namespace ash {

namespace {
// Recently launched apps this many days ago in the past will be recorded.
constexpr base::TimeDelta kOneDay = base::Days(1);

// UMA metrics for a snapshot count of installed and enabled extensions for a
// given family user.
constexpr char kInstalledExtensionsCountHistogramName[] =
    "FamilyUser.InstalledExtensionsCount2";
constexpr char kEnabledExtensionsCountHistogramName[] =
    "FamilyUser.EnabledExtensionsCount2";

// UMA metrics for a snapshot count of installed apps for a given family user.
constexpr char kUnknownAppsCountHistogramName[] =
    "FamilyUser.UnknownAppsCount2";
constexpr char kArcAppsCountHistogramName[] = "FamilyUser.ArcAppsCount2";
constexpr char kBuiltInAppsCountHistogramName[] =
    "FamilyUser.BuiltInAppsCount2";
constexpr char kCrostiniAppsCountHistogramName[] =
    "FamilyUser.CrostiniAppsCount2";
// The InstalledExtensionsCount only includes regular browser extensions and
// themes. This counter only includes apps. The two counters are mutually
// exclusive.
constexpr char kExtensionAppsCountHistogramName[] =
    "FamilyUser.ExtensionAppsCount2";
constexpr char kWebAppsCountHistogramName[] = "FamilyUser.WebAppsCount2";
constexpr char kPluginVmAppsCountHistogramName[] =
    "FamilyUser.PluginVmAppsCount2";
constexpr char kStandaloneBrowserAppsCountHistogramName[] = "FamilyUser.LacrosAppsCount2";
constexpr char kRemoteAppsCountHistogramName[] = "FamilyUser.RemoteAppsCount2";
constexpr char kBorealisAppsCountHistogramName[] =
    "FamilyUser.BorealisAppsCount2";
constexpr char kBruschettaAppsCountHistogramName[] =
    "FamilyUser.BruschettaAppsCount2";
constexpr char kSystemWebAppsCountHistogramName[] =
    "FamilyUser.SystemWebAppsCount2";
constexpr char kStandaloneBrowserChromeAppCountHistogramName[] =
    "FamilyUser.LacrosChromeAppsCount2";

// TODO(agawronska): Add metrics for extensions, possibly differentiating Ash
// from Lacros (AKA StandaloneBrowser).

const char* GetAppsCountHistogramName(apps::AppType app_type) {
  switch (app_type) {
    case apps::AppType::kUnknown:
    // Extensions are recorded separately, and AppService only has some
    // extensions with file browser handlers.
    case apps::AppType::kExtension:
    case apps::AppType::kStandaloneBrowserExtension:
      return kUnknownAppsCountHistogramName;
    case apps::AppType::kArc:
      return kArcAppsCountHistogramName;
    case apps::AppType::kBuiltIn:
      return kBuiltInAppsCountHistogramName;
    case apps::AppType::kCrostini:
      return kCrostiniAppsCountHistogramName;
    case apps::AppType::kChromeApp:
      return kExtensionAppsCountHistogramName;
    case apps::AppType::kWeb:
      return kWebAppsCountHistogramName;
    case apps::AppType::kPluginVm:
      return kPluginVmAppsCountHistogramName;
    case apps::AppType::kStandaloneBrowser:
      return kStandaloneBrowserAppsCountHistogramName;
    case apps::AppType::kRemote:
      return kRemoteAppsCountHistogramName;
    case apps::AppType::kBorealis:
      return kBorealisAppsCountHistogramName;
    case apps::AppType::kBruschetta:
      return kBruschettaAppsCountHistogramName;
    case apps::AppType::kSystemWeb:
      return kSystemWebAppsCountHistogramName;
    case apps::AppType::kStandaloneBrowserChromeApp:
      return kStandaloneBrowserChromeAppCountHistogramName;
  }
}

}  // namespace

// static
std::unique_ptr<FamilyUserAppMetrics> FamilyUserAppMetrics::Create(
    Profile* profile) {
  auto metrics = base::WrapUnique(new FamilyUserAppMetrics(profile));
  metrics->Init();
  return metrics;
}

FamilyUserAppMetrics::FamilyUserAppMetrics(Profile* profile)
    : extension_registry_(extensions::ExtensionRegistry::Get(profile)),
      app_registry_(&apps::AppServiceProxyFactory::GetForProfile(profile)
                         ->AppRegistryCache()),
      instance_registry_(&apps::AppServiceProxyFactory::GetForProfile(profile)
                              ->InstanceRegistry()),
      first_report_on_current_device_(
          user_manager::UserManager::Get()->IsCurrentUserNew()) {
  DCHECK(extension_registry_);
  DCHECK(app_registry_);
  app_registry_cache_observer_.Observe(app_registry_);
  DCHECK(instance_registry_);
}

FamilyUserAppMetrics::~FamilyUserAppMetrics() = default;

// static
const char*
FamilyUserAppMetrics::GetInstalledExtensionsCountHistogramNameForTest() {
  return kInstalledExtensionsCountHistogramName;
}
const char*
FamilyUserAppMetrics::GetEnabledExtensionsCountHistogramNameForTest() {
  return kEnabledExtensionsCountHistogramName;
}

// static
const char* FamilyUserAppMetrics::GetAppsCountHistogramNameForTest(
    apps::AppType app_type) {
  return GetAppsCountHistogramName(app_type);
}

void FamilyUserAppMetrics::Init() {
  for (const auto app_type : app_registry_->InitializedAppTypes()) {
    OnAppTypeInitialized(app_type);
  }
}

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

void FamilyUserAppMetrics::OnAppTypeInitialized(apps::AppType app_type) {
  DCHECK(!base::Contains(ready_app_types_, app_type));
  // Skip the extension app type, because extensions are recorded separately,
  // and AppService only has some extensions with file browser handlers.
  if (app_type == apps::AppType::kExtension)
    return;

  ready_app_types_.insert(app_type);
  if (should_record_metrics_on_new_day_)
    RecordRecentlyUsedAppsCount(app_type);
}

void FamilyUserAppMetrics::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  DCHECK_EQ(cache, app_registry_);
  app_registry_cache_observer_.Reset();
}

void FamilyUserAppMetrics::OnAppUpdate(const apps::AppUpdate& update) {}

bool FamilyUserAppMetrics::IsAppTypeReady(apps::AppType app_type) const {
  return base::Contains(ready_app_types_, app_type);
}

void FamilyUserAppMetrics::RecordInstalledExtensionsCount() {
  int extensions_count = 0;
  const extensions::ExtensionSet all_installed_extensions =
      extension_registry_->GenerateInstalledExtensionsSet();
  for (const auto& extension : all_installed_extensions) {
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

void FamilyUserAppMetrics::RecordRecentlyUsedAppsCount(apps::AppType app_type) {
  int app_count = 0;
  base::Time now = base::Time::Now();
  // The below will execute synchronously.
  app_registry_->ForEachApp(
      [app_type, now, this, &app_count](const apps::AppUpdate& update) {
        if (update.AppType() != app_type)
          return;
        // Only count apps that have been used recently.
        if (now - update.LastLaunchTime() <= kOneDay ||
            IsAppWindowOpen(update.AppId())) {
          app_count++;
        }
      });
  // If a family user has more than a thousand apps installed, then that count
  // is going into an overflow bucket. We don't expect this scenario to happen
  // often.
  const std::string histogram_name = GetAppsCountHistogramName(app_type);
  base::UmaHistogramCounts1000(histogram_name, app_count);
}

bool FamilyUserAppMetrics::IsAppWindowOpen(const std::string& app_id) {
  // An app is active if it has an open window.
  return instance_registry_->ContainsAppId(app_id);
}

}  // namespace ash
