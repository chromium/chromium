// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/window_predictor/arc_predictor_app_launch_handler.h"

#include "chrome/browser/ash/arc/window_predictor/window_predictor_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/window_info.h"
#include "ui/display/screen.h"

namespace arc {

ArcPredictorAppLaunchHandler::ArcPredictorAppLaunchHandler()
    : ash::AppLaunchHandler(ProfileManager::GetPrimaryUserProfile()) {
  set_restore_data(std::make_unique<app_restore::RestoreData>());
}

ArcPredictorAppLaunchHandler::~ArcPredictorAppLaunchHandler() = default;

void ArcPredictorAppLaunchHandler::AddPendingApp(
    const std::string& app_id,
    int event_flags,
    GhostWindowType window_type,
    arc::mojom::WindowInfoPtr window_info) {
  // TODO(sstan): May prevent launch the same app_id programmatically. Currently
  // if an ARC app ghost window launch, the window will be attached to shelf,
  // and user cannot launch another instance of the same app by click icon. But
  // from code side it still can launch the same app by calling this function.
  int arc_session_id = -1;
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  if (window_info) {
    arc_session_id = window_info->window_id;
    // Default invalid display id in WindowInfo struct.
    // See components/services/app_service/public/mojom/types.mojom#WindowInfo
    if (window_info->display_id != -1)
      display_id = window_info->display_id;
  }

  // TODO(sstan): Add window type to restore parameters.
  auto app_info =
      std::make_unique<app_restore::AppLaunchInfo>(app_id, arc_session_id);
  app_info->event_flag = event_flags;
  app_info->arc_session_id = arc_session_id;
  restore_data()->AddAppLaunchInfo(std::move(app_info));
  auto app_window_info = std::make_unique<app_restore::WindowInfo>();

  // The bounds should be |WindowInfo::ArcExtraInfo::bounds_in_root|, but here
  // we always launch the window in corresponding display, so for avoid
  // initialize nest struct here we use |current_bounds| (the original sementic
  // is bounds in global coordinate) instead.
  if (window_info->bounds.has_value())
    app_window_info->current_bounds = window_info->bounds.value();

  app_window_info->display_id = display_id;
  app_window_info->window_state_type =
      static_cast<chromeos::WindowStateType>(window_info->state);
  restore_data()->ModifyWindowInfo(app_id, arc_session_id, *app_window_info);
}

base::WeakPtr<ash::AppLaunchHandler>
ArcPredictorAppLaunchHandler::GetWeakPtrAppLaunchHandler() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ArcPredictorAppLaunchHandler::RecordRestoredAppLaunch(
    apps::AppTypeName app_type_name) {}

}  // namespace arc
