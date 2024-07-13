// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/feature_tile.h"

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_class_properties.h"

namespace ash {

constexpr int kDefaultButtonRadius = 16;

namespace {

class MockFeaturePodController : public FeaturePodControllerBase {
 public:
  MockFeaturePodController() = default;
  explicit MockFeaturePodController(bool togglable) : togglable_(togglable) {}

  MockFeaturePodController(const MockFeaturePodController&) = delete;
  MockFeaturePodController& operator=(const MockFeaturePodController&) = delete;

  ~MockFeaturePodController() override = default;

  std::unique_ptr<FeatureTile> CreateTile(bool compact = false) override {
    auto tile = std::make_unique<FeatureTile>(
        base::BindRepeating(&FeaturePodControllerBase::OnLabelPressed,
                            weak_ptr_factory_.GetWeakPtr()),
        togglable_,
        compact ? FeatureTile::TileType::kCompact
                : FeatureTile::TileType::kPrimary);
    tile->SetVectorIcon(vector_icons::kDogfoodIcon);
    tile->SetIconClickCallback(
        base::BindRepeating(&MockFeaturePodController::OnIconPressed,
                            weak_ptr_factory_.GetWeakPtr()));
    tile_ = tile.get();
    return tile;
  }

  QsFeatureCatalogName GetCatalogName() override {
    return QsFeatureCatalogName::kUnknown;
  }

  void OnIconPressed() override {
    was_icon_pressed_ = true;
    // FeaturePodController elements in production know if they are togglable,
    // but in this mock we need to check before changing the toggled state. This
    // attempts to match UX specs: tiles with clickable icons should toggle when
    // their icon is clicked rather than their label.
    if (togglable_ && tile_->is_icon_clickable()) {
      toggled_ = !toggled_;
      tile_->SetToggled(toggled_);
    }
  }

  void OnLabelPressed() override {
    was_label_pressed_ = true;
    // FeaturePodController elements in production know if they are togglable,
    // but in this mock we need to check before changing the toggled state. This
    // attempts to match UX specs: tiles with clickable icons should toggle when
    // their icon is clicked rather than their label.
    if (togglable_ && !tile_->is_icon_clickable()) {
      toggled_ = !toggled_;
      tile_->SetToggled(toggled_);
    }
  }

  bool WasIconPressed() { return was_icon_pressed_; }
  bool WasLabelPressed() { return was_label_pressed_; }

 private:
  raw_ptr<FeatureTile> tile_ = nullptr;
  bool was_icon_pressed_ = false;
  bool was_label_pressed_ = false;
  bool togglable_ = false;
  bool toggled_ = false;

  base::WeakPtrFactory<MockFeaturePodController> weak_ptr_factory_{this};
};

}  // namespace

class FeatureTileTest
    : public AshTestBase,
      public testing::WithParamInterface</*IsVcDlcUiEnabled*/ bool> {
 public:
  FeatureTileTest() = default;
  FeatureTileTest(const FeatureTileTest&) = delete;
  FeatureTileTest& operator=(const FeatureTileTest&) = delete;
  ~FeatureTileTest() override = default;

  // AshTestBase:
  void SetUp() override {
    if (IsVcDlcUiEnabled()) {
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
    }
    AshTestBase::SetUp();
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
  }

  void TearDown() override {
    widget_.reset();
    AshTestBase::TearDown();
    if (IsVcDlcUiEnabled()) {
      tray_controller_.reset();
    }
  }

  void PressTab() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_NONE);
  }

  bool IsVcDlcUiEnabled() { return GetParam(); }

  std::u16string GetExpectedDownloadPendingLabel() {
    return l10n_util::GetStringUTF16(
        IDS_ASH_FEATURE_TILE_DOWNLOAD_PENDING_TITLE);
  }

  std::u16string GetExpectedDownloadInProgressLabel(int progress) {
    return l10n_util::GetStringFUTF16(
        IDS_ASH_FEATURE_TILE_DOWNLOAD_IN_PROGRESS_TITLE,
        base::NumberToString16(progress));
  }

  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<FakeVideoConferenceTrayController> tray_controller_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(IsVcDlcUiEnabled, FeatureTileTest, testing::Bool());

