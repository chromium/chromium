// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_METRICS_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_METRICS_H_

#include <vector>

#include "ash/public/cpp/ash_public_export.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace ash {

class HoldingSpaceItem;

namespace holding_space_metrics {

// Enumeration of actions that can be taken on the holding space pod in the
// shelf. Note that these values are persisted to histograms so existing values
// should remain unchanged and new values should be added to the end.
enum class PodAction {
  kClick = 0,
  kMaxValue = kClick,
};

// Records the specified `action` taken on the holding space pod in the shelf.
ASH_PUBLIC_EXPORT void RecordPodAction(PodAction action);

// Enumeration of actions that can be taken on the holding space downloads
// button. Note that these values are persisted to histograms so existing
// values should remain unchanged and new values should be added to the end.
enum class DownloadsAction {
  kClick = 0,
  kMaxValue = kClick,
};

// Records the specified `action` taken on the holding space downloads header.
ASH_PUBLIC_EXPORT void RecordDownloadsAction(DownloadsAction action);

// Enumeration of actions that can be taken on holding space items. Note that
// these values are persisted to histograms so existing values should remain
// unchanged and new values should be added to the end.
enum class ItemAction {
  kCopy = 0,
  kDrag = 1,
  kLaunch = 2,
  kPin = 3,
  kShowInFolder = 4,
  kUnpin = 5,
  kMaxValue = kUnpin,
};

// Records the specified `action` taken on a set of holding space `items`.
ASH_PUBLIC_EXPORT void RecordItemAction(
    const std::vector<const HoldingSpaceItem*>& items,
    ItemAction action);

// Records counts for the specified holding space `items`.
ASH_PUBLIC_EXPORT void RecordItemCounts(
    const std::vector<const HoldingSpaceItem*>& items);

// Records time from first availability to the first entry into holding space.
ASH_PUBLIC_EXPORT void RecordTimeFromFirstAvailabilityToFirstEntry(
    base::TimeDelta time_delta);

// Records time from first entry to the first pin into holding space.
ASH_PUBLIC_EXPORT void RecordTimeFromFirstEntryToFirstPin(
    base::TimeDelta time_delta);

}  // namespace holding_space_metrics
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_METRICS_H_
