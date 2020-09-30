// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_metrics.h"

#include <map>
#include <string>

#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"

namespace ash {
namespace holding_space_metrics {

namespace {

// Helpers ---------------------------------------------------------------------

// Returns the string representation of the specified `action`. Note that these
// values are persisted to histograms so should remain unchanged.
std::string ItemActionToString(ItemAction action) {
  switch (action) {
    case ItemAction::kCopy:
      return "Copy";
    case ItemAction::kDrag:
      return "Drag";
    case ItemAction::kLaunch:
      return "Launch";
    case ItemAction::kPin:
      return "Pin";
    case ItemAction::kShowInFolder:
      return "ShowInFolder";
    case ItemAction::kUnpin:
      return "Unpin";
  }
  NOTREACHED();
  return std::string();
}

// Returns the string representation of the specified `type`. Note that these
// values are persisted to histograms so should remain unchanged.
std::string ItemTypeToString(HoldingSpaceItem::Type type) {
  switch (type) {
    case HoldingSpaceItem::Type::kDownload:
      return "Download";
    case HoldingSpaceItem::Type::kPinnedFile:
      return "PinnedFile";
    case HoldingSpaceItem::Type::kScreenshot:
      return "Screenshot";
  }
  NOTREACHED();
  return std::string();
}

}  // namespace

// Metrics ---------------------------------------------------------------------

void RecordPodAction(PodAction action) {
  base::UmaHistogramEnumeration("HoldingSpace.Pod.Action.All", action);
}

void RecordDownloadsAction(DownloadsAction action) {
  base::UmaHistogramEnumeration("HoldingSpace.Downloads.Action.All", action);
}

void RecordItemAction(const std::vector<const HoldingSpaceItem*>& items,
                      ItemAction action) {
  for (const HoldingSpaceItem* item : items) {
    base::UmaHistogramEnumeration("HoldingSpace.Item.Action.All", action);
    base::UmaHistogramEnumeration(
        "HoldingSpace.Item.Action." + ItemActionToString(action), item->type());
  }
}

void RecordItemCounts(const std::vector<const HoldingSpaceItem*>& items) {
  if (items.empty())
    return;

  base::UmaHistogramCounts1000("HoldingSpace.Item.Count.All", items.size());

  std::map<HoldingSpaceItem::Type, int> counts_by_type;
  for (const HoldingSpaceItem* item : items)
    ++counts_by_type[item->type()];

  for (const auto& count_by_type : counts_by_type) {
    base::UmaHistogramCounts1000(
        "HoldingSpace.Item.Count." + ItemTypeToString(count_by_type.first),
        count_by_type.second);
  }
}

void RecordTimeFromFirstAvailabilityToFirstEntry(base::TimeDelta time_delta) {
  // NOTE: 24 days appears to be the max supported number of days.
  base::UmaHistogramCustomTimes(
      "HoldingSpace.TimeFromFirstAvailabilityToFirstEntry", time_delta,
      /*min=*/base::TimeDelta(), /*max=*/base::TimeDelta::FromDays(24),
      /*buckets=*/50);
}

void RecordTimeFromFirstEntryToFirstPin(base::TimeDelta time_delta) {
  // NOTE: 24 days appears to be the max supported number of days.
  base::UmaHistogramCustomTimes("HoldingSpace.TimeFromFirstEntryToFirstPin",
                                time_delta,
                                /*min=*/base::TimeDelta(),
                                /*max=*/base::TimeDelta::FromDays(24),
                                /*buckets=*/50);
}

}  // namespace holding_space_metrics
}  // namespace ash
