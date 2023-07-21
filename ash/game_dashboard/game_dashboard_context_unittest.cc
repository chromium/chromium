// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/game_dashboard/game_dashboard_context.h"

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_test_util.h"
#include "ash/constants/ash_features.h"
#include "ash/game_dashboard/game_dashboard_controller.h"
#include "ash/game_dashboard/game_dashboard_test_base.h"
#include "ash/game_dashboard/game_dashboard_toolbar_view.h"
#include "ash/game_dashboard/game_dashboard_utils.h"
#include "ash/game_dashboard/game_dashboard_widget.h"
#include "ash/game_dashboard/test_game_dashboard_delegate.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/capture_mode/capture_mode_test_api.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/style/color_palette_controller.h"
#include "ash/style/icon_button.h"
#include "ash/style/pill_button.h"
#include "ash/style/switch.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/wallpaper/wallpaper_controller_test_api.h"
#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/types/cxx23_to_underlying.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/frame/frame_header.h"
#include "chromeos/ui/wm/window_util.h"
#include "extensions/common/constants.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/color/color_provider_key.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/widget/widget.h"

namespace ash {

class GameDashboardContextTest : public GameDashboardTestBase {
 public:
  GameDashboardContextTest() = default;
  GameDashboardContextTest(const GameDashboardContextTest&) = delete;
  GameDashboardContextTest& operator=(const GameDashboardContextTest&) = delete;
  ~GameDashboardContextTest() override = default;

  void TearDown() override {
    game_window_.reset();
    GameDashboardTestBase::TearDown();
  }

  GameDashboardWidget* GetMainMenuButtonWidget() {
    return game_context_->main_menu_button_widget_.get();
  }

  views::Widget* GetMainMenuDialogWidget() {
    return game_context_->main_menu_widget_.get();
  }

  GameDashboardWidget* GetToolbarWidget() {
    return game_context_->toolbar_widget_.get();
  }

  views::View* GetMainMenuViewById(int tile_view_id) {
    CHECK(GetMainMenuDialogWidget())
        << "The main menu must be opened first before trying to retrieve a "
           "main menu View.";
    return GetMainMenuDialogWidget()->GetContentsView()->GetViewByID(
        tile_view_id);
  }

  // If `is_arc_window` is true, this function creates the window as an ARC
  // game window. Otherwise, it creates the window as a GeForceNow window.
  void CreateGameWindow(bool is_arc_window) {
    ASSERT_FALSE(game_window_);
    game_window_ =
        CreateAppWindow((is_arc_window ? TestGameDashboardDelegate::kGameAppId
                                       : extension_misc::kGeForceNowAppId),
                        (is_arc_window ? AppType::ARC_APP : AppType::NON_APP),
                        gfx::Rect(0, 0, 400, 200));
    game_context_ = GameDashboardController::Get()->GetGameDashboardContext(
        game_window_.get());
    DCHECK(game_context_);
  }