TEST_P(FeatureTileTest, PrimaryTile_LaunchSurface) {
  auto mock_controller =
      std::make_unique<MockFeaturePodController>(/*togglable=*/false);
  auto* tile = widget_->SetContentsView(mock_controller->CreateTile());

  EXPECT_FALSE(tile->is_icon_clickable());
  EXPECT_FALSE(tile->drill_in_arrow());

  // Ensure label hasn't been pressed.
  EXPECT_FALSE(tile->IsToggled());
  EXPECT_FALSE(mock_controller->WasLabelPressed());

  LeftClickOn(tile);

  // Ensure label was pressed and button does not toggle after clicking it.
  EXPECT_TRUE(mock_controller->WasLabelPressed());
  EXPECT_FALSE(tile->IsToggled());
}

TEST_P(FeatureTileTest, PrimaryTile_Toggle) {
  auto mock_controller =
      std::make_unique<MockFeaturePodController>(/*togglable=*/true);
  auto* tile = widget_->SetContentsView(mock_controller->CreateTile());

  EXPECT_FALSE(tile->is_icon_clickable());
  EXPECT_FALSE(tile->drill_in_arrow());

  // Ensure label hasn't been pressed.
  EXPECT_FALSE(tile->IsToggled());
  EXPECT_FALSE(mock_controller->WasLabelPressed());

  LeftClickOn(tile);

  // Ensure label was pressed and button toggles after clicking it.
  EXPECT_TRUE(mock_controller->WasLabelPressed());
  EXPECT_TRUE(tile->IsToggled());

  LeftClickOn(tile);

  // Ensure button toggles after clicking it again.
  EXPECT_FALSE(tile->IsToggled());
}

TEST_P(FeatureTileTest, PrimaryTile_ToggleWithDrillIn) {
  auto mock_controller =
      std::make_unique<MockFeaturePodController>(/*togglable=*/true);
  auto* tile = widget_->SetContentsView(mock_controller->CreateTile());

  tile->SetIconClickable(true);
  views::test::RunScheduledLayout(tile);
  EXPECT_TRUE(tile->is_icon_clickable());
  ASSERT_TRUE(tile->icon_button());
  EXPECT_TRUE(tile->icon_button()->GetEnabled());

  // Ensure tile is not toggled and icon is not pressed.
  EXPECT_FALSE(tile->IsToggled());
  EXPECT_FALSE(mock_controller->WasIconPressed());

  LeftClickOn(tile);

  // Clicking the tile does not press the icon.
  EXPECT_FALSE(mock_controller->WasIconPressed());
  EXPECT_TRUE(mock_controller->WasLabelPressed());
  EXPECT_FALSE(tile->IsToggled());

  LeftClickOn(tile->icon_button());

  // Clicking the icon presses it and toggles the tile.
  EXPECT_TRUE(mock_controller->WasIconPressed());
  EXPECT_TRUE(tile->IsToggled());
}

TEST_P(FeatureTileTest, PrimaryTile_SetIconClickable) {
  auto mock_controller =
      std::make_unique<MockFeaturePodController>(/*togglable=*/true);
  auto* tile = widget_->SetContentsView(mock_controller->CreateTile());

  // Ensure clickable state is correct.
  tile->SetIconClickable(true);
  EXPECT_TRUE(tile->is_icon_clickable());
  EXPECT_TRUE(tile->icon_button()->GetEnabled());
  auto* ink_drop = views::InkDrop::Get(tile->icon_button());
  ASSERT_TRUE(ink_drop);
  EXPECT_EQ(ink_drop->GetMode(), views::InkDropHost::InkDropMode::ON);

  // Ensure icon button can take focus.
  auto* focus_manager = widget_->GetFocusManager();
  PressTab();
  EXPECT_EQ(tile, focus_manager->GetFocusedView());
  PressTab();
  EXPECT_EQ(tile->icon_button(), focus_manager->GetFocusedView());

  // Ensure button state changes when set to not clickable.
  tile->SetIconClickable(false);
  EXPECT_FALSE(tile->is_icon_clickable());
  EXPECT_FALSE(tile->icon_button()->GetEnabled());
  EXPECT_EQ(ink_drop->GetMode(), views::InkDropHost::InkDropMode::OFF);

  // Ensure icon button doesn't focus.
  PressTab();
  EXPECT_EQ(tile, focus_manager->GetFocusedView());
  PressTab();
  EXPECT_EQ(tile, focus_manager->GetFocusedView());
}

