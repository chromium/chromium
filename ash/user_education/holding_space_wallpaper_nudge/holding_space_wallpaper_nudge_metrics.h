// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_HOLDING_SPACE_WALLPAPER_NUDGE_HOLDING_SPACE_WALLPAPER_NUDGE_METRICS_H_
#define ASH_USER_EDUCATION_HOLDING_SPACE_WALLPAPER_NUDGE_HOLDING_SPACE_WALLPAPER_NUDGE_METRICS_H_

#include <string>

#include "ash/ash_export.h"
#include "base/containers/enum_set.h"

namespace ash::holding_space_wallpaper_nudge_metrics {

// Enums -----------------------------------------------------------------------

// Enumeration of interactions users may engage in after the Holding Space
// wallpaper nudge. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused. Be sure to update
// `kAllInteractionsSet` accordingly.
enum class Interaction {
  kMinValue = 0,
  kDroppedFileOnHoldingSpace = kMinValue,
  kDroppedFileOnWallpaper = 1,
  kDraggedFileOverWallpaper = 2,
  kOpenedHoldingSpace = 3,
  kPinnedFileFromAnySource = 4,
  kPinnedFileFromContextMenu = 5,
  kPinnedFileFromFilesApp = 6,
  kPinnedFileFromHoldingSpaceDrop = 7,
  kPinnedFileFromPinButton = 8,
  kPinnedFileFromWallpaperDrop = 9,
  kUsedOtherItem = 10,
  kUsedPinnedItem = 11,
  kMaxValue = kUsedPinnedItem,
};

static constexpr auto kAllInteractionsSet =
    base::EnumSet<Interaction, Interaction::kMinValue, Interaction::kMaxValue>({
        Interaction::kDroppedFileOnHoldingSpace,
        Interaction::kDroppedFileOnWallpaper,
        Interaction::kDraggedFileOverWallpaper,
        Interaction::kOpenedHoldingSpace,
        Interaction::kPinnedFileFromAnySource,
        Interaction::kPinnedFileFromContextMenu,
        Interaction::kPinnedFileFromFilesApp,
        Interaction::kPinnedFileFromHoldingSpaceDrop,
        Interaction::kPinnedFileFromPinButton,
        Interaction::kPinnedFileFromWallpaperDrop,
        Interaction::kUsedOtherItem,
        Interaction::kUsedPinnedItem,
    });

// Utilities -------------------------------------------------------------------

// Returns a string representation of the given `interaction`.
ASH_EXPORT std::string ToString(Interaction interaction);

}  // namespace ash::holding_space_wallpaper_nudge_metrics

#endif  // ASH_USER_EDUCATION_HOLDING_SPACE_WALLPAPER_NUDGE_HOLDING_SPACE_WALLPAPER_NUDGE_METRICS_H_