  // Opens the menu and checks Game Controls UI states. Then closes the menu in
  // the end.
  // `tile_states` is about feature tile states, {expect_exists, expect_enabled,
  // expect_toggled}.
  // `details_row_states` is about Game Controls details row, {expect_exist,
  // expect_enabled}.
  // `hint_states` is about hint switch button states, {expect_exists,
  // expect_enabled, expect_on}.
  // `setup_exists` shows if setup button exists.
  void OpenMenuCheckGameControlsUIState(std::array<bool, 3> tile_states,
                                        std::array<bool, 2> details_row_states,
                                        std::array<bool, 3> hint_states,
                                        bool setup_exist) {
    auto* menu_button = GetMainMenuButtonWidget()->GetContentsView();
    // Opens the main menu.
    LeftClickOn(menu_button);

    auto* tile = static_cast<FeatureTile*>(
        GetMainMenuViewById(VIEW_ID_GD_CONTROLS_TILE));
    if (tile_states[0]) {
      EXPECT_TRUE(tile);
      EXPECT_EQ(tile_states[1], tile->GetEnabled());
      EXPECT_EQ(tile_states[2], tile->IsToggled());
    } else {
      EXPECT_FALSE(tile);
    }

    auto* details_row = GetMainMenuViewById(VIEW_ID_GD_CONTROLS_DETAILS_ROW);
    if (details_row_states[0]) {
      EXPECT_TRUE(details_row);
      EXPECT_EQ(details_row_states[1], details_row->GetEnabled());
    } else {
      EXPECT_FALSE(details_row);
    }

    auto* switch_button = static_cast<Switch*>(
        GetMainMenuViewById(VIEW_ID_GD_CONTROLS_HINT_SWITCH));
    if (hint_states[0]) {
      EXPECT_TRUE(switch_button);
      EXPECT_EQ(hint_states[1], switch_button->GetEnabled());
      EXPECT_EQ(hint_states[2], switch_button->GetIsOn());
    } else {
      EXPECT_FALSE(switch_button);
    }

    auto* setup_button = static_cast<PillButton*>(
        GetMainMenuViewById(VIEW_ID_GD_CONTROLS_SETUP_BUTTON));
    if (setup_exist) {
      EXPECT_TRUE(setup_button);
    } else {
      EXPECT_FALSE(setup_button);
    }

    // Open toolbar and check the button state on toolbar.
    LeftClickOn(GetMainMenuViewById(VIEW_ID_GD_TOOLBAR_TILE));
    // The button state has the same state as the feature tile on the main menu.
    auto* toolbar_button = GetToolbarGameControlsButton();
    if (tile_states[0]) {
      EXPECT_TRUE(toolbar_button);
      EXPECT_EQ(tile_states[1], toolbar_button->GetEnabled());
      EXPECT_EQ(tile_states[2], toolbar_button->toggled());
    } else {
      EXPECT_FALSE(toolbar_button);
    }
    // Close toolbar.
    LeftClickOn(GetMainMenuViewById(VIEW_ID_GD_TOOLBAR_TILE));

    // Closes the main menu.
    LeftClickOn(menu_button);
  }

  IconButton* GetToolbarGameControlsButton() {
    return GetToolbarViewById(base::to_underlying(
        GameDashboardToolbarView::ToolbarViewId::kGameControlsButton));
  }

  IconButton* GetToolbarScreenRecordButton() {
    return GetToolbarViewById(base::to_underlying(
        GameDashboardToolbarView::ToolbarViewId::kScreenRecordButton));
  }

  IconButton* GetToolbarScreenshotButton() {
    return GetToolbarViewById(base::to_underlying(
        GameDashboardToolbarView::ToolbarViewId::kScreenshotButton));
  }

  IconButton* GetToolbarGamepadButton() {
    return GetToolbarViewById(base::to_underlying(
        GameDashboardToolbarView::ToolbarViewId::kGamepadButton));
  }

  int GetToolbarHeight() {
    CHECK(GetToolbarWidget()) << "The toolbar must be opened first before "
                                 "trying to retrieve its height.";
    return GetToolbarWidget()->GetContentsView()->GetPreferredSize().height();
  }

 protected:
  std::unique_ptr<aura::Window> game_window_;
  raw_ptr<GameDashboardContext, ExperimentalAsh> game_context_;

