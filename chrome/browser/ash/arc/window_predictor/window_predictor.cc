// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/window_predictor/window_predictor.h"

#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/app_restore/app_launch_handler.h"
#include "chrome/browser/ash/app_restore/app_restore_arc_task_handler.h"
#include "chrome/browser/ash/app_restore/app_restore_arc_task_handler_factory.h"
#include "chrome/browser/ash/app_restore/arc_app_single_restore_handler.h"
#include "chrome/browser/ash/app_restore/arc_ghost_window_handler.h"
#include "chromeos/ui/base/window_state_type.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point.h"

namespace arc {

namespace {

constexpr char kWindowPredictorLaunchHistogram[] = "Arc.WindowPredictorLaunch";

// Reason for Window Predictor launch action when failed to launch App
// enumeration; Used for UMA counter.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class WindowPredictorLaunchType {
  kSuccess = 0,
  kFailedNoArcTaskHandler = 1,
  kFailedAppPendingRestore = 2,
  kFailedNoArcAppLaunchHandler = 3,
  kMaxValue = kFailedNoArcAppLaunchHandler,
};

constexpr char kWindowPredictorUseCaseHistogram[] =
    "Arc.WindowPredictorUseCase";

// Pre-defined screen size for ARC. See ArcLaunchParamsModifier.java in ARC
// codebase.

// Screen size of Nexus 5x. The default bounds in default scale in Android.
constexpr gfx::Size kDefaultPortraitPhoneSize(412, 732);
constexpr gfx::Size kDefaultLandscapeTabletSize(1064, 600);

// TODO(sstan): User may apply zoom on per-display. The final DP value of the
// Android bounds should be FinalBounds = AndroidDpSize * AndroidDensityRound(
// kArcUniformScaleFactor * kChromeDpToAndroidDp * ChromeOSDisplayZoomFactor).
// Here AndroidDensityRound is a function to round density into one of a set
// of allowed density according to Android CDD. Here just ignore it. Also leave
// zoom facter as TODO here.
constexpr float kArcUniformScaleFactor = 1.2;
constexpr float kChromeDpToAndroidDp = 0.75;

gfx::Size GetPhoneSize() {
  return ScaleToCeiledSize(kDefaultPortraitPhoneSize,
                           kArcUniformScaleFactor * kChromeDpToAndroidDp);
}

gfx::Size GetTabletSize() {
  return ScaleToCeiledSize(kDefaultLandscapeTabletSize,
                           kArcUniformScaleFactor * kChromeDpToAndroidDp);
}

// Get window bounds in the middle of a display in global coordinate.
gfx::Rect GetMiddleBounds(const display::Display& display,
                          const gfx::Size& size) {
  // Shrink size if it larger then display size.
  // TODO(sstan): ARC P and R has different behavior for caption bar, verify if
  // it need consider the caption bar.
  auto shrink_size = size;
  shrink_size.SetToMin(display.work_area_size());

  auto bounds_rect = display.work_area();
  bounds_rect.ClampToCenteredSize(shrink_size);
  return bounds_rect;
}

}  // namespace

// static
WindowPredictor* WindowPredictor::GetInstance() {
  static base::NoDestructor<WindowPredictor> instance;
  return instance.get();
}

WindowPredictor::WindowPredictor() = default;

WindowPredictor::~WindowPredictor() = default;

