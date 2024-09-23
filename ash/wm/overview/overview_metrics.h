// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_METRICS_H_
#define ASH_WM_OVERVIEW_OVERVIEW_METRICS_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/overview/overview_types.h"
#include "base/time/time.h"
#include "ui/compositor/presentation_time_recorder.h"

namespace ui {
class PresentationTimeRecorder;
}  // namespace ui

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
  kMaxValue = kPine,
};
void RecordOverviewEndAction(OverviewEndAction type);

inline constexpr char kEnterOverviewPresentationHistogram[] =
    "Ash.Overview.Enter.PresentationTime2";
inline constexpr char kExitOverviewPresentationHistogram[] =
    "Ash.Overview.Exit.PresentationTime2";
inline constexpr char kOverviewDelayedDeskBarPresentationHistogram[] =
    "Ash.Overview.DelayedDeskBar.PresentationTime";

const ui::PresentationTimeRecorder::BucketParams&
GetOverviewPresentationTimeBucketParams();

// Records a metric name of the format"
// "Ash.Overview.[Enter|Exit].PresentationTime.WithDeskBarAndNumWindows[N]"
//
// Where N is the number of windows currently open across all desks. If N is
// greater than 10, the suffix becomes "MoreThan10". Metrics currently show that
// most users will have less than 10 open.
//
// TODO(http://b/294094124): Remove after bug is marked fixed. This metric is
// temporary and only to debug the experiment.
ASH_EXPORT void SchedulePresentationTimeMetricsWithDeskBar(
    std::unique_ptr<ui::PresentationTimeRecorder> enter_recorder,
    std::unique_ptr<ui::PresentationTimeRecorder> exit_recorder,
    DeskBarVisibility desk_bar_visibility);

// Records metric with format:
// "Ash.Overview.Enter.PresentationTime.{OverviewStartReason}"
//
// where {OverviewStartReason} is derived from the `start_action`. This is the
// exact same measurement as `kEnterOverviewPresentationHistogram`, but
// segmented by different use cases that have different profiles and performance
// characteristics.
ASH_EXPORT void RecordOverviewEnterPresentationTimeWithReason(
    OverviewStartAction start_action,
    base::TimeDelta presentation_time);

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_METRICS_H_
