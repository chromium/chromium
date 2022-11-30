// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_metrics.h"

#include <map>
#include <string>

#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
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
    case ItemAction::kShowInFolder:
      return "ShowInFolder";
    case ItemAction::kUnpin:
      return "Unpin";
  }
  NOTREACHED();
  return std::string();
}

}  // namespace

// Utilities -------------------------------------------------------------------

// Note that these values are persisted to histograms so must remain unchanged.
size_t FilePathToExtension(const base::FilePath& file_path) {
  if (file_path.empty())
    return kEmptyExtension;

  const std::string extension = base::ToLowerASCII(file_path.Extension());
  if (extension.empty())
    return kEmptyExtension;

  auto* const* it = base::ranges::find(kKnownExtensions, extension);
  if (it == kKnownExtensions.end())
    return kOtherExtension;

  return kFirstKnownExtension + std::distance(kKnownExtensions.begin(), it);
}

// Records either the total or visible counts of `items` as appropriate..
void RecordItemCounts(const std::vector<const HoldingSpaceItem*>& items,
                      bool visible) {
  constexpr char kTotalCount[] = "HoldingSpace.Item.TotalCount";
  constexpr char kVisibleCount[] = "HoldingSpace.Item.Count";
  const char* prefix = visible ? kVisibleCount : kTotalCount;

  base::UmaHistogramCounts1000(base::StrCat({prefix, ".All"}), items.size());

  std::map<HoldingSpaceItem::Type, int> counts_by_type;
  for (const HoldingSpaceItem* item : items)
    ++counts_by_type[item->type()];

  for (int i = 0; i <= static_cast<int>(HoldingSpaceItem::Type::kMaxValue);
       ++i) {
    const auto type = static_cast<HoldingSpaceItem::Type>(i);
    base::UmaHistogramCounts1000(
        base::StrCat({prefix, ".", holding_space_util::ToString(type)}),
        counts_by_type[type]);
  }
}

// Metrics ---------------------------------------------------------------------

void RecordPodAction(PodAction action) {
  base::UmaHistogramEnumeration("HoldingSpace.Pod.Action.All", action);
}

void RecordDownloadsAction(DownloadsAction action) {
  base::UmaHistogramEnumeration("HoldingSpace.Downloads.Action.All", action);
}

void RecordFilesAppChipAction(FilesAppChipAction action) {
  base::UmaHistogramEnumeration("HoldingSpace.FilesAppChip.Action.All", action);
}

void RecordItemAction(const std::vector<const HoldingSpaceItem*>& items,
                      ItemAction action) {
  const std::string action_string = ToString(action);

  for (const HoldingSpaceItem* item : items) {
    base::UmaHistogramEnumeration("HoldingSpace.Item.Action.All", action);
    base::UmaHistogramEnumeration("HoldingSpace.Item.Action." + action_string,
                                  item->type());
    base::UmaHistogramExactLinear(
        "HoldingSpace.Item.Action." + action_string + ".Extension",
        FilePathToExtension(item->file_path()), kExtensionsSize);
  }
}

void RecordItemCounts(const std::vector<const HoldingSpaceItem*>& items) {
  RecordItemCounts(items, /*visible=*/false);
}

void RecordItemFailureToLaunch(HoldingSpaceItem::Type type,
                               const base::FilePath& file_path,
                               ItemFailureToLaunchReason reason) {
  base::UmaHistogramEnumeration("HoldingSpace.Item.FailureToLaunch", type);
  base::UmaHistogramExactLinear("HoldingSpace.Item.FailureToLaunch.Extension",
                                FilePathToExtension(file_path),
                                kExtensionsSize);
  base::UmaHistogramEnumeration("HoldingSpace.Item.FailureToLaunch.Reason",
                                reason);
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

void RecordBubbleResizeAnimationSmoothness(int smoothness) {
  DCHECK_GE(smoothness, 0);
  DCHECK_LE(smoothness, 100);
  base::UmaHistogramPercentage("HoldingSpace.Animation.BubbleResize.Smoothness",
                               smoothness);
}

void RecordPodResizeAnimationSmoothness(int smoothness) {
  DCHECK_GE(smoothness, 0);
  DCHECK_LE(smoothness, 100);
  base::UmaHistogramPercentage("HoldingSpace.Animation.PodResize.Smoothness",
                               smoothness);
}

void RecordUserPreferences(UserPreferences preferences) {
  base::UmaHistogramBoolean("HoldingSpace.UserPreferences.PreviewsEnabled",
                            preferences.previews_enabled);
  base::UmaHistogramBoolean("HoldingSpace.UserPreferences.SuggestionsExpanded",
                            preferences.suggestions_expanded);
}

void RecordVisibleItemCounts(
    const std::vector<const HoldingSpaceItem*>& items) {
  RecordItemCounts(items, /*visible=*/true);
}

}  // namespace ash::holding_space_metrics
