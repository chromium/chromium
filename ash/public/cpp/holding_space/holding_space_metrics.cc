// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_metrics.h"

#include <map>
#include <string>

#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/containers/fixed_flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"

namespace ash::holding_space_metrics {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Note that 2, 3, and 4 are reserved in
// case additional special values need to be added in the future.
constexpr size_t kEmptyExtension = 0u;
constexpr size_t kOtherExtension = 1u;
constexpr size_t kFirstKnownExtension = 5u;
constexpr std::array<const char*, 72> kKnownExtensions = {
    ".3ga",     ".3gp",  ".aac",        ".alac", ".arw",   ".asf",  ".avi",
    ".bmp",     ".cr2",  ".crdownload", ".crx",  ".csv",   ".dmg",  ".dng",
    ".doc",     ".docx", ".exe",        ".flac", ".gdoc",  ".gif",  ".gsheet",
    ".gslides", ".htm",  ".html",       ".jar",  ".jpeg",  ".jpg",  ".log",
    ".m3u",     ".m3u8", ".m4a",        ".m4v",  ".mhtml", ".mid",  ".mkv",
    ".mov",     ".mp3",  ".mp4",        ".mpg",  ".nef",   ".nrw",  ".odf",
    ".odp",     ".ods",  ".odt",        ".oga",  ".ogg",   ".ogv",  ".orf",
    ".pdf",     ".png",  ".ppt",        ".pptx", ".ps",    ".ra",   ".raf",
    ".ram",     ".rar",  ".rm",         ".rtf",  ".rw2",   ".tini", ".torrent",
    ".txt",     ".wav",  ".webm",       ".webp", ".wma",   ".wmv",  ".xls",
    ".xlsx",    ".zip",
};
constexpr size_t kExtensionsSize =
    kFirstKnownExtension + kKnownExtensions.size();

// Helpers ---------------------------------------------------------------------

// Returns the list of holding space metrics observers.
base::ObserverList<Observer>& GetObserverList() {
  static base::NoDestructor<base::ObserverList<Observer>> observer_list;
  return *observer_list;
}

// Returns the `FilePickerBindingContext` representation of the specified
// `file_picker_binding_context`. Note that these values are persisted to
// histograms so should remain unchanged.
FilePickerBindingContext ToFilePickerBindingContext(
    const GURL& file_picker_binding_context) {
  return file_picker_binding_context.DomainIs("photoshop.adobe.com")
             ? FilePickerBindingContext::kPhotoshopWeb
             : FilePickerBindingContext::kUnknown;
}

// Returns the string representation of the specified `action`. Note that these
// values are persisted to histograms so should remain unchanged.
std::string ToString(ItemAction action) {
  switch (action) {
    case ItemAction::kCancel:
      return "Cancel";
    case ItemAction::kCopy:
      return "Copy";
    case ItemAction::kDrag:
      return "Drag";
    case ItemAction::kLaunch:
      return "Launch";
    case ItemAction::kPause:
      return "Pause";
    case ItemAction::kPin:
      return "Pin";
    case ItemAction::kRemove:
      return "Remove";
    case ItemAction::kResume:
      return "Resume";
    case ItemAction::kShowInBrowser:
      return "ShowInBrowser";
    case ItemAction::kShowInFolder:
      return "ShowInFolder";
    case ItemAction::kUnpin:
      return "Unpin";
    case ItemAction::kViewDetailsInBrowser:
      return "ViewDetailsInBrowser";
  }
  NOTREACHED();
}

// Records the counts of the specified holding space `items` to the item count
// histograms associated with the specified `prefix`.
void RecordItemCounts(const std::string& prefix,
                      const std::vector<const HoldingSpaceItem*>& items) {
  // Aliases.
  using FileSystemType = HoldingSpaceFile::FileSystemType;
  using Type = HoldingSpaceItem::Type;

  // Struct to hold partitioned counts.
  struct Counts {
    std::map<FileSystemType, size_t> by_fs_type;
    std::map<Type, size_t> by_type;
    std::map<Type, std::map<FileSystemType, size_t>> by_type_and_fs_type;
  };

  // Iterate over all `items` and partition counts.
  Counts counts;
  for (const HoldingSpaceItem* item : items) {
    const FileSystemType fs_type = item->file().file_system_type;
    const Type type = item->type();

    ++counts.by_fs_type[fs_type];
    ++counts.by_type[type];
    ++counts.by_type_and_fs_type[type][fs_type];
  }

  // It is discouraged to use exact linear histograms with max values greater
  // than 100. Though it's possible for holding space users to have item counts
  // in excess of 100, that is exceedingly rare and we can lump them together.
  constexpr size_t kExclusiveMax = 101;

  // Record "{prefix}.All".
  base::UmaHistogramExactLinear(base::StrCat({prefix, ".All"}), items.size(),
                                kExclusiveMax);

  // File system types are allowlisted based on need to limit the number of
  // recorded histograms arising from combinations with holding space item type.
  constexpr auto kAllowlistedFsTypes = base::MakeFixedFlatSet<FileSystemType>(
      {FileSystemType::kDriveFs, FileSystemType::kLocal});

  std::map<FileSystemType, std::string> fs_type_strings;
  for (const FileSystemType fs_type : kAllowlistedFsTypes) {
    const std::string& fs_type_string =
        fs_type_strings.emplace(fs_type, holding_space_util::ToString(fs_type))
            .first->second;

    // Record "{prefix}.All.FileSystemType.{fs_type}".
    base::UmaHistogramExactLinear(
        base::StrCat({prefix, ".All.FileSystemType.", fs_type_string}),
        counts.by_fs_type[fs_type], kExclusiveMax);
  }

  for (const Type type : holding_space_util::GetAllItemTypes()) {
    // Record "{prefix}.{type}";
    const std::string type_string = holding_space_util::ToString(type);
    base::UmaHistogramExactLinear(base::StrCat({prefix, ".", type_string}),
                                  counts.by_type[type], kExclusiveMax);

    // Record "{prefix}.{type}.FileSystemType.{fs_type}".
    for (const FileSystemType fs_type : kAllowlistedFsTypes) {
      base::UmaHistogramExactLinear(
          base::StrCat({prefix, ".", type_string, ".FileSystemType.",
                        fs_type_strings.at(fs_type)}),
          counts.by_type_and_fs_type[type][fs_type], kExclusiveMax);
    }
  }
}

}  // namespace

