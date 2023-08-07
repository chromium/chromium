// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_context.h"

#include <memory>

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_test_util.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/constants/ash_features.h"
#include "ash/game_dashboard/game_dashboard_context_test_api.h"
#include "ash/game_dashboard/game_dashboard_controller.h"
#include "ash/game_dashboard/game_dashboard_main_menu_view.h"
#include "ash/game_dashboard/game_dashboard_test_base.h"
#include "ash/game_dashboard/game_dashboard_toolbar_view.h"
#include "ash/game_dashboard/game_dashboard_utils.h"
#include "ash/game_dashboard/game_dashboard_widget.h"
#include "ash/game_dashboard/test_game_dashboard_delegate.h"
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
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/frame/frame_header.h"
#include "chromeos/ui/wm/window_util.h"
#include "extensions/common/constants.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ash {

// Toolbar padding copied from `GameDashboardContext`.
static const int kToolbarEdgePadding = 10;
static constexpr gfx::Rect kAppBounds = gfx::Rect(50, 50, 800, 400);

enum class Movement { kTouch, kMouse };

class GameDashboardContextTest : public GameDashboardTestBase {
 public:
  GameDashboardContextTest() = default;
  GameDashboardContextTest(const GameDashboardContextTest&) = delete;
  GameDashboardContextTest& operator=(const GameDashboardContextTest&) = delete;
  ~GameDashboardContextTest() override = default;

  void TearDown() override {
    game_window_.reset();
    test_api_.reset();
    GameDashboardTestBase::TearDown();
  }

  int GetToolbarHeight() {
    CHECK(test_api_->GetToolbarWidget())
        << "The toolbar must be opened first before trying to retrieve its "
           "height.";
    return test_api_->GetToolbarWidget()->GetWindowBoundsInScreen().height();
  }

  // Starts the video recording from `CaptureModeBarView`.
  void ClickOnStartRecordingButtonInCaptureModeBarView() {
    auto* start_recording_button = GetStartRecordingButton();
    ASSERT_TRUE(start_recording_button);
    LeftClickOn(start_recording_button);
    WaitForRecordingToStart();
    EXPECT_TRUE(CaptureModeController::Get()->is_recording_in_progress());
  }

  // If `is_arc_window` is true, this function creates the window as an ARC
  // game window. Otherwise, it creates the window as a GeForceNow window.
  void CreateGameWindow(bool is_arc_window) {
    ASSERT_FALSE(game_window_);
    ASSERT_FALSE(test_api_);
    game_window_ = CreateAppWindow(
        (is_arc_window ? TestGameDashboardDelegate::kGameAppId
                       : extension_misc::kGeForceNowAppId),
        (is_arc_window ? AppType::ARC_APP : AppType::NON_APP), kAppBounds);
    auto* context = GameDashboardController::Get()->GetGameDashboardContext(
        game_window_.get());
    ASSERT_TRUE(context);
    test_api_ = std::make_unique<GameDashboardContextTestApi>(
        context, GetEventGenerator());
    ASSERT_TRUE(test_api_);
    frame_header_ = chromeos::FrameHeader::Get(
        views::Widget::GetWidgetForNativeWindow(game_window_.get()));
  }

