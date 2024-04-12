// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/snap_group/snap_group_metrics.h"

#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item_base.h"
#include "base/metrics/histogram_functions.h"

namespace ash {

void RecordPartialOverviewMetrics(OverviewItemBase* item) {
  auto& item_list = item->overview_grid()->window_list();
  base::UmaHistogramCounts1000(kPartialOverviewWindowListSize,
                               item_list.size());
  for (size_t i = 0; i < item_list.size(); ++i) {
    if (item_list[i].get() == item) {
      base::UmaHistogramCounts1000(kPartialOverviewSelectedWindowIndex, i);
    }
  }
}

}  // namespace ash
