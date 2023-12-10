// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/holding_space_wallpaper_nudge/holding_space_wallpaper_nudge_metrics.h"

#include "base/containers/enum_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::holding_space_wallpaper_nudge_metrics {

// HoldingSpaceWallpaperNudgeMetricsEnumTest -----------------------------------

// Base class of tests that verify all valid enum values and no other are
// included in the relevant `base::EnumSet`s.
using HoldingSpaceWallpaperNudgeMetricsEnumTest = testing::Test;

// Tests -----------------------------------------------------------------------

TEST_F(HoldingSpaceWallpaperNudgeMetricsEnumTest, AllInteractions) {
  // If a value in `Interactions` is added or deprecated, the below switch
  // statement must be modified accordingly. It should be a canonical list of
  // what values are considered valid.
  for (auto interaction : base::EnumSet<Interaction, Interaction::kMinValue,
                                        Interaction::kMaxValue>::All()) {
    bool should_exist_in_all_set = false;

    switch (interaction) {
      case Interaction::kDroppedFileOnHoldingSpace:
      case Interaction::kDroppedFileOnWallpaper:
      case Interaction::kDraggedFileOverWallpaper:
      case Interaction::kOpenedHoldingSpace:
      case Interaction::kPinnedFileFromAnySource:
      case Interaction::kPinnedFileFromContextMenu:
      case Interaction::kPinnedFileFromFilesApp:
      case Interaction::kPinnedFileFromHoldingSpaceDrop:
      case Interaction::kPinnedFileFromPinButton:
      case Interaction::kPinnedFileFromWallpaperDrop:
      case Interaction::kUsedOtherItem:
      case Interaction::kUsedPinnedItem:
        should_exist_in_all_set = true;
    }

    EXPECT_EQ(kAllInteractionsSet.Has(interaction), should_exist_in_all_set);
  }
}

}  // namespace ash::holding_space_wallpaper_nudge_metrics