TEST_P(FeatureTileTest, PrimaryTile_DecorativeDrillIn) {
  auto mock_controller =
      std::make_unique<MockFeaturePodController>(/*togglable=*/false);
  auto* tile = widget_->SetContentsView(mock_controller->CreateTile());

  tile->CreateDecorativeDrillInArrow();
  views::test::RunScheduledLayout(tile);
  EXPECT_FALSE(tile->is_icon_clickable());
  ASSERT_TRUE(tile->drill_in_arrow());
  EXPECT_TRUE(tile->drill_in_arrow()->GetVisible());

  // Ensure label hasn't been pressed.
  EXPECT_FALSE(tile->IsToggled());
  EXPECT_FALSE(mock_controller->WasLabelPressed());

  LeftClickOn(tile->drill_in_arrow());

  // Ensure label was pressed and button does not toggle after clicking it.
  EXPECT_TRUE(mock_controller->WasLabelPressed());
  EXPECT_FALSE(tile->IsToggled());

  // Ensure drill-in button doesn't focus.
  auto* focus_manager = widget_->GetFocusManager();
  PressTab();
  EXPECT_EQ(tile, focus_manager->GetFocusedView());
  PressTab();
  EXPECT_EQ(tile, focus_manager->GetFocusedView());
}

// Togglable tiles with a decorative drill-in button do not toggle when
// clicked, but show a detailed view from where the user can trigger an action
// which toggles the button state (e.g. selecting a VPN network).
// Since this test uses mock feature tiles and cannot easily create detailed
// views, this toggle behavior will be omitted.
TEST_P(FeatureTileTest, PrimaryTile_ToggleWithDecorativeDrillIn) {
  auto mock_controller =
      std::make_unique<MockFeaturePodController>(/*togglable=*/true);
  auto* tile = widget_->SetContentsView(mock_controller->CreateTile());

  tile->CreateDecorativeDrillInArrow();
  views::test::RunScheduledLayout(tile);
  EXPECT_FALSE(tile->is_icon_clickable());
  ASSERT_TRUE(tile->drill_in_arrow());
  EXPECT_TRUE(tile->drill_in_arrow()->GetVisible());

  // Ensure label is not pressed.
  EXPECT_FALSE(mock_controller->WasLabelPressed());

  LeftClickOn(tile);

  // Ensure label was pressed after clicking it.
  EXPECT_FALSE(mock_controller->WasIconPressed());
  EXPECT_TRUE(mock_controller->WasLabelPressed());

  LeftClickOn(tile->drill_in_arrow());

  // Ensure `WasIconPressed` not pressed after clicking drill-in button.
  EXPECT_FALSE(mock_controller->WasIconPressed());

  // Ensure drill-in button doesn't focus.
  auto* focus_manager = widget_->GetFocusManager();
  PressTab();
  EXPECT_EQ(tile, focus_manager->GetFocusedView());
  PressTab();
  EXPECT_EQ(tile, focus_manager->GetFocusedView());
}

TEST_P(FeatureTileTest, PrimaryTile_WithSubLabel) {
  FeatureTile primary_tile_with_sub_label(views::Button::PressedCallback(),
                                          /*is_togglable=*/true,
                                          FeatureTile::TileType::kPrimary);
  primary_tile_with_sub_label.SetLabel(u"Button label");
  primary_tile_with_sub_label.SetSubLabel(u"Sub label");

  EXPECT_EQ(primary_tile_with_sub_label.label()->GetHorizontalAlignment(),
            gfx::ALIGN_LEFT);
  EXPECT_EQ(primary_tile_with_sub_label.label()->GetMultiLine(), false);
  EXPECT_EQ((int)primary_tile_with_sub_label.label()->GetMaxLines(), 0);
}

TEST_P(FeatureTileTest, PrimaryTile_UpdatedCornerRadius) {
  int updated_radius = 10;
  auto mock_controller =
      std::make_unique<MockFeaturePodController>(/*togglable=*/false);
  auto* tile = widget_->SetContentsView(mock_controller->CreateTile());

  // Verify the initial tile state utilizes the default radius.
  views::RoundRectHighlightPathGenerator* path_generator =
      static_cast<views::RoundRectHighlightPathGenerator*>(
          tile->GetProperty(views::kHighlightPathGeneratorKey));
  gfx::RectF bounds = gfx::RectF(tile->GetLocalBounds());
  EXPECT_EQ(path_generator->GetRoundRect(bounds),
            gfx::RRectF(bounds, kDefaultButtonRadius));

  tile->SetButtonCornerRadius(updated_radius);

  // Verify the tile utilizes the updated radius.
  path_generator = static_cast<views::RoundRectHighlightPathGenerator*>(
      tile->GetProperty(views::kHighlightPathGeneratorKey));
  bounds = gfx::RectF(tile->GetLocalBounds());
  EXPECT_EQ(path_generator->GetRoundRect(bounds),
            gfx::RRectF(bounds, updated_radius));
}

