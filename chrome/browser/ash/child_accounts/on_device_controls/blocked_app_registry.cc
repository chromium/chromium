// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/on_device_controls/blocked_app_registry.h"

#include "ash/constants/ash_pref_names.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"

namespace ash::on_device_controls {

namespace {

bool ShouldIncludeApp(apps::AppType app_type) {
  return app_type == apps::AppType::kArc;
}

}  // namespace

BlockedAppRegistry::AppDetails::AppDetails()
    : AppDetails(base::TimeTicks::Now()) {}

BlockedAppRegistry::AppDetails::AppDetails(base::TimeTicks block_timestamp)
    : block_timestamp(block_timestamp) {}

BlockedAppRegistry::AppDetails::~AppDetails() = default;

// static
void BlockedAppRegistry::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kOnDeviceAppControlsBlockedApps);
}

BlockedAppRegistry::BlockedAppRegistry(apps::AppServiceProxy* app_service,
                                       PrefService* pref_service)
    : app_service_(app_service), pref_service_(pref_service) {
  CHECK(app_service_);
  CHECK(pref_service_);

  app_registry_cache_observer_.Observe(&GetAppCache());
}

BlockedAppRegistry::~BlockedAppRegistry() = default;

void BlockedAppRegistry::AddApp(const std::string& app_id) {
  if (base::Contains(registry_, app_id)) {
    LOG(WARNING) << app_id << " already in blocked app registry";
    return;
  }
  registry_[app_id] = AppDetails();

  // TODO(b/338247185): Update pref state.
}

void BlockedAppRegistry::RemoveApp(const std::string& app_id) {
  if (!base::Contains(registry_, app_id)) {
    LOG(WARNING) << app_id << " not in blocked app registry";
    return;
  }
  registry_.erase(app_id);

  // TODO(b/338247185): Update pref state.
}

std::set<std::string> BlockedAppRegistry::GetBlockedApps() {
  std::set<std::string> blocked_app_ids_;
  for (const auto& app : registry_) {
    blocked_app_ids_.insert(app.first);
  }
  return blocked_app_ids_;
}

LocalAppState BlockedAppRegistry::GetAppState(const std::string& app_id) const {
  if (!base::Contains(registry_, app_id)) {
    return LocalAppState::kAvailable;
  }

  if (registry_.at(app_id).uninstall_timestamp.has_value()) {
    return LocalAppState::kBlockedUninstalled;
  }

  return LocalAppState::kBlocked;
}

void BlockedAppRegistry::OnAppUpdate(const apps::AppUpdate& update) {
  if (!update.ReadinessChanged()) {
    return;
  }

  if (!ShouldIncludeApp(update.AppType())) {
    return;
  }

  const std::string app_id = update.AppId();
  switch (update.Readiness()) {
    case apps::Readiness::kReady:
      OnAppReady(app_id);
      break;
    case apps::Readiness::kUninstalledByUser:
    case apps::Readiness::kUninstalledByNonUser:
      OnAppUninstalled(app_id);
      break;
    default:
      break;
  }
}
void BlockedAppRegistry::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}

apps::AppRegistryCache& BlockedAppRegistry::GetAppCache() {
  return app_service_->AppRegistryCache();
}

void BlockedAppRegistry::OnAppReady(const std::string& app_id) {
  if (GetAppState(app_id) == LocalAppState::kAvailable) {
    return;
  }

  if (GetAppState(app_id) == LocalAppState::kBlockedUninstalled) {
    // Clear the uninstall timestamp, but keep the initial blocked timestamp.
    registry_[app_id].uninstall_timestamp.reset();
    return;
  }

  // TODO(b/332963662): Call block app in AppService.
  // TODO(b/338247185): Update pref state.
}

void BlockedAppRegistry::OnAppUninstalled(const std::string& app_id) {
  if (GetAppState(app_id) == LocalAppState::kAvailable) {
    return;
  }

  registry_[app_id].uninstall_timestamp = base::TimeTicks::Now();

  // TODO(b/338247185): Update pref state.
}

}  // namespace ash::on_device_controls
