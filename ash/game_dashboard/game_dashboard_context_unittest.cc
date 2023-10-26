// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_context.h"

#include <memory>
#include <string>

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_test_util.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/constants/ash_features.h"
#include "ash/game_dashboard/game_dashboard_button.h"
#include "ash/game_dashboard/game_dashboard_context_test_api.h"
#include "ash/game_dashboard/game_dashboard_controller.h"
#include "ash/game_dashboard/game_dashboard_main_menu_view.h"
#include "ash/game_dashboard/game_dashboard_test_base.h"
#include "ash/game_dashboard/game_dashboard_toolbar_view.h"
#include "ash/game_dashboard/game_dashboard_utils.h"
#include "ash/game_dashboard/game_dashboard_widget.h"
#include "ash/game_dashboard/test_game_dashboard_delegate.h"
#include "ash/public/cpp/arc_game_controls_flag.h"
#include "ash/public/cpp/capture_mode/capture_mode_test_api.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/color_palette_controller.h"
#include "ash/style/icon_button.h"
#include "ash/style/pill_button.h"
#include "ash/style/switch.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_observer.h"
#include "base/check.h"
#include "base/timer/timer.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/frame/frame_header.h"
#include "chromeos/ui/wm/window_util.h"
#include "extensions/common/constants.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ash {

using ToolbarSnapLocation = GameDashboardContext::ToolbarSnapLocation;

// Toolbar padding copied from `GameDashboardContext`.
static const int kToolbarEdgePadding = 10;
static constexpr gfx::Rect kAppBounds = gfx::Rect(50, 50, 800, 400);

