// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_HISTORY_UTIL_H_
#define ASH_CLIPBOARD_CLIPBOARD_HISTORY_UTIL_H_

#include "ash/ash_export.h"
#include "base/optional.h"
#include "base/strings/string16.h"

namespace ui {
class ClipboardData;
enum class ClipboardInternalFormat;
}  // namespace ui

namespace ash {
class ClipboardHistoryItem;

namespace ClipboardHistoryUtil {

// The command id for deletion.
constexpr int kDeleteCommandId = 0;

// The first available command id for normal clipboard history menu items.
constexpr int kFirstItemCommandId = 1;

// The max number of items stored in ClipboardHistory.
constexpr int kMaxClipboardItemsShared = 5;

// The max command ID, used to record histograms.
constexpr int kMaxCommandId = kFirstItemCommandId + kMaxClipboardItemsShared;
// Used in histograms, each value corresponds with an underlying format
// displayed by a ClipboardHistoryItemView. Do not reorder entries, if you must
// add to it, add at the end.
enum class ClipboardHistoryDisplayFormat {
  kText = 0,
  kBitmap = 1,
  kHtml = 2,
  kMaxValue = 2,
};

// Returns the main format of the specified clipboard `data`.
// NOTE: One `ui::ClipboardData` instance may contain multiple formats.
ASH_EXPORT base::Optional<ui::ClipboardInternalFormat> CalculateMainFormat(
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

// Returns file system sources contained in `data`. If `data` does not contain
// file system sources, an empty string is returned.
ASH_EXPORT base::string16 GetFileSystemSources(const ui::ClipboardData& data);

// Returns true if `data` is supported by clipboard history.
ASH_EXPORT bool IsSupported(const ui::ClipboardData& data);

}  // namespace ClipboardHistoryUtil
}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_HISTORY_UTIL_H_
