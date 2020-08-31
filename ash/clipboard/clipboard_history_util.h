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
namespace ClipboardHistoryUtil {

// The command id for deletion.
constexpr int kDeleteCommandId = 0;

// The first available command id for normal clipboard history menu items.
constexpr int kFirstItemCommandId = 1;

// Returns the main format of the specified clipboard `data`.
// NOTE: One `ui::ClipboardData` instance may contain multiple formats.
ASH_EXPORT base::Optional<ui::ClipboardInternalFormat> CalculateMainFormat(
    const ui::ClipboardData& data);

// Returns true if `data` contains the specified `format`.
ASH_EXPORT bool ContainsFormat(const ui::ClipboardData& data,
                               ui::ClipboardInternalFormat format);

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
