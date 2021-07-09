// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/full_restore/arc_app_launch_handler.h"

#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/contains.h"
#include "chrome/browser/apps/app_service/app_platform_metrics.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/full_restore/arc_window_handler.h"
#include "chrome/browser/chromeos/full_restore/arc_window_utils.h"
#include "chrome/browser/chromeos/full_restore/full_restore_app_launch_handler.h"
#include "chrome/browser/chromeos/full_restore/full_restore_arc_task_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "components/full_restore/app_launch_info.h"
#include "components/full_restore/full_restore_read_handler.h"
#include "components/full_restore/restore_data.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

namespace chromeos {
namespace full_restore {

namespace {

constexpr int kCpuUsageRefreshIntervalInSeconds = 1;
constexpr int kCpuUsageCountWindowLength =
    6 * kCpuUsageRefreshIntervalInSeconds;

}  // namespace

ArcAppLaunchHandler::ArcAppLaunchHandler() = default;
ArcAppLaunchHandler::~ArcAppLaunchHandler() = default;

void ArcAppLaunchHandler::RestoreArcApps(
    FullRestoreAppLaunchHandler* app_launch_handler) {
  DCHECK(app_launch_handler);
  handler_ = app_launch_handler;

  LoadRestoreData();

  if (!HasRestoreData())
    return;

  window_handler_ = FullRestoreArcTaskHandler::GetForProfile(handler_->profile_)
                        ->window_handler();

  DCHECK(apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
      handler_->profile_));

  apps::AppRegistryCache& cache =
      apps::AppServiceProxyFactory::GetForProfile(handler_->profile_)
          ->AppRegistryCache();

  // Observe AppRegistryCache to get the notification when the app is ready.
  if (!app_registry_cache_observer_.IsObserving())
    app_registry_cache_observer_.Observe(&cache);

  // Add the app to `app_ids` if there is a launch list from the restore data
  // for the app.
  std::set<std::string> app_ids;
  cache.ForEachApp([&app_ids, this](const apps::AppUpdate& update) {
    if (update.Readiness() == apps::mojom::Readiness::kReady &&
        app_ids_.find(update.AppId()) != app_ids_.end()) {
      app_ids.insert(update.AppId());
    }
  });

  for (const auto& app_id : app_ids)
    PrepareAppLaunching(app_id);
}

void ArcAppLaunchHandler::OnAppUpdate(const apps::AppUpdate& update) {
  if (!HasRestoreData() || !update.ReadinessChanged())
    return;

  if (!apps_util::IsInstalled(update.Readiness())) {
    RemoveApp(update.AppId());
    return;
  }

  // If the app is not ready, don't launch the app for the restoration.
  if (update.Readiness() != apps::mojom::Readiness::kReady)
    return;

  if (base::Contains(app_ids_, update.AppId()))
    PrepareAppLaunching(update.AppId());
}

void ArcAppLaunchHandler::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  apps::AppRegistryCache::Observer::Observe(nullptr);
}

