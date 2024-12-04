// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Quick Settings `FeatureTile` size constants.
constexpr gfx::Size kQSPrimaryTileSize =
    gfx::Size(kPrimaryFeatureTileWidth, kFeatureTileHeight);
constexpr gfx::Size kQSCompactTileSize =
    gfx::Size(kCompactFeatureTileWidth, kFeatureTileHeight);

// Creates a `Feature Tile` base that follows Quick Settings sizing standards.
FeatureTile* CreateQSFeatureTileBase(views::Widget* widget,
                                     bool is_compact = false) {
  auto tile = std::make_unique<FeatureTile>(
      views::Button::PressedCallback(), /*is_togglable=*/true,
      is_compact ? FeatureTile::TileType::kCompact
                 : FeatureTile::TileType::kPrimary);

  // Quick Settings Feature Tiles set a fixed size for their feature tiles.
  tile->SetPreferredSize(is_compact ? kQSCompactTileSize : kQSPrimaryTileSize);
  tile->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kPreferred,
                               /*adjust_height_for_width=*/false));

  return widget->GetContentsView()->AddChildView(std::move(tile));
}

}  // namespace

// Pixel tests for the quick settings feature tile view.
class FeatureTilePixelTest : public AshTestBase {
 public:
  FeatureTilePixelTest() {
    feature_list_.InitAndEnableFeature(chromeos::features::kJelly);
  }

  void SetUp() override {
    AshTestBase::SetUp();

    // We don't want tooltips to get in the way of the images we're testing.
    ::views::View::DisableKeyboardTooltipsForTesting();

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
    ::views::View::EnableKeyboardTooltipsForTesting();
    AshTestBase::TearDown();
  }

  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(FeatureTilePixelTest, PrimaryTile) {
  auto* tile = CreateQSFeatureTileBase(widget_.get());
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

TEST_F(FeatureTilePixelTest, PrimaryTileWithoutDiveInButton) {
  auto* tile = CreateQSFeatureTileBase(widget_.get());
  tile->SetVectorIcon(vector_icons::kDogfoodIcon);
  tile->SetLabel(u"Label");
  tile->SetSubLabel(u"Sub-label");
  // Needed for accessibility paint checks.
  tile->SetTooltipText(u"Tooltip");

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

TEST_F(FeatureTilePixelTest, PrimaryTile_RTL) {
  // Turn on RTL mode.
  base::i18n::SetRTLForTesting(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(base::i18n::IsRTL());

  auto* tile = CreateQSFeatureTileBase(widget_.get());
  tile->SetVectorIcon(vector_icons::kDogfoodIcon);
  tile->SetLabel(u"Label");
  tile->SetSubLabel(u"Sub-label");
  tile->CreateDecorativeDrillInArrow();

  // Needed for accessibility paint checks.
  tile->SetTooltipText(u"Tooltip");

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "basic",
      /*revision_number=*/0, widget_.get()));
}

TEST_F(FeatureTilePixelTest, CompactTile) {
  auto* tile = CreateQSFeatureTileBase(widget_.get(), /*is_compact=*/true);
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
      /*revision_number=*/2, widget_.get()));

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

  // Test one-line labels with one-line sub-labels.
  tile->SetLabel(u"One line");
  tile->SetSubLabel(u"One line");
  tile->SetSubLabelVisibility(true);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "one_line_with_sub_label",
      /*revision_number=*/0, widget_.get()));

  // Test eliding with sub-labels.
  tile->SetLabel(u"A very very long label");
  tile->SetSubLabel(u"A very very long sub-label");
  tile->SetSubLabelVisibility(true);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "elided_with_sub_label",
      /*revision_number=*/0, widget_.get()));
}

class FeatureTileVcDlcUiEnabledPixelTest : public FeatureTilePixelTest {
 public:
  FeatureTileVcDlcUiEnabledPixelTest() = default;
  FeatureTileVcDlcUiEnabledPixelTest(
      const FeatureTileVcDlcUiEnabledPixelTest&) = delete;
  FeatureTileVcDlcUiEnabledPixelTest& operator=(
      const FeatureTileVcDlcUiEnabledPixelTest&) = delete;
  ~FeatureTileVcDlcUiEnabledPixelTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_
        .InitWithFeatures(/*enabled_features=*/
                          {features::kFeatureManagementVideoConference,
                           features::kVcDlcUi},
                          /*disabled_features=*/{});
    // Need to create a fake VC tray controller if VcDlcUi is enabled because
    // this implies `features::IsVideoConferenceEnabled()` is true, and when
    // that is true the VC tray is created (and the VC tray depends on the
    // VC tray controller being available).
    tray_controller_ = std::make_unique<FakeVideoConferenceTrayController>();

