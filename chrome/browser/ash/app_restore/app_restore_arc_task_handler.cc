// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/app_restore_arc_task_handler.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/app_restore/app_restore_arc_task_handler_factory.h"
#include "chrome/browser/ash/app_restore/arc_app_launch_handler.h"
#include "chrome/browser/ash/app_restore/arc_ghost_window_handler.h"
#include "chrome/browser/ash/app_restore/arc_window_utils.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/app_restore/app_restore_arc_info.h"
#include "components/app_restore/features.h"

namespace ash {
namespace app_restore {
namespace {

constexpr int kFullRestoreId = -1;
constexpr int kArcWindowPredictorId = -2;

::app_restore::AppRestoreArcInfo* GetAppRestoreArcInfo() {
  return ::app_restore::AppRestoreArcInfo::GetInstance();
}

}  // namespace

// static
AppRestoreArcTaskHandler* AppRestoreArcTaskHandler::GetForProfile(
    Profile* profile) {
  return AppRestoreArcTaskHandlerFactory::GetForProfile(profile);
}

AppRestoreArcTaskHandler::AppRestoreArcTaskHandler(Profile* profile) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile);
  if (!prefs)
    return;

  arc_prefs_observer_.Observe(prefs);

#if BUILDFLAG(ENABLE_WAYLAND_SERVER)
  if (full_restore::IsArcGhostWindowEnabled())
    window_handler_ = std::make_unique<full_restore::ArcGhostWindowHandler>();
#endif

  arc_app_launch_handlers_[kFullRestoreId] =
      std::make_unique<ArcAppLaunchHandler>();
  full_restore_arc_app_launch_handler_observer_ =
      arc_app_launch_handlers_[kFullRestoreId].get();

  // TODO(sstan): Modify ArcAppLaunchHandler to prevent redundant launch.
  if (::full_restore::features::IsArcWindowPredictorEnabled()) {
    arc_app_launch_handlers_[kArcWindowPredictorId] =
        std::make_unique<ArcAppLaunchHandler>();
    window_predictor_arc_app_launch_handler_observer_ =
        arc_app_launch_handlers_[kArcWindowPredictorId].get();
  }
  arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  // arc::ArcSessionManager might not be set in tests.
  if (arc_session_manager)
    arc_session_manager->AddObserver(this);
}

ArcAppLaunchHandler*
AppRestoreArcTaskHandler::GetDeskTemplateArcAppLaunchHandler(
    int32_t launch_id) {
  DCHECK_GT(launch_id, 0);
  auto& handler = arc_app_launch_handlers_[launch_id];
  if (!handler) {
    // We haven't seen this launch id before. Create a new entry.
    handler = std::make_unique<ArcAppLaunchHandler>();

    handler->OnArcPlayStoreEnabledChanged(arc_play_store_enabled_);
    if (app_connection_ready_)
      handler->OnAppConnectionReady();
    if (shelf_ready_)
      handler->OnShelfReady();
  }

  return handler.get();
}

void AppRestoreArcTaskHandler::ClearDeskTemplateArcAppLaunchHandler(
    int32_t launch_id) {
  DCHECK_GT(launch_id, 0);
  arc_app_launch_handlers_.erase(launch_id);
}

AppRestoreArcTaskHandler::~AppRestoreArcTaskHandler() {
  arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  // arc::ArcSessionManager may be released first.
  if (arc_session_manager)
    arc_session_manager->RemoveObserver(this);
}

bool AppRestoreArcTaskHandler::IsAppPendingRestore(
    const std::string& arc_app_id) const {
  for (auto& handler : arc_app_launch_handlers_) {
    if (handler.second->IsAppPendingRestore(arc_app_id))
      return true;
  }
  return false;
}

void AppRestoreArcTaskHandler::OnTaskCreated(int32_t task_id,
                                             const std::string& package_name,
                                             const std::string& activity,
                                             const std::string& intent,
                                             int32_t session_id) {
  const std::string app_id = ArcAppListPrefs::GetAppId(package_name, activity);
  GetAppRestoreArcInfo()->NotifyTaskCreated(app_id, task_id, session_id);
}

void AppRestoreArcTaskHandler::OnTaskDestroyed(int32_t task_id) {
  GetAppRestoreArcInfo()->NotifyTaskDestroyed(task_id);
}

void AppRestoreArcTaskHandler::OnTaskDescriptionChanged(
    int32_t task_id,
    const std::string& label,
    const arc::mojom::RawIconPngData& icon,
    uint32_t primary_color,
    uint32_t status_bar_color) {
  GetAppRestoreArcInfo()->NotifyTaskThemeColorUpdated(task_id, primary_color,
                                                      status_bar_color);
}

void AppRestoreArcTaskHandler::OnAppConnectionReady() {
  app_connection_ready_ = true;

#if BUILDFLAG(ENABLE_WAYLAND_SERVER)
  if (window_handler_)
    window_handler_->OnAppInstanceConnected();
#endif

  for (auto& handler : arc_app_launch_handlers_)
    handler.second->OnAppConnectionReady();

  GetAppRestoreArcInfo()->NotifyArcConnectionChanged(
      /*is_connection_ready=*/true);
}

void AppRestoreArcTaskHandler::OnAppConnectionClosed() {
  GetAppRestoreArcInfo()->NotifyArcConnectionChanged(
      /*is_connection_ready=*/false);
}

void AppRestoreArcTaskHandler::OnArcAppListPrefsDestroyed() {
  arc_prefs_observer_.Reset();
  Shutdown();
}

void AppRestoreArcTaskHandler::OnArcPlayStoreEnabledChanged(bool enabled) {
  arc_play_store_enabled_ = enabled;

  for (auto& handler : arc_app_launch_handlers_)
    handler.second->OnArcPlayStoreEnabledChanged(enabled);

  GetAppRestoreArcInfo()->NotifyPlayStoreEnabledChanged(enabled);
}

void AppRestoreArcTaskHandler::OnShelfReady() {
  shelf_ready_ = true;

  for (auto& handler : arc_app_launch_handlers_)
    handler.second->OnShelfReady();
}

void AppRestoreArcTaskHandler::Shutdown() {
  arc_app_launch_handlers_.clear();
  full_restore_arc_app_launch_handler_observer_ = nullptr;
  window_predictor_arc_app_launch_handler_observer_ = nullptr;

  window_handler_.reset();
}

void AppRestoreArcTaskHandler::CreateFullRestoreHandlerForTest() {
  auto& full_restore_handler = arc_app_launch_handlers_[kFullRestoreId];
  if (!full_restore_handler) {
    full_restore_handler = std::make_unique<ArcAppLaunchHandler>();
    full_restore_arc_app_launch_handler_observer_ = full_restore_handler.get();
  }
}

}  // namespace app_restore
}  // namespace ash
