// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/system/unified/feature_tile.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Pixel tests for the quick settings feature tile view.
class FeatureTilePixelTest : public AshTestBase {
 public:
  FeatureTilePixelTest() {
    feature_list_.InitAndEnableFeature(chromeos::features::kJelly);
  }

  void SetUp() override {
    AshTestBase::SetUp();

    widget_ = CreateFramelessTestWidget();
    // Ensure the widget is large enough for the tile's focus ring (which is
    // drawn outside the tile's bounds).
    widget_->SetBounds(gfx::Rect(200, 80));
    auto* contents =
        widget_->SetContentsView(std::make_unique<views::BoxLayoutView>());
    contents->SetMainAxisAlignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
    contents->SetCrossAxisAlignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    // The tile colors have transparency, so set a background color so they
    // render like in production.
    contents->SetBackground(views::CreateThemedSolidBackground(
        cros_tokens::kCrosSysSystemBaseElevated));
  }

  void TearDown() override {
    widget_.reset();
    AshTestBase::TearDown();
  }

  // AshTestBase:
  absl::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(FeatureTilePixelTest, PrimaryTile) {
  auto* tile = widget_->GetContentsView()->AddChildView(
      std::make_unique<FeatureTile>(base::DoNothing(), /*toggleable=*/true,
                                    FeatureTile::TileType::kPrimary));
  // Use the default size from go/cros-quick-settings-spec
  tile->SetPreferredSize(gfx::Size(180, 64));
  tile->SetVectorIcon(vector_icons::kDogfoodIcon);
  tile->SetLabel(u"Label");
  tile->SetSubLabel(u"Sub-label");
  // Needed for accessibility paint checks.
  tile->SetTooltipText(u"Tooltip");
  tile->CreateDecorativeDrillInArrow();

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "basic",
      /*revision_number=*/0, widget_.get()));

  widget_->GetFocusManager()->SetFocusedView(tile);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "focused",
      /*revision_number=*/0, widget_.get()));

  tile->SetToggled(true);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "toggled",
      /*revision_number=*/0, widget_.get()));

  // Test eliding.
  tile->SetLabel(u"A very very long label");
  tile->SetSubLabel(u"A very very long label");
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "elided",
      /*revision_number=*/0, widget_.get()));
}

TEST_F(FeatureTilePixelTest, CompactTile) {
  auto* tile = widget_->GetContentsView()->AddChildView(
      std::make_unique<FeatureTile>(base::DoNothing(), /*toggleable=*/true,
                                    FeatureTile::TileType::kCompact));
  // Use the default size from go/cros-quick-settings-spec
  tile->SetPreferredSize(gfx::Size(86, 64));
  tile->SetVectorIcon(vector_icons::kDogfoodIcon);
  tile->SetLabel(u"Multi-line label");
  // Needed for accessibility paint checks.
  tile->SetTooltipText(u"Tooltip");

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "basic",
      /*revision_number=*/1, widget_.get()));

  widget_->GetFocusManager()->SetFocusedView(tile);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "focused",
      /*revision_number=*/1, widget_.get()));

  tile->SetToggled(true);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "toggled",
      /*revision_number=*/1, widget_.get()));

  // Test eliding.
  tile->SetLabel(u"A very very long label");
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "elided",
      /*revision_number=*/1, widget_.get()));

  // Test font descenders ("g").
  tile->SetLabel(u"Multi-line ggggg");
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "descenders",
      /*revision_number=*/0, widget_.get()));

  // Test one-line labels.
  tile->SetLabel(u"One line");
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "one_line",
      /*revision_number=*/0, widget_.get()));
}

}  // namespace
}  // namespace ash
