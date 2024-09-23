// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/back_gesture/back_gesture_metrics.h"

#include "ash/display/screen_orientation_controller.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/window_util.h"
#include "base/metrics/histogram_macros.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"

namespace ash {

constexpr char kBackGestureStartScenarioHistogram[] =
    "Ash.BackGesture.StartScenarioType";

constexpr char kBackGestureEndScenarioHistogram[] =
    "Ash.BackGesture.EndScenarioType";

constexpr char kBackGestureUnderneathWindowTypeHistogram[] =
    "Ash.BackGesture.UnderneathWindowType";

BackGestureStartScenarioType GetStartScenarioType(
    bool dragged_from_splitview_divider,
    const gfx::Point& start_screen_location) {
  auto* split_view_controller = SplitViewController::Get(
      window_util::GetRootWindowAt(start_screen_location));
  if (!split_view_controller->InTabletSplitViewMode()) {
    return Shell::Get()->overview_controller()->InOverviewSession()
               ? BackGestureStartScenarioType::kOverview
               : BackGestureStartScenarioType::kNonSnappedWindow;
  }

  const auto* left_window = split_view_controller->primary_window();
  const auto* right_window = split_view_controller->secondary_window();
  const bool is_primary = IsCurrentScreenOrientationPrimary();
  const auto* physical_left_or_top_window =
      is_primary ? left_window : right_window;
  const auto* physical_right_or_bottom_window =
      is_primary ? right_window : left_window;

  if (IsCurrentScreenOrientationLandscape()) {
    if (dragged_from_splitview_divider) {
      return physical_right_or_bottom_window
                 ? BackGestureStartScenarioType::kRightSnappedWindow
                 : BackGestureStartScenarioType::kRightOverview;
    }
    return physical_left_or_top_window
               ? BackGestureStartScenarioType::kLeftSnappedWindow
               : BackGestureStartScenarioType::kLeftOverview;
  }
  if (start_screen_location.y() <= split_view_controller->split_view_divider()
                                       ->GetDividerBoundsInScreen(
                                           /*is_dragging=*/false)
                                       .bottom()) {
    return physical_left_or_top_window
               ? BackGestureStartScenarioType::kTopSnappedWindow
               : BackGestureStartScenarioType::kTopOverview;
  }
  return physical_right_or_bottom_window
             ? BackGestureStartScenarioType::kBottomSnappedWindow
             : BackGestureStartScenarioType::kBottomOverview;
}

void RecordStartScenarioType(BackGestureStartScenarioType type) {
  UMA_HISTOGRAM_ENUMERATION(kBackGestureStartScenarioHistogram, type);
}

BackGestureEndScenarioType GetEndScenarioType(
    BackGestureStartScenarioType start_type,
    BackGestureEndType end_type) {
  BackGestureEndScenarioType type = BackGestureEndScenarioType::kMaxValue;
  switch (start_type) {
    case BackGestureStartScenarioType::kNonSnappedWindow:
      if (end_type == BackGestureEndType::kAbort)
        type = BackGestureEndScenarioType::kNonSnappedWindowAbort;
      else if (end_type == BackGestureEndType::kBack)
        type = BackGestureEndScenarioType::kNonSnappedWindowGoBack;
      else
        type = BackGestureEndScenarioType::kNonSnappedWindowMinimize;
      break;
    case BackGestureStartScenarioType::kLeftSnappedWindow:
      if (end_type == BackGestureEndType::kAbort)
        type = BackGestureEndScenarioType::kLeftSnappedWindowAbort;
      else if (end_type == BackGestureEndType::kBack)
        type = BackGestureEndScenarioType::kLeftSnappedWindowGoBack;
      else
        type = BackGestureEndScenarioType::kLeftSnappedWindowMinimize;
      break;
    case BackGestureStartScenarioType::kRightSnappedWindow:
      if (end_type == BackGestureEndType::kAbort)
        type = BackGestureEndScenarioType::kRightSnappedWindowAbort;
      else if (end_type == BackGestureEndType::kBack)
        type = BackGestureEndScenarioType::kRightSnappedWindowGoBack;
      else
        type = BackGestureEndScenarioType::kRightSnappedWindowMinimize;
      break;
    case BackGestureStartScenarioType::kTopSnappedWindow:
      if (end_type == BackGestureEndType::kAbort)
        type = BackGestureEndScenarioType::kTopSnappedWindowAbort;
      else if (end_type == BackGestureEndType::kBack)
        type = BackGestureEndScenarioType::kTopSnappedWindowGoBack;
      else
        type = BackGestureEndScenarioType::kTopSnappedWindowMinimize;
      break;
    case BackGestureStartScenarioType::kBottomSnappedWindow:
      if (end_type == BackGestureEndType::kAbort)
        type = BackGestureEndScenarioType::kBottomSnappedWindowAbort;
      else if (end_type == BackGestureEndType::kBack)
        type = BackGestureEndScenarioType::kBottomSnappedWindowGoBack;
      else
        type = BackGestureEndScenarioType::kBottomSnappedWindowMinimize;
      break;
    case BackGestureStartScenarioType::kOverview:
    case BackGestureStartScenarioType::kLeftOverview:
    case BackGestureStartScenarioType::kRightOverview:
    case BackGestureStartScenarioType::kTopOverview:
    case BackGestureStartScenarioType::kBottomOverview:
      if (end_type == BackGestureEndType::kAbort)
        type = BackGestureEndScenarioType::kOverviewAbort;
      else if (end_type == BackGestureEndType::kBack)
        type = BackGestureEndScenarioType::kOverviewGoBack;
      break;
    default:
      break;
  }
  return type;
}

void RecordEndScenarioType(BackGestureEndScenarioType type) {
  UMA_HISTOGRAM_ENUMERATION(kBackGestureEndScenarioHistogram, type);
}

BackGestureUnderneathWindowType GetUnderneathWindowType(
    BackGestureStartScenarioType start_type) {
  if (start_type == BackGestureStartScenarioType::kOverview ||
      start_type == BackGestureStartScenarioType::kLeftOverview ||
      start_type == BackGestureStartScenarioType::kRightOverview ||
      start_type == BackGestureStartScenarioType::kTopOverview ||
      start_type == BackGestureStartScenarioType::kBottomOverview) {
    return BackGestureUnderneathWindowType::kOverview;
  }

  const auto* window = window_util::GetTopWindow();
  DCHECK(window);
  const chromeos::AppType app_type = window->GetProperty(chromeos::kAppTypeKey);
  if (app_type == chromeos::AppType::BROWSER) {
    return BackGestureUnderneathWindowType::kBrowser;
  }
  if (app_type == chromeos::AppType::CHROME_APP) {
    return BackGestureUnderneathWindowType::kChromeApp;
  }
  if (app_type == chromeos::AppType::ARC_APP) {
    return BackGestureUnderneathWindowType::kArcApp;
  }
  return BackGestureUnderneathWindowType::kOthers;
}

void RecordUnderneathWindowType(BackGestureUnderneathWindowType type) {
  UMA_HISTOGRAM_ENUMERATION(kBackGestureUnderneathWindowTypeHistogram, type);
}

}  // namespace ash
