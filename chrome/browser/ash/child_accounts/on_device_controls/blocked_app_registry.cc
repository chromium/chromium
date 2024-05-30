// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/on_device_controls/blocked_app_registry.h"

#include <algorithm>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "blocked_app_registry.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"

namespace ash::on_device_controls {

constexpr int kMaxUninstalledBlockedAppCount = 100;

namespace {

bool ShouldIncludeApp(apps::AppType app_type) {
  return app_type == apps::AppType::kArc;
}

}  // namespace

BlockedAppRegistry::BlockedAppRegistry(apps::AppServiceProxy* app_service,
                                       PrefService* pref_service)
    : store_(pref_service),
      app_service_(app_service),
      app_activity_watcher_(this, app_service) {
  CHECK(app_service_);

  registry_ = store_.GetFromPref();
  app_registry_cache_observer_.Observe(&GetAppCache());

  VLOG(1) << "app-controls: calling block apps to initialize the state in app "
             "service";
  app_service_->BlockApps(GetBlockedApps());
}

BlockedAppRegistry::~BlockedAppRegistry() = default;

void BlockedAppRegistry::AddApp(const std::string& app_id) {
  VLOG(1) << "app-controls: adding blocked app " << app_id;

  if (base::Contains(registry_, app_id)) {
    LOG(WARNING) << app_id << " already in blocked app registry";
    return;
  }
  registry_[app_id] = BlockedAppDetails();

  VLOG(1) << "app-controls: calling block app: " << app_id;
  app_service_->BlockApps({app_id});

  // TODO(b/338247185): Only update value that changed.
  store_.SaveToPref(registry_);
}

void BlockedAppRegistry::RemoveApp(const std::string& app_id) {
  VLOG(1) << "app-controls: removing blocked app " << app_id;

  if (!base::Contains(registry_, app_id)) {
    LOG(WARNING) << app_id << " not in blocked app registry";
    return;
  }
  registry_.erase(app_id);

  VLOG(1) << "app-controls: calling unblock app: " << app_id;
  app_service_->UnblockApps({app_id});

  // TODO(b/338247185): Only update value that changed.
  store_.SaveToPref(registry_);
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

  if (!registry_.at(app_id).IsInstalled()) {
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
  VLOG(1) << "app-controls: persisting apps before app service cache destroyed";
  store_.SaveToPref(registry_);
  app_registry_cache_observer_.Reset();
}

apps::AppRegistryCache& BlockedAppRegistry::GetAppCache() {
  return app_service_->AppRegistryCache();
}

void BlockedAppRegistry::OnAppReady(const std::string& app_id) {
  VLOG(1) << "app-controls: app ready " << app_id;
  if (GetAppState(app_id) == LocalAppState::kAvailable) {
    return;
  }

  if (GetAppState(app_id) == LocalAppState::kBlockedUninstalled) {
    // Clear the uninstall timestamp, but keep the initial blocked timestamp.
    registry_[app_id].MarkInstalled();

    // TODO(b/338247185): Only update value that changed.
    store_.SaveToPref(registry_);
  }

  VLOG(1) << "app-controls: calling block app: " << app_id;
  app_service_->BlockApps({app_id});
}

void BlockedAppRegistry::OnAppUninstalled(const std::string& app_id) {
  VLOG(1) << "app-controls: app uninstalled " << app_id;
  if (GetAppState(app_id) == LocalAppState::kAvailable) {
    return;
  }

  if (GetUninstalledBlockedAppCount() == kMaxUninstalledBlockedAppCount) {
    RemoveOldestUninstalledApp();
  }

  registry_[app_id].SetUninstallTimestamp(base::Time::Now());

  // TODO(b/338247185): Only update value that changed.
  store_.SaveToPref(registry_);
}

int BlockedAppRegistry::GetUninstalledBlockedAppCount() const {
  return std::count_if(
      registry_.begin(), registry_.end(),
      [](std::pair<std::string, BlockedAppDetails> const& blocked_app) {
        return !blocked_app.second.IsInstalled();
      });
}

void BlockedAppRegistry::RemoveOldestUninstalledApp() {
  base::Time oldest_uninstall_timestamp = base::Time::Now();
  std::string oldest_app;
  for (const auto& app : registry_) {
    if (!app.second.IsInstalled()) {
      base::Time uninstall_time = *app.second.uninstall_timestamp();
      if (uninstall_time < oldest_uninstall_timestamp) {
        oldest_uninstall_timestamp = uninstall_time;
        oldest_app = app.first;
      }
    }
  }

  if (oldest_app.empty()) {
    VLOG(1)
        << "app-controls: removing oldest uninstalled blocked app - not found";
    return;
  }

  VLOG(1) << "app-controls: removing oldest uninstalled blocked app "
          << oldest_app;
  RemoveApp(oldest_app);
}

}  // namespace ash::on_device_controls
