// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/feature_tile.h"

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/shell.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/rrect_f.h"
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
  raw_ptr<FeatureTile, ExperimentalAsh> tile_ = nullptr;
  bool was_icon_pressed_ = false;
  bool was_label_pressed_ = false;
  bool togglable_ = false;
  bool toggled_ = false;

  base::WeakPtrFactory<MockFeaturePodController> weak_ptr_factory_{this};
};

}  // namespace

class FeatureTileTest : public AshTestBase {
 public:
  FeatureTileTest() = default;
  FeatureTileTest(const FeatureTileTest&) = delete;
  FeatureTileTest& operator=(const FeatureTileTest&) = delete;
  ~FeatureTileTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
  }

  void TearDown() override {
    widget_.reset();
    AshTestBase::TearDown();
  }

  void PressTab() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_NONE);
  }

  std::unique_ptr<views::Widget> widget_;
};

TEST_F(FeatureTileTest, PrimaryTile_LaunchSurface) {
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

TEST_F(FeatureTileTest, PrimaryTile_Toggle) {
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

TEST_F(FeatureTileTest, PrimaryTile_ToggleWithDrillIn) {
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

TEST_F(FeatureTileTest, PrimaryTile_SetIconClickable) {
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

TEST_F(FeatureTileTest, PrimaryTile_DecorativeDrillIn) {
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
TEST_F(FeatureTileTest, PrimaryTile_ToggleWithDecorativeDrillIn) {
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

TEST_F(FeatureTileTest, PrimaryTile_WithSubLabel) {
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

TEST_F(FeatureTileTest, PrimaryTile_UpdatedCornerRadius) {
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

TEST_F(FeatureTileTest, CompactTile_AddedAndRemoveSubLabel) {
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

TEST_F(FeatureTileTest, CompactTile_LaunchSurface) {
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

TEST_F(FeatureTileTest, CompactTile_Toggle) {
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

TEST_F(FeatureTileTest, TogglingTileUpdatesInkDropColor) {
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
TEST_F(FeatureTileTest, TogglingTileHidesInkDrop) {
  auto mock_controller = std::make_unique<MockFeaturePodController>(
      /*togglable=*/true);
  auto* tile = widget_->SetContentsView(mock_controller->CreateTile());

  LeftClickOn(tile);
  ASSERT_TRUE(tile->IsToggled());
  EXPECT_EQ(views::InkDrop::Get(tile)->GetInkDrop()->GetTargetInkDropState(),
            views::InkDropState::HIDDEN);
}

TEST_F(FeatureTileTest, AccessibilityRoles) {
  // Togglable feature tiles (like Do Not Disturb) have role "toggle button".
  FeatureTile togglable_tile(views::Button::PressedCallback(),
                             /*is_togglable=*/true);
  togglable_tile.SetToggled(true);
  ui::AXNodeData node_data;
  togglable_tile.GetAccessibleNodeData(&node_data);
  EXPECT_EQ(node_data.role, ax::mojom::Role::kToggleButton);
  EXPECT_EQ(node_data.GetCheckedState(), ax::mojom::CheckedState::kTrue);

  togglable_tile.SetToggled(false);
  ui::AXNodeData node_data2;
  togglable_tile.GetAccessibleNodeData(&node_data2);
  EXPECT_EQ(node_data2.role, ax::mojom::Role::kToggleButton);
  EXPECT_EQ(node_data2.GetCheckedState(), ax::mojom::CheckedState::kFalse);

  // However, togglable feature tiles that have a clickable icon (like Network
  // and Bluetooth) do not have role "toggle button", since clicking the main
  // tile takes the user to a detail page.
  togglable_tile.SetIconClickable(true);
  ui::AXNodeData node_data3;
  togglable_tile.GetAccessibleNodeData(&node_data3);
  EXPECT_EQ(node_data3.role, ax::mojom::Role::kButton);

  // Non-togglable feature tiles are just buttons.
  FeatureTile non_togglable_tile(views::Button::PressedCallback(),
                                 /*is_togglable=*/false);
  ui::AXNodeData node_data4;
  non_togglable_tile.GetAccessibleNodeData(&node_data4);
  EXPECT_EQ(node_data4.role, ax::mojom::Role::kButton);
}

}  // namespace ash