// Sub-label strings.
const std::u16string& hidden_label = u"Hidden";
const std::u16string& visible_label = u"Visible";

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
    auto* widget = test_api_->GetToolbarWidget();
    CHECK(widget) << "The toolbar must be opened first before trying to "
                     "retrieve its height.";
    return widget->GetNativeWindow()->GetBoundsInScreen().height();
  }

  // Starts the video recording from `CaptureModeBarView`.
  void ClickOnStartRecordingButtonInCaptureModeBarView() {
    PillButton* start_recording_button = GetStartRecordingButton();
    ASSERT_TRUE(start_recording_button);
    LeftClickOn(start_recording_button);
    WaitForRecordingToStart();
    EXPECT_TRUE(CaptureModeController::Get()->is_recording_in_progress());
  }

  // If `is_arc_window` is true, this function creates the window as an ARC
  // game window. Otherwise, it creates the window as a GeForceNow window.
  // For ARC game windows, if `set_arc_game_controls_flags_prop` is true, then
  // the `kArcGameControlsFlagsKey` window property will be set to
  // `ArcGameControlsFlag::kKnown`, otherwise the property will not be set.
  void CreateGameWindow(bool is_arc_window,
                        bool set_arc_game_controls_flags_prop = true) {
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

    if (is_arc_window && set_arc_game_controls_flags_prop) {
      // Initially, Game Controls is not available.
      game_window_->SetProperty(kArcGameControlsFlagsKey,
                                ArcGameControlsFlag::kKnown);
    }

    auto* game_dashboard_button_widget =
        test_api_->GetGameDashboardButton()->GetWidget();
    CHECK(game_dashboard_button_widget);
    ASSERT_FALSE(game_dashboard_button_widget->CanActivate());
    ASSERT_FALSE(game_dashboard_button_widget->IsActive());
  }

  // Opens the main menu and toolbar, and checks Game Controls UI states. At the
  // end of the test, closes the main menu and toolbar.
  // `hint_tile_states` is about feature tile states, {expect_exists,
  // expect_enabled, expect_on}.
  // `details_row_exists` is about if the Game Controls details row exists.
  // `feature_switch_states` is about feature switch button states,
  // {expect_exists, expect_toggled}.
  // `setup_exists` shows if setup button exists.
  void OpenMenuCheckGameControlsUIState(
      std::array<bool, 3> hint_tile_states,
      bool details_row_exists,
      std::array<bool, 2> feature_switch_states,
      bool setup_exists) {
    test_api_->OpenTheMainMenu();

    auto* tile = test_api_->GetMainMenuGameControlsTile();
    if (hint_tile_states[0]) {
      ASSERT_TRUE(tile);
      EXPECT_EQ(hint_tile_states[1], tile->GetEnabled());
      EXPECT_EQ(hint_tile_states[2], tile->IsToggled());
    } else {
      EXPECT_FALSE(tile);
    }

    auto* details_row = test_api_->GetMainMenuGameControlsDetailsButton();
    ASSERT_EQ(!!details_row, details_row_exists);

    auto* switch_button = test_api_->GetMainMenuGameControlsFeatureSwitch();
    if (feature_switch_states[0]) {
      ASSERT_TRUE(switch_button);
      EXPECT_EQ(feature_switch_states[1], switch_button->GetIsOn());
    } else {
      EXPECT_FALSE(switch_button);
    }

    auto* setup_button = test_api_->GetMainMenuGameControlsSetupButton();
    ASSERT_EQ(!!setup_button, setup_exists);

    // Open toolbar and check the toolbar's Game Controls button state.
    test_api_->OpenTheToolbar();
    // The button state has the same state as the hint tile on the main menu.
    auto* game_controls_button = test_api_->GetToolbarGameControlsButton();
    if (hint_tile_states[0]) {
      ASSERT_TRUE(game_controls_button);
      EXPECT_EQ(hint_tile_states[1], game_controls_button->GetEnabled());
      EXPECT_EQ(hint_tile_states[2], game_controls_button->toggled());
    } else {
      EXPECT_FALSE(game_controls_button);
    }

    test_api_->CloseTheToolbar();
    test_api_->CloseTheMainMenu();
  }

  void VerifyToolbarDrag(Movement move_type) {
    test_api_->OpenTheMainMenu();
    test_api_->OpenTheToolbar();
    gfx::Rect window_bounds = game_window_->GetBoundsInScreen();
    gfx::Point window_center_point = window_bounds.CenterPoint();
    int x_offset = window_bounds.width() / 4;
    int y_offset = window_bounds.height() / 4;

    // Verify that be default the snap position should be `kTopRight` and
    // toolbar is placed in the top right quadrant.
    EXPECT_EQ(test_api_->GetToolbarSnapLocation(),
              ToolbarSnapLocation::kTopRight);

    // Move toolbar but not outside of the top right quadrant. Tests that even
    // though the snap position does not change, the toolbar is snapped back to
    // its previous position.
    DragToolbarToPoint(move_type, {window_center_point.x() + x_offset,
                                   window_center_point.y() - y_offset});
    EXPECT_EQ(test_api_->GetToolbarSnapLocation(),
              ToolbarSnapLocation::kTopRight);

    // Move toolbar to bottom right quadrant and verify snap location is
    // updated.
    DragToolbarToPoint(move_type, {window_center_point.x() + x_offset,
                                   window_center_point.y() + y_offset});
    EXPECT_EQ(test_api_->GetToolbarSnapLocation(),
              ToolbarSnapLocation::kBottomRight);

    // Move toolbar to bottom left quadrant and verify snap location is updated.
    DragToolbarToPoint(move_type, {window_center_point.x() - x_offset,
                                   window_center_point.y() + y_offset});
    EXPECT_EQ(test_api_->GetToolbarSnapLocation(),
              ToolbarSnapLocation::kBottomLeft);

    // Move toolbar to top left quadrant and verify snap location is updated.
    DragToolbarToPoint(move_type, {window_center_point.x() - x_offset,
                                   window_center_point.y() - y_offset});
    EXPECT_EQ(test_api_->GetToolbarSnapLocation(),
              ToolbarSnapLocation::kTopLeft);
  }

  // Verifies the Game Dashboard button is in the respective state for the given
  // `test_api`. If `is_recording` is true, then the Game Dashboard button must
  // be in the recording state, and the recording timer is running. Otherwise,
  // it should be in the default state and the timer should not be running.
  void VerifyGameDashboardButtonState(GameDashboardContextTestApi* test_api,
                                      bool is_recording) {
    EXPECT_EQ(is_recording, test_api->GetGameDashboardButton()->is_recording());

    std::u16string expected_title;
    if (is_recording) {
      expected_title = l10n_util::GetStringFUTF16(
          IDS_ASH_GAME_DASHBOARD_GAME_DASHBOARD_BUTTON_RECORDING,
          test_api->GetRecordingDuration());
    } else {
      expected_title = l10n_util::GetStringUTF16(
          IDS_ASH_GAME_DASHBOARD_GAME_DASHBOARD_BUTTON_TITLE);
    }
    EXPECT_EQ(expected_title,
              test_api->GetGameDashboardButtonTitle()->GetText());
  }

  void VerifyGameDashboardButtonState(bool is_recording) {
    VerifyGameDashboardButtonState(test_api_.get(), is_recording);
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
    const base::RepeatingTimer& recording_window_timer =
        recording_window_test_api->GetRecordingTimer();
    const base::RepeatingTimer& other_window_timer =
        other_window_test_api->GetRecordingTimer();

    // Verify the recording timer is not running in both windows.
    EXPECT_FALSE(recording_window_timer.IsRunning());
    EXPECT_FALSE(other_window_timer.IsRunning());

    // Verify the game dashboard buttons are not in the recording state.
    VerifyGameDashboardButtonState(recording_window_test_api,
                                   /*is_recording=*/false);
    VerifyGameDashboardButtonState(other_window_test_api,
                                   /*is_recording=*/false);

    // Activate the recording_window.
    auto* recording_window =
        recording_window_test_api->context()->game_window();
    ASSERT_TRUE(recording_window);
    wm::ActivateWindow(recording_window);

    // Start recording recording_window.
    recording_window_test_api->OpenTheMainMenu();
    LeftClickOn(recording_window_test_api->GetMainMenuRecordGameTile());
    ClickOnStartRecordingButtonInCaptureModeBarView();

    // Reopen the recording window's main menu, because clicking on the button
    // closed it.
    recording_window_test_api->OpenTheMainMenu();

    // Verify the recording timer is only running in `recording_window`.
    EXPECT_TRUE(recording_window_timer.IsRunning());
    EXPECT_FALSE(other_window_timer.IsRunning());

    // Verify the game dashboard button state.
    VerifyGameDashboardButtonState(recording_window_test_api,
                                   /*is_recording=*/true);
    VerifyGameDashboardButtonState(other_window_test_api,
                                   /*is_recording=*/false);

    // Retrieve the record game buttons for the `recording_window` and verify
    // they're enabled and toggled on.
    VerifyRecordGameStatus(
        recording_window_test_api->GetMainMenuRecordGameTile(),
        recording_window_test_api->GetToolbarRecordGameButton(),
        /*enabled=*/true, /*toggled=*/true);

    // Retrieve the record game buttons for the `other_window`.
    auto* other_window = other_window_test_api->context()->game_window();
    wm::ActivateWindow(other_window);
    other_window_test_api->OpenTheMainMenu();

    // Retrieve the record game buttons for the `other_window` and verify
    // they're disabled and toggled off.
    VerifyRecordGameStatus(other_window_test_api->GetMainMenuRecordGameTile(),
                           other_window_test_api->GetToolbarRecordGameButton(),
                           /*enabled=*/false, /*toggled=*/false);

    // Stop the video recording session.
    wm::ActivateWindow(recording_window);
    recording_window_test_api->OpenTheMainMenu();
    LeftClickOn(recording_window_test_api->GetMainMenuRecordGameTile());
    EXPECT_FALSE(CaptureModeController::Get()->is_recording_in_progress());
    WaitForCaptureFileToBeSaved();

    // TODO(b/286889161): Update the record game button pointers after the bug
    // has been addressed. The main menu will no longer remain open, which makes
    // button pointers invalid.
    // Verify all the record game buttons for the `recording_window` are enabled
    // and toggled off.
    VerifyRecordGameStatus(
        recording_window_test_api->GetMainMenuRecordGameTile(),
        recording_window_test_api->GetToolbarRecordGameButton(),
        /*enabled=*/true, /*toggled=*/false);

    // Verify all the `other_window` buttons are enabled and toggled off.
    wm::ActivateWindow(other_window);
    other_window_test_api->OpenTheMainMenu();
    VerifyRecordGameStatus(other_window_test_api->GetMainMenuRecordGameTile(),
                           other_window_test_api->GetToolbarRecordGameButton(),
                           /*enabled=*/true, /*toggled=*/false);

    // Verify the recording timer is not running in both windows.
    EXPECT_FALSE(recording_window_timer.IsRunning());
    EXPECT_FALSE(other_window_timer.IsRunning());

    // Verify the game dashboard buttons are no longer in the recording state.
    VerifyGameDashboardButtonState(recording_window_test_api,
                                   /*is_recording=*/false);
    VerifyGameDashboardButtonState(other_window_test_api,
                                   /*is_recording=*/false);

    // Close the toolbar and main menu in the `other_window`, which is currently
    // open.
    other_window_test_api->CloseTheToolbar();
    other_window_test_api->CloseTheMainMenu();

    // Open the main menu of the recording window to close the toolbar and then
    // the main menu.
    wm::ActivateWindow(recording_window);
    recording_window_test_api->OpenTheMainMenu();
    recording_window_test_api->CloseTheToolbar();
    recording_window_test_api->CloseTheMainMenu();
  }

  void VerifyRecordGameStatus(FeatureTile* game_tile,
                              IconButton* game_button,
                              bool enabled,
                              bool toggled) {
    ASSERT_TRUE(game_tile);
    ASSERT_TRUE(game_button);
    EXPECT_EQ(enabled, game_tile->GetEnabled());
    EXPECT_EQ(enabled, game_button->GetEnabled());
    EXPECT_EQ(toggled, game_tile->IsToggled());
    EXPECT_EQ(toggled, game_button->toggled());
  }

  void PressKeyAndVerify(ui::KeyboardCode key,
                         ToolbarSnapLocation desired_location) {
    GetEventGenerator()->PressAndReleaseKey(key);
    EXPECT_EQ(test_api_->GetToolbarSnapLocation(), desired_location);
  }

 protected:
  std::unique_ptr<aura::Window> game_window_;
  raw_ptr<chromeos::FrameHeader, DanglingUntriaged | ExperimentalAsh>
      frame_header_;
  std::unique_ptr<GameDashboardContextTestApi> test_api_;

  void DragToolbarToPoint(Movement move_type,
                          const gfx::Point& new_location,
                          bool drop = true) {
    auto* widget = test_api_->GetToolbarWidget();
    DCHECK(widget) << "Cannot drag toolbar because it's unavailable on screen.";
    gfx::Rect toolbar_bounds = widget->GetNativeWindow()->GetBoundsInScreen();
    ui::test::EventGenerator* event_generator = GetEventGenerator();
    event_generator->set_current_screen_location(toolbar_bounds.CenterPoint());

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

    // Dragging the toolbar causes the main menu to close asynchronously. Run
    // until idle to ensure that this posted task runs synchronously and
    // completes before proceeding.
    base::RunLoop().RunUntilIdle();
  }
};