bool WindowPredictor::LaunchArcAppWithGhostWindow(
    Profile* profile,
    const std::string& arc_app_id,
    const ArcAppListPrefs::AppInfo& app_info,
    const apps::IntentPtr& intent,
    int event_flags,
    GhostWindowType window_type,
    WindowPredictorUseCase use_case,
    const arc::mojom::WindowInfoPtr& window_info) {
  // ArcGhostWindowHandler maybe null in the test env.
  if (!ash::full_restore::ArcGhostWindowHandler::Get())
    return false;
  auto* arc_task_handler =
      ash::app_restore::AppRestoreArcTaskHandlerFactory::GetForProfile(profile);
  if (!arc_task_handler) {
    base::UmaHistogramEnumeration(
        kWindowPredictorLaunchHistogram,
        WindowPredictorLaunchType::kFailedNoArcTaskHandler);
    return false;
  }

  // Do not launch ghost window and App if it exist in full restore pending
  // launch queue.
  if (arc_task_handler->IsAppPendingRestore(arc_app_id)) {
    base::UmaHistogramEnumeration(
        kWindowPredictorLaunchHistogram,
        WindowPredictorLaunchType::kFailedAppPendingRestore);
    return false;
  }

  launch_counter++;

  arc::mojom::WindowInfoPtr predict_window_info =
      PredictAppWindowInfo(app_info, window_info.Clone());

  DCHECK(predict_window_info);

  arc_task_handler->GetWindowPredictorArcAppRestoreHandler(launch_counter)
      ->LaunchGhostWindowWithApp(
          profile, arc_app_id, intent ? intent->Clone() : nullptr, event_flags,
          window_type, std::move(predict_window_info));

  base::UmaHistogramEnumeration(kWindowPredictorLaunchHistogram,
                                WindowPredictorLaunchType::kSuccess);
  base::UmaHistogramEnumeration(kWindowPredictorUseCaseHistogram, use_case);
  return true;
}

arc::mojom::WindowInfoPtr WindowPredictor::PredictAppWindowInfo(
    const ArcAppListPrefs::AppInfo& app_info,
    arc::mojom::WindowInfoPtr window_info) {
  // TODO(sstan): Consider fallback case.
  // TODO(sstan): Verify the affect from per-display density.
  // TODO(sstan): Consider multi display case.
  if (!window_info)
    return nullptr;
  auto disp = display::Screen::GetScreen()->GetPrimaryDisplay();
  if (window_info->display_id != display::kInvalidDisplayId) {
    display::Screen::GetScreen()->GetDisplayWithDisplayId(
        window_info->display_id, &disp);
  }

  if (display::Screen::GetScreen()->InTabletMode()) {
    // TODO: Figure out why setting kMaximized doesn't work.
    // Note that the ghost window state type is default, but the ARC app
    // window state will be assigned by ARC and not be affected by this state.
    window_info->state =
        static_cast<int32_t>(chromeos::WindowStateType::kDefault);
    window_info->bounds = disp.work_area();
    return window_info;
  }

  const auto& layout = app_info.initial_window_layout;
  switch (layout.type) {
    case arc::mojom::WindowSizeType::kMaximize:
      window_info->state =
          static_cast<int32_t>(chromeos::WindowStateType::kMaximized);
      window_info->bounds = disp.work_area();
      break;
    case arc::mojom::WindowSizeType::kTabletSize:
      window_info->state =
          static_cast<int32_t>(chromeos::WindowStateType::kNormal);
      window_info->bounds = GetMiddleBounds(disp, GetTabletSize());
      break;
    case arc::mojom::WindowSizeType::kPhoneSize:
      window_info->state =
          static_cast<int32_t>(chromeos::WindowStateType::kNormal);
      window_info->bounds = GetMiddleBounds(disp, GetPhoneSize());
      break;
    case arc::mojom::WindowSizeType::kUnknown:
    default:
      if (IsGoogleSeriesPackage(app_info.package_name)) {
        // Google apps support tablet, launch them in tablet size on chromebook.
        // This is the short-term workaround, should be removed in the future.
        window_info->state =
            static_cast<int32_t>(chromeos::WindowStateType::kNormal);
        window_info->bounds = GetMiddleBounds(disp, GetTabletSize());
      } else {
        window_info->state =
            static_cast<int32_t>(chromeos::WindowStateType::kDefault);
        window_info->bounds = GetMiddleBounds(disp, GetPhoneSize());
      }
  }

  return window_info;
}

bool WindowPredictor::IsAppPendingLaunch(Profile* profile,
                                         const std::string& app_id) {
  auto* arc_task_handler =
      ash::app_restore::AppRestoreArcTaskHandlerFactory::GetForProfile(profile);

  return arc_task_handler && arc_task_handler->IsAppPendingRestore(app_id);
}

}  // namespace arc
