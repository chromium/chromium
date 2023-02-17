// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/metrics/app_discovery_metrics.h"

#include "base/logging.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_utils.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/metrics/structured/structured_events.h"
#include "components/services/app_service/public/cpp/instance.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_service.h"

namespace apps {
namespace {

namespace cros_events = metrics::structured::events::v2::cr_os_events;

}  // namespace

AppDiscoveryMetrics::AppDiscoveryMetrics(
    Profile* profile,
    InstanceRegistry& instance_registry,
    AppPlatformMetrics* app_platform_metrics)
    : profile_(profile), app_platform_metrics_(app_platform_metrics) {
  DCHECK(app_platform_metrics);

  apps::InstanceRegistry::Observer::Observe(&instance_registry);
  app_platform_metrics_->AddObserver(this);
}

AppDiscoveryMetrics::~AppDiscoveryMetrics() {
  if (app_platform_metrics_) {
    app_platform_metrics_->RemoveObserver(this);
  }
}

void AppDiscoveryMetrics::OnAppInstalled(const std::string& app_id,
                                         AppType app_type,
                                         InstallSource app_install_source,
                                         InstallReason app_install_reason,
                                         InstallTime app_install_time) {
  // Do not record if app-sync is disabled.
  if (!IsAppSyncEnabled()) {
    return;
  }

  cros_events::AppDiscovery_AppInstalled event;
  event.SetAppId(app_id)
      .SetAppType(static_cast<int>(app_type))
      .SetInstallSource(static_cast<int>(app_install_source))
      .SetInstallReason(static_cast<int>(app_install_reason));
  event.Record();
}

void AppDiscoveryMetrics::OnAppLaunched(const std::string& app_id,
                                        AppType app_type,
                                        LaunchSource launch_source) {
  // Do not record if app-sync is disabled.
  if (!IsAppSyncEnabled()) {
    return;
  }

  cros_events::AppDiscovery_AppLaunched event;
  event.SetAppId(app_id)
      .SetAppType(static_cast<int>(app_type))
      .SetLaunchSource(static_cast<int>(launch_source));
  event.Record();
}

void AppDiscoveryMetrics::OnAppUninstalled(
    const std::string& app_id,
    AppType app_type,
    UninstallSource app_uninstall_source) {
  // Do not record if app-sync is disabled.
  if (!IsAppSyncEnabled()) {
    return;
  }

  cros_events::AppDiscovery_AppUninstall event;
  event.SetAppId(app_id)
      .SetAppType(static_cast<int>(app_type))
      .SetUninstallSource(static_cast<int>(app_uninstall_source));
  event.Record();
}

void AppDiscoveryMetrics::OnAppPlatformMetricsDestroyed() {
  app_platform_metrics_ = nullptr;
}

void AppDiscoveryMetrics::OnInstanceUpdate(
    const InstanceUpdate& instance_update) {
  // No state changes. Ignore the update.
  if (!instance_update.StateChanged()) {
    return;
  }

  // Only record if app-sync is enabled. Recording is done before internal model
  // update to check for previous state.
  if (IsAppSyncEnabled()) {
    RecordAppState(instance_update);
  }

  // Apply state to internal model.

  // Instance is destroyed.
  if (instance_update.IsDestruction()) {
    instance_to_state_.erase(instance_update.InstanceId());
    app_id_to_instance_ids_[instance_update.AppId()].erase(
        instance_update.InstanceId());

    // If the set is now empty, all instances of the app are closed.
    // Remove app_id from the map and log app closed.
    if (app_id_to_instance_ids_[instance_update.AppId()].empty()) {
      app_id_to_instance_ids_.erase(instance_update.AppId());
    }

    return;
  }

  // First instance of the app requires set construction.
  if (app_id_to_instance_ids_.find(instance_update.AppId()) ==
      app_id_to_instance_ids_.end()) {
    app_id_to_instance_ids_[instance_update.AppId()] =
        std::set<base::UnguessableToken>();
  }

  app_id_to_instance_ids_[instance_update.AppId()].insert(
      instance_update.InstanceId());

  instance_to_state_[instance_update.InstanceId()] = instance_update.State();
}

void AppDiscoveryMetrics::OnInstanceRegistryWillBeDestroyed(
    InstanceRegistry* cache) {
  apps::InstanceRegistry::Observer::Observe(nullptr);
}

bool AppDiscoveryMetrics::IsAppSyncEnabled() {
  return ShouldRecordUkm(profile_);
}

bool AppDiscoveryMetrics::IsAnyAppInstanceActive(
    const std::string& app_id,
    absl::optional<base::UnguessableToken> exclude_instance_id) {
  bool is_any_instance_active = false;

  // App id not found.
  if (app_id_to_instance_ids_.find(app_id) == app_id_to_instance_ids_.end()) {
    return false;
  }

  for (const auto& instance_id : app_id_to_instance_ids_[app_id]) {
    if (instance_to_state_[instance_id] == InstanceState::kActive) {
      // Ignore excluded instance_id if it exists.
      if (!exclude_instance_id.has_value() ||
          exclude_instance_id.value() != instance_id) {
        is_any_instance_active = true;
        break;
      }
    }
  }

  return is_any_instance_active;
}

void AppDiscoveryMetrics::RecordAppState(
    const InstanceUpdate& instance_update) {
  InstanceState prev_state =
      instance_to_state_.find(instance_update.InstanceId()) ==
              instance_to_state_.end()
          ? InstanceState::kUnknown
          : instance_to_state_[instance_update.InstanceId()];

  switch (prev_state) {
    case kUnknown:
    case kStarted:
    case kRunning:
      RecordFromStartState(instance_update);
      return;
    case kVisible:
    case kHidden:
      RecordFromInactiveState(instance_update);
      return;
    case kDestroyed:
      // Previous state should not be destroyed.
      NOTREACHED();
      return;
    case kActive:
      RecordFromActiveState(instance_update);
      return;
  }
}

void AppDiscoveryMetrics::RecordFromInactiveState(
    const InstanceUpdate& instance_update) {
  bool is_any_instance_active = IsAnyAppInstanceActive(instance_update.AppId());

  switch (instance_update.State()) {
    case kUnknown:
    case kStarted:
    case kRunning:
    case kVisible:
    case kHidden:
      return;
    case kDestroyed:
      RecordAppClosed(instance_update);
      return;
    case kActive: {
      // Only record if there are no active instances of the app.
      if (!is_any_instance_active) {
        cros_events::AppDiscovery_AppStateChanged active_event;
        active_event.SetAppId(instance_update.AppId())
            .SetAppState(static_cast<int>(AppStateChange::kActive))
            .Record();
      }
      return;
    }
  }
}
void AppDiscoveryMetrics::RecordFromActiveState(
    const InstanceUpdate& instance_update) {
  bool is_any_instance_active = IsAnyAppInstanceActive(
      instance_update.AppId(), instance_update.InstanceId());

  switch (instance_update.State()) {
    case kUnknown:
    case kStarted:
    case kRunning:
    case kActive:
      return;
    case kDestroyed:
      RecordAppClosed(instance_update);
      return;
    case kVisible:
    case kHidden: {
      // Only record if there are no active instances of the app.
      if (!is_any_instance_active) {
        cros_events::AppDiscovery_AppStateChanged inactive_event;
        inactive_event.SetAppId(instance_update.AppId())
            .SetAppState(static_cast<int>(AppStateChange::kInactive))
            .Record();
      }
      return;
    }
  }
}

void AppDiscoveryMetrics::RecordFromStartState(
    const InstanceUpdate& instance_update) {
  bool is_any_instance_active = IsAnyAppInstanceActive(instance_update.AppId());

  switch (instance_update.State()) {
    case kActive: {
      // Record if no instances of app are already active.
      if (!is_any_instance_active) {
        cros_events::AppDiscovery_AppStateChanged active_event;
        active_event.SetAppId(instance_update.AppId())
            .SetAppState(static_cast<int>(AppStateChange::kActive))
            .Record();
      }

      return;
    }
    case kVisible:
    case kHidden: {
      // Only record if there aren't any active instances.
      if (!is_any_instance_active) {
        cros_events::AppDiscovery_AppStateChanged inactive_event;
        inactive_event.SetAppId(instance_update.AppId())
            .SetAppState(static_cast<int>(AppStateChange::kInactive))
            .Record();
      }
      return;
    }
    case kRunning:
    case kUnknown:
    case kStarted:
      return;
    case kDestroyed:
      RecordAppClosed(instance_update);
      return;
  }
}

void AppDiscoveryMetrics::RecordAppClosed(
    const InstanceUpdate& instance_update) {
  DCHECK(instance_update.IsDestruction());
  auto prev_instances = app_id_to_instance_ids_[instance_update.AppId()];

  // If instance_update is the only instance of the app.
  if (prev_instances.size() == 1) {
    cros_events::AppDiscovery_AppStateChanged app_state_change_event;
    app_state_change_event.SetAppId(instance_update.AppId())
        .SetAppState(static_cast<int>(AppStateChange::kClosed))
        .Record();
  }
}

}  // namespace apps
