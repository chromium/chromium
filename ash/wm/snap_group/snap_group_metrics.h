// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SNAP_GROUP_SNAP_GROUP_METRICS_H_
#define ASH_WM_SNAP_GROUP_SNAP_GROUP_METRICS_H_

namespace ash {

class OverviewItemBase;

inline constexpr char kPartialOverviewSelectedWindowIndex[] =
    "Ash.SplitViewOverviewSession.SelectedWindowIndex";

inline constexpr char kPartialOverviewWindowListSize[] =
    "Ash.SplitViewOverviewSession.WindowListSize";

// Records the partial overview metrics for `item`. Should only be called while
// overview is in session.
void RecordPartialOverviewMetrics(OverviewItemBase* item);

}  // namespace ash

#endif  // ASH_WM_SNAP_GROUP_SNAP_GROUP_METRICS_H_
