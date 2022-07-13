// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_nudge.h"

#include "ash/public/cpp/shelf_config.h"
#include "ash/test/ash_test_base.h"

namespace ash {

namespace {
constexpr int kNudgeMargin = 8;
constexpr int kNudgeWidth = 128;
constexpr int kNudgeHeight = 32;
}  // namespace

class SystemNudgeTest : public AshTestBase {
 public:
  SystemNudgeTest() = default;

  SystemNudgeTest(const SystemNudgeTest&) = delete;
  SystemNudgeTest& operator=(const SystemNudgeTest&) = delete;

  ~SystemNudgeTest() override = default;

  gfx::Rect CalculateWidgetBounds(const gfx::Rect& display_bounds,
                                  Shelf* shelf,
                                  int nudge_width,
                                  int nudge_height,
                                  bool anchor_status_area) {
    return SystemNudge::CalculateWidgetBounds(
        display_bounds, shelf, nudge_width, nudge_height, anchor_status_area);
  }
};

TEST_F(SystemNudgeTest, NudgeDefaultOnLeftSide) {
  Shelf* shelf = GetPrimaryShelf();
  display::Display primary_display = GetPrimaryDisplay();
  gfx::Rect display_bounds = primary_display.bounds();
  int shelf_size = ShelfConfig::Get()->shelf_size();
  gfx::Rect nudge_bounds;

  nudge_bounds =
      CalculateWidgetBounds(display_bounds, shelf, kNudgeWidth, kNudgeHeight,
                            /*anchor_status_area=*/false);
  EXPECT_EQ(nudge_bounds.size(), gfx::Size(kNudgeWidth, kNudgeHeight));
  nudge_bounds.Outset(kNudgeMargin);
  EXPECT_EQ(nudge_bounds.x(), display_bounds.x());
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom() - shelf_size);

  shelf->SetAlignment(ShelfAlignment::kBottomLocked);
  nudge_bounds =
      CalculateWidgetBounds(display_bounds, shelf, kNudgeWidth, kNudgeHeight,
                            /*anchor_status_area=*/false);
  EXPECT_EQ(nudge_bounds.size(), gfx::Size(kNudgeWidth, kNudgeHeight));
  nudge_bounds.Outset(kNudgeMargin);
  EXPECT_EQ(nudge_bounds.x(), display_bounds.x());
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom() - shelf_size);

  shelf->SetAlignment(ShelfAlignment::kRight);
  nudge_bounds =
      CalculateWidgetBounds(display_bounds, shelf, kNudgeWidth, kNudgeHeight,
                            /*anchor_status_area=*/false);
  EXPECT_EQ(nudge_bounds.size(), gfx::Size(kNudgeWidth, kNudgeHeight));
  nudge_bounds.Outset(kNudgeMargin);
  EXPECT_EQ(nudge_bounds.x(), display_bounds.x());
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom());

  shelf->SetAlignment(ShelfAlignment::kLeft);
  nudge_bounds =
      CalculateWidgetBounds(display_bounds, shelf, kNudgeWidth, kNudgeHeight,
                            /*anchor_status_area=*/false);
  EXPECT_EQ(nudge_bounds.size(), gfx::Size(kNudgeWidth, kNudgeHeight));
  nudge_bounds.Outset(kNudgeMargin);
  EXPECT_EQ(nudge_bounds.x(), display_bounds.x() + shelf_size);
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom());
}

TEST_F(SystemNudgeTest, NudgeAnchorStatusArea) {
  Shelf* shelf = GetPrimaryShelf();
  display::Display primary_display = GetPrimaryDisplay();
  gfx::Rect display_bounds = primary_display.bounds();
  int shelf_size = ShelfConfig::Get()->shelf_size();
  gfx::Rect nudge_bounds;

  nudge_bounds =
      CalculateWidgetBounds(display_bounds, shelf, kNudgeWidth, kNudgeHeight,
                            /*anchor_status_area=*/true);
  EXPECT_EQ(nudge_bounds.size(), gfx::Size(kNudgeWidth, kNudgeHeight));
  nudge_bounds.Outset(kNudgeMargin);
  EXPECT_EQ(nudge_bounds.right(), display_bounds.right());
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom() - shelf_size);

  shelf->SetAlignment(ShelfAlignment::kBottomLocked);
  nudge_bounds =
      CalculateWidgetBounds(display_bounds, shelf, kNudgeWidth, kNudgeHeight,
                            /*anchor_status_area=*/true);
  EXPECT_EQ(nudge_bounds.size(), gfx::Size(kNudgeWidth, kNudgeHeight));
  nudge_bounds.Outset(kNudgeMargin);
  EXPECT_EQ(nudge_bounds.right(), display_bounds.right());
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom() - shelf_size);

  shelf->SetAlignment(ShelfAlignment::kRight);
  nudge_bounds =
      CalculateWidgetBounds(display_bounds, shelf, kNudgeWidth, kNudgeHeight,
                            /*anchor_status_area=*/true);
  EXPECT_EQ(nudge_bounds.size(), gfx::Size(kNudgeWidth, kNudgeHeight));
  nudge_bounds.Outset(kNudgeMargin);
  EXPECT_EQ(nudge_bounds.right(), display_bounds.right() - shelf_size);
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom());

  shelf->SetAlignment(ShelfAlignment::kLeft);
  nudge_bounds =
      CalculateWidgetBounds(display_bounds, shelf, kNudgeWidth, kNudgeHeight,
                            /*anchor_status_area=*/true);
  EXPECT_EQ(nudge_bounds.size(), gfx::Size(kNudgeWidth, kNudgeHeight));
  nudge_bounds.Outset(kNudgeMargin);
  EXPECT_EQ(nudge_bounds.x(), display_bounds.x() + shelf_size);
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom());
}

}  // namespace ash