// Utilities -------------------------------------------------------------------

// Note that these values are persisted to histograms so must remain unchanged.
size_t FilePathToExtension(const base::FilePath& file_path) {
  if (file_path.empty()) {
    return kEmptyExtension;
  }

  const std::string extension = base::ToLowerASCII(file_path.Extension());
  if (extension.empty()) {
    return kEmptyExtension;
  }

  const auto it = base::ranges::find(kKnownExtensions, extension);
  if (it == kKnownExtensions.end()) {
    return kOtherExtension;
  }

  return kFirstKnownExtension + std::distance(kKnownExtensions.begin(), it);
}

// Metrics ---------------------------------------------------------------------

void RecordBubbleResizeAnimationSmoothness(int smoothness) {
  CHECK_GE(smoothness, 0);
  CHECK_LE(smoothness, 100);
  base::UmaHistogramPercentage("HoldingSpace.Animation.BubbleResize.Smoothness",
                               smoothness);
}

void RecordDownloadsAction(DownloadsAction action) {
  base::UmaHistogramEnumeration("HoldingSpace.Downloads.Action.All", action);
}

void RecordFileCreatedFromShowSaveFilePicker(
    const GURL& file_picker_binding_context,
    const base::FilePath& file_path) {
  base::UmaHistogramExactLinear(
      "HoldingSpace.FileCreatedFromShowSaveFilePicker.Extension",
      FilePathToExtension(file_path), kExtensionsSize + 1);
  base::UmaHistogramEnumeration(
      "HoldingSpace.FileCreatedFromShowSaveFilePicker.FilePickerBindingContext",
      ToFilePickerBindingContext(file_picker_binding_context));
}

void RecordFilesAppChipAction(FilesAppChipAction action) {
  base::UmaHistogramEnumeration("HoldingSpace.FilesAppChip.Action.All", action);
}

void RecordItemAction(const std::vector<const HoldingSpaceItem*>& items,
                      ItemAction action,
                      EventSource event_source) {
  if (event_source == EventSource::kTest) {
    CHECK_IS_TEST();
  }

  const std::string action_string = ToString(action);

  for (const HoldingSpaceItem* item : items) {
    // Record "HoldingSpace.Item.Action.All".
    base::UmaHistogramEnumeration("HoldingSpace.Item.Action.All", action);

    // Record "HoldingSpace.Item.Action.{action}".
    base::UmaHistogramEnumeration(
        base::StrCat({"HoldingSpace.Item.Action.", action_string}),
        item->type());

    // Record "HoldingSpace.Item.Action.{action}.Extension".
    base::UmaHistogramExactLinear(base::StrCat({"HoldingSpace.Item.Action.",
                                                action_string, ".Extension"}),
                                  FilePathToExtension(item->file().file_path),
                                  kExtensionsSize + 1);

    // Record "HoldingSpace.Item.Action.{action}.FileSystemType".
    base::UmaHistogramEnumeration(
        base::StrCat(
            {"HoldingSpace.Item.Action.", action_string, ".FileSystemType"}),
        item->file().file_system_type);
  }

  // Notify observers.
  for (Observer& observer : GetObserverList()) {
    observer.OnHoldingSpaceItemActionRecorded(items, action, event_source);
  }
}

