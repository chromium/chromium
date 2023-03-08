// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_METRICS_H_
#define ASH_WM_OVERVIEW_OVERVIEW_METRICS_H_

namespace ash {

// Used for histograms. See OverviewStartAction at
// tools/metrics/histograms/enums.xml.
enum class OverviewStartAction {
  kSplitView,
  kAccelerator,
  kDragWindowFromShelf,
  kExitHomeLauncher,
  kOverviewButton,
  kOverviewButtonLongPress,
  kBentoBar,
  k3FingerVerticalScroll,
  kDevTools,
  kTests,
  kOverviewDeskSwitch,
  kMaxValue = kOverviewDeskSwitch,
};
void RecordOverviewStartAction(OverviewStartAction type);

// Used for histograms. See OverviewEndAction at
// tools/metrics/histograms/enums.xml.
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
  kMaxValue = kShowGlanceables_DEPRECATED,
};
void RecordOverviewEndAction(OverviewEndAction type);

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_METRICS_H_
