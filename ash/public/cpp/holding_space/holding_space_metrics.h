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
// shelf. These values are persisted to logs. Entries should not be renumbered
// and numeric values should never be reused.
enum class PodAction {
  // kClick (Deprecated) = 0,
  kShowBubble = 1,
  kCloseBubble = 2,
  kShowContextMenu = 3,
  kShowPreviews = 4,
  kHidePreviews = 5,
  kShowPod = 6,
  kHidePod = 7,
  kMaxValue = kHidePod,
};

// Records the specified `action` taken on the holding space pod in the shelf.
ASH_PUBLIC_EXPORT void RecordPodAction(PodAction action);

// Enumeration of actions that can be taken on the holding space downloads
// button. These values are persisted to logs. Entries should not be renumbered
// and numeric values should never be reused.
enum class DownloadsAction {
  kClick = 0,
  kMaxValue = kClick,
};

// Records the specified `action` taken on the holding space downloads header.
ASH_PUBLIC_EXPORT void RecordDownloadsAction(DownloadsAction action);

// Enumeration of actions that can be taken on the holding space Files app chip.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class FilesAppChipAction {
  kClick = 0,
  kMaxValue = kClick,
};

// Records the specified `action` taken on the holding space Files app chip.
ASH_PUBLIC_EXPORT void RecordFilesAppChipAction(FilesAppChipAction action);

// Enumeration of actions that can be taken on holding space items. These values
// are persisted to logs. Entries should not be renumbered and numeric values
// should never be reused.
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

// Records time from the first availability of the holding space feature to the
// first item being added to holding space.
ASH_PUBLIC_EXPORT void RecordTimeFromFirstAvailabilityToFirstAdd(
    base::TimeDelta time_delta);

// Records time from first availability to the first entry into holding space.
ASH_PUBLIC_EXPORT void RecordTimeFromFirstAvailabilityToFirstEntry(
    base::TimeDelta time_delta);

// Records time from first entry to the first pin into holding space.
ASH_PUBLIC_EXPORT void RecordTimeFromFirstEntryToFirstPin(
    base::TimeDelta time_delta);

}  // namespace holding_space_metrics
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_METRICS_H_
