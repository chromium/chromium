// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/date_tray.h"

#include "ash/shelf/shelf.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"

namespace ash {

class DateTrayPixelTest : public AshTestBase {
 public:
  DateTrayPixelTest() = default;
  DateTrayPixelTest(const DateTrayPixelTest&) = delete;
  DateTrayPixelTest& operator=(const DateTrayPixelTest&) = delete;
  ~DateTrayPixelTest() override = default;

  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

 protected:
  DateTray* GetDateTray() {
    return GetPrimaryShelf()->GetStatusAreaWidget()->date_tray();
  }
};

// Tests the inactive date tray UI for bottom shelf alignment and side shelf
// alignment.
TEST_F(DateTrayPixelTest, InactiveDateTrayInBottomAndSideShelfPositions) {
  auto* shelf = GetPrimaryShelf();

  // Tests the bottom shelf.
  shelf->SetAlignment(ShelfAlignment::kBottom);
  ASSERT_EQ(shelf->alignment(), ShelfAlignment::kBottom);
  auto* bottom_date_tray = GetDateTray();
  // By default `bottom_date_tray` should be inactive.
  ASSERT_FALSE(bottom_date_tray->is_active());
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "bottom_shelf_inactive_date_tray", /*revision_number=*/0,
      bottom_date_tray));

  // Tests the side shelf.
  shelf->SetAlignment(ShelfAlignment::kLeft);
  ASSERT_EQ(shelf->alignment(), ShelfAlignment::kLeft);
  auto* side_date_tray = GetDateTray();
  // `side_date_tray` should remain inactive.
  ASSERT_FALSE(side_date_tray->is_active());
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "side_shelf_inactive_date_tray", /*revision_number=*/0, side_date_tray));
}

// Tests the active date tray UI for bottom shelf alignment and side shelf
// alignment.
TEST_F(DateTrayPixelTest, ActiveDateTrayInBottomAndSideShelfPositions) {
  auto* shelf = GetPrimaryShelf();

  // Tests the bottom shelf.
  shelf->SetAlignment(ShelfAlignment::kBottom);
  ASSERT_EQ(shelf->alignment(), ShelfAlignment::kBottom);
  auto* bottom_date_tray = GetDateTray();
  // Sets `bottom_date_tray` to be active in the bottom shelf.
  bottom_date_tray->SetIsActive(/*is_active=*/true);
  ASSERT_TRUE(bottom_date_tray->is_active());
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "bottom_shelf_active_date_tray", /*revision_number=*/0,
      bottom_date_tray));

  // Tests the side shelf.
  shelf->SetAlignment(ShelfAlignment::kLeft);
  ASSERT_EQ(shelf->alignment(), ShelfAlignment::kLeft);
  auto* side_date_tray = GetDateTray();
  // Sets `side_date_tray` to be active in the side shelf.
  side_date_tray->SetIsActive(/*is_active=*/true);
  ASSERT_TRUE(side_date_tray->is_active());
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "side_shelf_active_date_tray", /*revision_number=*/0, side_date_tray));
}

}  // namespace ash