    FeatureTilePixelTest::SetUp();

    // Shrink the container's contents bounds slightly to give even padding
    // around the tile, and ensure that focus rings don't get cut off in
    // screenshots.
    auto* layout_manager = static_cast<views::BoxLayout*>(
        widget_->GetContentsView()->GetLayoutManager());
    layout_manager->set_inside_border_insets(gfx::Insets::VH(0, 8));

    // Create the tile. The tooltip text needs to be set to pass accessibility
    // paint checks.
    tile_ = widget_->GetContentsView()
                ->AddChildView(std::make_unique<FeatureTile>(
                    views::Button::PressedCallback(), /*is_togglable=*/true,
                    FeatureTile::TileType::kCompact))
                ->GetWeakPtr();
    tile_->SetProperty(views::kBoxLayoutFlexKey,
                       views::BoxLayoutFlexSpecification());
    tile_->SetTooltipText(u"Tooltip");
    tile_->SetVectorIcon(vector_icons::kDogfoodIcon);
    tile_->SetLabel(u"One-line label");
  }
  void TearDown() override {
    FeatureTilePixelTest::TearDown();
    tray_controller_.reset();
  }

  void SetDownloadProgress(FeatureTile* tile, int progress) {
    tile->SetDownloadState(FeatureTile::DownloadState::kDownloading, progress);
  }

  FeatureTile* tile() { return tile_.get(); }

 private:
  std::unique_ptr<FakeVideoConferenceTrayController> tray_controller_ = nullptr;
  base::WeakPtr<FeatureTile> tile_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests the UI of a compact tile that is allowed to fill the size of its
// container.
//
// TODO(b/312771691): Add more cases, like the ones in
// `FeatureTilePixelTest.CompactTile`.
TEST_F(FeatureTileVcDlcUiEnabledPixelTest, CompactTileCanFillContainer) {
  // Use the default, one-line compact tile that is created during test set-up.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "basic",
      /*revision_number=*/1, widget_.get()));

  // Focus the tile (and reset the focus after the screenshot is taken).
  auto* previous_focused_view = widget_->GetFocusManager()->GetFocusedView();
  widget_->GetFocusManager()->SetFocusedView(tile());
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "focused",
      /*revision_number=*/1, widget_.get()));
  widget_->GetFocusManager()->SetFocusedView(previous_focused_view);

  // Toggle the tile (and reset the toggle state after the screenshot is taken).
  tile()->SetToggled(true);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "toggled",
      /*revision_number=*/1, widget_.get()));
  tile()->SetToggled(false);
}

// Tests the UI of a compact tile that has an in-progress download, for various
// download percentages.
TEST_F(FeatureTileVcDlcUiEnabledPixelTest, DownloadInProgress) {
  // Set the tile's download to be 0% complete.
  SetDownloadProgress(tile(), 0);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "0_percent",
      /*revision_number=*/1, widget_.get()));

  // Set the tile's download to be 1% complete.
  SetDownloadProgress(tile(), 1);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "1_percent",
      /*revision_number=*/1, widget_.get()));

  // Set the tile's download to be 50% complete.
  SetDownloadProgress(tile(), 50);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "50_percent",
      /*revision_number=*/1, widget_.get()));

  // Set the tile's download to be 99% complete.
  SetDownloadProgress(tile(), 99);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "99_percent",
      /*revision_number=*/1, widget_.get()));

  // Set the tile's download to be 100% complete.
  SetDownloadProgress(tile(), 100);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "100_percent",
      /*revision_number=*/1, widget_.get()));
}

// Tests the UI of a compact tile that has an error during download.
TEST_F(FeatureTileVcDlcUiEnabledPixelTest, ErrorInDlcDownload) {
  tile()->SetDownloadState(FeatureTile::DownloadState::kError, 0);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "error",
      /*revision_number=*/1, widget_.get()));
}

// Tests the UI of a compact tile that has a pending download.
TEST_F(FeatureTileVcDlcUiEnabledPixelTest, PendingDownload) {
  tile()->SetDownloadState(FeatureTile::DownloadState::kPending, 0);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "pending",
      /*revision_number=*/0, widget_.get()));
}

}  // namespace ash
