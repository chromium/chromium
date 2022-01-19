// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/window_predictor/arc_predictor_app_launch_handler.h"

#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/window_info.h"
#include "ui/display/types/display_constants.h"

namespace {
std::unique_ptr<app_restore::WindowInfo> GetAppWindowInfo(
    arc::mojom::WindowInfoPtr window_info) {
  auto app_window_info = std::make_unique<app_restore::WindowInfo>();
  if (window_info->bounds.has_value()) {
    app_window_info->current_bounds = window_info->bounds.value();
  }
  return app_window_info;
}
}  // namespace

namespace arc {
ArcPredictorAppLaunchHandler::ArcPredictorAppLaunchHandler(Profile* profile)
    : ash::AppLaunchHandler(profile) {
  set_restore_data(std::make_unique<app_restore::RestoreData>());
}

ArcPredictorAppLaunchHandler::~ArcPredictorAppLaunchHandler() = default;

void ArcPredictorAppLaunchHandler::AddPendingApp(
    const std::string& app_id,
    int event_flags,
    arc::mojom::WindowInfoPtr window_info) {
  // TODO(sstan): May prevent launch the same app_id programmatically. Currently
  // if an ARC app ghost window launch, the window will be attached to shelf,
  // and user cannot launch another instance of the same app by click icon. But
  // from code side it still can launch the same app by calling this function.
  int arc_session_id = -1;
  int64_t display_id = display::kDefaultDisplayId;
  if (window_info) {
    arc_session_id = window_info->window_id;
    // Default invalid display id in WindowInfo struct.
    // See components/services/app_service/public/mojom/types.mojom#WindowInfo
    if (window_info->display_id != -1)
      display_id = window_info->display_id;
  }

  auto app_info =
      std::make_unique<app_restore::AppLaunchInfo>(app_id, arc_session_id);
  app_info->event_flag = event_flags;
  app_info->arc_session_id = arc_session_id;
  restore_data()->AddAppLaunchInfo(std::move(app_info));
  auto app_window_info = std::make_unique<app_restore::WindowInfo>();
  if (window_info->bounds.has_value())
    app_window_info->current_bounds = window_info->bounds.value();
  app_window_info->display_id = display_id;
  restore_data()->ModifyWindowInfo(app_id, arc_session_id, *app_window_info);
}

base::WeakPtr<ash::AppLaunchHandler>
ArcPredictorAppLaunchHandler::GetWeakPtrAppLaunchHandler() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ArcPredictorAppLaunchHandler::RecordRestoredAppLaunch(
    apps::AppTypeName app_type_name) {}

}  // namespace arc
