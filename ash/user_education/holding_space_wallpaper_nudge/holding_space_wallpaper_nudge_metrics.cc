// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/holding_space_wallpaper_nudge/holding_space_wallpaper_nudge_metrics.h"

#include "base/notreached.h"

namespace ash::holding_space_wallpaper_nudge_metrics {

std::string ToString(Interaction interaction) {
  switch (interaction) {
    case Interaction::kDroppedFileOnHoldingSpace:
      return "DroppedFileOnHoldingSpace";
    case Interaction::kDroppedFileOnWallpaper:
      return "DroppedFileOnWallpaper";
    case Interaction::kDraggedFileOverWallpaper:
      return "DraggedFileOverWallpaper";
    case Interaction::kOpenedHoldingSpace:
      return "OpenedHoldingSpace";
    case Interaction::kPinnedFileFromAnySource:
      return "PinnedFileFromAnySource";
    case Interaction::kPinnedFileFromContextMenu:
      return "PinnedFileFromContextMenu";
    case Interaction::kPinnedFileFromFilesApp:
      return "PinnedFileFromFilesApp";
    case Interaction::kPinnedFileFromHoldingSpaceDrop:
      return "PinnedFileFromHoldingSpaceDrop";
    case Interaction::kPinnedFileFromPinButton:
      return "PinnedFileFromPinButton";
    case Interaction::kPinnedFileFromWallpaperDrop:
      return "PinnedFileFromWallpaperDrop";
    case Interaction::kUsedOtherItem:
      return "UsedOtherItem";
    case Interaction::kUsedPinnedItem:
      return "UsedPinnedItem";
  }
  NOTREACHED_NORETURN();
}

}  // namespace ash::holding_space_wallpaper_nudge_metrics