void ArcAppLaunchHandler::OnAppConnectionReady() {
  if (!HasRestoreData())
    return;

  // Receive the memory pressure level.
  if (chromeos::ResourcedClient::Get() &&
      !resourced_client_observer_.IsObserving()) {
    resourced_client_observer_.Observe(chromeos::ResourcedClient::Get());
  }

  // Receive the system CPU usage rate.
  if (!probe_service_ || !probe_service_.is_connected()) {
    cros_healthd::ServiceConnection::GetInstance()->GetProbeService(
        probe_service_.BindNewPipeAndPassReceiver());
    probe_service_.set_disconnect_handler(
        base::BindOnce(&ArcAppLaunchHandler::OnProbeServiceDisconnect,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  StartCpuUsageCount();
}

void ArcAppLaunchHandler::LoadRestoreData() {
  DCHECK(handler_);
  apps::AppRegistryCache& cache =
      apps::AppServiceProxyFactory::GetForProfile(handler_->profile_)
          ->AppRegistryCache();

  for (const auto& it : handler_->restore_data_->app_id_to_launch_list()) {
    if (cache.GetAppType(it.first) != apps::mojom::AppType::kArc)
      continue;

    app_ids_.insert(it.first);
    for (const auto& data_it : it.second) {
      if (data_it.second->activation_index.has_value()) {
        windows_[data_it.second->activation_index.value()] = {it.first,
                                                              data_it.first};
      } else {
        no_stack_windows_.push_back({it.first, data_it.first});
      }
    }
  }
}

void ArcAppLaunchHandler::PrepareAppLaunching(const std::string& app_id) {
  DCHECK(handler_);
  app_ids_.erase(app_id);

  const auto it = handler_->restore_data_->app_id_to_launch_list().find(app_id);
  if (it == handler_->restore_data_->app_id_to_launch_list().end())
    return;

  if (it->second.empty()) {
    handler_->restore_data_->RemoveApp(app_id);
    return;
  }

  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(handler_->profile_);
  DCHECK(proxy);
  auto* arc_handler =
      FullRestoreArcTaskHandler::GetForProfile(handler_->profile_);

  for (const auto& data_it : it->second) {
    handler_->RecordRestoredAppLaunch(apps::AppTypeName::kArc);

    DCHECK(data_it.second->event_flag.has_value());

    apps::mojom::WindowInfoPtr window_info =
        HandleArcWindowInfo(data_it.second->GetAppWindowInfo());

    // Set an ARC session id to find the restore window id based on the new
    // created ARC task id in FullRestoreReadHandler.
    int32_t arc_session_id =
        ::full_restore::FullRestoreReadHandler::GetInstance()
            ->GetArcSessionId();
    window_info->window_id = arc_session_id;
    ::full_restore::FullRestoreReadHandler::GetInstance()
        ->SetArcSessionIdForWindowId(arc_session_id, data_it.first);
    window_id_to_session_id_[data_it.first] = arc_session_id;
    session_id_to_window_id_[arc_session_id] = data_it.first;

    bool launch_ghost_window = false;
#if BUILDFLAG(ENABLE_WAYLAND_SERVER)
    if (!window_info->bounds.is_null() && arc_handler &&
        arc_handler->window_handler()) {
      handler_->RecordArcGhostWindowLaunch(/*is_arc_ghost_window=*/true);
      arc_handler->window_handler()->LaunchArcGhostWindow(
          app_id, arc_session_id, data_it.second.get());
      launch_ghost_window = true;
    } else {
      handler_->RecordArcGhostWindowLaunch(/*is_arc_ghost_window=*/false);
    }
#endif

    if (launch_ghost_window)
      continue;

    if (data_it.second->intent.has_value()) {
      proxy->LaunchAppWithIntent(app_id, data_it.second->event_flag.value(),
                                 std::move(data_it.second->intent.value()),
                                 apps::mojom::LaunchSource::kFromFullRestore,
                                 std::move(window_info));
    } else {
      proxy->Launch(app_id, data_it.second->event_flag.value(),
                    apps::mojom::LaunchSource::kFromFullRestore,
                    std::move(window_info));
    }
  }
}

void ArcAppLaunchHandler::OnMemoryPressure(
    chromeos::ResourcedClient::PressureLevel level,
    uint64_t reclaim_target_kb) {
  pressure_level_ = level;
}

bool ArcAppLaunchHandler::HasRestoreData() {
  return !(windows_.empty() && no_stack_windows_.empty());
}

bool ArcAppLaunchHandler::CanLaunchApp() {
  switch (pressure_level_) {
    case chromeos::ResourcedClient::PressureLevel::NONE:
      return true;
    case chromeos::ResourcedClient::PressureLevel::MODERATE:
    case chromeos::ResourcedClient::PressureLevel::CRITICAL:
      return false;
  }
}

bool ArcAppLaunchHandler::IsAppReady(const std::string& app_id) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(handler_->profile_);
  if (!prefs)
    return false;

  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
  if (!app_info || app_info->suspended || !app_info->ready)
    return false;

  return true;
}

void ArcAppLaunchHandler::LaunchApp(const std::string& app_id,
                                    int32_t window_id) {
  DCHECK(handler_);
  const auto it = handler_->restore_data_->app_id_to_launch_list().find(app_id);
  if (it == handler_->restore_data_->app_id_to_launch_list().end())
    return;

  if (it->second.empty()) {
    handler_->restore_data_->RemoveApp(app_id);
    return;
  }

  const auto data_it = it->second.find(window_id);
  if (data_it == it->second.end())
    return;

  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(handler_->profile_);
  DCHECK(proxy);

  DCHECK(data_it->second->event_flag.has_value());

  apps::mojom::WindowInfoPtr window_info =
      HandleArcWindowInfo(data_it->second->GetAppWindowInfo());
  const auto window_it = window_id_to_session_id_.find(window_id);
  if (window_it != window_id_to_session_id_.end()) {
    window_info->window_id = window_it->second;
    window_id_to_session_id_.erase(window_it);
  } else {
    // Set an ARC session id to find the restore window id based on the new
    // created ARC task id in FullRestoreReadHandler.
    int32_t arc_session_id =
        ::full_restore::FullRestoreReadHandler::GetInstance()
            ->GetArcSessionId();
    window_info->window_id = arc_session_id;
    ::full_restore::FullRestoreReadHandler::GetInstance()
        ->SetArcSessionIdForWindowId(arc_session_id, window_id);
    window_id_to_session_id_[window_id] = arc_session_id;
  }

  if (data_it->second->intent.has_value()) {
    proxy->LaunchAppWithIntent(app_id, data_it->second->event_flag.value(),
                               std::move(data_it->second->intent.value()),
                               apps::mojom::LaunchSource::kFromFullRestore,
                               std::move(window_info));
  } else {
    proxy->Launch(app_id, data_it->second->event_flag.value(),
                  apps::mojom::LaunchSource::kFromFullRestore,
                  std::move(window_info));
  }
}

void ArcAppLaunchHandler::RemoveApp(const std::string& app_id) {
  app_ids_.erase(app_id);
  std::vector<int32_t> window_stacks;
  for (auto& it : windows_) {
    if (it.second.app_id == app_id)
      window_stacks.push_back(it.first);
  }

  for (auto window_stack : window_stacks)
    windows_.erase(window_stack);

  std::vector<std::list<WindowInfo>::iterator> windows;
  for (auto it = no_stack_windows_.begin(); it != no_stack_windows_.end();
       ++it) {
    if (it->app_id == app_id)
      windows.push_back(it);
  }

  for (auto it : windows)
    no_stack_windows_.erase(it);
}

int ArcAppLaunchHandler::GetCpuUsageRate() {
  uint64_t idle = 0, sum = 0;
  for (const auto& tick : cpu_tick_window_) {
    idle += tick.idle_time;
    sum += tick.idle_time + tick.used_time;
  }

  // Convert to xx% percentage.
  return sum ? int(100 * (sum - idle) / sum) : 0;
}

void ArcAppLaunchHandler::StartCpuUsageCount() {
  cpu_tick_count_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(kCpuUsageRefreshIntervalInSeconds),
      base::BindRepeating(&ArcAppLaunchHandler::UpdateCpuUsage,
                          base::Unretained(this)));
}

void ArcAppLaunchHandler::StopCpuUsageCount() {
  cpu_tick_count_timer_.Stop();
}

void ArcAppLaunchHandler::UpdateCpuUsage() {
  probe_service_->ProbeTelemetryInfo(
      {chromeos::cros_healthd::mojom::ProbeCategoryEnum::kCpu},
      base::BindOnce(&ArcAppLaunchHandler::OnCpuUsageUpdated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcAppLaunchHandler::OnCpuUsageUpdated(
    chromeos::cros_healthd::mojom::TelemetryInfoPtr info_ptr) {
  CpuTick tick;
  // For simplicity, assume that device has only one physical CPU.
  for (const auto& logical_cpu :
       info_ptr->cpu_result->get_cpu_info()->physical_cpus[0]->logical_cpus) {
    tick.idle_time += logical_cpu->idle_time_user_hz;
    tick.used_time +=
        logical_cpu->user_time_user_hz + logical_cpu->system_time_user_hz;
  }

  if (last_cpu_tick_.has_value())
    cpu_tick_window_.push_back(tick - last_cpu_tick_.value());
  last_cpu_tick_ = tick;

  // Sliding window for CPU usage count.
  while (cpu_tick_window_.size() > kCpuUsageCountWindowLength)
    cpu_tick_window_.pop_front();
}

void ArcAppLaunchHandler::OnProbeServiceDisconnect() {
  probe_service_.reset();
}

}  // namespace full_restore
}  // namespace chromeos