 private:
  IconButton* GetToolbarViewById(int button_id) {
    CHECK(GetToolbarWidget()) << "The toolbar must be opened first before "
                                 "trying to retrieve an button from it.";
    return static_cast<IconButton*>(
        GetToolbarWidget()->GetContentsView()->GetViewByID(button_id));
  }
};

// Verifies Game Controls tile state.
// - The tile exists when Game Controls is available.
// - The tile is disabled if Game Controls has empty actions.
// - The tile can only be toggled when Game Controls has at least one action and
//   Game Controls feature is enabled.
TEST_F(GameDashboardContextTest, GameControlsMenuState) {
  CreateGameWindow(/*is_arc_window=*/true);

  // Game controls is not available.
  game_window_->SetProperty(kArcGameControlsFlagsKey,
                            ArcGameControlsFlag::kKnown);
  OpenMenuCheckGameControlsUIState(
      /*tile_states=*/{/*expect_exists=*/false, /*expect_enabled=*/false,
                       /*expect_toggled=*/false},
      /*details_row_states=*/
      {/*expect_exists=*/false, /*expect_enabled=*/false},
      /*hint_states=*/
      {/*expect_exists=*/false, /*expect_enabled=*/false, /*expect_on=*/false},
      /*setup_exists=*/false);

  // Game controls is available, not empty, but not enabled.
  game_window_->SetProperty(
      kArcGameControlsFlagsKey,
      static_cast<ArcGameControlsFlag>(ArcGameControlsFlag::kKnown |
                                       ArcGameControlsFlag::kAvailable));
  OpenMenuCheckGameControlsUIState(
      /*tile_states=*/{/*expect_exists=*/true, /*expect_enabled=*/true,
                       /*expect_toggled=*/false},
      /*details_row_states=*/{/*expect_exists=*/true, /*expect_enabled=*/false},
      /*hint_states=*/
      {/*expect_exists=*/true, /*expect_enabled=*/false, /*expect_on=*/false},
      /*setup_exists=*/false);

  // Game controls is available, but empty. Even Game controls is set enabled,
  // the tile is disabled and can't be toggled.
  game_window_->SetProperty(
      kArcGameControlsFlagsKey,
      static_cast<ArcGameControlsFlag>(
          ArcGameControlsFlag::kKnown | ArcGameControlsFlag::kAvailable |
          ArcGameControlsFlag::kEmpty | ArcGameControlsFlag::kEnabled));
  OpenMenuCheckGameControlsUIState(
      /*tile_states=*/{/*expect_exists=*/true, /*expect_enabled=*/false,
                       /*expect_toggled=*/false},
      /*details_row_states=*/{/*expect_exists=*/true, /*expect_enabled=*/true},
      /*hint_states=*/
      {/*expect_exists=*/false, /*expect_enabled=*/false, /*expect_on=*/false},
      /*setup_states=*/true);

  // Game controls is available, not empty and enabled.
  game_window_->SetProperty(
      kArcGameControlsFlagsKey,
      static_cast<ArcGameControlsFlag>(ArcGameControlsFlag::kKnown |
                                       ArcGameControlsFlag::kAvailable |
                                       ArcGameControlsFlag::kEnabled));
  OpenMenuCheckGameControlsUIState(
      /*tile_states=*/{/*expect_exists=*/true, /*expect_enabled=*/true,
                       /*expect_toggled=*/true},
      /*details_row_states=*/{/*expect_exists=*/true, /*expect_enabled=*/true},
      /*hint_states=*/
      {/*expect_exists=*/true, /*expect_enabled=*/true, /*expect_on=*/false},
      /*setup_states=*/false);
}

// Verifies Game Controls button logics.
TEST_F(GameDashboardContextTest, GameControlsMenuFunctions) {
  CreateGameWindow(/*is_arc_window=*/true);
  // Game controls is available, not empty, enabled and hint on.
  game_window_->SetProperty(
      kArcGameControlsFlagsKey,
      static_cast<ArcGameControlsFlag>(
          ArcGameControlsFlag::kKnown | ArcGameControlsFlag::kAvailable |
          ArcGameControlsFlag::kEnabled | ArcGameControlsFlag::kHint));
  EXPECT_FALSE(game_dashboard_utils::IsFlagSet(
      game_window_->GetProperty(kArcGameControlsFlagsKey),
      ArcGameControlsFlag::kMenu));

  // Open the main menu and disable Game Controls.
  auto* menu_button = GetMainMenuButtonWidget()->GetContentsView();
  LeftClickOn(menu_button);
  EXPECT_TRUE(game_dashboard_utils::IsFlagSet(
      game_window_->GetProperty(kArcGameControlsFlagsKey),
      ArcGameControlsFlag::kMenu));

  // Open the quick toolbar.
  LeftClickOn(GetMainMenuViewById(VIEW_ID_GD_TOOLBAR_TILE));
  auto* toolbar_button = GetToolbarGameControlsButton();

  auto* tile =
      static_cast<FeatureTile*>(GetMainMenuViewById(VIEW_ID_GD_CONTROLS_TILE));
  auto* detail_row = GetMainMenuViewById(VIEW_ID_GD_CONTROLS_DETAILS_ROW);
  auto* switch_button = static_cast<Switch*>(
      GetMainMenuViewById(VIEW_ID_GD_CONTROLS_HINT_SWITCH));
  EXPECT_TRUE(detail_row->GetEnabled());
  EXPECT_TRUE(switch_button->GetEnabled());
  EXPECT_TRUE(switch_button->GetIsOn());
  EXPECT_TRUE(toolbar_button->GetEnabled());
  EXPECT_TRUE(toolbar_button->toggled());
  // Disable Game Controls.
  LeftClickOn(tile);
  EXPECT_FALSE(detail_row->GetEnabled());
  EXPECT_FALSE(switch_button->GetEnabled());
  EXPECT_FALSE(switch_button->GetIsOn());
  // Toolbar button should also get updated.
  EXPECT_TRUE(toolbar_button->GetEnabled());
  EXPECT_FALSE(toolbar_button->toggled());

  EXPECT_FALSE(game_dashboard_utils::IsFlagSet(
      game_window_->GetProperty(kArcGameControlsFlagsKey),
      ArcGameControlsFlag::kEnabled));

  // Close the quick toolbar.
  LeftClickOn(GetMainMenuViewById(VIEW_ID_GD_TOOLBAR_TILE));

  // Close the main menu.
  LeftClickOn(menu_button);
  EXPECT_FALSE(game_dashboard_utils::IsFlagSet(
      game_window_->GetProperty(kArcGameControlsFlagsKey),
      ArcGameControlsFlag::kMenu));

  // Open the main menu again to check if the states are preserved and close it.
  OpenMenuCheckGameControlsUIState(
      /*tile_states=*/{/*expect_exists=*/true, /*expect_enabled=*/true,
                       /*expect_toggled=*/false},
      /*details_row_states=*/{/*expect_exists=*/true, /*expect_enabled=*/false},
      /*hint_states=*/
      {/*expect_exists=*/true, /*expect_enabled=*/false, /*expect_on=*/false},
      /*setup_exists=*/false);

  // Open the main menu, enable Game Controls and switch hint button off.
  LeftClickOn(menu_button);
  tile =
      static_cast<FeatureTile*>(GetMainMenuViewById(VIEW_ID_GD_CONTROLS_TILE));
  detail_row = GetMainMenuViewById(VIEW_ID_GD_CONTROLS_DETAILS_ROW);
  switch_button = static_cast<Switch*>(
      GetMainMenuViewById(VIEW_ID_GD_CONTROLS_HINT_SWITCH));
  // Open the quick toolbar.
  LeftClickOn(GetMainMenuViewById(VIEW_ID_GD_TOOLBAR_TILE));
  toolbar_button = GetToolbarGameControlsButton();
  // Enable Game Controls.
  LeftClickOn(tile);
  EXPECT_TRUE(detail_row->GetEnabled());
  EXPECT_TRUE(switch_button->GetEnabled());
  EXPECT_TRUE(switch_button->GetIsOn());
  EXPECT_TRUE(toolbar_button->GetEnabled());
  EXPECT_TRUE(toolbar_button->toggled());
  // Switch hint off.
  LeftClickOn(switch_button);
  EXPECT_FALSE(switch_button->GetIsOn());
  // Close the quick toolbar and main menu.
  LeftClickOn(GetMainMenuViewById(VIEW_ID_GD_TOOLBAR_TILE));
  LeftClickOn(menu_button);

  // Open the main menu again to check if the states are preserved and close it.
  OpenMenuCheckGameControlsUIState(
      /*tile_states=*/{/*expect_exists=*/true, /*expect_enabled=*/true,
                       /*expect_toggled=*/true},
      /*details_row_states=*/{/*expect_exists=*/true, /*expect_enabled=*/true},
      /*hint_states=*/
      {/*expect_exists=*/true, /*expect_enabled=*/true, /*expect_on=*/false},
      /*setup_exists=*/false);
}

// -----------------------------------------------------------------------------
// GameTypeGameDashboardContextTest:
// Test fixture to test both ARC and GeForceNow game window depending on the
// test param (true for ARC game window, false for GeForceNow window).
class GameTypeGameDashboardContextTest
    : public GameDashboardContextTest,
      public testing::WithParamInterface<bool> {
 public:
  GameTypeGameDashboardContextTest() = default;
  ~GameTypeGameDashboardContextTest() override = default;

  // GameDashboardContextTest:
  void SetUp() override {
    GameDashboardContextTest::SetUp();
    CreateGameWindow(IsArcGame());
  }

 protected:
  bool IsArcGame() const { return GetParam(); }
};

// Tests
// -----------------------------------------------------------------------
// Verifies the initial location of the main menu button widget relative to the
// game window.
TEST_P(GameTypeGameDashboardContextTest, MainMenuButtonWidget_InitialLocation) {
  auto* frame_header = chromeos::FrameHeader::Get(
      views::Widget::GetWidgetForNativeWindow(game_window_.get()));
  const gfx::Point expected_button_center_point(
      game_window_->GetBoundsInScreen().top_center().x(),
      frame_header->GetHeaderHeight() / 2);
  EXPECT_EQ(expected_button_center_point,
            GetMainMenuButtonWidget()->GetWindowBoundsInScreen().CenterPoint());
}

// Verifies the main menu button widget bounds are updated, relative to the
// game window.
TEST_P(GameTypeGameDashboardContextTest,
       MainMenuButtonWidget_MoveWindowAndVerifyLocation) {
  const gfx::Vector2d move_vector = gfx::Vector2d(100, 200);
  const gfx::Rect expected_widget_location =
      GetMainMenuButtonWidget()->GetWindowBoundsInScreen() + move_vector;

  game_window_->SetBoundsInScreen(
      game_window_->GetBoundsInScreen() + move_vector, GetPrimaryDisplay());

  EXPECT_EQ(expected_widget_location,
            GetMainMenuButtonWidget()->GetWindowBoundsInScreen());
}

// Verifies clicking the main menu button will open the main menu widget.
TEST_P(GameTypeGameDashboardContextTest, OpenMainMenuButtonWidget) {
  // Verifies the initial state.
  EXPECT_FALSE(GetMainMenuDialogWidget());

  if (IsArcGame()) {
    // Main menu button is not enabled util the Game Controls state is known.
    EXPECT_FALSE(GetMainMenuButtonWidget()->GetContentsView()->GetEnabled());
    LeftClickOn(GetMainMenuButtonWidget()->GetContentsView());
    EXPECT_FALSE(GetMainMenuDialogWidget());
    game_window_->SetProperty(kArcGameControlsFlagsKey,
                              ArcGameControlsFlag::kKnown);
  }

  // Opens main menu dialog.
  LeftClickOn(GetMainMenuButtonWidget()->GetContentsView());

  // Verifies that the menu is visible.
  EXPECT_TRUE(GetMainMenuDialogWidget());
}

// Verifies clicking the main menu button will close the main menu widget if
// it's already open.
TEST_P(GameTypeGameDashboardContextTest, CloseMainMenuButtonWidget) {
  if (IsArcGame()) {
    game_window_->SetProperty(kArcGameControlsFlagsKey,
                              ArcGameControlsFlag::kKnown);
  }

  // Opens the main menu widget and Verifies the initial state.
  LeftClickOn(GetMainMenuButtonWidget()->GetContentsView());
  EXPECT_TRUE(GetMainMenuDialogWidget());

  // Closes the main menu dialog.
  LeftClickOn(GetMainMenuButtonWidget()->GetContentsView());

  // Verifies that the menu is no longer visible.
  EXPECT_FALSE(GetMainMenuDialogWidget());
}

// Verifies the main menu shows all items allowed.
TEST_P(GameTypeGameDashboardContextTest,
       MainMenuDialogWidget_AvailabelFeatures) {
  if (IsArcGame()) {
    game_window_->SetProperty(
        kArcGameControlsFlagsKey,
        static_cast<ArcGameControlsFlag>(ArcGameControlsFlag::kKnown |
                                         ArcGameControlsFlag::kAvailable));
  }

  // Open the main menu.
  LeftClickOn(GetMainMenuButtonWidget()->GetContentsView());
  ASSERT_TRUE(GetMainMenuDialogWidget());

  // Verify whether each element available in the main menu is available as
  // expected.
  EXPECT_TRUE(GetMainMenuViewById(VIEW_ID_GD_TOOLBAR_TILE));
  EXPECT_TRUE(GetMainMenuViewById(VIEW_ID_GD_RECORD_GAME_TILE));
  EXPECT_TRUE(GetMainMenuViewById(VIEW_ID_GD_SCREENSHOT_TILE));
  EXPECT_TRUE(GetMainMenuViewById(VIEW_ID_GD_FEEDBACK_BUTTON));
  EXPECT_TRUE(GetMainMenuViewById(VIEW_ID_GD_HELP_BUTTON));
  EXPECT_TRUE(GetMainMenuViewById(VIEW_ID_GD_GENERAL_SETTINGS_BUTTON));
  if (IsArcGame()) {
    EXPECT_TRUE(GetMainMenuViewById(VIEW_ID_GD_CONTROLS_TILE));
    EXPECT_TRUE(GetMainMenuViewById(VIEW_ID_GD_SCREEN_SIZE_TILE));
  } else {
    EXPECT_FALSE(GetMainMenuViewById(VIEW_ID_GD_CONTROLS_TILE));
    EXPECT_FALSE(GetMainMenuViewById(VIEW_ID_GD_SCREEN_SIZE_TILE));
  }
}

// Verifies the main menu doesn't show the record game tile, when the feature is
// disabled.
TEST_P(GameTypeGameDashboardContextTest,
       MainMenuDialogWidget_RecordGameDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      {features::kFeatureManagementGameDashboardRecordGame});

