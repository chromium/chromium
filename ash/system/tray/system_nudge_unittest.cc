// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_nudge.h"

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/shelf_config.h"
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
      bool anchor_status_area,
      NudgeCatalogName catalog_name = NudgeCatalogName::kTestCatalogName)
      : SystemNudge(kNudgeName,
                    catalog_name,
                    kIconSize,
                    kIconLabelSpacing,
                    kNudgePadding,
                    anchor_status_area) {}

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

using SystemNudgeTest = AshTestBase;

TEST_F(SystemNudgeTest, NudgeDefaultOnLeftSide) {
  Shelf* shelf = GetPrimaryShelf();
  display::Display primary_display = GetPrimaryDisplay();
  gfx::Rect display_bounds = primary_display.bounds();
  int shelf_size = ShelfConfig::Get()->shelf_size();
  gfx::Rect nudge_bounds;

  TestSystemNudge nudge(/*anchor_status_area=*/false);

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

TEST_F(SystemNudgeTest, NudgeAnchorStatusArea) {
  Shelf* shelf = GetPrimaryShelf();
  display::Display primary_display = GetPrimaryDisplay();
  gfx::Rect display_bounds = primary_display.bounds();
  int shelf_size = ShelfConfig::Get()->shelf_size();
  gfx::Rect nudge_bounds;

  TestSystemNudge nudge(/*anchor_status_area=*/true);

  nudge.Show();
  nudge_bounds = nudge.GetWidgetBounds();
  nudge_bounds.Outset(kNudgeMargin);
  EXPECT_EQ(nudge_bounds.right(), display_bounds.right());
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom() - shelf_size);

  shelf->SetAlignment(ShelfAlignment::kBottomLocked);
  nudge_bounds = nudge.GetWidgetBounds();
  nudge_bounds.Outset(kNudgeMargin);
  EXPECT_EQ(nudge_bounds.right(), display_bounds.right());
  EXPECT_EQ(nudge_bounds.bottom(), display_bounds.bottom() - shelf_size);

  shelf->SetAlignment(ShelfAlignment::kRight);
  nudge_bounds = nudge.GetWidgetBounds();
  nudge_bounds.Outset(kNudgeMargin);
  EXPECT_EQ(nudge_bounds.right(), display_bounds.right() - shelf_size);
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
  TestSystemNudge nudge_1(/*anchor_status_area=*/true, catalog_name_1);
  TestSystemNudge nudge_2(/*anchor_status_area=*/true, catalog_name_2);

  nudge_1.Show();
  histogram_tester.ExpectBucketCount(kNudgeShownCountHistogramName,
                                     catalog_name_1, 1);

  nudge_2.Show();
  nudge_2.Show();
  histogram_tester.ExpectBucketCount(kNudgeShownCountHistogramName,
                                     catalog_name_2, 2);
}

}  // namespace ash