TEST_P(FeatureTileTest, CompactTile_AddedAndRemoveSubLabel) {
  // Create initial compact `FeatureTile` without a sub-label and verify default
  // parameters.
  FeatureTile compact_tile_with_sub_label(views::Button::PressedCallback(),
                                          /*is_togglable=*/true,
                                          FeatureTile::TileType::kCompact);
  compact_tile_with_sub_label.SetLabel(u"Button label");

  EXPECT_FALSE(compact_tile_with_sub_label.sub_label()->GetVisible());
  EXPECT_EQ(compact_tile_with_sub_label.label()->GetHorizontalAlignment(),
            gfx::ALIGN_CENTER);
  EXPECT_EQ(compact_tile_with_sub_label.label()->GetMultiLine(), true);
  EXPECT_EQ((int)compact_tile_with_sub_label.label()->GetMaxLines(), 2);

  // Add a sub-label, update visibility, and verify parameters are updated.
  compact_tile_with_sub_label.SetSubLabel(u"Sub label");
  compact_tile_with_sub_label.SetSubLabelVisibility(true);

  EXPECT_EQ(compact_tile_with_sub_label.label()->GetText(), u"Button label");
  EXPECT_EQ(compact_tile_with_sub_label.label()->GetHorizontalAlignment(),
            gfx::ALIGN_CENTER);
  EXPECT_EQ(compact_tile_with_sub_label.label()->GetMultiLine(), false);
  EXPECT_EQ((int)compact_tile_with_sub_label.label()->GetMaxLines(), 1);
  EXPECT_TRUE(compact_tile_with_sub_label.sub_label()->GetVisible());

  // Hide sub-label and verify parameters are back to defaults.
  compact_tile_with_sub_label.SetSubLabelVisibility(false);

  EXPECT_FALSE(compact_tile_with_sub_label.sub_label()->GetVisible());
  EXPECT_EQ(compact_tile_with_sub_label.label()->GetHorizontalAlignment(),
            gfx::ALIGN_CENTER);
  EXPECT_EQ(compact_tile_with_sub_label.label()->GetMultiLine(), true);
  EXPECT_EQ((int)compact_tile_with_sub_label.label()->GetMaxLines(), 2);
}

TEST_P(FeatureTileTest, CompactTile_LaunchSurface) {
  auto mock_controller = std::make_unique<MockFeaturePodController>(
      /*togglable=*/false);
  auto* tile =
      widget_->SetContentsView(mock_controller->CreateTile(/*compact=*/true));
  EXPECT_FALSE(tile->is_icon_clickable());

  // Ensure label hasn't been pressed.
  EXPECT_FALSE(tile->IsToggled());
  EXPECT_FALSE(mock_controller->WasLabelPressed());

  LeftClickOn(tile);

  // Ensure label was pressed and button does not toggle after clicking it.
  EXPECT_TRUE(mock_controller->WasLabelPressed());
  EXPECT_FALSE(tile->IsToggled());
}

TEST_P(FeatureTileTest, CompactTile_Toggle) {
  auto mock_controller = std::make_unique<MockFeaturePodController>(
      /*togglable=*/true);
  auto* tile =
      widget_->SetContentsView(mock_controller->CreateTile(/*compact=*/true));
  EXPECT_FALSE(tile->is_icon_clickable());

  // Ensure label hasn't been pressed.
  EXPECT_FALSE(tile->IsToggled());
  EXPECT_FALSE(mock_controller->WasLabelPressed());

  LeftClickOn(tile);

  // Ensure label was pressed and button toggles after clicking it.
  EXPECT_TRUE(mock_controller->WasLabelPressed());
  EXPECT_TRUE(tile->IsToggled());

  // Ensure button toggles after clicking it again.
  LeftClickOn(tile);
  EXPECT_FALSE(tile->IsToggled());
}

