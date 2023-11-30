// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_METRICS_H_
#define ASH_WM_OVERVIEW_OVERVIEW_METRICS_H_

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
  kMaxValue = kFasterSplitScreenSetup,
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
  kMaxValue = kFullRestore,
};
void RecordOverviewEndAction(OverviewEndAction type);

inline constexpr char kEnterOverviewPresentationHistogram[] =
    "Ash.Overview.Enter.PresentationTime";
inline constexpr char kExitOverviewPresentationHistogram[] =
    "Ash.Overview.Exit.PresentationTime";

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_METRICS_H_