// Verifies Game Controls tile state.
// - The tile exists when Game Controls is available.
// - The tile is disabled if Game Controls has empty actions.
// - The tile can only be toggled when Game Controls has at least one action and
//   Game Controls feature is enabled.
TEST_F(GameDashboardContextTest, GameControlsMenuState) {
  CreateGameWindow(/*is_arc_window=*/true);

  OpenMenuCheckGameControlsUIState(
      /*hint_tile_states=*/
      {/*expect_exists=*/false, /*expect_enabled=*/false, /*expect_on=*/false},
      /*details_row_exists=*/false,
      /*feature_switch_states=*/
      {/*expect_exists=*/false, /*expect_toggled=*/false},
      /*setup_exists=*/false);

  // Game Controls is available, not empty, but not enabled.
  game_window_->SetProperty(
      kArcGameControlsFlagsKey,
      static_cast<ArcGameControlsFlag>(ArcGameControlsFlag::kKnown |
                                       ArcGameControlsFlag::kAvailable));
  OpenMenuCheckGameControlsUIState(
      /*hint_tile_states=*/
      {/*expect_exists=*/true, /*expect_enabled=*/false, /*expect_on=*/false},
      /*details_row_exists=*/true,
      /*feature_switch_states=*/
      {/*expect_exists=*/true, /*expect_toggled=*/false},
      /*setup_exists=*/false);

  // Game Controls is available, but empty. Even Game Controls is set enabled,
  // the tile is disabled and can't be toggled.
  game_window_->SetProperty(
      kArcGameControlsFlagsKey,
      static_cast<ArcGameControlsFlag>(
          ArcGameControlsFlag::kKnown | ArcGameControlsFlag::kAvailable |
          ArcGameControlsFlag::kEmpty | ArcGameControlsFlag::kEnabled));
  OpenMenuCheckGameControlsUIState(
      /*hint_tile_states=*/
      {/*expect_exists=*/true, /*expect_enabled=*/false, /*expect_on=*/false},
      /*details_row_exists=*/true,
      /*feature_switch_states=*/
      {/*expect_exists=*/false, /*expect_toggled=*/false},
      /*setup_exists=*/true);

  // Game controls is available, not empty, enabled and no mapping hint.
  game_window_->SetProperty(
      kArcGameControlsFlagsKey,
      static_cast<ArcGameControlsFlag>(ArcGameControlsFlag::kKnown |
                                       ArcGameControlsFlag::kAvailable |
                                       ArcGameControlsFlag::kEnabled));
  OpenMenuCheckGameControlsUIState(
      /*hint_tile_states=*/
      {/*expect_exists=*/true, /*expect_enabled=*/true, /*expect_on=*/false},
      /*details_row_exists=*/true,
      /*feature_switch_states=*/
      {/*expect_exists=*/true, /*expect_toggled=*/true},
      /*setup_exists=*/false);

  // Game controls is available, not empty, enabled and has mapping hint on.
  game_window_->SetProperty(
      kArcGameControlsFlagsKey,
      static_cast<ArcGameControlsFlag>(
          ArcGameControlsFlag::kKnown | ArcGameControlsFlag::kAvailable |
          ArcGameControlsFlag::kEnabled | ArcGameControlsFlag::kHint));
  OpenMenuCheckGameControlsUIState(
      /*hint_tile_states=*/
      {/*expect_exists=*/true, /*expect_enabled=*/true, /*expect_on=*/true},
      /*details_row_exists=*/true,
      /*feature_switch_states=*/
      {/*expect_exists=*/true, /*expect_toggled=*/true},
      /*setup_exists=*/false);
}