  // Opens the main menu and toolbar, and checks Game Controls UI states. At the
  // end of the test, closes the main menu and toolbar.
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
                                        bool setup_exists) {
    test_api_->OpenTheMainMenu();

    auto* tile = test_api_->GetMainMenuGameControlsTile();
    if (tile_states[0]) {
      ASSERT_TRUE(tile);
      EXPECT_EQ(tile_states[1], tile->GetEnabled());
      EXPECT_EQ(tile_states[2], tile->IsToggled());
    } else {
      EXPECT_FALSE(tile);
    }

    auto* details_row = test_api_->GetMainMenuGameControlsDetailsButton();
    if (details_row_states[0]) {
      ASSERT_TRUE(details_row);
      EXPECT_EQ(details_row_states[1], details_row->GetEnabled());
    } else {
      EXPECT_FALSE(details_row);
    }

    auto* switch_button = test_api_->GetMainMenuGameControlsHintSwitch();
    if (hint_states[0]) {
      ASSERT_TRUE(switch_button);
      EXPECT_EQ(hint_states[1], switch_button->GetEnabled());
      EXPECT_EQ(hint_states[2], switch_button->GetIsOn());
    } else {
      EXPECT_FALSE(switch_button);
    }

    auto* setup_button = test_api_->GetMainMenuGameControlsSetupButton();
    if (setup_exists) {
      EXPECT_TRUE(setup_button);
    } else {
      EXPECT_FALSE(setup_button);
    }

    // Open toolbar and check the toolbar's game controls button state.
    test_api_->OpenTheToolbar();
    // The button state has the same state as the feature tile on the main menu.
    auto* game_controls_button = test_api_->GetToolbarGameControlsButton();
    if (tile_states[0]) {
      ASSERT_TRUE(game_controls_button);
      EXPECT_EQ(tile_states[1], game_controls_button->GetEnabled());
      EXPECT_EQ(tile_states[2], game_controls_button->toggled());
    } else {
      EXPECT_FALSE(game_controls_button);
    }

    test_api_->CloseTheToolbar();
    test_api_->CloseTheMainMenu();
  }

  void VerifyToolbarMovement(Movement move_type) {
    test_api_->OpenTheMainMenu();
    test_api_->OpenTheToolbar();
    gfx::Rect window_bounds = game_window_->GetBoundsInScreen();
    gfx::Point window_center_point = window_bounds.CenterPoint();
    int x_offset = window_bounds.width() / 4;
    int y_offset = window_bounds.height() / 4;

    // Verify that be default the snap position should be `kTopRight` and
    // toolbar is placed in the top right quadrant.
    EXPECT_EQ(test_api_->GetToolbarSnapLocation(),
              GameDashboardContext::ToolbarSnapLocation::kTopRight);

    // Move toolbar but not outside of the top right quadrant. Tests that even
    // though the snap position does not change, the toolbar is snapped back to
    // its previous position.
    DragToolbarToPoint(move_type, {window_center_point.x() + x_offset,
                                   window_center_point.y() - y_offset});
    EXPECT_EQ(test_api_->GetToolbarSnapLocation(),
              GameDashboardContext::ToolbarSnapLocation::kTopRight);

    // Move toolbar to bottom right quadrant and verify snap location is
    // updated.
    DragToolbarToPoint(move_type, {window_center_point.x() + x_offset,
                                   window_center_point.y() + y_offset});
    EXPECT_EQ(test_api_->GetToolbarSnapLocation(),
              GameDashboardContext::ToolbarSnapLocation::kBottomRight);

    // Move toolbar to bottom left quadrant and verify snap location is updated.
    DragToolbarToPoint(move_type, {window_center_point.x() - x_offset,
                                   window_center_point.y() + y_offset});
    EXPECT_EQ(test_api_->GetToolbarSnapLocation(),
              GameDashboardContext::ToolbarSnapLocation::kBottomLeft);

    // Move toolbar to top left quadrant and verify snap location is updated.
    DragToolbarToPoint(move_type, {window_center_point.x() - x_offset,
                                   window_center_point.y() - y_offset});
    EXPECT_EQ(test_api_->GetToolbarSnapLocation(),
              GameDashboardContext::ToolbarSnapLocation::kTopLeft);
  }

  // Starts recording `recording_window_test_api`'s window, and verifies its
  // record game buttons are enabled and toggled on, while the record game
  // buttons in `other_window_test_api` are disabled and toggled off.
  void RecordGameAndVerifyButtons(
      GameDashboardContextTestApi* recording_window_test_api,
      GameDashboardContextTestApi* other_window_test_api) {
    // Verify the initial state of the record buttons.
    for (auto* test_api : {recording_window_test_api, other_window_test_api}) {
      wm::ActivateWindow(test_api->context()->game_window());

      test_api->OpenTheMainMenu();
      auto* record_game_tile = test_api->GetMainMenuRecordGameTile();
      ASSERT_TRUE(record_game_tile);
      EXPECT_TRUE(record_game_tile->GetEnabled());
      EXPECT_FALSE(record_game_tile->IsToggled());

      test_api->OpenTheToolbar();
      auto* record_game_button = test_api->GetToolbarRecordGameButton();
      ASSERT_TRUE(record_game_button);
      EXPECT_TRUE(record_game_button->GetEnabled());
      EXPECT_FALSE(record_game_button->toggled());
    }

    // Activate the recording_window.
    auto* recording_window =
        recording_window_test_api->context()->game_window();
    ASSERT_TRUE(recording_window);
    wm::ActivateWindow(recording_window);

    // Start recording recording_window.
    LeftClickOn(recording_window_test_api->GetMainMenuRecordGameTile());
    ClickOnStartRecordingButtonInCaptureModeBarView();

    // Reopen the recording window's main menu, because clicking on the button
    // closed it.
    recording_window_test_api->OpenTheMainMenu();

    // Retrieve the record game buttons from both windows.
    auto* recording_window_record_game_tile =
        recording_window_test_api->GetMainMenuRecordGameTile();
    ASSERT_TRUE(recording_window_record_game_tile);
    auto* recording_window_record_game_button =
        recording_window_test_api->GetToolbarRecordGameButton();
    ASSERT_TRUE(recording_window_record_game_button);
    auto* other_window_record_game_tile =
        other_window_test_api->GetMainMenuRecordGameTile();
    ASSERT_TRUE(other_window_record_game_tile);
    auto* other_window_record_game_button =
        other_window_test_api->GetToolbarRecordGameButton();
    ASSERT_TRUE(other_window_record_game_button);

    // Verify the recording_window's buttons are enabled and toggled on.
    EXPECT_TRUE(recording_window_record_game_tile->GetEnabled());
    EXPECT_TRUE(recording_window_record_game_tile->IsToggled());
    EXPECT_TRUE(recording_window_record_game_button->GetEnabled());
    EXPECT_TRUE(recording_window_record_game_button->toggled());

    // Verify the other window's buttons are disabled and toggled off.
    EXPECT_FALSE(other_window_record_game_tile->GetEnabled());
    EXPECT_FALSE(other_window_record_game_tile->IsToggled());
    EXPECT_FALSE(other_window_record_game_button->GetEnabled());
    EXPECT_FALSE(other_window_record_game_button->toggled());

    // Stop the video recording session.
    CaptureModeTestApi().StopVideoRecording();
    EXPECT_FALSE(CaptureModeController::Get()->is_recording_in_progress());

    // TODO(b/286889161): Update the record game button pointers after the bug
    // has been addressed. The main menu will no longer remain open, which makes
    // button pointers invalid.
    // Verify all the record game buttons are enabled and toggled off.
    EXPECT_TRUE(recording_window_record_game_tile->GetEnabled());
    EXPECT_TRUE(recording_window_record_game_button->GetEnabled());
    EXPECT_TRUE(other_window_record_game_tile->GetEnabled());
    EXPECT_TRUE(other_window_record_game_button->GetEnabled());

    // Verify all the record game buttons are toggled off.
    EXPECT_FALSE(recording_window_record_game_tile->IsToggled());
    EXPECT_FALSE(recording_window_record_game_button->toggled());
    EXPECT_FALSE(other_window_record_game_tile->IsToggled());
    EXPECT_FALSE(other_window_record_game_button->toggled());

    // Close the toolbar and main menu in both windows.
    for (auto* test_api : {recording_window_test_api, other_window_test_api}) {
      wm::ActivateWindow(test_api->context()->game_window());
      test_api->CloseTheToolbar();
      test_api->CloseTheMainMenu();
    }
  }

 protected:
  std::unique_ptr<aura::Window> game_window_;
  raw_ptr<chromeos::FrameHeader, ExperimentalAsh> frame_header_;
  std::unique_ptr<GameDashboardContextTestApi> test_api_;

  void DragToolbarToPoint(Movement move_type,
                          const gfx::Point& new_location,
                          bool drop = true) {
    DCHECK(test_api_->GetToolbarWidget())
        << "Cannot drag toolbar because it's not available on screen.";
    gfx::Rect toolbar_bounds =
        test_api_->GetToolbarWidget()->GetWindowBoundsInScreen();
    ui::test::EventGenerator* event_generator = GetEventGenerator();
    // TODO (b/290696780): Update entry point to use center of toolbar once
    // mouse supports dragging on buttons.
    event_generator->set_current_screen_location(
        gfx::Point(toolbar_bounds.x() + 1, toolbar_bounds.y() + 1));

    switch (move_type) {
      case Movement::kMouse:
        event_generator->PressLeftButton();
        event_generator->MoveMouseTo(new_location);
        if (drop) {
          event_generator->ReleaseLeftButton();
        }
        break;
      case Movement::kTouch:
        event_generator->PressTouch();
        // Move the touch by an enough amount in X to make sure it generates a
        // series of gesture scroll events instead of a fling event.
        event_generator->MoveTouchBy(50, 0);
        event_generator->MoveTouch(new_location);
        if (drop) {
          event_generator->ReleaseTouch();
        }
        break;
    }
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
      /*tile_states=*/
      {/*expect_exists=*/false, /*expect_enabled=*/false,
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
      /*tile_states=*/
      {/*expect_exists=*/true, /*expect_enabled=*/true,
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
      /*tile_states=*/
      {/*expect_exists=*/true, /*expect_enabled=*/false,
       /*expect_toggled=*/false},
      /*details_row_states=*/{/*expect_exists=*/true, /*expect_enabled=*/true},
      /*hint_states=*/
      {/*expect_exists=*/false, /*expect_enabled=*/false, /*expect_on=*/false},
      /*setup_exists=*/true);

  // Game controls is available, not empty and enabled.
  game_window_->SetProperty(
      kArcGameControlsFlagsKey,
      static_cast<ArcGameControlsFlag>(ArcGameControlsFlag::kKnown |
                                       ArcGameControlsFlag::kAvailable |
                                       ArcGameControlsFlag::kEnabled));
  OpenMenuCheckGameControlsUIState(
      /*tile_states=*/
      {/*expect_exists=*/true, /*expect_enabled=*/true,
       /*expect_toggled=*/true},
      /*details_row_states=*/{/*expect_exists=*/true, /*expect_enabled=*/true},
      /*hint_states=*/
      {/*expect_exists=*/true, /*expect_enabled=*/true, /*expect_on=*/false},
      /*setup_exists=*/false);
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

  test_api_->OpenTheMainMenu();
  // Disable Game Controls.
  EXPECT_TRUE(game_dashboard_utils::IsFlagSet(
      game_window_->GetProperty(kArcGameControlsFlagsKey),
      ArcGameControlsFlag::kMenu));
  test_api_->OpenTheToolbar();

  auto* detail_row = test_api_->GetMainMenuGameControlsDetailsButton();
  auto* switch_button = test_api_->GetMainMenuGameControlsHintSwitch();
  auto* game_controls_button = test_api_->GetToolbarGameControlsButton();
  EXPECT_TRUE(detail_row->GetEnabled());
  EXPECT_TRUE(switch_button->GetEnabled());
  EXPECT_TRUE(switch_button->GetIsOn());
  EXPECT_TRUE(game_controls_button->GetEnabled());
  EXPECT_TRUE(game_controls_button->toggled());
  // Disable Game Controls.
  LeftClickOn(test_api_->GetMainMenuGameControlsTile());
  EXPECT_FALSE(detail_row->GetEnabled());
  EXPECT_FALSE(switch_button->GetEnabled());
  EXPECT_FALSE(switch_button->GetIsOn());
  // Toolbar button should also get updated.
  EXPECT_TRUE(game_controls_button->GetEnabled());
  EXPECT_FALSE(game_controls_button->toggled());

  EXPECT_FALSE(game_dashboard_utils::IsFlagSet(
      game_window_->GetProperty(kArcGameControlsFlagsKey),
      ArcGameControlsFlag::kEnabled));

  test_api_->CloseTheToolbar();
  test_api_->CloseTheMainMenu();
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

  // Open the main menu and toolbar. Enable Game Controls and switch hint button
  // off.
  test_api_->OpenTheMainMenu();
  test_api_->OpenTheToolbar();
  detail_row = test_api_->GetMainMenuGameControlsDetailsButton();
  switch_button = test_api_->GetMainMenuGameControlsHintSwitch();
  game_controls_button = test_api_->GetToolbarGameControlsButton();
  // Enable Game Controls.
  LeftClickOn(test_api_->GetMainMenuGameControlsTile());
  EXPECT_TRUE(detail_row->GetEnabled());
  EXPECT_TRUE(switch_button->GetEnabled());
  EXPECT_TRUE(switch_button->GetIsOn());
  EXPECT_TRUE(game_controls_button->GetEnabled());
  EXPECT_TRUE(game_controls_button->toggled());
  // Switch hint off.
  LeftClickOn(switch_button);
  EXPECT_FALSE(switch_button->GetIsOn());
  test_api_->CloseTheToolbar();
  test_api_->CloseTheMainMenu();

  // Open the main menu again to check if the states are preserved and close it.
  OpenMenuCheckGameControlsUIState(
      /*tile_states=*/{/*expect_exists=*/true, /*expect_enabled=*/true,
                       /*expect_toggled=*/true},
      /*details_row_states=*/{/*expect_exists=*/true, /*expect_enabled=*/true},
      /*hint_states=*/
      {/*expect_exists=*/true, /*expect_enabled=*/true, /*expect_on=*/false},
      /*setup_exists=*/false);
}

// Verifies that when one game window starts a recording session, it's
// record game buttons are enabled and the other game's record game buttons
// are disabled.
TEST_F(GameDashboardContextTest, TwoGameWindowsRecordingState) {
  // Create an ARC game window.
  CreateGameWindow(/*is_arc_window=*/true);
  game_window_->SetProperty(kArcGameControlsFlagsKey,
                            ArcGameControlsFlag::kKnown);
  // Create a GFN game window.
  auto gfn_game_window =
      CreateAppWindow(extension_misc::kGeForceNowAppId, AppType::NON_APP,
                      gfx::Rect(50, 50, 400, 200));
  auto* gfn_game_context =
      GameDashboardController::Get()->GetGameDashboardContext(
          gfn_game_window.get());
  ASSERT_TRUE(gfn_game_context);
  auto gfn_window_test_api =
      GameDashboardContextTestApi(gfn_game_context, GetEventGenerator());

  // Start recording the ARC game window, and verify both windows' record game
  // button states.
  RecordGameAndVerifyButtons(
      /*recording_window_test_api=*/test_api_.get(),
      /*other_window_test_api=*/&gfn_window_test_api);

  // Start recording the GFN game window, and verify both windows' "record
  // game" button states.
  RecordGameAndVerifyButtons(
      /*recording_window_test_api=*/&gfn_window_test_api,
      /*other_window_test_api=*/test_api_.get());
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
  const gfx::Point expected_button_center_point(
      game_window_->GetBoundsInScreen().top_center().x(),
      kAppBounds.y() + frame_header_->GetHeaderHeight() / 2);
  EXPECT_EQ(expected_button_center_point, test_api_->GetMainMenuButtonWidget()
                                              ->GetWindowBoundsInScreen()
                                              .CenterPoint());
}

// Verifies the main menu button widget bounds are updated, relative to the
// game window.
TEST_P(GameTypeGameDashboardContextTest,
       MainMenuButtonWidget_MoveWindowAndVerifyLocation) {
  const gfx::Vector2d move_vector = gfx::Vector2d(100, 200);
  const gfx::Rect expected_widget_location =
      test_api_->GetMainMenuButtonWidget()->GetWindowBoundsInScreen() +
      move_vector;

  game_window_->SetBoundsInScreen(
      game_window_->GetBoundsInScreen() + move_vector, GetPrimaryDisplay());

  EXPECT_EQ(expected_widget_location,
            test_api_->GetMainMenuButtonWidget()->GetWindowBoundsInScreen());
}

// Verifies clicking the main menu button will open the main menu widget.
TEST_P(GameTypeGameDashboardContextTest, OpenMainMenuButtonWidget) {
  // Verifies the main menu is closed.
  EXPECT_FALSE(test_api_->GetMainMenuWidget());

  if (IsArcGame()) {
    // Main menu button is not enabled util the Game Controls state is known.
    EXPECT_FALSE(test_api_->GetMainMenuButton()->GetEnabled());
    LeftClickOn(test_api_->GetMainMenuButton());
    EXPECT_FALSE(test_api_->GetMainMenuWidget());
    game_window_->SetProperty(kArcGameControlsFlagsKey,
                              ArcGameControlsFlag::kKnown);
  }

  // Open the main menu dialog and verify the main menu is open.
  test_api_->OpenTheMainMenu();
}

// Verifies clicking the main menu button will close the main menu widget if
// it's already open.
TEST_P(GameTypeGameDashboardContextTest, CloseMainMenuButtonWidget) {
  if (IsArcGame()) {
    game_window_->SetProperty(kArcGameControlsFlagsKey,
                              ArcGameControlsFlag::kKnown);
  }
  // Open the main menu widget and verify the main menu open.
  test_api_->OpenTheMainMenu();

  // Close the main menu dialog and verify the main menu is closed.
  test_api_->CloseTheMainMenu();
}

// Verifies the main menu shows all items allowed.
TEST_P(GameTypeGameDashboardContextTest,
       MainMenuDialogWidget_AvailableFeatures) {
  if (IsArcGame()) {
    game_window_->SetProperty(
        kArcGameControlsFlagsKey,
        static_cast<ArcGameControlsFlag>(ArcGameControlsFlag::kKnown |
                                         ArcGameControlsFlag::kAvailable));
  }

  test_api_->OpenTheMainMenu();

  // Verify whether each element available in the main menu is available as
  // expected.
  EXPECT_TRUE(test_api_->GetMainMenuToolbarTile());
  EXPECT_TRUE(test_api_->GetMainMenuRecordGameTile());
  EXPECT_TRUE(test_api_->GetMainMenuScreenshotTile());
  EXPECT_TRUE(test_api_->GetMainMenuFeedbackButton());
  EXPECT_TRUE(test_api_->GetMainMenuHelpButton());
  EXPECT_TRUE(test_api_->GetMainMenuSettingsButton());
  if (IsArcGame()) {
    EXPECT_TRUE(test_api_->GetMainMenuGameControlsTile());
    EXPECT_TRUE(test_api_->GetMainMenuScreenSizeSettingsButton());
  } else {
    EXPECT_FALSE(test_api_->GetMainMenuGameControlsTile());
    EXPECT_FALSE(test_api_->GetMainMenuScreenSizeSettingsButton());
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
  test_api_->OpenTheMainMenu();
  test_api_->OpenTheToolbar();

  // Verify that the record game tile is unavailable in the main menu.
  EXPECT_FALSE(test_api_->GetMainMenuRecordGameTile());
  // Verify that the record game button is unavailable in the toolbar.
  EXPECT_FALSE(test_api_->GetToolbarRecordGameButton());
}

// Verifies the main menu screenshot tile will take a screenshot of the game
// window.
TEST_P(GameTypeGameDashboardContextTest, TakeScreenshotFromMainMenu) {
  if (IsArcGame()) {
    game_window_->SetProperty(kArcGameControlsFlagsKey,
                              ArcGameControlsFlag::kKnown);
  }
  test_api_->OpenTheMainMenu();

  // Retrieve the screenshot button and verify the initial state.
  auto* screenshot_tile = test_api_->GetMainMenuScreenshotTile();
  ASSERT_TRUE(screenshot_tile);

  LeftClickOn(screenshot_tile);

  // Verify that a screenshot is taken of the game window.
  const auto file_path = WaitForCaptureFileToBeSaved();
  const gfx::Image image = ReadAndDecodeImageFile(file_path);
  EXPECT_EQ(image.Size(), game_window_->bounds().size());
}

// Verifies the main menu record game tile can video record the game window.
TEST_P(GameTypeGameDashboardContextTest, RecordGameFromMainMenu) {
  if (IsArcGame()) {
    game_window_->SetProperty(kArcGameControlsFlagsKey,
                              ArcGameControlsFlag::kKnown);
  }
  test_api_->OpenTheMainMenu();

  // Retrieve the video record tile and verify the initial state.
  auto* record_game_tile = test_api_->GetMainMenuRecordGameTile();
  ASSERT_TRUE(record_game_tile);

  LeftClickOn(record_game_tile);

  // Start the video recording using the record game tile.
  LeftClickOn(record_game_tile);
  ClickOnStartRecordingButtonInCaptureModeBarView();

  // Stop video recording.
  // TODO(b/286889385): Stop video recording using `GameDashboardMainMenuView`.
  CaptureModeTestApi().StopVideoRecording();
  EXPECT_FALSE(CaptureModeController::Get()->is_recording_in_progress());
}

// Verifies the record game buttons in the main menu and toolbar are disabled,
// if a recording session was started outside of the Game Dashboard.
TEST_P(GameTypeGameDashboardContextTest,
       CaptureSessionStartedOutsideOfTheGameDashboard) {
  if (IsArcGame()) {
    game_window_->SetProperty(kArcGameControlsFlagsKey,
                              ArcGameControlsFlag::kKnown);
  }
  auto* capture_mode_controller = CaptureModeController::Get();

  test_api_->OpenTheMainMenu();

  // Retrieve the record game tile from the main menu, and verify it's
  // enabled and toggled off.
  auto* main_menu_record_game_button = test_api_->GetMainMenuRecordGameTile();
  EXPECT_TRUE(main_menu_record_game_button);
  EXPECT_TRUE(main_menu_record_game_button->GetEnabled());
  EXPECT_FALSE(main_menu_record_game_button->IsToggled());

  test_api_->OpenTheToolbar();
  // Retrieve the record game button from the toolbar, and verify it's
  // enabled and toggled off.
  auto* toolbar_record_game_button = test_api_->GetToolbarRecordGameButton();
  EXPECT_TRUE(toolbar_record_game_button);
  EXPECT_TRUE(toolbar_record_game_button->GetEnabled());
  EXPECT_FALSE(toolbar_record_game_button->toggled());

  // Start video recording from `CaptureModeController`.
  EXPECT_FALSE(capture_mode_controller->is_recording_in_progress());
  StartCaptureSession(CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  StartVideoRecordingImmediately();
  EXPECT_TRUE(capture_mode_controller->is_recording_in_progress());

  // Verify the record game buttons are disabled and toggled off.
  EXPECT_FALSE(main_menu_record_game_button->GetEnabled());
  EXPECT_FALSE(main_menu_record_game_button->IsToggled());
  EXPECT_FALSE(toolbar_record_game_button->GetEnabled());
  EXPECT_FALSE(toolbar_record_game_button->toggled());

  // Stop video recording.
  CaptureModeTestApi().StopVideoRecording();
  EXPECT_FALSE(capture_mode_controller->is_recording_in_progress());

  // Verify the record game buttons are now enabled and toggled off.
  EXPECT_TRUE(main_menu_record_game_button->GetEnabled());
  EXPECT_FALSE(main_menu_record_game_button->IsToggled());
  EXPECT_TRUE(toolbar_record_game_button->GetEnabled());
  EXPECT_FALSE(toolbar_record_game_button->toggled());
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

  test_api_->OpenTheMainMenu();

  // Retrieve the toolbar button and verify the toolbar widget is not available.
  auto* toolbar_tile = test_api_->GetMainMenuToolbarTile();
  ASSERT_TRUE(toolbar_tile);
  EXPECT_FALSE(toolbar_tile->IsToggled());

  // Open the toolbar and verify available feature buttons.
  test_api_->OpenTheToolbar();
  EXPECT_TRUE(test_api_->GetToolbarGamepadButton());
  EXPECT_TRUE(test_api_->GetToolbarRecordGameButton());
  EXPECT_TRUE(test_api_->GetToolbarScreenshotButton());
  if (IsArcGame()) {
    EXPECT_TRUE(test_api_->GetToolbarGameControlsButton());
  } else {
    EXPECT_FALSE(test_api_->GetToolbarGameControlsButton());
  }

  test_api_->CloseTheToolbar();

  // Verify that the toolbar widget is no longer available and is toggled off.
  EXPECT_FALSE(test_api_->GetToolbarWidget());
  EXPECT_FALSE(toolbar_tile->IsToggled());
}

// Verifies the toolbar screenshot button will take a screenshot of the game
// window.
TEST_P(GameTypeGameDashboardContextTest, TakeScreenshotFromToolbar) {
  if (IsArcGame()) {
    game_window_->SetProperty(kArcGameControlsFlagsKey,
                              ArcGameControlsFlag::kKnown);
  }
  // Open the toolbar via the main menu.
  test_api_->OpenTheMainMenu();
  test_api_->OpenTheToolbar();

  // Click on the screenshot button within the toolbar.
  IconButton* screenshot_button = test_api_->GetToolbarScreenshotButton();
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
  test_api_->OpenTheMainMenu();
  test_api_->OpenTheToolbar();
  const int initial_height = GetToolbarHeight();
  EXPECT_NE(initial_height, 0);

  // Click on the gamepad button within the toolbar.
  auto* gamepad_button = test_api_->GetToolbarGamepadButton();
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
  test_api_->OpenTheMainMenu();
  test_api_->OpenTheToolbar();

  const GameDashboardWidget* widgets[] = {test_api_->GetMainMenuButtonWidget(),
                                          test_api_->GetToolbarWidget()};

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

// Verifies the toolbar won't follow the mouse cursor outside of the game window
// bounds.
TEST_P(GameTypeGameDashboardContextTest, MoveToolbarOutOfBounds) {
  if (IsArcGame()) {
    game_window_->SetProperty(ash::kArcGameControlsFlagsKey,
                              ArcGameControlsFlag::kKnown);
  }

  test_api_->OpenTheMainMenu();
  test_api_->OpenTheToolbar();
  ASSERT_TRUE(test_api_->GetToolbarWidget());
  ASSERT_EQ(test_api_->GetToolbarSnapLocation(),
            GameDashboardContext::ToolbarSnapLocation::kTopRight);

  gfx::Rect window_bounds = game_window_->GetBoundsInScreen();
  int screen_point_x = kScreenBounds.x();
  int screen_point_right = screen_point_x + kScreenBounds.width();
  int screen_point_y = kScreenBounds.y();
  int screen_point_bottom = screen_point_y + kScreenBounds.height();

  // Verify the screen bounds are larger than the game bounds.
  ASSERT_LT(screen_point_x, kAppBounds.x());
  ASSERT_LT(screen_point_y, kAppBounds.y());
  ASSERT_GT(screen_point_right, kAppBounds.x() + kAppBounds.width());
  ASSERT_GT(screen_point_bottom, kAppBounds.y() + kAppBounds.height());

  // Drag toolbar, moving the mouse past the game window to the top right corner
  // of the screen bounds, and verify the toolbar doesn't go past the game
  // window.
  DragToolbarToPoint(Movement::kMouse, {screen_point_right, screen_point_y},
                     false);
  auto toolbar_bounds =
      test_api_->GetToolbarWidget()->GetWindowBoundsInScreen();
  EXPECT_EQ(toolbar_bounds.right(), window_bounds.right());
  EXPECT_EQ(toolbar_bounds.y(), window_bounds.y());

  // Drag toolbar, moving the mouse past the game window to the top left corner
  // of the screen bounds.
  DragToolbarToPoint(Movement::kMouse, {screen_point_x, screen_point_y}, false);
  toolbar_bounds = test_api_->GetToolbarWidget()->GetWindowBoundsInScreen();
  EXPECT_EQ(toolbar_bounds.x(), window_bounds.x());
  EXPECT_EQ(toolbar_bounds.y(), window_bounds.y());

  // Drag toolbar, moving the mouse past the game window to the bottom left
  // corner of the screen bounds.
  DragToolbarToPoint(Movement::kMouse, {screen_point_x, screen_point_bottom},
                     false);
  toolbar_bounds = test_api_->GetToolbarWidget()->GetWindowBoundsInScreen();
  EXPECT_EQ(toolbar_bounds.x(), window_bounds.x());
  EXPECT_EQ(toolbar_bounds.bottom(), window_bounds.bottom());

  // Drag toolbar, moving the mouse past the game window to the bottom right
  // corner of the screen bounds.
  DragToolbarToPoint(Movement::kMouse,
                     {screen_point_right, screen_point_bottom}, false);
  toolbar_bounds = test_api_->GetToolbarWidget()->GetWindowBoundsInScreen();
  EXPECT_EQ(toolbar_bounds.right(), window_bounds.right());
  EXPECT_EQ(toolbar_bounds.bottom(), window_bounds.bottom());

  GetEventGenerator()->ReleaseLeftButton();
}

// Verifies the toolbar can be moved around via the mouse.
TEST_P(GameTypeGameDashboardContextTest, MoveToolbarWidgetViaMouse) {
  if (IsArcGame()) {
    game_window_->SetProperty(ash::kArcGameControlsFlagsKey,
                              ArcGameControlsFlag::kKnown);
  }
  VerifyToolbarMovement(Movement::kMouse);
}

// Verifies the toolbar can be moved around via touch.
TEST_P(GameTypeGameDashboardContextTest, MoveToolbarWidgetViaTouch) {
  if (IsArcGame()) {
    game_window_->SetProperty(ash::kArcGameControlsFlagsKey,
                              ArcGameControlsFlag::kKnown);
  }
  VerifyToolbarMovement(Movement::kTouch);
}

// Verifies the toolbar's physical placement on screen in each quadrant.
TEST_P(GameTypeGameDashboardContextTest, VerifyToolbarPlacementInQuadrants) {
  if (IsArcGame()) {
    game_window_->SetProperty(ash::kArcGameControlsFlagsKey,
                              ArcGameControlsFlag::kKnown);
  }

  test_api_->OpenTheMainMenu();
  test_api_->OpenTheToolbar();
  gfx::Rect window_bounds = game_window_->GetBoundsInScreen();
  gfx::Point window_center_point = window_bounds.CenterPoint();
  int x_offset = window_bounds.width() / 4;
  int y_offset = window_bounds.height() / 4;

  // Verify initial placement in top right quadrant.
  auto toolbar_bounds =
      test_api_->GetToolbarWidget()->GetWindowBoundsInScreen();
  gfx::Size toolbar_size =
      test_api_->GetToolbarWidget()->GetContentsView()->GetPreferredSize();
  const int frame_header_height = frame_header_->GetHeaderHeight();
  EXPECT_EQ(test_api_->GetToolbarSnapLocation(),
            GameDashboardContext::ToolbarSnapLocation::kTopRight);
  EXPECT_EQ(toolbar_bounds.x(),
            kAppBounds.right() - kToolbarEdgePadding - toolbar_size.width());
  EXPECT_EQ(toolbar_bounds.y(),
            kAppBounds.y() + kToolbarEdgePadding + frame_header_height);

  // Move toolbar to top left quadrant and verify toolbar placement.
  DragToolbarToPoint(Movement::kMouse, {window_center_point.x() - x_offset,
                                        window_center_point.y() - y_offset});
  EXPECT_EQ(test_api_->GetToolbarSnapLocation(),
            GameDashboardContext::ToolbarSnapLocation::kTopLeft);
  toolbar_bounds = test_api_->GetToolbarWidget()->GetWindowBoundsInScreen();
  EXPECT_EQ(toolbar_bounds.x(), kAppBounds.x() + kToolbarEdgePadding);
  EXPECT_EQ(toolbar_bounds.y(),
            kAppBounds.y() + kToolbarEdgePadding + frame_header_height);

  // Move toolbar to bottom right quadrant and verify toolbar placement.
  DragToolbarToPoint(Movement::kMouse, {window_center_point.x() + x_offset,
                                        window_center_point.y() + y_offset});
  toolbar_bounds = test_api_->GetToolbarWidget()->GetWindowBoundsInScreen();
  EXPECT_EQ(toolbar_bounds.x(),
            kAppBounds.right() - kToolbarEdgePadding - toolbar_size.width());
  EXPECT_EQ(toolbar_bounds.y(),
            kAppBounds.bottom() - kToolbarEdgePadding - toolbar_size.height());

  // Move toolbar to bottom left quadrant and verify toolbar placement.
  DragToolbarToPoint(Movement::kMouse, {window_center_point.x() - x_offset,
                                        window_center_point.y() + y_offset});
  toolbar_bounds = test_api_->GetToolbarWidget()->GetWindowBoundsInScreen();
  EXPECT_EQ(toolbar_bounds.x(), kAppBounds.x() + kToolbarEdgePadding);
  EXPECT_EQ(toolbar_bounds.y(),
            kAppBounds.bottom() - kToolbarEdgePadding - toolbar_size.height());
}

// Verifies the toolbar's snap location is preserved even after the visibility
// is hidden via the main menu view.
TEST_P(GameTypeGameDashboardContextTest, MoveAndHideToolbarWidget) {
  if (IsArcGame()) {
    game_window_->SetProperty(ash::kArcGameControlsFlagsKey,
                              ArcGameControlsFlag::kKnown);
  }

  test_api_->OpenTheMainMenu();
  test_api_->OpenTheToolbar();

  // Move toolbar to bottom left quadrant and verify snap location is updated.
  gfx::Rect window_bounds = game_window_->GetBoundsInScreen();
  gfx::Point window_center_point = window_bounds.CenterPoint();
  DragToolbarToPoint(Movement::kMouse,
                     {window_center_point.x() - (window_bounds.width() / 4),
                      window_center_point.y() + (window_bounds.height() / 4)});
  EXPECT_EQ(test_api_->GetToolbarSnapLocation(),
            GameDashboardContext::ToolbarSnapLocation::kBottomLeft);

  // Hide then show the toolbar and verify the toolbar was placed back into the
  // bottom left quadrant.
  test_api_->CloseTheToolbar();
  test_api_->OpenTheToolbar();
  EXPECT_EQ(test_api_->GetToolbarSnapLocation(),
            GameDashboardContext::ToolbarSnapLocation::kBottomLeft);
}

INSTANTIATE_TEST_SUITE_P(All,
                         GameTypeGameDashboardContextTest,
                         ::testing::Bool());

}  // namespace ash
