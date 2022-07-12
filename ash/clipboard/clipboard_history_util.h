// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_HISTORY_UTIL_H_
#define ASH_CLIPBOARD_CLIPBOARD_HISTORY_UTIL_H_

#include <string>

#include "ash/ash_export.h"
#include "base/strings/string_piece_forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace gfx {
class ImageSkia;
}

namespace ui {
class ClipboardData;
enum class ClipboardInternalFormat;
}  // namespace ui

namespace ash {
class ClipboardHistoryItem;

namespace ClipboardHistoryUtil {

// The first available command id for normal clipboard history menu items.
constexpr int kFirstItemCommandId = 1;

// The maximum available command id for normal clipboard history menu items.
constexpr int kMaxItemCommandId = 5;

// The max number of items stored in ClipboardHistory.
constexpr int kMaxClipboardItemsShared =
    kMaxItemCommandId - kFirstItemCommandId + 1;

// The max command ID, used to record histograms.
constexpr int kMaxCommandId = kFirstItemCommandId + kMaxClipboardItemsShared;

// The type of the action to take when the clipboard history menu item is
// activated.
enum class Action {
  kEmpty,

  // Pastes the activated item's corresponding clipboard data.
  kPaste,

  // Deletes the activated item.
  kDelete,

  // Selects the activated item.
  kSelect,

  // Selects the item hovered by mouse if any.
  kSelectItemHoveredByMouse
};

// IDs for the views used by the clipboard history menu.
enum ClipboardHistoryMenuViewID {
  // We start at 1 because 0 is not a valid view ID.
  kDeleteButtonViewID = 1,

  kMainButtonViewID
};

// Used in histograms, each value corresponds with an underlying format
// displayed by a ClipboardHistoryItemView, shown as
// ClipboardHistoryDisplayFormat in enums.xml. Do not reorder entries, if you
// must add to it, add at the end.
enum class ClipboardHistoryDisplayFormat {
  kText = 0,
  kPng = 1,
  kHtml = 2,
  kFile = 3,
  kMaxValue = 3,
};

// Modes for specifying a clipboard history pause's semantics.
enum PauseBehavior {
  // Clipboard history should be truly paused, i.e., any data change or read
  // should be ignored.
  kDefault = 0,

  // The operation guarded by this pause is a paste-based reorder, which is
  // allowed to change the clipboard history list.
  kAllowReorderOnPaste = 1,
};

// Returns the main format of the specified clipboard `data`.
// NOTE: One `ui::ClipboardData` instance may contain multiple formats.
ASH_EXPORT absl::optional<ui::ClipboardInternalFormat> CalculateMainFormat(
    const ui::ClipboardData& data);

// Returns the display format of the specified clipboard `data`. This determines
// which type of view is shown, and which type of histograms are recorded.
ASH_EXPORT ClipboardHistoryDisplayFormat
CalculateDisplayFormat(const ui::ClipboardData& data);

// Returns true if `data` contains the specified `format`.
ASH_EXPORT bool ContainsFormat(const ui::ClipboardData& data,
                               ui::ClipboardInternalFormat format);

// Records the histogram for deleting ClipboardHistoryItems.
ASH_EXPORT void RecordClipboardHistoryItemDeleted(
    const ClipboardHistoryItem& item);

// Records the histogram for pasting ClipboardHistoryItems.
ASH_EXPORT void RecordClipboardHistoryItemPasted(
    const ClipboardHistoryItem& item);

// Returns true if `data` contains file system data.
ASH_EXPORT bool ContainsFileSystemData(const ui::ClipboardData& data);

// Updates `sources` with the file system sources contained in `data`; updates
// `source_list` by splitting `sources` into pieces each of which corresponds to
// a file. Note that multiple files can be copied simultaneously. `sources` is
// referenced by `source_list` to reduce memory copies.
ASH_EXPORT void GetSplitFileSystemData(
    const ui::ClipboardData& data,
    std::vector<base::StringPiece16>* source_list,
    std::u16string* sources);

// Returns the count of copied files contained by the clipboard data.
ASH_EXPORT size_t GetCountOfCopiedFiles(const ui::ClipboardData& data);

// Returns file system sources contained in `data`. If `data` does not contain
// file system sources, an empty string is returned.
ASH_EXPORT std::u16string GetFileSystemSources(const ui::ClipboardData& data);

// Returns true if `data` is supported by clipboard history.
ASH_EXPORT bool IsSupported(const ui::ClipboardData& data);

// Returns whether the clipboard history is enabled for the current user mode.
ASH_EXPORT bool IsEnabledInCurrentMode();

// Returns an image icon for the file clipboard item.
ASH_EXPORT gfx::ImageSkia GetIconForFileClipboardItem(
    const ClipboardHistoryItem& item,
    const std::string& file_name);

}  // namespace ClipboardHistoryUtil
}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_HISTORY_UTIL_H_