// Verifies Game Controls button logics.
TEST_F(GameDashboardContextTest, GameControlsMenuFunctions) {
  CreateGameWindow(/*is_arc_window=*/true);

  // Game Controls is available, not empty, enabled and hint on.
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
  auto* switch_button = test_api_->GetMainMenuGameControlsFeatureSwitch();
  auto* game_controls_button = test_api_->GetToolbarGameControlsButton();
  EXPECT_TRUE(detail_row->GetEnabled());
  EXPECT_TRUE(switch_button->GetEnabled());
  EXPECT_TRUE(switch_button->GetIsOn());
  EXPECT_TRUE(game_controls_button->GetEnabled());
  EXPECT_TRUE(game_controls_button->toggled());
  // Disable Game Controls.
  LeftClickOn(switch_button);
  EXPECT_TRUE(detail_row->GetEnabled());
  EXPECT_TRUE(switch_button->GetEnabled());
  EXPECT_FALSE(switch_button->GetIsOn());
  // Toolbar button should also get updated.
  EXPECT_FALSE(game_controls_button->GetEnabled());

  EXPECT_FALSE(game_dashboard_utils::IsFlagSet(
      game_window_->GetProperty(kArcGameControlsFlagsKey),
      ArcGameControlsFlag::kHint));

  // Since Game Controls is disabled, press on `detail_row` should not turn on
  // `kEdit` flag.
  LeftClickOn(detail_row);
  EXPECT_FALSE(game_dashboard_utils::IsFlagSet(
      game_window_->GetProperty(kArcGameControlsFlagsKey),
      ArcGameControlsFlag::kEdit));

  test_api_->CloseTheToolbar();
  test_api_->CloseTheMainMenu();
  EXPECT_FALSE(game_dashboard_utils::IsFlagSet(
      game_window_->GetProperty(kArcGameControlsFlagsKey),
      ArcGameControlsFlag::kMenu));

  // Open the main menu again to check if the states are preserved and close it.
  OpenMenuCheckGameControlsUIState(
      /*hint_tile_states=*/
      {/*expect_exists=*/true, /*expect_enabled=*/false, /*expect_on=*/false},
      /*details_row_exists=*/true,
      /*feature_switch_states=*/
      {/*expect_exists=*/true, /*expect_toggled=*/false},
      /*setup_exists=*/false);

  // Open the main menu and toolbar. Enable Game Controls and switch hint button
  // off.
  test_api_->OpenTheMainMenu();
  test_api_->OpenTheToolbar();
  detail_row = test_api_->GetMainMenuGameControlsDetailsButton();
  switch_button = test_api_->GetMainMenuGameControlsFeatureSwitch();
  game_controls_button = test_api_->GetToolbarGameControlsButton();
  auto* game_controls_tile = test_api_->GetMainMenuGameControlsTile();
  // Enable Game Controls.
  LeftClickOn(switch_button);
  EXPECT_TRUE(detail_row->GetEnabled());
  EXPECT_TRUE(switch_button->GetEnabled());
  EXPECT_TRUE(switch_button->GetIsOn());
  EXPECT_TRUE(game_controls_button->GetEnabled());
  EXPECT_TRUE(game_controls_button->toggled());
  EXPECT_TRUE(game_controls_tile->IsToggled());
  // Switch hint off.
  LeftClickOn(game_controls_tile);
  test_api_->CloseTheToolbar();
  test_api_->CloseTheMainMenu();

  // Open the main menu again to check if the states are preserved and close it.
  OpenMenuCheckGameControlsUIState(
      /*hint_tile_states=*/
      {/*expect_exists=*/true, /*expect_enabled=*/true, /*expect_on=*/false},
      /*details_row_exists=*/true,
      /*feature_switch_states=*/
      {/*expect_exists=*/true, /*expect_toggled=*/true},
      /*setup_exists=*/false);
}

// Verify Game Dashboard button is disabled and toolbar hides in the edit mode.
TEST_F(GameDashboardContextTest, GameControlsEditMode) {
  CreateGameWindow(/*is_arc_window=*/true);
  // Game Controls is available, not empty, enabled and hint on.
  game_window_->SetProperty(
      kArcGameControlsFlagsKey,
      static_cast<ArcGameControlsFlag>(
          ArcGameControlsFlag::kKnown | ArcGameControlsFlag::kAvailable |
          ArcGameControlsFlag::kEnabled | ArcGameControlsFlag::kHint));
  auto* game_dashboard_button = test_api_->GetGameDashboardButton();
  EXPECT_TRUE(game_dashboard_button->GetEnabled());
  LeftClickOn(game_dashboard_button);
  EXPECT_TRUE(test_api_->GetMainMenuWidget());
  // Show the toolbar.
  test_api_->OpenTheToolbar();
  auto* tool_bar_widget = test_api_->GetToolbarWidget();
  EXPECT_TRUE(tool_bar_widget);
  EXPECT_TRUE(tool_bar_widget->IsVisible());

  // Enter Game Controls edit mode.
  LeftClickOn(test_api_->GetMainMenuGameControlsDetailsButton());
  EXPECT_TRUE(game_dashboard_utils::IsFlagSet(
      game_window_->GetProperty(kArcGameControlsFlagsKey),
      ArcGameControlsFlag::kEdit));
  EXPECT_FALSE(test_api_->GetMainMenuWidget());
  EXPECT_FALSE(tool_bar_widget->IsVisible());
  // In the edit mode, Game Dashboard button is disabled and it doesn't show
  // menu after clicked. The toolbar is also hidden if it shows up.
  EXPECT_FALSE(game_dashboard_button->GetEnabled());
  LeftClickOn(game_dashboard_button);
  EXPECT_FALSE(test_api_->GetMainMenuWidget());
  // Exit edit mode and verify Game Dashboard button and toolbar are resumed.
  ArcGameControlsFlag flags =
      game_window_->GetProperty(kArcGameControlsFlagsKey);
  flags = game_dashboard_utils::UpdateFlag(flags, ArcGameControlsFlag::kEdit,
                                           /*enable_flag=*/false);
  game_window_->SetProperty(kArcGameControlsFlagsKey, flags);
  EXPECT_TRUE(game_dashboard_button->GetEnabled());
  LeftClickOn(game_dashboard_button);
  EXPECT_TRUE(test_api_->GetMainMenuWidget());
  EXPECT_TRUE(tool_bar_widget->IsVisible());
}

