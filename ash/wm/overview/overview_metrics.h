// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_METRICS_H_
#define ASH_WM_OVERVIEW_OVERVIEW_METRICS_H_

#include "ash/ash_export.h"
#include "ash/wm/overview/overview_types.h"
#include "base/time/time.h"
#include "ui/compositor/presentation_time_recorder.h"

namespace ash {

// Used for histograms. Current values should not be renumbered or removed.
// Please keep in sync with "OverviewStartAction" in
// tools/metrics/histograms/metadata/ash/enums.xml.
enum class OverviewStartAction {
  kSplitView,
  kAccelerator,
  kDragWindowFromShelf,
  kExitHomeLauncher,
  kOverviewButton,
  kOverviewButtonLongPress,
  kBentoBar_DEPRECATED,
  k3FingerVerticalScroll,
  kDevTools,
  kTests,
  kOverviewDeskSwitch,
  kDeskButton,
  // Partial overview shows automatically on one window snapped.
  kFasterSplitScreenSetup,
  kPine,
  kWallpaper,
  kMaxValue = kWallpaper,
};
void RecordOverviewStartAction(OverviewStartAction type);

// Used for histograms. Current values should not be renumbered or removed.
// Please keep in sync with "OverviewEndAction" in
// tools/metrics/histograms/metadata/ash/enums.xml.
enum class OverviewEndAction {
  kSplitView,
  kDragWindowFromShelf,
  kEnterHomeLauncher,
  kClickingOutsideWindowsInOverview,
  kWindowActivating,
  kLastWindowRemoved,
  kDisplayAdded,
  kAccelerator,
  kKeyEscapeOrBack,
  kDeskActivation,
  kOverviewButton,
  kOverviewButtonLongPress,
  k3FingerVerticalScroll,
  kEnabledDockedMagnifier,
  kUserSwitch,
  kStartedWindowCycle,
  kShuttingDown,
  kAppListActivatedInClamshell,
  kShelfAlignmentChanged,
  kDevTools,
  kTests,
  kShowGlanceables_DEPRECATED,
  kWindowDeactivating,
  kFullRestore,
  kPine,
  kCoral,
  kMaxValue = kCoral,
};
void RecordOverviewEndAction(OverviewEndAction type);

inline constexpr char kExitOverviewPresentationHistogram[] =
    "Ash.Overview.Exit.PresentationTime2";
inline constexpr char kOverviewDelayedDeskBarPresentationHistogram[] =
    "Ash.Overview.DelayedDeskBar.PresentationTime";

const ui::PresentationTimeRecorder::BucketParams&
GetOverviewPresentationTimeBucketParams();

// Returns metric name with format:
// "Ash.Overview.Enter.PresentationTime.{OverviewStartReason}"
//
// This segments the overview presentation time into separate categories/use
// cases that have different profiles and characteristics and hence, should be
// analyzed independently.
ASH_EXPORT const char* GetOverviewEnterPresentationTimeMetricName(
    OverviewStartAction start_action);

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_METRICS_H_
