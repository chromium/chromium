// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_METRICS_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_METRICS_H_

#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list_types.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace ash::holding_space_metrics {

// Enums -----------------------------------------------------------------------

// Enumeration of actions that can be taken on the holding space downloads
// button. These values are persisted to logs. Entries should not be renumbered
// and numeric values should never be reused.
enum class DownloadsAction {
  kClick = 0,
  kMaxValue = kClick,
};

// Enumeration of sources for events that occur in (and to) holding space.
enum class EventSource {
  kHoldingSpaceBubble = 0,
  kHoldingSpaceItem = 1,
  kHoldingSpaceItemContextMenu = 2,
  kHoldingSpaceTray = 3,
  kFilesApp = 4,
  kTest = 5,
  kWallpaper = 6,
  kMaxValue = kWallpaper,
};

// Enumeration of binding contexts for the file picker used to create a file in
// fulfillment of a `window.showSaveFilePicker()` request. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class FilePickerBindingContext {
  kUnknown = 0,
  kPhotoshopWeb = 1,
  kMaxValue = kPhotoshopWeb,
};

// Enumeration of actions that can be taken on the holding space Files app chip.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class FilesAppChipAction {
  kClick = 0,
  kMaxValue = kClick,
};

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
  kShowInBrowser = 10,
  kViewDetailsInBrowser = 11,
  kMaxValue = kViewDetailsInBrowser,
};

// Enumeration of reasons that a holding space item might fail to launch. These
// values are persisted to logs. Entries should not be renumbered and numeric
// values should never be reused.
enum class ItemLaunchFailureReason {
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

// Enumeration of actions that can be taken on the holding space suggestions
// section button. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused.
enum class SuggestionsAction {
  kCollapse = 0,
  kExpand = 1,
  kMaxValue = kExpand,
};

// Structs ---------------------------------------------------------------------

// Representation of a user's preferences.
struct ASH_PUBLIC_EXPORT UserPreferences {
  bool previews_enabled = false;
  bool suggestions_expanded = false;
};

// Utilities -------------------------------------------------------------------

// Returns the numeric representation of the extension for `file_path`.
ASH_PUBLIC_EXPORT size_t FilePathToExtension(const base::FilePath& file_path);

// Metrics ---------------------------------------------------------------------

// Records the `smoothness` of the holding space bubble resize animation. Note
// that `smoothness` is expected to be between 0 and 100 (inclusively) with
// 100 representing ideal smoothness of >= 60 frames per second.
ASH_PUBLIC_EXPORT void RecordBubbleResizeAnimationSmoothness(int smoothness);

// Records the specified `action` taken on the holding space downloads header.
ASH_PUBLIC_EXPORT void RecordDownloadsAction(DownloadsAction action);

// Records that a file picker with the specified `file_picker_binding_context`
// was used to create the file at the specified `file_path` in fulfillment of a
// `window.showSaveFilePicker()` request.
ASH_PUBLIC_EXPORT void RecordFileCreatedFromShowSaveFilePicker(
    const GURL& file_picker_binding_context,
    const base::FilePath& file_path);

// Records the specified `action` taken on the holding space Files app chip.
ASH_PUBLIC_EXPORT void RecordFilesAppChipAction(FilesAppChipAction action);

// Records the specified `action` taken on a set of holding space `items`.
ASH_PUBLIC_EXPORT void RecordItemAction(
    const std::vector<const HoldingSpaceItem*>& items,
    ItemAction action,
    EventSource event_source);

// Records an attempt to launch a holding space item of the specified `type`
// backed by the empty file at the specified `file_path`.
ASH_PUBLIC_EXPORT void RecordItemLaunchEmpty(HoldingSpaceItem::Type type,
                                             const base::FilePath& file_path);

// Records a failure to launch a holding space item of the specified `type`
// backed by the file at the specified `file_path` with the specified `reason`.
ASH_PUBLIC_EXPORT void RecordItemLaunchFailure(HoldingSpaceItem::Type type,
                                               const base::FilePath& file_path,
                                               ItemLaunchFailureReason reason);

// Records the specified `action` taken on the holding space pod in the shelf.
ASH_PUBLIC_EXPORT void RecordPodAction(PodAction action);

// Records the `smoothness` of the holding space pod resize animation. Note that
// `smoothness` is expected to be between 0 and 100 (inclusively) with 100
// representing ideal smoothness of >= 60 frames per second.
ASH_PUBLIC_EXPORT void RecordPodResizeAnimationSmoothness(int smoothness);

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

// Records total counts for the specified holding space `items`.
ASH_PUBLIC_EXPORT void RecordTotalItemCounts(
    const std::vector<const HoldingSpaceItem*>& items);

// Records a user's preferences.
ASH_PUBLIC_EXPORT void RecordUserPreferences(UserPreferences user_preferences);

// Records counts for the visible holding space `items` specified.
ASH_PUBLIC_EXPORT void RecordVisibleItemCounts(
    const std::vector<const HoldingSpaceItem*>& items);

// Observation -----------------------------------------------------------------

// An observer which receives notification of holding space metrics events.
class ASH_PUBLIC_EXPORT Observer : public base::CheckedObserver {
 public:
  // Invoked when holding space item action metrics are recorded.
  // See `RecordItemAction()`.
  virtual void OnHoldingSpaceItemActionRecorded(
      const std::vector<const HoldingSpaceItem*>& items,
      ItemAction action,
      EventSource event_source) {}

  // Invoked when holding space pod action metrics are recorded.
  // See `RecordPodAction()`.
  virtual void OnHoldingSpacePodActionRecorded(PodAction action) {}
};

// A scoped object which registers a specified `observer` to receive
// notification of holding space metrics events until its destruction.
class ASH_PUBLIC_EXPORT ScopedObservation {
 public:
  explicit ScopedObservation(Observer* observer);
  ScopedObservation(const ScopedObservation&) = delete;
  ScopedObservation& operator=(const ScopedObservation&) = delete;
  ~ScopedObservation();

 private:
  const raw_ptr<Observer> observer_;
};

}  // namespace ash::holding_space_metrics

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_METRICS_H_
