// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/app_restore_arc_task_handler.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/app_restore/app_restore_arc_task_handler_factory.h"
#include "chrome/browser/ash/app_restore/arc_app_launch_handler.h"
#include "chrome/browser/ash/app_restore/arc_window_handler.h"
#include "chrome/browser/ash/app_restore/arc_window_utils.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/app_restore/app_restore_arc_info.h"
#include "components/app_restore/features.h"

namespace ash {
namespace app_restore {
namespace {

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
    window_handler_ = std::make_unique<full_restore::ArcWindowHandler>();
#endif

  if (ash::features::AreDesksTemplatesEnabled()) {
    arc_app_launcher_handers_.push_back(
        std::make_unique<ArcAppLaunchHandler>());
    desks_templates_arc_app_launch_handler_observer_ =
        arc_app_launcher_handers_.back().get();
  }
  arc_app_launcher_handers_.push_back(std::make_unique<ArcAppLaunchHandler>());
  full_restore_arc_app_launch_handler_observer_ =
      arc_app_launcher_handers_.back().get();

  // TODO(sstan): Modify ArcAppLaunchHandler to prevent redundant launch.
  if (::full_restore::features::IsArcWindowPredictorEnabled()) {
    arc_app_launcher_handers_.push_back(
        std::make_unique<ArcAppLaunchHandler>());
    window_predictor_arc_app_launch_handler_observer_ =
        arc_app_launcher_handers_.back().get();
  }
  arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  // arc::ArcSessionManager might not be set in tests.
  if (arc_session_manager)
    arc_session_manager->AddObserver(this);
}

AppRestoreArcTaskHandler::~AppRestoreArcTaskHandler() {
  arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  // arc::ArcSessionManager may be released first.
  if (arc_session_manager)
    arc_session_manager->RemoveObserver(this);
}

bool AppRestoreArcTaskHandler::IsAppPendingRestore(
    const std::string& arc_app_id) const {
  for (auto& handler : arc_app_launcher_handers_) {
    if (handler && handler->IsAppPendingRestore(arc_app_id))
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
#if BUILDFLAG(ENABLE_WAYLAND_SERVER)
  if (window_handler_)
    window_handler_->OnAppInstanceConnected();
#endif

  for (auto& handler : arc_app_launcher_handers_) {
    if (handler)
      handler->OnAppConnectionReady();
  }

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
  for (auto& handler : arc_app_launcher_handers_) {
    if (handler)
      handler->OnArcPlayStoreEnabledChanged(enabled);
  }

  GetAppRestoreArcInfo()->NotifyPlayStoreEnabledChanged(enabled);
}

void AppRestoreArcTaskHandler::OnShelfReady() {
  for (auto& handler : arc_app_launcher_handers_) {
    if (handler)
      handler->OnShelfReady();
  }
}

void AppRestoreArcTaskHandler::Shutdown() {
  for (auto& handler : arc_app_launcher_handers_) {
    handler.reset();
  }
  desks_templates_arc_app_launch_handler_observer_ = nullptr;
  full_restore_arc_app_launch_handler_observer_ = nullptr;
  window_predictor_arc_app_launch_handler_observer_ = nullptr;

  window_handler_.reset();
}

}  // namespace app_restore
}  // namespace ash
