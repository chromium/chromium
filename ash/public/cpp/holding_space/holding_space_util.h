// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_UTIL_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_UTIL_H_

#include <string>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/holding_space/holding_space_file.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "base/containers/flat_set.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class FilePath;
}  // namespace base

namespace ui {
class OSExchangeData;
}  // namespace ui

namespace ash {

namespace holding_space_metrics {
enum class EventSource;
}  // namespace holding_space_metrics

namespace holding_space_util {

// Returns the file paths extracted from the specified `data` at one of two
// possible storage locations, either:
// (a) the file system sources storage location used by the Files app, or
// (b) the filenames storage location if and only if:
//     * no file paths were extracted from (a), and
//     * `fallback_to_filenames` is `true`.
ASH_PUBLIC_EXPORT std::vector<base::FilePath> ExtractFilePaths(
    const ui::OSExchangeData& data,
    bool fallback_to_filenames);

// Returns the set of all defined holding space file system types. This differs
// from `base::EnumSet<HoldingSpaceFile::FileSystemType, ...>::All()` which
// contains undefined values if the underlying enum is not contiguous within its
// range.
ASH_PUBLIC_EXPORT base::flat_set<HoldingSpaceFile::FileSystemType>
GetAllFileSystemTypes();

// Returns the set of all defined holding space item types. This differs from
// `base::EnumSet<HoldingSpaceItem::Type, ...>::All()` which contains undefined
// values if the underlying enum is not contiguous within its range.
ASH_PUBLIC_EXPORT base::flat_set<HoldingSpaceItem::Type> GetAllItemTypes();

// Returns the maximum image size required for a holding space item of `type`.
ASH_PUBLIC_EXPORT gfx::Size GetMaxImageSizeForType(HoldingSpaceItem::Type type);

// Returns whether the specified `command_id` refers to a command for an
// in-progress item which is shown in an item's context menu and possibly, in
// the case of cancel/pause/resume, as primary/secondary actions on the item
// view itself.
ASH_PUBLIC_EXPORT bool IsInProgressCommand(HoldingSpaceCommandId command_id);

// Returns whether the specified `item` supports a given in-progress command
// which is shown in the `item`'s context menu and possibly, in the case of
// cancel/pause/resume, as primary/secondary actions on the `item` view itself.
ASH_PUBLIC_EXPORT bool SupportsInProgressCommand(
    const HoldingSpaceItem* item,
    HoldingSpaceCommandId command_id);

// Attempts to execute the in-progress command specified by `command_id` on
// `item`, returning whether the attempt was successful.
ASH_PUBLIC_EXPORT bool ExecuteInProgressCommand(
    const HoldingSpaceItem* item,
    HoldingSpaceCommandId command_id,
    holding_space_metrics::EventSource event_source);

// Returns the string representation of the specified file system `type`.
ASH_PUBLIC_EXPORT std::string ToString(HoldingSpaceFile::FileSystemType type);

// Returns the string representation of the specified holding space item `type`.
ASH_PUBLIC_EXPORT std::string ToString(HoldingSpaceItem::Type type);

}  // namespace holding_space_util
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_UTIL_H_
