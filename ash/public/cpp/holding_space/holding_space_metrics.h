// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_METRICS_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_METRICS_H_

#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace ash::holding_space_metrics {

// Returns the numeric representation of the extension for `file_path`.
ASH_PUBLIC_EXPORT size_t FilePathToExtension(const base::FilePath& file_path);

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
  kDragAndDropToPin = 8,
  kMaxValue = kDragAndDropToPin,
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
  kRemove = 6,
  kCancel = 7,
  kPause = 8,
  kResume = 9,
  kMaxValue = kResume,
};

// Records the specified `action` taken on a set of holding space `items`.
ASH_PUBLIC_EXPORT void RecordItemAction(
    const std::vector<const HoldingSpaceItem*>& items,
    ItemAction action);

// Records counts for the specified holding space `items`.
ASH_PUBLIC_EXPORT void RecordItemCounts(
    const std::vector<const HoldingSpaceItem*>& items);

// Enumeration of reasons that a holding space item might fail to launch. These
// values are persisted to logs. Entries should not be renumbered and numeric
// values should never be reused.
enum class ItemFailureToLaunchReason {
  kUnknown = 0,
  kCrosApiNotFound = 1,
  kDownloadNotFound = 2,
  kFileError = 3,
  kFileInfoError = 4,
  kInvalidType = 5,
  kNoHandlerForFileType = 6,
  kNoHandlerForItemType = 7,
  kPathEmpty = 8,
  kPathNotFound = 9,
  kReattemptToOpenWhenComplete = 10,
  kShutdown = 11,
  kMaxValue = kShutdown,
};

// Records a failure to launch a holding space item of the specified `type`
// backed by the file at the specified `file_path` with the specified `reason`.
ASH_PUBLIC_EXPORT void RecordItemFailureToLaunch(
    HoldingSpaceItem::Type type,
    const base::FilePath& file_path,
    ItemFailureToLaunchReason reason);

// Enumeration of actions that can be taken on the holding space suggestions
// section button. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused.
enum class SuggestionsAction {
  kCollapse = 0,
  kExpand = 1,
  kMaxValue = kExpand,
};

// Records the specified `action` taken on the holding space suggestions header.
ASH_PUBLIC_EXPORT void RecordSuggestionsAction(SuggestionsAction action);

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

// Records the `smoothness` of the holding space bubble resize animation. Note
// that `smoothness` is expected to be between 0 and 100 (inclusively) with
// 100 representing ideal smoothness of >= 60 frames per second.
ASH_PUBLIC_EXPORT void RecordBubbleResizeAnimationSmoothness(int smoothness);

// Records the `smoothness` of the holding space pod resize animation. Note that
// `smoothness` is expected to be between 0 and 100 (inclusively) with 100
// representing ideal smoothness of >= 60 frames per second.
ASH_PUBLIC_EXPORT void RecordPodResizeAnimationSmoothness(int smoothness);

// Representation of a user's preferences.
struct ASH_PUBLIC_EXPORT UserPreferences {
  bool previews_enabled = false;
  bool suggestions_expanded = false;
};

// Records a user's preferences.
ASH_PUBLIC_EXPORT void RecordUserPreferences(UserPreferences user_preferences);

// Records counts for the visible holding space `items` specified.
ASH_PUBLIC_EXPORT void RecordVisibleItemCounts(
    const std::vector<const HoldingSpaceItem*>& items);

}  // namespace ash::holding_space_metrics

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_METRICS_H_
