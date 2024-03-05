// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_GESTURES_BACK_GESTURE_BACK_GESTURE_METRICS_H_
#define ASH_WM_GESTURES_BACK_GESTURE_BACK_GESTURE_METRICS_H_

#include "ui/gfx/geometry/point.h"

namespace ash {

// Used for histograms. Keep in sync with `BackGestureStartScenarioType` in
// tools/metrics/histograms/metadata/ash/enums.xml.
enum class BackGestureStartScenarioType {
  kNonSnappedWindow = 0,
  kLeftSnappedWindow,
  kRightSnappedWindow,
  kTopSnappedWindow,
  kBottomSnappedWindow,
  kOverview,
  kLeftOverview,
  kRightOverview,
  kTopOverview,
  kBottomOverview,
  kMaxValue = kBottomOverview,
};

// Helper function to get the start scenario type for RecordStartScenarioType.
BackGestureStartScenarioType GetStartScenarioType(
    bool dragged_from_splitview_divider,
    const gfx::Point& start_screen_location);
void RecordStartScenarioType(BackGestureStartScenarioType type);

// Used for histograms. Keep in sync with `BackGestureEndScenarioType` in
// tools/metrics/histograms/metadata/ash/enums.xml.
enum class BackGestureEndScenarioType {
  kNonSnappedWindowAbort = 0,
  kNonSnappedWindowGoBack,
  kNonSnappedWindowMinimize,
  kLeftSnappedWindowAbort,
  kLeftSnappedWindowGoBack,
  kLeftSnappedWindowMinimize,
  kRightSnappedWindowAbort,
  kRightSnappedWindowGoBack,
  kRightSnappedWindowMinimize,
  kTopSnappedWindowAbort,
  kTopSnappedWindowGoBack,
  kTopSnappedWindowMinimize,
  kBottomSnappedWindowAbort,
  kBottomSnappedWindowGoBack,
  kBottomSnappedWindowMinimize,
  kOverviewAbort,
  kOverviewGoBack,
  kExitFullscreen,
  kShowShelfAndHotseat,
  kMaxValue = kShowShelfAndHotseat,
};

// The end type of back gesture. Used to get the end scenario type.
enum class BackGestureEndType { kBack = 0, kAbort, kMinimize };

// Helper function to get the end scenario type for RecordEndScenarioType.
BackGestureEndScenarioType GetEndScenarioType(
    BackGestureStartScenarioType start_type,
    BackGestureEndType end_type);
void RecordEndScenarioType(BackGestureEndScenarioType type);

// Used for histograms. Keep in sync with `BackGestureUnderneathWindowType` in
// tools/metrics/histograms/metadata/ash/enums.xml.
enum class BackGestureUnderneathWindowType {
  kBrowser = 0,
  kChromeApp,
  kArcApp,
  kOverview,
  kOthers,
  kMaxValue = kOthers,
};

// Helper function to get the underneath window type for
// RecordUnderneathWindowType.
BackGestureUnderneathWindowType GetUnderneathWindowType(
    BackGestureStartScenarioType start_type);
void RecordUnderneathWindowType(BackGestureUnderneathWindowType type);

}  // namespace ash

#endif  // ASH_WM_GESTURES_BACK_GESTURE_BACK_GESTURE_METRICS_H_