// Verifies that when one game window starts a recording session, it's
// record game buttons are enabled and the other game's record game buttons
// are disabled.
TEST_F(GameDashboardContextTest, TwoGameWindowsRecordingState) {
  // Create an ARC game window.
  CreateGameWindow(/*is_arc_window=*/true);
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

TEST_F(GameDashboardContextTest, RecordingTimerStringFormat) {
  // Create an ARC game window.
  CreateGameWindow(/*is_arc_window=*/true);

  // Start recording the game window.
  test_api_->OpenTheMainMenu();
  test_api_->OpenTheToolbar();
  auto* record_game_button = test_api_->GetToolbarRecordGameButton();
  ASSERT_TRUE(record_game_button);
  LeftClickOn(record_game_button);

  // Get timer and verify it's running.
  const base::RepeatingTimer& timer = test_api_->GetRecordingTimer();
  EXPECT_TRUE(timer.IsRunning());

  // Verify initial time of 0 seconds.
  EXPECT_EQ(u"00:00", test_api_->GetRecordingDuration());

  // Advance clock by 1 minute, and verify overflow from seconds to minutes.
  AdvanceClock(base::Minutes(1));
  EXPECT_EQ(u"01:00", test_api_->GetRecordingDuration());

  // Advance clock by 30 seconds.
  AdvanceClock(base::Seconds(30));
  EXPECT_EQ(u"01:30", test_api_->GetRecordingDuration());

  // Advance clock by 50 minutes.
  AdvanceClock(base::Minutes(50));
  EXPECT_EQ(u"51:30", test_api_->GetRecordingDuration());

  // Advance clock by 9 minutes, and verify overflow from minutes to hours.
  AdvanceClock(base::Minutes(9));
  EXPECT_EQ(u"1:00:30", test_api_->GetRecordingDuration());

  // Advance clock by 23 hours, and verify hours doesn't overflow to days.
  AdvanceClock(base::Hours(23));
  EXPECT_EQ(u"24:00:30", test_api_->GetRecordingDuration());
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

// GameTypeGameDashboardContextTest Tests
// -----------------------------------------------------------------------
// Verifies the initial location of the Game Dashboard button widget relative to
// the game window.
TEST_P(GameTypeGameDashboardContextTest,
       GameDashboardButtonWidget_InitialLocation) {
  const gfx::Point expected_button_center_point(
      game_window_->GetBoundsInScreen().top_center().x(),
      kAppBounds.y() + frame_header_->GetHeaderHeight() / 2);
  EXPECT_EQ(expected_button_center_point,
            test_api_->GetGameDashboardButtonWidget()
                ->GetNativeWindow()
                ->GetBoundsInScreen()
                .CenterPoint());
}

// Verifies the Game Dashboard button widget bounds are updated, relative to the
// game window.
TEST_P(GameTypeGameDashboardContextTest,
       GameDashboardButtonWidget_MoveWindowAndVerifyLocation) {
  const gfx::Vector2d move_vector = gfx::Vector2d(100, 200);
  aura::Window* native_window =
      test_api_->GetGameDashboardButtonWidget()->GetNativeWindow();
  const gfx::Rect expected_widget_location =
      native_window->GetBoundsInScreen() + move_vector;

  game_window_->SetBoundsInScreen(
      game_window_->GetBoundsInScreen() + move_vector, GetPrimaryDisplay());

  EXPECT_EQ(expected_widget_location, native_window->GetBoundsInScreen());
}

// Verifies clicking the Game Dashboard button will open the main menu widget.
TEST_P(GameTypeGameDashboardContextTest, OpenGameDashboardButtonWidget) {
  // Close the window and create a new game window without setting the
  // `kArcGameControlsFlagsKey` property.
  game_window_.reset();
  test_api_.reset();
  CreateGameWindow(IsArcGame(), /*set_arc_game_controls_flags_prop=*/false);

  // Verifies the main menu is closed.
  EXPECT_FALSE(test_api_->GetMainMenuWidget());

  if (IsArcGame()) {
    // Game Dashboard button is not enabled util the Game Controls state is
    // known.
    EXPECT_FALSE(test_api_->GetGameDashboardButton()->GetEnabled());
    LeftClickOn(test_api_->GetGameDashboardButton());
    EXPECT_FALSE(test_api_->GetMainMenuWidget());
    game_window_->SetProperty(kArcGameControlsFlagsKey,
                              ArcGameControlsFlag::kKnown);
  }

  // Open the main menu dialog and verify the main menu is open.
  test_api_->OpenTheMainMenu();
}

// Verifies clicking the Game Dashboard button will close the main menu widget
// if it's already open.
TEST_P(GameTypeGameDashboardContextTest, CloseGameDashboardButtonWidget) {
  // Open the main menu widget and verify the main menu open.
  test_api_->OpenTheMainMenu();

  // Close the main menu dialog and verify the main menu is closed.
  test_api_->CloseTheMainMenu();
}

// Verifies clicking outside the main menu view will close the main menu
// widget. Then, clicking on the main menu button will still toggle the main
// menu widget visibility.
TEST_P(GameTypeGameDashboardContextTest, CloseMainMenuOutsideButtonWidget) {
  // Open the main menu widget and verify the main menu open.
  test_api_->OpenTheMainMenu();

  // Close the main menu dialog by clicking outside the main menu view bounds.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  const gfx::Point& new_location = {kAppBounds.x() + kAppBounds.width(),
                                    kAppBounds.y() + kAppBounds.height()};
  event_generator->set_current_screen_location(new_location);
  event_generator->ClickLeftButton();

  // Clicking outside the main menu causes the main menu to close
  // asynchronously. Run until idle to ensure that this posted task runs
  // synchronously and completes before proceeding.
  base::RunLoop().RunUntilIdle();

  // Open the main menu widget via the main menu button.
  test_api_->OpenTheMainMenu();

  // Close the main menu widget via the main menu button.
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

// Verifies the record game buttons in the main menu and toolbar are disabled,
// if a recording session was started outside of the Game Dashboard.
TEST_P(GameTypeGameDashboardContextTest,
       CaptureSessionStartedOutsideOfTheGameDashboard) {
  auto* capture_mode_controller = CaptureModeController::Get();

  test_api_->OpenTheMainMenu();

  // Verify the game dashboard button is initially not in the recording state.
  VerifyGameDashboardButtonState(/*is_recording=*/false);

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

  // Verify the game dashboard button is not in the recording state.
  VerifyGameDashboardButtonState(/*is_recording=*/false);

  // Stop video recording.
  CaptureModeTestApi().StopVideoRecording();
  EXPECT_FALSE(capture_mode_controller->is_recording_in_progress());

  // Verify the record game buttons are now enabled and toggled off.
  EXPECT_TRUE(main_menu_record_game_button->GetEnabled());
  EXPECT_FALSE(main_menu_record_game_button->IsToggled());
  EXPECT_TRUE(toolbar_record_game_button->GetEnabled());
  EXPECT_FALSE(toolbar_record_game_button->toggled());

  // Verify the game dashboard button is still in not in the recording state.
  VerifyGameDashboardButtonState(/*is_recording=*/false);
}

// Verifies the toolbar opens and closes when the toolbar button in the main
// menu is clicked.
TEST_P(GameTypeGameDashboardContextTest, OpenAndCloseToolbarWidget) {
  if (IsArcGame()) {
    game_window_->SetProperty(
        kArcGameControlsFlagsKey,
        static_cast<ArcGameControlsFlag>(ArcGameControlsFlag::kKnown |
                                         ArcGameControlsFlag::kAvailable));
  }

  test_api_->OpenTheMainMenu();

  // Retrieve the toolbar button and verify the toolbar widget is not enabled.
  auto* toolbar_tile = test_api_->GetMainMenuToolbarTile();
  ASSERT_TRUE(toolbar_tile);
  EXPECT_FALSE(toolbar_tile->IsToggled());
  EXPECT_EQ(toolbar_tile->sub_label()->GetText(), hidden_label);

  // Open the toolbar, verify the main menu toolbar tile's sub-label is updated,
  // and verify available feature buttons.
  test_api_->OpenTheToolbar();
  EXPECT_EQ(toolbar_tile->sub_label()->GetText(), visible_label);
  EXPECT_TRUE(test_api_->GetToolbarGamepadButton());
  EXPECT_TRUE(test_api_->GetToolbarRecordGameButton());
  EXPECT_TRUE(test_api_->GetToolbarScreenshotButton());
  if (IsArcGame()) {
    EXPECT_TRUE(test_api_->GetToolbarGameControlsButton());
  } else {
    EXPECT_FALSE(test_api_->GetToolbarGameControlsButton());
  }

  // Verify toggling the main menu visibility doesn't affect the toolbar.
  test_api_->CloseTheMainMenu();
  EXPECT_TRUE(test_api_->GetToolbarWidget());
  test_api_->OpenTheMainMenu();
  toolbar_tile = test_api_->GetMainMenuToolbarTile();
  EXPECT_EQ(toolbar_tile->sub_label()->GetText(), visible_label);
  EXPECT_TRUE(test_api_->GetToolbarWidget());

  test_api_->CloseTheToolbar();

  // Verify that the toolbar widget is no longer available and is toggled off.
  EXPECT_FALSE(test_api_->GetToolbarWidget());
  EXPECT_FALSE(toolbar_tile->IsToggled());
  EXPECT_EQ(toolbar_tile->sub_label()->GetText(), hidden_label);
}

// Verifies the toolbar screenshot button will take a screenshot of the game
// window.
TEST_P(GameTypeGameDashboardContextTest, TakeScreenshotFromToolbar) {
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
  test_api_->OpenTheMainMenu();
  test_api_->OpenTheToolbar();

  const GameDashboardWidget* widgets[] = {
      test_api_->GetGameDashboardButtonWidget(), test_api_->GetToolbarWidget()};

  for (auto* widget : widgets) {
    auto color_provider_key = widget->GetColorProviderKey();
    EXPECT_EQ(ui::ColorProviderKey::ColorMode::kDark,
              color_provider_key.color_mode);
  }

  // Update and verify the color mode doesn't change.
  DarkLightModeController::Get()->SetDarkModeEnabledForTest(false);
  for (auto* widget : widgets) {
    EXPECT_EQ(ui::ColorProviderKey::ColorMode::kDark, widget->GetColorMode());
  }
}

// Verifies the toolbar won't follow the mouse cursor outside of the game window
// bounds.
TEST_P(GameTypeGameDashboardContextTest, MoveToolbarOutOfBounds) {
  test_api_->OpenTheMainMenu();
  test_api_->OpenTheToolbar();
  ASSERT_TRUE(test_api_->GetToolbarWidget());
  ASSERT_EQ(test_api_->GetToolbarSnapLocation(),
            ToolbarSnapLocation::kTopRight);

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
  aura::Window* native_window =
      test_api_->GetToolbarWidget()->GetNativeWindow();
  auto toolbar_bounds = native_window->GetBoundsInScreen();
  EXPECT_EQ(toolbar_bounds.right(), window_bounds.right());
  EXPECT_EQ(toolbar_bounds.y(), window_bounds.y());

  // Drag toolbar, moving the mouse past the game window to the top left corner
  // of the screen bounds.
  DragToolbarToPoint(Movement::kMouse, {screen_point_x, screen_point_y}, false);
  toolbar_bounds = native_window->GetBoundsInScreen();
  EXPECT_EQ(toolbar_bounds.x(), window_bounds.x());
  EXPECT_EQ(toolbar_bounds.y(), window_bounds.y());

  // Drag toolbar, moving the mouse past the game window to the bottom left
  // corner of the screen bounds.
  DragToolbarToPoint(Movement::kMouse, {screen_point_x, screen_point_bottom},
                     false);
  toolbar_bounds = native_window->GetBoundsInScreen();
  EXPECT_EQ(toolbar_bounds.x(), window_bounds.x());
  EXPECT_EQ(toolbar_bounds.bottom(), window_bounds.bottom());

  // Drag toolbar, moving the mouse past the game window to the bottom right
  // corner of the screen bounds.
  DragToolbarToPoint(Movement::kMouse,
                     {screen_point_right, screen_point_bottom}, false);
  toolbar_bounds = native_window->GetBoundsInScreen();
  EXPECT_EQ(toolbar_bounds.right(), window_bounds.right());
  EXPECT_EQ(toolbar_bounds.bottom(), window_bounds.bottom());

  GetEventGenerator()->ReleaseLeftButton();
}

// Verifies the toolbar can be moved around via the mouse.
TEST_P(GameTypeGameDashboardContextTest, MoveToolbarWidgetViaMouse) {
  VerifyToolbarDrag(Movement::kMouse);
}

// Verifies the toolbar can be moved around via touch.
TEST_P(GameTypeGameDashboardContextTest, MoveToolbarWidgetViaTouch) {
  VerifyToolbarDrag(Movement::kTouch);
}

// Verifies the toolbar can be moved around via keyboard arrows.
TEST_P(GameTypeGameDashboardContextTest, MoveToolbarWidgetViaArrowKeys) {
  test_api_->OpenTheMainMenu();
  test_api_->OpenTheToolbar();
  test_api_->SetFocusOnToolbar();

  // Verify that be default the snap position should be `kTopRight` and
  // toolbar is placed in the top right quadrant.
  EXPECT_EQ(test_api_->GetToolbarSnapLocation(),
            ToolbarSnapLocation::kTopRight);

  // Press tab so the toolbar gains focus
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_TAB);

  // Press right arrow key and verify toolbar does not leave top right quadrant.
  PressKeyAndVerify(ui::VKEY_RIGHT, ToolbarSnapLocation::kTopRight);

  // Press left arrow key and verify toolbar moved to top left quadrant.
  PressKeyAndVerify(ui::VKEY_LEFT, ToolbarSnapLocation::kTopLeft);

  // Press down arrow key and verify toolbar moved to bottom left quadrant.
  PressKeyAndVerify(ui::VKEY_DOWN, ToolbarSnapLocation::kBottomLeft);

  // Press right arrow key and verify toolbar moved to bottom right quadrant.
  PressKeyAndVerify(ui::VKEY_RIGHT, ToolbarSnapLocation::kBottomRight);

  // Press up arrow key and verify toolbar moved to top right quadrant.
  PressKeyAndVerify(ui::VKEY_UP, ToolbarSnapLocation::kTopRight);

  // Press up arrow key again and verify toolbar does not leave top right
  // quadrant.
  PressKeyAndVerify(ui::VKEY_UP, ToolbarSnapLocation::kTopRight);

  // Press down arrow key and verify toolbar moved to bottom right quadrant.
  PressKeyAndVerify(ui::VKEY_DOWN, ToolbarSnapLocation::kBottomRight);

  // Press down arrow key again and verify toolbar does not leave bottom right
  // quadrant.
  PressKeyAndVerify(ui::VKEY_DOWN, ToolbarSnapLocation::kBottomRight);

  // Press left arrow key and verify toolbar moved to bottom left quadrant.
  PressKeyAndVerify(ui::VKEY_LEFT, ToolbarSnapLocation::kBottomLeft);

  // Press up arrow key and verify toolbar moved to top left quadrant.
  PressKeyAndVerify(ui::VKEY_UP, ToolbarSnapLocation::kTopLeft);

  // Press right arrow key and verify toolbar moved to top right quadrant.
  PressKeyAndVerify(ui::VKEY_RIGHT, ToolbarSnapLocation::kTopRight);
}

// Verifies the toolbar's physical placement on screen in each quadrant.
TEST_P(GameTypeGameDashboardContextTest, VerifyToolbarPlacementInQuadrants) {
  test_api_->OpenTheMainMenu();
  test_api_->OpenTheToolbar();
  gfx::Rect window_bounds = game_window_->GetBoundsInScreen();
  gfx::Point window_center_point = window_bounds.CenterPoint();
  int x_offset = window_bounds.width() / 4;
  int y_offset = window_bounds.height() / 4;

  // Verify initial placement in top right quadrant.
  aura::Window* native_window =
      test_api_->GetToolbarWidget()->GetNativeWindow();
  auto toolbar_bounds = native_window->GetBoundsInScreen();
  gfx::Size toolbar_size =
      test_api_->GetToolbarWidget()->GetContentsView()->GetPreferredSize();
  const int frame_header_height = frame_header_->GetHeaderHeight();
  EXPECT_EQ(test_api_->GetToolbarSnapLocation(),
            ToolbarSnapLocation::kTopRight);
  EXPECT_EQ(toolbar_bounds.x(),
            kAppBounds.right() - kToolbarEdgePadding - toolbar_size.width());
  EXPECT_EQ(toolbar_bounds.y(),
            kAppBounds.y() + kToolbarEdgePadding + frame_header_height);

  // Move toolbar to top left quadrant and verify toolbar placement.
  DragToolbarToPoint(Movement::kMouse, {window_center_point.x() - x_offset,
                                        window_center_point.y() - y_offset});
  EXPECT_EQ(test_api_->GetToolbarSnapLocation(), ToolbarSnapLocation::kTopLeft);
  toolbar_bounds = native_window->GetBoundsInScreen();
  EXPECT_EQ(toolbar_bounds.x(), kAppBounds.x() + kToolbarEdgePadding);
  EXPECT_EQ(toolbar_bounds.y(),
            kAppBounds.y() + kToolbarEdgePadding + frame_header_height);

  // Move toolbar to bottom right quadrant and verify toolbar placement.
  DragToolbarToPoint(Movement::kMouse, {window_center_point.x() + x_offset,
                                        window_center_point.y() + y_offset});
  toolbar_bounds = native_window->GetBoundsInScreen();
  EXPECT_EQ(toolbar_bounds.x(),
            kAppBounds.right() - kToolbarEdgePadding - toolbar_size.width());
  EXPECT_EQ(toolbar_bounds.y(),
            kAppBounds.bottom() - kToolbarEdgePadding - toolbar_size.height());

  // Move toolbar to bottom left quadrant and verify toolbar placement.
  DragToolbarToPoint(Movement::kMouse, {window_center_point.x() - x_offset,
                                        window_center_point.y() + y_offset});
  toolbar_bounds = native_window->GetBoundsInScreen();
  EXPECT_EQ(toolbar_bounds.x(), kAppBounds.x() + kToolbarEdgePadding);
  EXPECT_EQ(toolbar_bounds.y(),
            kAppBounds.bottom() - kToolbarEdgePadding - toolbar_size.height());
}

// Verifies the toolbar's snap location is preserved even after the visibility
// is hidden via the main menu view.
TEST_P(GameTypeGameDashboardContextTest, MoveAndHideToolbarWidget) {
  test_api_->OpenTheMainMenu();
  test_api_->OpenTheToolbar();

  // Move toolbar to bottom left quadrant and verify snap location is updated.
  gfx::Rect window_bounds = game_window_->GetBoundsInScreen();
  gfx::Point window_center_point = window_bounds.CenterPoint();
  DragToolbarToPoint(Movement::kMouse,
                     {window_center_point.x() - (window_bounds.width() / 4),
                      window_center_point.y() + (window_bounds.height() / 4)});
  EXPECT_EQ(test_api_->GetToolbarSnapLocation(),
            ToolbarSnapLocation::kBottomLeft);

  // Hide then show the toolbar and verify the toolbar was placed back into the
  // bottom left quadrant.
  test_api_->OpenTheMainMenu();
  test_api_->CloseTheToolbar();
  test_api_->OpenTheToolbar();
  EXPECT_EQ(test_api_->GetToolbarSnapLocation(),
            ToolbarSnapLocation::kBottomLeft);
}

// -----------------------------------------------------------------------------
// OnOverviewModeEndedWaiter:
class OnOverviewModeEndedWaiter : public OverviewObserver {
 public:
  OnOverviewModeEndedWaiter()
      : overview_controller_(OverviewController::Get()) {
    CHECK(overview_controller_);
    overview_controller_->AddObserver(this);
  }
  OnOverviewModeEndedWaiter(const OnOverviewModeEndedWaiter&) = delete;
  OnOverviewModeEndedWaiter& operator=(const OnOverviewModeEndedWaiter&) =
      delete;
  ~OnOverviewModeEndedWaiter() override {
    overview_controller_->RemoveObserver(this);
  }

  void Wait() { run_loop_.Run(); }

  // OverviewObserver:
  void OnOverviewModeEnded() override { run_loop_.Quit(); }

 private:
  base::RunLoop run_loop_;
  // Owned by Shell.
  const raw_ptr<OverviewController, ExperimentalAsh> overview_controller_;
};

// Verifies that in overview mode, the Game Dashboard button is not visible, the
// main menu is closed, and the toolbar visibility is unchanged.
TEST_P(GameTypeGameDashboardContextTest, OverviewMode) {
  auto* game_dashboard_button_widget =
      test_api_->GetGameDashboardButtonWidget();
  ASSERT_TRUE(game_dashboard_button_widget);

  // Open the main menu view and toolbar.
  test_api_->OpenTheMainMenu();
  test_api_->OpenTheToolbar();

  // Verify the initial state.
  // Game Dashboard button is visible.
  EXPECT_TRUE(game_dashboard_button_widget->IsVisible());
  // Toolbar is visible.
  auto* toolbar_widget = test_api_->GetToolbarWidget();
  ASSERT_TRUE(toolbar_widget);
  EXPECT_TRUE(toolbar_widget->IsVisible());
  // Main menu is visible.
  auto* main_menu_widget = test_api_->GetMainMenuWidget();
  ASSERT_TRUE(main_menu_widget);
  EXPECT_TRUE(main_menu_widget->IsVisible());

  EnterOverview();
  auto* overview_controller = OverviewController::Get();
  ASSERT_TRUE(overview_controller->InOverviewSession());

  // Verify states in overview mode.
  EXPECT_FALSE(game_dashboard_button_widget->IsVisible());
  ASSERT_EQ(toolbar_widget, test_api_->GetToolbarWidget());
  EXPECT_TRUE(toolbar_widget->IsVisible());
  EXPECT_FALSE(test_api_->GetMainMenuWidget());

  OnOverviewModeEndedWaiter waiter;
  ExitOverview();
  waiter.Wait();
  ASSERT_FALSE(overview_controller->InOverviewSession());

  // Verify states after exiting overview mode.
  EXPECT_TRUE(game_dashboard_button_widget->IsVisible());
  ASSERT_EQ(toolbar_widget, test_api_->GetToolbarWidget());
  EXPECT_TRUE(toolbar_widget->IsVisible());
  EXPECT_FALSE(test_api_->GetMainMenuWidget());
}

INSTANTIATE_TEST_SUITE_P(All,
                         GameTypeGameDashboardContextTest,
                         testing::Bool());

// -----------------------------------------------------------------------------
// GameDashboardStartAndStopCaptureSessionTest:
// Test fixture to verify the game window can be started and stopped from the
// main menu and toolbar, for both ARC and GeForceNow game windows.
class GameDashboardStartAndStopCaptureSessionTest
    : public GameDashboardContextTest,
      public testing::WithParamInterface<
          std::tuple</*is_arc_game_=*/bool,
                     /*should_start_from_main_menu_=*/bool,
                     /*should_stop_from_main_menu_=*/bool>> {
 public:
  GameDashboardStartAndStopCaptureSessionTest()
      : is_arc_game_(std::get<0>(GetParam())),
        should_start_from_main_menu_(std::get<1>(GetParam())),
        should_stop_from_main_menu_(std::get<2>(GetParam())) {}
  ~GameDashboardStartAndStopCaptureSessionTest() override = default;

  void SetUp() override {
    GameDashboardContextTest::SetUp();
    CreateGameWindow(is_arc_game_);
  }

 protected:
  const bool is_arc_game_;
  const bool should_start_from_main_menu_;
  const bool should_stop_from_main_menu_;
};

// GameDashboardStartAndStopCaptureSessionTest Tests
// -----------------------------------------------------------------------
// Verifies the game window recording starts and stops for the given set of test
// parameters.
TEST_P(GameDashboardStartAndStopCaptureSessionTest, RecordGameFromMainMenu) {
  auto* capture_mode_controller = CaptureModeController::Get();
  const base::RepeatingTimer& timer = test_api_->GetRecordingTimer();

  test_api_->OpenTheMainMenu();
  EXPECT_FALSE(capture_mode_controller->is_recording_in_progress());
  EXPECT_FALSE(timer.IsRunning());
  VerifyGameDashboardButtonState(/*is_recording=*/false);

  if (should_start_from_main_menu_) {
    // Retrieve the record game tile from the main menu.
    auto* record_game_tile = test_api_->GetMainMenuRecordGameTile();
    ASSERT_TRUE(record_game_tile);

    // Start the video recording from the main menu.
    LeftClickOn(record_game_tile);
    ClickOnStartRecordingButtonInCaptureModeBarView();
  } else {
    // Retrieve the record game button from the toolbar.
    CHECK(!test_api_->GetToolbarView());
    test_api_->OpenTheToolbar();
    test_api_->CloseTheMainMenu();
    auto* record_game_button = test_api_->GetToolbarRecordGameButton();
    ASSERT_TRUE(record_game_button);

    // Start the video recording from the toolbar.
    LeftClickOn(record_game_button);
  }

  EXPECT_TRUE(capture_mode_controller->is_recording_in_progress());
  EXPECT_TRUE(timer.IsRunning());
  VerifyGameDashboardButtonState(/*is_recording=*/true);

  if (should_stop_from_main_menu_) {
    // Stop the video recording from the main menu.
    test_api_->OpenTheMainMenu();
    LeftClickOn(test_api_->GetMainMenuRecordGameTile());
  } else {
    // Open the toolbar, if the video recording started from the main menu.
    if (should_start_from_main_menu_) {
      test_api_->OpenTheMainMenu();
      test_api_->OpenTheToolbar();
      test_api_->CloseTheMainMenu();
    }
    // Verify the toolbar is open.
    CHECK(test_api_->GetToolbarView());
    // Stop the video recording from the toolbar.
    LeftClickOn(test_api_->GetToolbarRecordGameButton());
  }
  EXPECT_FALSE(capture_mode_controller->is_recording_in_progress());
  EXPECT_FALSE(timer.IsRunning());
  VerifyGameDashboardButtonState(/*is_recording=*/false);
  WaitForCaptureFileToBeSaved();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GameDashboardStartAndStopCaptureSessionTest,
    testing::Combine(/*is_arc_game_=*/testing::Bool(),
                     /*should_start_from_main_menu_=*/testing::Bool(),
                     /*should_stop_from_main_menu_=*/testing::Bool()));

}  // namespace ash