TEST_P(FeatureTileTest, TogglingTileUpdatesInkDropColor) {
  auto* tile = widget_->SetContentsView(
      std::make_unique<FeatureTile>(views::Button::PressedCallback()));
  auto* color_provider = tile->GetColorProvider();

  tile->SetToggled(true);
  EXPECT_EQ(views::InkDrop::Get(tile)->GetBaseColor(),
            color_provider->GetColor(cros_tokens::kCrosSysRipplePrimary));

  tile->SetToggled(false);
  EXPECT_EQ(
      views::InkDrop::Get(tile)->GetBaseColor(),
      color_provider->GetColor(cros_tokens::kCrosSysRippleNeutralOnSubtle));
}

// Regression test for http://b/284318391
TEST_P(FeatureTileTest, TogglingTileHidesInkDrop) {
  auto mock_controller = std::make_unique<MockFeaturePodController>(
      /*togglable=*/true);
  auto* tile = widget_->SetContentsView(mock_controller->CreateTile());

  LeftClickOn(tile);
  ASSERT_TRUE(tile->IsToggled());
  EXPECT_EQ(views::InkDrop::Get(tile)->GetInkDrop()->GetTargetInkDropState(),
            views::InkDropState::HIDDEN);
}

TEST_P(FeatureTileTest, AccessibilityRoles) {
  // Togglable feature tiles (like Do Not Disturb) have role "toggle button".
  FeatureTile togglable_tile(views::Button::PressedCallback(),
                             /*is_togglable=*/true);
  togglable_tile.SetToggled(true);
  ui::AXNodeData node_data;
  togglable_tile.GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(node_data.role, ax::mojom::Role::kToggleButton);
  EXPECT_EQ(node_data.GetCheckedState(), ax::mojom::CheckedState::kTrue);

  togglable_tile.SetToggled(false);
  ui::AXNodeData node_data2;
  togglable_tile.GetViewAccessibility().GetAccessibleNodeData(&node_data2);
  EXPECT_EQ(node_data2.role, ax::mojom::Role::kToggleButton);
  EXPECT_EQ(node_data2.GetCheckedState(), ax::mojom::CheckedState::kFalse);

  // However, togglable feature tiles that have a clickable icon (like Network
  // and Bluetooth) do not have role "toggle button", since clicking the main
  // tile takes the user to a detail page.
  togglable_tile.SetIconClickable(true);
  ui::AXNodeData node_data3;
  togglable_tile.GetViewAccessibility().GetAccessibleNodeData(&node_data3);
  EXPECT_EQ(node_data3.role, ax::mojom::Role::kButton);

  // Non-togglable feature tiles are just buttons.
  FeatureTile non_togglable_tile(views::Button::PressedCallback(),
                                 /*is_togglable=*/false);
  ui::AXNodeData node_data4;
  non_togglable_tile.GetViewAccessibility().GetAccessibleNodeData(&node_data4);
  EXPECT_EQ(node_data4.role, ax::mojom::Role::kButton);
}

// Tests that the tile's label and tooltip are set according to the feature tile
// DLC's download state.
TEST_P(FeatureTileTest, DownloadLabelAndTooltip) {
  // Download states are only supported when `VcDlcUi` is enabled.
  if (!IsVcDlcUiEnabled()) {
    return;
  }

  // Create a tile and verify that it has its client-specified label by default.
  std::u16string client_specified_label(u"Client Specified Label");
  std::u16string client_specified_tooltip(u"Client Specified Tooltip");
  FeatureTile tile(views::Button::PressedCallback(),
                   /*is_togglable=*/true);
  tile.SetLabel(client_specified_label);
  tile.SetTooltipText(client_specified_tooltip);
  EXPECT_EQ(client_specified_label, tile.label()->GetText());
  EXPECT_EQ(client_specified_tooltip, tile.GetTooltipText());

  // Set the tile to have a pending download and verify that the label and
  // tooltip are updated.
  tile.SetDownloadState(FeatureTile::DownloadState::kPending, /*progress=*/0);

  EXPECT_EQ(GetExpectedDownloadPendingLabel(), tile.label()->GetText());
  EXPECT_EQ(GetExpectedDownloadPendingLabel(), tile.GetTooltipText());

  // Set the tile to have an in-progress download and verify that the label is
  // updated accordingly.
  tile.SetDownloadState(FeatureTile::DownloadState::kDownloading,
                        /*progress=*/7);

  EXPECT_EQ(GetExpectedDownloadInProgressLabel(/*progress=*/7),
            tile.label()->GetText());
  EXPECT_EQ(GetExpectedDownloadInProgressLabel(/*progress=*/7),
            tile.GetTooltipText());

  // Set the tile to have a successfully-completed download and verify that the
  // label is set to the client-specified label.
  tile.SetDownloadState(FeatureTile::DownloadState::kDownloaded,
                        /*progress=*/0);

  EXPECT_EQ(client_specified_label, tile.label()->GetText());
  EXPECT_EQ(client_specified_tooltip, tile.GetTooltipText());

  // Set the tile to have an error with its download and verify that the label
  // is set to the client-specified label with additional info.
  tile.SetDownloadState(FeatureTile::DownloadState::kError, /*progress=*/0);
  EXPECT_EQ(client_specified_label, tile.label()->GetText());
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_ASH_FEATURE_TILE_DOWNLOAD_ERROR,
                                       client_specified_label),
            tile.GetTooltipText());

  // Set the tile to have no associated download and verify that the label is
  // set to the client-specified label.
  tile.SetDownloadState(FeatureTile::DownloadState::kNone, /*progress=*/0);
  EXPECT_EQ(client_specified_label, tile.label()->GetText());
  EXPECT_EQ(client_specified_tooltip, tile.GetTooltipText());
}