  if (IsArcGame()) {
    game_window_->SetProperty(kArcGameControlsFlagsKey,
                              ArcGameControlsFlag::kKnown);
  }

  // Open the main menu.
  LeftClickOn(GetMainMenuButtonWidget()->GetContentsView());
  ASSERT_TRUE(GetMainMenuDialogWidget());

  // Verify that the record game tile is unavailable in the main menu.
  EXPECT_FALSE(GetMainMenuViewById(VIEW_ID_GD_RECORD_GAME_TILE));
}

// Verifies the main menu screenshot tile will take a screenshot of the game
// window.
TEST_P(GameTypeGameDashboardContextTest, TakeScreenshotFromMainMenu) {
  if (IsArcGame()) {
    game_window_->SetProperty(kArcGameControlsFlagsKey,
                              ArcGameControlsFlag::kKnown);
  }
  // Retrieve the screenshot button and verify the initial state.
  LeftClickOn(GetMainMenuButtonWidget()->GetContentsView());
  FeatureTile* screenshot_tile = static_cast<FeatureTile*>(
      GetMainMenuViewById(VIEW_ID_GD_SCREENSHOT_TILE));
  ASSERT_TRUE(screenshot_tile);

  LeftClickOn(screenshot_tile);

  // Verify that a screenshot is taken of the game window.
  const auto file_path = WaitForCaptureFileToBeSaved();
  const gfx::Image image = ReadAndDecodeImageFile(file_path);
  EXPECT_EQ(image.Size(), game_window_->bounds().size());
}

