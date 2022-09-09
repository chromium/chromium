// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/window_predictor/window_predictor_utils.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/app_restore/app_restore_arc_task_handler.h"
#include "chrome/browser/ash/app_restore/arc_app_launch_handler.h"
#include "chrome/browser/ash/arc/window_predictor/window_predictor.h"

namespace arc {

namespace {

constexpr char kWindowPredictorLaunchHistogram[] = "Arc.WindowPredictorLaunch";

// Reason for Window Predictor launch action enumeration; Used for UMA counter.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class WindowPredictorLaunchType {
  kSuccess = 0,
  kFailedNoArcTaskHandler = 1,
  kFailedAppPendingRestore = 2,
  kFailedNoArcAppLaunchHandler = 3,
  kMaxValue = kFailedNoArcAppLaunchHandler,
};

}  // namespace

bool LaunchArcAppWithGhostWindow(Profile* profile,
                                 const std::string& arc_app_id,
                                 const ArcAppListPrefs::AppInfo& app_info,
                                 int event_flags,
                                 arc::UserInteractionType user_interaction_type,
                                 const arc::mojom::WindowInfoPtr& window_info) {
  WindowPredictor::GetInstance()->MaybeCreateAppLaunchHandler(profile);

  auto* arc_task_handler =
      ash::app_restore::AppRestoreArcTaskHandler::GetForProfile(profile);
  if (!arc_task_handler) {
    base::UmaHistogramEnumeration(
        kWindowPredictorLaunchHistogram,
        WindowPredictorLaunchType::kFailedNoArcTaskHandler);
    return false;
  }

  // Do not launch ghost window and App if it exist in any pending launch
  // queue.
  if (arc_task_handler->IsAppPendingRestore(arc_app_id)) {
    base::UmaHistogramEnumeration(
        kWindowPredictorLaunchHistogram,
        WindowPredictorLaunchType::kFailedAppPendingRestore);
    return false;
  }

  auto* arc_app_launch_handler =
      arc_task_handler->window_predictor_arc_app_launch_handler();
  if (!arc_app_launch_handler) {
    base::UmaHistogramEnumeration(
        kWindowPredictorLaunchHistogram,
        WindowPredictorLaunchType::kFailedNoArcAppLaunchHandler);
    return false;
  }

  arc::mojom::WindowInfoPtr predict_window_info =
      WindowPredictor::GetInstance()->PredictAppWindowInfo(app_info,
                                                           window_info.Clone());
  auto* app_launch_handler =
      WindowPredictor::GetInstance()->app_launch_handler();
  DCHECK(app_launch_handler);

  app_launch_handler->AddPendingApp(arc_app_id, event_flags,
                                    std::move(predict_window_info));
  arc_app_launch_handler->RestoreArcApps(app_launch_handler);

  base::UmaHistogramEnumeration(kWindowPredictorLaunchHistogram,
                                WindowPredictorLaunchType::kSuccess);
  return true;
}

bool CanLaunchGhostWindowByRestoreData(
    const app_restore::AppRestoreData& restore_data) {
  const bool not_need_bounds =
      restore_data.window_state_type == chromeos::WindowStateType::kMaximized ||
      restore_data.window_state_type == chromeos::WindowStateType::kFullscreen;
  return not_need_bounds || restore_data.bounds_in_root.has_value() ||
         restore_data.current_bounds.has_value();
}

}  // namespace arc