void RecordItemLaunchEmpty(HoldingSpaceItem::Type type,
                           const base::FilePath& file_path) {
  base::UmaHistogramEnumeration("HoldingSpace.Item.Action.Launch.Empty", type);
  base::UmaHistogramExactLinear(
      "HoldingSpace.Item.Action.Launch.Empty.Extension",
      FilePathToExtension(file_path), kExtensionsSize + 1);
}

void RecordItemLaunchFailure(HoldingSpaceItem::Type type,
                             const base::FilePath& file_path,
                             ItemLaunchFailureReason reason) {
  base::UmaHistogramEnumeration("HoldingSpace.Item.Action.Launch.Failure",
                                type);
  base::UmaHistogramExactLinear(
      "HoldingSpace.Item.Action.Launch.Failure.Extension",
      FilePathToExtension(file_path), kExtensionsSize + 1);
  base::UmaHistogramEnumeration(
      "HoldingSpace.Item.Action.Launch.Failure.Reason", reason);
}

void RecordPodAction(PodAction action) {
  base::UmaHistogramEnumeration("HoldingSpace.Pod.Action.All", action);

  // Notify observers.
  for (Observer& observer : GetObserverList()) {
    observer.OnHoldingSpacePodActionRecorded(action);
  }
}

void RecordPodResizeAnimationSmoothness(int smoothness) {
  CHECK_GE(smoothness, 0);
  CHECK_LE(smoothness, 100);
  base::UmaHistogramPercentage("HoldingSpace.Animation.PodResize.Smoothness",
                               smoothness);
}

void RecordSuggestionsAction(SuggestionsAction action) {
  base::UmaHistogramEnumeration("HoldingSpace.Suggestions.Action.All", action);
}

void RecordTimeFromFirstAvailabilityToFirstAdd(base::TimeDelta time_delta) {
  // NOTE: 24 days appears to be the max supported number of days.
  base::UmaHistogramCustomTimes(
      "HoldingSpace.TimeFromFirstAvailabilityToFirstAdd", time_delta,
      /*min=*/base::Minutes(1),
      /*max=*/base::Days(24),
      /*buckets=*/50);
}

void RecordTimeFromFirstAvailabilityToFirstEntry(base::TimeDelta time_delta) {
  // NOTE: 24 days appears to be the max supported number of days.
  base::UmaHistogramCustomTimes(
      "HoldingSpace.TimeFromFirstAvailabilityToFirstEntry", time_delta,
      /*min=*/base::TimeDelta(), /*max=*/base::Days(24),
      /*buckets=*/50);
}

void RecordTimeFromFirstEntryToFirstPin(base::TimeDelta time_delta) {
  // NOTE: 24 days appears to be the max supported number of days.
  base::UmaHistogramCustomTimes("HoldingSpace.TimeFromFirstEntryToFirstPin",
                                time_delta,
                                /*min=*/base::TimeDelta(),
                                /*max=*/base::Days(24),
                                /*buckets=*/50);
}

void RecordTotalItemCounts(const std::vector<const HoldingSpaceItem*>& items) {
  RecordItemCounts("HoldingSpace.Item.TotalCountV2", items);
}

void RecordUserPreferences(UserPreferences preferences) {
  base::UmaHistogramBoolean("HoldingSpace.UserPreferences.PreviewsEnabled",
                            preferences.previews_enabled);
  base::UmaHistogramBoolean("HoldingSpace.UserPreferences.SuggestionsExpanded",
                            preferences.suggestions_expanded);
}

void RecordVisibleItemCounts(
    const std::vector<const HoldingSpaceItem*>& items) {
  RecordItemCounts("HoldingSpace.Item.VisibleCount", items);
}

// Observation -----------------------------------------------------------------

ScopedObservation::ScopedObservation(Observer* observer) : observer_(observer) {
  GetObserverList().AddObserver(observer_);
}

ScopedObservation::~ScopedObservation() {
  GetObserverList().RemoveObserver(observer_);
}

}  // namespace ash::holding_space_metrics
