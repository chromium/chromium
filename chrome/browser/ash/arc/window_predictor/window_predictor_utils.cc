// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/window_predictor/window_predictor_utils.h"
#include "chrome/browser/ash/app_restore/app_restore_arc_task_handler.h"
#include "chrome/browser/ash/app_restore/arc_app_launch_handler.h"
#include "chrome/browser/ash/arc/window_predictor/window_predictor.h"

namespace arc {

bool LaunchArcAppWithGhostWindow(Profile* profile,
                                 const std::string& arc_app_id,
                                 int event_flags,
                                 arc::UserInteractionType user_interaction_type,
                                 arc::mojom::WindowInfoPtr window_info) {
  WindowPredictor::GetInstance()->MaybeCreateAppLaunchHandler(profile);

  auto* arc_task_handler =
      ash::app_restore::AppRestoreArcTaskHandler::GetForProfile(profile);
  if (!arc_task_handler)
    return false;

  auto* arc_app_launch_handler =
      arc_task_handler->window_predictor_arc_app_launch_handler();
  if (!arc_app_launch_handler)
    return false;

  arc::mojom::WindowInfoPtr predict_window_info =
      WindowPredictor::GetInstance()->PredictAppWindowInfo(
          arc_app_id, std::move(window_info));
  auto* app_launch_handler =
      WindowPredictor::GetInstance()->app_launch_handler();
  DCHECK(app_launch_handler);

  app_launch_handler->AddPendingApp(arc_app_id, event_flags,
                                    std::move(predict_window_info));
  arc_app_launch_handler->RestoreArcApps(app_launch_handler);
  return true;
}

}  // namespace arc
