// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SNAP_GROUP_SNAP_GROUP_METRICS_H_
#define ASH_WM_SNAP_GROUP_SNAP_GROUP_METRICS_H_

#include <string>

#include "ash/ash_export.h"
#include "base/time/time.h"

namespace ash {

class OverviewItemBase;

// -----------------------------------------------------------------------------
// Faster Split Screen Session Metrics:

inline constexpr char kPartialOverviewSelectedWindowIndex[] =
    "Ash.SplitViewOverviewSession.SelectedWindowIndex";

inline constexpr char kPartialOverviewWindowListSize[] =
    "Ash.SplitViewOverviewSession.WindowListSize";

// -----------------------------------------------------------------------------
// Snap Groups Metrics:

inline constexpr char kSnapGroupsMetricCommonPrefix[] = "Ash.SnapGroups.";

// The duration of a Snap Group where one of the windows remains in the Snap
// Group even after the other window is replaced using 'Snap to Replace'.
inline constexpr char kSnapGroupPersistenceDurationRootWord[] =
    "SnapGroupPersistenceDuration";

// The duration of a Snap Group where the two snapped windows remain unchanged
// throughout its existence.
inline constexpr char kSnapGroupActualDurationRootWord[] =
    "SnapGroupActualDuration";

inline constexpr char kSnapGroupExitPointRootWord[] = "SnapGroupExitPoint";

inline constexpr char kSnapGroupsCountRootWord[] = "SnapGroupsCount";

// Enumeration of Snap Group exit point.
// Note values are persisted to histograms so existing values should
// remain unchanged and new values should be added to the end. This should be
// kept in sync with `SnapGroupExitPoint` enum in
// tools/metrics/histograms/metadata/ash/enums.xml.
// Please keep it in sync with `WindowStateType` in
// chromeos/ui/base/window_state_type.h
enum class SnapGroupExitPoint {
  kDragWindowOut,
  kWindowStateChangeDeprecated,  // Deprecated
  kWindowDestruction,
  kTabletTransition,
  kWindowStateChangedDefault,
  kWindowStateChangedNormal,
  kWindowStateChangedMinimized,
  kWindowStateChangedMaximized,
  kWindowStateChangedInactive,
  kWindowStateChangedFullscreen,
  kWindowStateChangedPrimarySnapped,
  kWindowStateChangedSecondarySnapped,
  kWindowStateChangedPinned,
  kWindowStateChangedTrustedPinned,
  kWindowStateChangedPip,
  kWindowStateChangedFloated,
  kWindowBoundsChange,
  kCanNotFitInWorkArea,
  kSnapToReplace,
  kVisibleOnAllDesks,
  kSelectWindowInSnapGroupInPartialOverview,
  kMaxValue = kSelectWindowInSnapGroupInPartialOverview,
};

// Records the partial overview metrics for `item`. Should only be called while
// overview is in session.
void RecordPartialOverviewMetrics(OverviewItemBase* item);

void RecordSnapGroupPersistenceDuration(base::TimeDelta persistence_duration);

void RecordSnapGroupActualDuration(base::TimeDelta actual_duration);

void RecordSnapGroupExitPoint(SnapGroupExitPoint exit_point);

// Records the number of snap groups, up to 101.
void ReportSnapGroupsCountHistogram(int count);

ASH_EXPORT std::string BuildHistogramName(const char* const root_word);

}  // namespace ash

#endif  // ASH_WM_SNAP_GROUP_SNAP_GROUP_METRICS_H_
