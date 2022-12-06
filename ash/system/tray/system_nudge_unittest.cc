// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_nudge.h"

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/system/tray/system_nudge_label.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "ui/gfx/vector_icon_types.h"

namespace ash {

namespace {

constexpr int kNudgeMargin = 8;
constexpr int kIconSize = 20;
constexpr int kIconLabelSpacing = 16;
constexpr int kNudgePadding = 16;
constexpr int kNudgeWidth = 120;

constexpr char kNudgeName[] = "TestSystemNudge";

gfx::VectorIcon kEmptyIcon;

class TestSystemNudge : public SystemNudge {
 public:
  explicit TestSystemNudge(
      NudgeCatalogName catalog_name = NudgeCatalogName::kTestCatalogName)
      : SystemNudge(kNudgeName,
                    catalog_name,
                    kIconSize,
                    kIconLabelSpacing,
                    kNudgePadding) {}

  gfx::Rect GetWidgetBounds() {
    return widget()->GetClientAreaBoundsInScreen();
  }

 private:
  std::unique_ptr<SystemNudgeLabel> CreateLabelView() const override {
    return std::make_unique<SystemNudgeLabel>(std::u16string(), kNudgeWidth);
  }

  const gfx::VectorIcon& GetIcon() const override { return kEmptyIcon; }

  std::u16string GetAccessibilityText() const override {
    return std::u16string();
  }
};

constexpr char kNudgeShownCountHistogramName[] =
    "Ash.NotifierFramework.Nudge.ShownCount";

}  // namespace

class SystemNudgeTest : public AshTestBase {
 public:
  SystemNudgeTest() = default;
  SystemNudgeTest(const SystemNudgeTest&) = delete;
  SystemNudgeTest& operator=(const SystemNudgeTest&) = delete;
  ~SystemNudgeTest() override = default;

  void SetState(ShelfVisibilityState visibility_state) {
    GetPrimaryShelf()->shelf_layout_manager()->SetState(visibility_state);
  }
};

TEST_F(SystemNudgeTest, NudgeDefaultOnLeftSide) {
  Shelf* shelf = GetPrimaryShelf();
  display::Display primary_display = GetPrimaryDisplay();
  gfx::Rect display_bounds = primary_display.bounds();
  int shelf_size = ShelfConfig::Get()->shelf_size();
  gfx::Rect nudge_bounds;

  TestSystemNudge nudge;

  nudge.Show();
  nudge_bounds = nudge.GetWidgetBounds();
  nudge_bounds.Outset(kNudgeMargin);
  EXPECT_EQ(nudge_bounds.x(), display_bounds.x());
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom() - shelf_size);

  shelf->SetAlignment(ShelfAlignment::kBottomLocked);
  nudge_bounds = nudge.GetWidgetBounds();
  nudge_bounds.Outset(kNudgeMargin);
  EXPECT_EQ(nudge_bounds.x(), display_bounds.x());
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom() - shelf_size);

  shelf->SetAlignment(ShelfAlignment::kRight);
  nudge_bounds = nudge.GetWidgetBounds();
  nudge_bounds.Outset(kNudgeMargin);
  EXPECT_EQ(nudge_bounds.x(), display_bounds.x());
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom());

  shelf->SetAlignment(ShelfAlignment::kLeft);
  nudge_bounds = nudge.GetWidgetBounds();
  nudge_bounds.Outset(kNudgeMargin);
  EXPECT_EQ(nudge_bounds.x(), display_bounds.x() + shelf_size);
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom());
}

TEST_F(SystemNudgeTest, ShownCountMetric) {
  base::HistogramTester histogram_tester;

  const NudgeCatalogName catalog_name_1 = static_cast<NudgeCatalogName>(1);
  const NudgeCatalogName catalog_name_2 = static_cast<NudgeCatalogName>(2);
  TestSystemNudge nudge_1(catalog_name_1);
  TestSystemNudge nudge_2(catalog_name_2);

  nudge_1.Show();
  histogram_tester.ExpectBucketCount(kNudgeShownCountHistogramName,
                                     catalog_name_1, 1);

  nudge_2.Show();
  nudge_2.Show();
  histogram_tester.ExpectBucketCount(kNudgeShownCountHistogramName,
                                     catalog_name_2, 2);
}

TEST_F(SystemNudgeTest, NudgePositionChangeWhenShelfAutoHide) {
  Shelf* shelf = GetPrimaryShelf();
  display::Display primary_display = GetPrimaryDisplay();
  gfx::Rect display_bounds = primary_display.bounds();
  int shelf_size = ShelfConfig::Get()->shelf_size();
  gfx::Rect nudge_bounds;
  TestSystemNudge nudge;

  // Enables the auto hide behavior.
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  // Tests when the shelf is on the bottom.
  shelf->SetAlignment(ShelfAlignment::kBottom);

  // When the shelf is visiblie, the bottom of the nudge should be on the top of
  // the the shelf (with `-self_size`).
  SetState(ShelfVisibilityState::SHELF_VISIBLE);
  nudge.Show();
  nudge_bounds = nudge.GetWidgetBounds();
  nudge_bounds.Outset(kNudgeMargin);
  EXPECT_EQ(nudge_bounds.x(), display_bounds.x());
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom() - shelf_size);

  // When the shelf is hidden, the bottom of the nudge should be just on the
  // display bottom.
  SetState(ShelfVisibilityState::SHELF_HIDDEN);
  nudge.Show();
  nudge_bounds = nudge.GetWidgetBounds();
  nudge_bounds.Outset(kNudgeMargin);
  EXPECT_EQ(nudge_bounds.x(), display_bounds.x());
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom());
}

TEST_F(SystemNudgeTest, NudgePositionWithBottomLocked) {
  Shelf* shelf = GetPrimaryShelf();
  display::Display primary_display = GetPrimaryDisplay();
  gfx::Rect display_bounds = primary_display.bounds();
  int shelf_size = ShelfConfig::Get()->shelf_size();
  gfx::Rect nudge_bounds;
  TestSystemNudge nudge;

  // Enables the auto hide behavior.
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  // Tests when the shelf is on the bottom with locked state.
  shelf->SetAlignment(ShelfAlignment::kBottomLocked);

  // When the shelf is visiblie, the bottom of the nudge should be on the top of
  // the the shelf (with `-self_size`).
  SetState(ShelfVisibilityState::SHELF_VISIBLE);
  nudge.Show();
  nudge_bounds = nudge.GetWidgetBounds();
  nudge_bounds.Outset(kNudgeMargin);
  EXPECT_EQ(nudge_bounds.x(), display_bounds.x());
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom() - shelf_size);

  // When the shelf is hidden, the bottom of the nudge should be just on the
  // display bottom.
  SetState(ShelfVisibilityState::SHELF_HIDDEN);
  nudge.Show();
  nudge_bounds = nudge.GetWidgetBounds();
  nudge_bounds.Outset(kNudgeMargin);
  EXPECT_EQ(nudge_bounds.x(), display_bounds.x());
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom());
}

}  // namespace ash
