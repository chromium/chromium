// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_HISTORY_UTIL_H_
#define ASH_CLIPBOARD_CLIPBOARD_HISTORY_UTIL_H_

#include <list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ash/ash_export.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "ui/base/models/image_model.h"

namespace ui {
class ClipboardData;
enum class ClipboardInternalFormat;
}  // namespace ui

namespace ash {
class ClipboardHistoryItem;

namespace clipboard_history_util {

// The first available command id for clipboard history menu items.
constexpr int kFirstItemCommandId = 1;

// The maximum available command id for clipboard history menu items.
constexpr int kMaxItemCommandId = 5;

// The max number of items stored in ClipboardHistory.
constexpr int kMaxClipboardItems = kMaxItemCommandId - kFirstItemCommandId + 1;

// A value greater than the maximum command ID, used to record histograms.
constexpr int kCommandIdBoundary = kMaxItemCommandId + 1;

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
enum MenuViewID {
  // We start at 1 because 0 is not a valid view ID.
  kBitmapItemView = 1,
  kContentsViewID,
  kCtrlVLabelID,
  kDeleteButtonViewID,
  kDisplayTextLabelID,
  kFooterContentViewID,
  kFooterContentV2LabelID,
  kFooterContentV2ViewID,
  kSecondaryDisplayTextLabelID,
};

// Modes for specifying a clipboard history pause's semantics.
enum class PauseBehavior {
  // Clipboard history should be truly paused, i.e., any data change or read
  // should be ignored.
  kDefault = 0,

  // The operation guarded by this pause is a paste-based reorder, which is
  // allowed to change the clipboard history list.
  kAllowReorderOnPaste = 1,
};

// The different operations clipboard history sees. These values are written to
// logs. New enum values can be added, but existing enums must never be
// renumbered, deleted, or reused. Keep this up to date with the
// `ClipboardHistoryOperation` enum in enums.xml.
enum class Operation {
  // Emitted when the user initiates a clipboard write.
  kCopy = 0,

  // Emitted when the user initiates a clipboard read.
  kPaste = 1,

  // Insert new types above this line.
  kMaxValue = kPaste
};

// The different ways a clipboard history list reorder can occur. These values
// are written to logs. New enum values can be added, but existing enums must
// never be renumbered, deleted, or reused. Keep this up to date with the
// `ClipboardHistoryReorderType` enum in enums.xml.
enum class ReorderType {
  // Emitted when the user copies an existing clipboard history item other than
  // the top item, causing the copied item to move to the top.
  kOnCopy = 0,

  // Emitted when the user pastes an existing clipboard history item other than
  // the top item, causing the pasted item to move to the top. This behavior is
  // gated by the `kClipboardHistoryReorder` flag.
  kOnPaste = 1,

  // Insert new types above this line.
  kMaxValue = kOnPaste
};

// Returns the main format of the specified clipboard `data`.
// NOTE: One `ui::ClipboardData` instance may contain multiple formats.
ASH_EXPORT std::optional<ui::ClipboardInternalFormat> CalculateMainFormat(
    const ui::ClipboardData& data);

// Returns true if `data` contains the specified `format`.
ASH_EXPORT bool ContainsFormat(const ui::ClipboardData& data,
                               ui::ClipboardInternalFormat format);

// Records the histogram for deleting clipboard history items.
ASH_EXPORT void RecordClipboardHistoryItemDeleted(
    const ClipboardHistoryItem& item);

// Records the histogram for pasting clipboard history items.
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
    std::vector<std::u16string_view>* source_list,
    std::u16string* sources);

// Returns the count of copied files contained by the clipboard data.
ASH_EXPORT size_t GetCountOfCopiedFiles(const ui::ClipboardData& data);

// Returns file system sources contained in `data`. If `data` does not contain
// file system sources, an empty string is returned.
ASH_EXPORT std::u16string GetFileSystemSources(const ui::ClipboardData& data);

// Returns the icon representation of the shortcut modifier key based on
// keyboard layout and whether the Assistant feature is enabled.
ASH_EXPORT const gfx::VectorIcon& GetShortcutKeyIcon();

// Returns the name of the shortcut modifier key based on keyboard layout.
ASH_EXPORT std::u16string GetShortcutKeyName();

// Returns true if `data` is supported by clipboard history.
ASH_EXPORT bool IsSupported(const ui::ClipboardData& data);

// Returns whether the clipboard history is enabled for the current user mode.
ASH_EXPORT bool IsEnabledInCurrentMode();

// Returns an image icon for the file clipboard item.
ASH_EXPORT ui::ImageModel GetIconForFileClipboardItem(
    const ClipboardHistoryItem& item);

// Returns a placeholder image to display for HTML items while their previews
// render.
ASH_EXPORT ui::ImageModel GetHtmlPreviewPlaceholder();

// Returns an item descriptor based on `item`.
crosapi::mojom::ClipboardHistoryItemDescriptor ItemToDescriptor(
    const ClipboardHistoryItem& item);

// Calculates the preferred width for clipboard history menu item views.
int GetPreferredItemViewWidth();

}  // namespace clipboard_history_util
}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_HISTORY_UTIL_H_
