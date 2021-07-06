// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/full_restore/arc_app_launch_handler.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/apps/app_service/app_platform_metrics.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/chromeos/full_restore/arc_window_handler.h"
#include "chrome/browser/chromeos/full_restore/arc_window_utils.h"
#include "chrome/browser/chromeos/full_restore/full_restore_app_launch_handler.h"
#include "chrome/browser/chromeos/full_restore/full_restore_arc_task_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "components/full_restore/app_launch_info.h"
#include "components/full_restore/full_restore_read_handler.h"
#include "components/full_restore/full_restore_save_handler.h"
#include "components/full_restore/restore_data.h"

namespace chromeos {
namespace full_restore {

ArcAppLaunchHandler::ArcAppLaunchHandler() = default;
ArcAppLaunchHandler::~ArcAppLaunchHandler() = default;

void ArcAppLaunchHandler::RestoreArcApps(
    FullRestoreAppLaunchHandler* app_launch_handler) {
  handler_ = app_launch_handler;

  window_handler_ = FullRestoreArcTaskHandler::GetForProfile(handler_->profile_)
                        ->window_handler();

  LoadRestoreData();

  if (windows_.empty() && no_stack_windows_.empty())
    return;

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
  if (base::Contains(app_ids_, update.AppId()))
    PrepareAppLaunching(update.AppId());
}

void ArcAppLaunchHandler::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  apps::AppRegistryCache::Observer::Observe(nullptr);
}

void ArcAppLaunchHandler::OnAppConnectionReady() {
  if (chromeos::ResourcedClient::Get() &&
      !resourced_client_observer_.IsObserving()) {
    resourced_client_observer_.Observe(chromeos::ResourcedClient::Get());
  }
}

void ArcAppLaunchHandler::LoadRestoreData() {
  apps::AppRegistryCache& cache =
      apps::AppServiceProxyFactory::GetForProfile(handler_->profile_)
          ->AppRegistryCache();

  for (const auto& it : handler_->restore_data_->app_id_to_launch_list()) {
    if (cache.GetAppType(it.first) != apps::mojom::AppType::kArc)
      continue;

    app_ids_.insert(it.first);
    for (const auto& data_it : it.second) {
      if (data_it.second->activation_index.has_value()) {
        windows_[data_it.second->activation_index.value()] =
            std::make_pair(it.first, data_it.first);
      } else {
        no_stack_windows_.insert(std::make_pair(it.first, data_it.first));
      }
    }
  }
}

void ArcAppLaunchHandler::PrepareAppLaunching(const std::string& app_id) {
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

}  // namespace full_restore
}  // namespace chromeos