// Tests that updates to the tile's client-specified label and tooltip are
// delayed until the current download is finished.
TEST_P(FeatureTileTest, LabelAndTooltipUpdatesDelayedDuringDownload) {
  // Download states are only supported when `VcDlcUi` is enabled.
  if (!IsVcDlcUiEnabled()) {
    return;
  }

  // Create a tile and set it to have an in-progress download.
  std::u16string label_1(u"Client Specified Label");
  std::u16string tooltip_1(u"Client Specified Tooltip");
  FeatureTile tile(views::Button::PressedCallback(),
                   /*is_togglable=*/true);
  tile.SetLabel(label_1);
  tile.SetTooltipText(tooltip_1);
  tile.SetDownloadState(FeatureTile::DownloadState::kDownloading,
                        /*progress=*/7);
  ASSERT_EQ(GetExpectedDownloadInProgressLabel(/*progress=*/7),
            tile.label()->GetText());
  ASSERT_EQ(GetExpectedDownloadInProgressLabel(/*progress=*/7),
            tile.GetTooltipText());

  // Change the tile's client-specified label and tooltip, and verify that the
  // change is not yet reflected due to the on-going download.
  std::u16string label_2(u"New Label");
  std::u16string tooltip_2(u"New Tooltip");
  tile.SetLabel(label_2);
  tile.SetTooltipText(tooltip_2);
  EXPECT_EQ(GetExpectedDownloadInProgressLabel(/*progress=*/7),
            tile.label()->GetText());
  EXPECT_EQ(GetExpectedDownloadInProgressLabel(/*progress=*/7),
            tile.GetTooltipText());

  // Set the tile's download to be finished and verify that the tile now has the
  // new client-specified label.
  tile.SetDownloadState(FeatureTile::DownloadState::kDownloaded,
                        /*progress=*/0);
  EXPECT_EQ(label_2, tile.label()->GetText());
  EXPECT_EQ(tooltip_2, tile.GetTooltipText());

  // Set the tile to have a pending download.
  tile.SetDownloadState(FeatureTile::DownloadState::kPending, /*progress=*/0);
  ASSERT_EQ(GetExpectedDownloadPendingLabel(), tile.label()->GetText());

  // Change the tile's client-specified label and verify that the change is not
  // yet reflected due to the pending download.
  std::u16string label_3(u"Another new label");
  std::u16string tooltip_3(u"Another new label");
  tile.SetLabel(label_3);
  tile.SetTooltipText(tooltip_3);
  EXPECT_EQ(GetExpectedDownloadPendingLabel(), tile.label()->GetText());
  EXPECT_EQ(GetExpectedDownloadPendingLabel(), tile.GetTooltipText());

  // Set the tile's download to be finished (with an error this time, for
  // for variety) and verify that the tile now has the new client-specified
  // label, but the tooltip shows the error message.
  tile.SetDownloadState(FeatureTile::DownloadState::kError, /*progress=*/0);
  EXPECT_EQ(label_3, tile.label()->GetText());
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(IDS_ASH_FEATURE_TILE_DOWNLOAD_ERROR, label_3),
      tile.GetTooltipText());
}

}  // namespace ash
