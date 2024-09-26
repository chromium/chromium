// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/app_restore_arc_task_handler.h"

#include "ash/components/arc/arc_features.h"
#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/app_restore/arc_app_queue_restore_handler.h"
#include "chrome/browser/ash/app_restore/arc_app_single_restore_handler.h"
#include "chrome/browser/ash/app_restore/arc_ghost_window_handler.h"
#include "chrome/browser/ash/app_restore/arc_window_utils.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/app_restore/app_restore_arc_info.h"
#include "components/app_restore/features.h"

namespace ash::app_restore {

namespace {

::app_restore::AppRestoreArcInfo* GetAppRestoreArcInfo() {
  return ::app_restore::AppRestoreArcInfo::GetInstance();
}

constexpr LauncherTag kFullRestoreLaunchHandlerTag = {
    LauncherType::kFullRestore, 0};

}  // namespace

AppRestoreArcTaskHandler::AppRestoreArcTaskHandler(Profile* profile) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile);
  if (!prefs)
    return;

  arc_prefs_observer_.Observe(prefs);

  if (full_restore::IsArcGhostWindowEnabled())
    window_handler_ = std::make_unique<full_restore::ArcGhostWindowHandler>();

  // Create full restore arc app launch handler.
  GetFullRestoreArcAppQueueRestoreHandler();

  arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  // arc::ArcSessionManager might not be set in tests.
  if (arc_session_manager)
    arc_session_manager->AddObserver(this);
}

ArcAppQueueRestoreHandler*
AppRestoreArcTaskHandler::GetDeskTemplateArcAppQueueRestoreHandler(
    int32_t launch_id) {
  DCHECK_GT(launch_id, 0);

  return CreateOrGetArcAppQueueRestoreHandler(
      {LauncherType::kDeskTemplate, launch_id}, /*call_init_callback=*/true);
}

void AppRestoreArcTaskHandler::ClearDeskTemplateArcAppQueueRestoreHandler(
    int32_t launch_id) {
  DCHECK_GT(launch_id, 0);
  arc_app_queue_restore_handlers_.erase(
      {LauncherType::kDeskTemplate, launch_id});
}

AppRestoreArcTaskHandler::~AppRestoreArcTaskHandler() {
  arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  // arc::ArcSessionManager may be released first.
  if (arc_session_manager)
    arc_session_manager->RemoveObserver(this);
}

bool AppRestoreArcTaskHandler::IsAppPendingRestore(
    const std::string& arc_app_id) const {
  for (auto& [unused, launcher] : arc_app_queue_restore_handlers_) {
    if (launcher->IsAppPendingRestore(arc_app_id))
      return true;
  }
  for (auto& [unused, launcher] : arc_app_single_restore_handlers_) {
    if (launcher->IsAppPendingRestore(arc_app_id))
      return true;
  }
  return false;
}

ArcAppQueueRestoreHandler*
AppRestoreArcTaskHandler::GetFullRestoreArcAppQueueRestoreHandler() {
  return CreateOrGetArcAppQueueRestoreHandler(kFullRestoreLaunchHandlerTag,
                                              /*call_init_callback=*/false);
}

ArcAppSingleRestoreHandler*
AppRestoreArcTaskHandler::GetWindowPredictorArcAppRestoreHandler(
    int32_t launch_id) {
  return CreateOrGetArcAppSingleRestoreHandler(
      {LauncherType::kWindowPredictor, launch_id});
}

void AppRestoreArcTaskHandler::OnAppStatesChanged(
    const std::string& id,
    const ArcAppListPrefs::AppInfo& app_info) {
  if (window_handler_)
    window_handler_->OnAppStatesUpdate(id, app_info.ready, app_info.need_fixup);
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

  if (window_handler_)
    window_handler_->OnAppInstanceConnected();

  for (auto& [unused, launcher] : arc_app_queue_restore_handlers_)
    launcher->OnAppConnectionReady();

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

  for (auto& [unused, launcher] : arc_app_queue_restore_handlers_)
    launcher->OnArcPlayStoreEnabledChanged(enabled);

  GetAppRestoreArcInfo()->NotifyPlayStoreEnabledChanged(enabled);
}

void AppRestoreArcTaskHandler::OnShelfReady() {
  shelf_ready_ = true;

  for (auto& [unused, launcher] : arc_app_queue_restore_handlers_)
    launcher->OnShelfReady();

  for (auto& [unused, launcher] : arc_app_single_restore_handlers_)
    launcher->OnShelfReady();
}

void AppRestoreArcTaskHandler::Shutdown() {
  arc_app_queue_restore_handlers_.clear();
  window_handler_.reset();
}

ArcAppQueueRestoreHandler*
AppRestoreArcTaskHandler::CreateOrGetArcAppQueueRestoreHandler(
    LauncherTag launcher_tag,
    bool call_init_callback) {
  if (!arc_app_queue_restore_handlers_.count(launcher_tag)) {
    auto handler = std::make_unique<ArcAppQueueRestoreHandler>();
    if (call_init_callback) {
      handler->OnArcPlayStoreEnabledChanged(arc_play_store_enabled_);
      if (app_connection_ready_)
        handler->OnAppConnectionReady();
      if (shelf_ready_)
        handler->OnShelfReady();
    }
    return arc_app_queue_restore_handlers_
        .insert({launcher_tag, std::move(handler)})
        .first->second.get();
  }
  return arc_app_queue_restore_handlers_[launcher_tag].get();
}

ArcAppSingleRestoreHandler*
AppRestoreArcTaskHandler::CreateOrGetArcAppSingleRestoreHandler(
    LauncherTag launcher_tag) {
  if (!arc_app_single_restore_handlers_.count(launcher_tag)) {
    auto handler = std::make_unique<ArcAppSingleRestoreHandler>();
    if (shelf_ready_)
      handler->OnShelfReady();
    return arc_app_single_restore_handlers_
        .insert({launcher_tag, std::move(handler)})
        .first->second.get();
  }
  return arc_app_single_restore_handlers_[launcher_tag].get();
}

}  // namespace ash::app_restore
