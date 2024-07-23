// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/snap_group/snap_group_metrics.h"

#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"

namespace ash {

void RecordPartialOverviewMetrics(OverviewItemBase* item) {
  auto& item_list = item->overview_grid()->item_list();
  base::UmaHistogramCounts1000(kPartialOverviewWindowListSize,
                               item_list.size());
  for (size_t i = 0; i < item_list.size(); ++i) {
    if (item_list[i].get() == item) {
      base::UmaHistogramCounts1000(kPartialOverviewSelectedWindowIndex, i);
    }
  }
}

void RecordSnapGroupPersistenceDuration(base::TimeDelta persistence_duration) {
  base::UmaHistogramCustomCounts(
      BuildHistogramName(kSnapGroupPersistenceDurationRootWord),
      persistence_duration.InSeconds(), /*min=*/1,
      /*exclusive_max=*/base::Hours(8).InSeconds(),
      /*buckets=*/50);
}

void RecordSnapGroupActualDuration(base::TimeDelta actual_duration) {
  base::UmaHistogramCustomCounts(
      BuildHistogramName(kSnapGroupActualDurationRootWord),
      actual_duration.InSeconds(), /*min=*/1,
      /*exclusive_max=*/base::Hours(8).InSeconds(),
      /*buckets=*/50);
}

void RecordSnapGroupExitPoint(SnapGroupExitPoint exit_point) {
  base::UmaHistogramEnumeration(BuildHistogramName(kSnapGroupExitPointRootWord),
                                exit_point);
}

void ReportSnapGroupsCountHistogram(int count) {
  UMA_HISTOGRAM_EXACT_LINEAR(BuildHistogramName(kSnapGroupsCountRootWord),
                             count,
                             /*exclusive_max=*/101);
}

std::string BuildHistogramName(const char* const root_word) {
  std::string histogram_name(kSnapGroupsMetricCommonPrefix);
  histogram_name.append(root_word);
  return histogram_name;
}

}  // namespace ash