// Verifies the main menu record game tile can video record the game window.
TEST_P(GameTypeGameDashboardContextTest, ScreenCaptureFromMainMenu) {
  if (IsArcGame()) {
    game_window_->SetProperty(kArcGameControlsFlagsKey,
                              ArcGameControlsFlag::kKnown);
  }
  // Retrieve the video record tile and verify the initial state.
  LeftClickOn(GetMainMenuButtonWidget()->GetContentsView());
  FeatureTile* record_game_tile = static_cast<FeatureTile*>(
      GetMainMenuViewById(VIEW_ID_GD_RECORD_GAME_TILE));
  ASSERT_TRUE(record_game_tile);

  LeftClickOn(record_game_tile);

  // Start video recording from `CaptureModeBarView`
  auto* recording_button = GetStartRecordingButton();
  ASSERT_TRUE(recording_button);
  LeftClickOn(recording_button);
  WaitForRecordingToStart();
  EXPECT_TRUE(CaptureModeController::Get()->is_recording_in_progress());

  // Stop video recording.
  // TODO(b/286889385): Stop video recording using `GameDashboardMainMenuView`.
  CaptureModeTestApi().StopVideoRecording();
  EXPECT_FALSE(CaptureModeController::Get()->is_recording_in_progress());
}

// Verifies the toolbar opens and closes when the toolbar button in the main
// menu is clicked.
TEST_P(GameTypeGameDashboardContextTest, OpenAndCloseToolbarWidget) {
  if (IsArcGame()) {
    game_window_->SetProperty(
        ash::kArcGameControlsFlagsKey,
        static_cast<ArcGameControlsFlag>(ArcGameControlsFlag::kKnown |
                                         ArcGameControlsFlag::kAvailable));
  }
  // Retrieve the toolbar button and verify the toolbar widget is not available.
  LeftClickOn(GetMainMenuButtonWidget()->GetContentsView());
  FeatureTile* toolbar_tile =
      static_cast<FeatureTile*>(GetMainMenuViewById(VIEW_ID_GD_TOOLBAR_TILE));
  ASSERT_TRUE(toolbar_tile);
  EXPECT_FALSE(toolbar_tile->IsToggled());
  EXPECT_FALSE(GetToolbarWidget());

  LeftClickOn(toolbar_tile);

  // Verify that the toolbar widget is now available and is toggled on.
  EXPECT_TRUE(GetToolbarWidget());
  EXPECT_TRUE(toolbar_tile->IsToggled());

  // Verify available feature buttons.
  EXPECT_TRUE(GetToolbarGamepadButton());
  EXPECT_TRUE(GetToolbarScreenRecordButton());
  EXPECT_TRUE(GetToolbarScreenshotButton());
  if (IsArcGame()) {
    EXPECT_TRUE(GetToolbarGameControlsButton());
  } else {
    EXPECT_FALSE(GetToolbarGameControlsButton());
  }

  LeftClickOn(toolbar_tile);

  // Verify that the toolbar widget is no longer available and is toggled off.
  EXPECT_FALSE(GetToolbarWidget());
  EXPECT_FALSE(toolbar_tile->IsToggled());
}

