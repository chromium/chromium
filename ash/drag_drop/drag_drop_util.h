// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DRAG_DROP_DRAG_DROP_UTIL_H_
#define ASH_DRAG_DROP_DRAG_DROP_UTIL_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/style/ash_color_id.h"
#include "ui/color/color_id.h"

namespace base {
class FilePath;
}  // namespace base

namespace ui {
class OSExchangeData;
}  // namespace ui

namespace ash::drag_drop {

// Indicates the background color of the drag image.
inline constexpr ui::ColorId kDragImageBackgroundColor =
    kColorAshShieldAndBaseOpaque;

// Indicates the shadow elevation of the drag image.
inline constexpr int kDragImageElevation = 2;

// Returns the file paths extracted from the specified `data` at one of two
// possible storage locations, either:
// (a) the file system sources storage location used by the Files app, or
// (b) the filenames storage location if and only if:
//     * no file paths were extracted from (a), and
//     * `fallback_to_filenames` is `true`.
ASH_EXPORT std::vector<base::FilePath> ExtractFilePaths(
    const ui::OSExchangeData& data,
    bool fallback_to_filenames);

}  // namespace ash::drag_drop

#endif  // ASH_DRAG_DROP_DRAG_DROP_UTIL_H_
