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
#include "ash/game_dashboard/test_game_dashboard_delegate.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/capture_mode/capture_mode_test_api.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/style/pill_button.h"
#include "ash/system/unified/feature_tile.h"
#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/frame/frame_header.h"
#include "chromeos/ui/wm/window_util.h"
#include "extensions/common/constants.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"
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

  views::Widget* GetMainMenuButtonWidget() {
    return game_context_->main_menu_button_widget_.get();
  }

  views::Widget* GetMainMenuDialogWidget() {
    return game_context_->main_menu_widget_.get();
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

  // Opens the menu and checks whether the feature tile with `tile_id` is
  // `expect_exists`, `expect_enabled` and `expect_toggled`. Then closes the
  // menu in the end.
  void OpenMenuCheckFeatureTileState(ViewID tile_id,
                                     bool expect_exists,
                                     bool expect_enabled = false,
                                     bool expect_toggled = false) {
    auto* menu_button = GetMainMenuButtonWidget()->GetContentsView();
    // Opens the main menu.
    LeftClickOn(menu_button);

    auto* tile = static_cast<FeatureTile*>(GetMainMenuViewById(tile_id));
    if (expect_exists) {
      EXPECT_TRUE(tile);
      EXPECT_EQ(expect_enabled, tile->GetEnabled());
      EXPECT_EQ(expect_toggled, tile->IsToggled());
    } else {
      EXPECT_FALSE(tile);
    }

    // Closes the main menu.
    LeftClickOn(menu_button);
  }

 protected:
  std::unique_ptr<aura::Window> game_window_;
  raw_ptr<GameDashboardContext, ExperimentalAsh> game_context_;
};

// Verifies Game Controls tile state.
// - The tile exists when Game Controls is available.
// - The tile is disabled if Game Controls has empty actions.
// - The tile can only be toggled when Game Controls has at least one action and
//   Game Controls feature is enabled.
TEST_F(GameDashboardContextTest, GameControlsTileState) {
  CreateGameWindow(/*is_arc_window=*/true);

  // Game controls is not available.
  game_window_->SetProperty(kArcGameControlsFlagsKey,
                            ArcGameControlsFlag::kKnown);
  OpenMenuCheckFeatureTileState(VIEW_ID_GD_CONTROLS_TILE,
                                /*expect_exists=*/false);

  // Game controls is available, not empty, but not enabled.
  game_window_->SetProperty(
      kArcGameControlsFlagsKey,
      static_cast<ArcGameControlsFlag>(ArcGameControlsFlag::kKnown |
                                       ArcGameControlsFlag::kAvailable));
  OpenMenuCheckFeatureTileState(VIEW_ID_GD_CONTROLS_TILE,
                                /*expect_exists=*/true,
                                /*expect_enabled=*/true,
                                /*expect_toggled=*/false);

  // Game controls is available, but empty.
  game_window_->SetProperty(
      kArcGameControlsFlagsKey,
      static_cast<ArcGameControlsFlag>(ArcGameControlsFlag::kKnown |
                                       ArcGameControlsFlag::kAvailable |
                                       ArcGameControlsFlag::kEmpty));
  OpenMenuCheckFeatureTileState(VIEW_ID_GD_CONTROLS_TILE,
                                /*expect_exists=*/true,
                                /*expect_enabled=*/false,
                                /*expect_toggled=*/false);

  // Game controls is available, but empty. Even Game controls is set enabled,
  // the tile can't be toggled.
  game_window_->SetProperty(
      kArcGameControlsFlagsKey,
      static_cast<ArcGameControlsFlag>(
          ArcGameControlsFlag::kKnown | ArcGameControlsFlag::kAvailable |
          ArcGameControlsFlag::kEmpty | ArcGameControlsFlag::kEnabled));
  OpenMenuCheckFeatureTileState(VIEW_ID_GD_CONTROLS_TILE,
                                /*expect_exists=*/true,
                                /*expect_enabled=*/false,
                                /*expect_toggled=*/false);

  // Game controls is available, not empty and enabled.
  game_window_->SetProperty(
      kArcGameControlsFlagsKey,
      static_cast<ArcGameControlsFlag>(ArcGameControlsFlag::kKnown |
                                       ArcGameControlsFlag::kAvailable |
                                       ArcGameControlsFlag::kEnabled));
  OpenMenuCheckFeatureTileState(VIEW_ID_GD_CONTROLS_TILE,
                                /*expect_exists=*/true,
                                /*expect_enabled=*/true,
                                /*expect_toggled=*/true);
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

TEST_P(GameTypeGameDashboardContextTest, TakeScreenshot) {
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

INSTANTIATE_TEST_SUITE_P(All,
                         GameTypeGameDashboardContextTest,
                         ::testing::Bool());

}  // namespace ash