// Verifies the toolbar screenshot button will take a screenshot of the game
// window.
TEST_P(GameTypeGameDashboardContextTest, TakeScreenshotFromToolbar) {
  if (IsArcGame()) {
    game_window_->SetProperty(kArcGameControlsFlagsKey,
                              ArcGameControlsFlag::kKnown);
  }
  // Retrieve the toolbar via the main menu.
  LeftClickOn(GetMainMenuButtonWidget()->GetContentsView());
  LeftClickOn(
      static_cast<FeatureTile*>(GetMainMenuViewById(VIEW_ID_GD_TOOLBAR_TILE)));
  ASSERT_TRUE(GetToolbarWidget());

  // Click on the screenshot button within the toolbar.
  IconButton* screenshot_button = GetToolbarScreenshotButton();
  ASSERT_TRUE(screenshot_button);
  LeftClickOn(screenshot_button);

  // Verify that a screenshot is taken of the game window.
  const auto file_path = WaitForCaptureFileToBeSaved();
  const gfx::Image image = ReadAndDecodeImageFile(file_path);
  EXPECT_EQ(image.Size(), game_window_->GetBoundsInScreen().size());
}

// Verifies clicking the toolbar's gamepad button will expand and collapse the
// toolbar.
TEST_P(GameTypeGameDashboardContextTest, CollapseAndExpandToolbarWidget) {
  if (IsArcGame()) {
    game_window_->SetProperty(kArcGameControlsFlagsKey,
                              ArcGameControlsFlag::kKnown);
  }
  // Retrieve the toolbar via the main menu.
  LeftClickOn(GetMainMenuButtonWidget()->GetContentsView());
  LeftClickOn(
      static_cast<FeatureTile*>(GetMainMenuViewById(VIEW_ID_GD_TOOLBAR_TILE)));
  ASSERT_TRUE(GetToolbarWidget());
  const int initial_height = GetToolbarHeight();
  EXPECT_NE(initial_height, 0);

  // Click on the gamepad button within the toolbar.
  IconButton* gamepad_button = GetToolbarGamepadButton();
  ASSERT_TRUE(gamepad_button);
  LeftClickOn(gamepad_button);
  int updated_height = GetToolbarHeight();

  // Verify that the initial y coordinate of the toolbar was larger than the
  // updated y value.
  EXPECT_GT(initial_height, updated_height);

  // Click on the gamepad button within the toolbar again.
  LeftClickOn(gamepad_button);
  updated_height = GetToolbarHeight();

  // Verify that the toolbar is back to its initially expanded height.
  EXPECT_EQ(initial_height, updated_height);
}

// Verifies the color mode, user color, and scheme variant never change.
TEST_P(GameTypeGameDashboardContextTest, ColorProviderKey) {
  // The user color to always use for GameDashboard widgets.
  constexpr SkColor kExpectedUserColor = SkColorSetRGB(0x3F, 0x5A, 0xA9);

  if (IsArcGame()) {
    game_window_->SetProperty(kArcGameControlsFlagsKey,
                              ArcGameControlsFlag::kKnown);
  }

  // Retrieve the toolbar via the main menu.
  LeftClickOn(GetMainMenuButtonWidget()->GetContentsView());
  LeftClickOn(
      static_cast<FeatureTile*>(GetMainMenuViewById(VIEW_ID_GD_TOOLBAR_TILE)));
  ASSERT_TRUE(GetToolbarWidget());

  const GameDashboardWidget* widgets[] = {GetMainMenuButtonWidget(),
                                          GetToolbarWidget()};

  for (auto* widget : widgets) {
    auto color_provider_key = widget->GetColorProviderKey();
    EXPECT_EQ(ui::ColorProviderKey::ColorMode::kDark,
              color_provider_key.color_mode);
    EXPECT_EQ(kExpectedUserColor, color_provider_key.user_color.value());
    EXPECT_EQ(ui::ColorProviderKey::SchemeVariant::kTonalSpot,
              color_provider_key.scheme_variant);
  }

  // Update and verify the color mode doesn't change.
  DarkLightModeController::Get()->SetDarkModeEnabledForTest(false);
  for (auto* widget : widgets) {
    EXPECT_EQ(ui::ColorProviderKey::ColorMode::kDark, widget->GetColorMode());
  }

  // Update and verify the color scheme doesn't change.
  Shell::Get()->color_palette_controller()->SetColorScheme(
      ColorScheme::kExpressive,
      AccountId::FromUserEmailGaiaId("user@gmail.com", "user@gmail.com"),
      base::DoNothing());
  for (auto* widget : widgets) {
    EXPECT_EQ(ui::ColorProviderKey::SchemeVariant::kTonalSpot,
              widget->GetColorProviderKey().scheme_variant);
  }

  // Update and verify the user color doesn't change.
  WallpaperControllerTestApi wallpaper(Shell::Get()->wallpaper_controller());
  wallpaper.SetCalculatedColors(WallpaperCalculatedColors(
      {}, SkColorSetRGB(0xae, 0x00, 0xff), SK_ColorWHITE));
  for (auto* widget : widgets) {
    EXPECT_EQ(kExpectedUserColor, *widget->GetColorProviderKey().user_color);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         GameTypeGameDashboardContextTest,
                         ::testing::Bool());

}  // namespace ash
