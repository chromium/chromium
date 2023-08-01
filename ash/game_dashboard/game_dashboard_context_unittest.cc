// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_context.h"

#include <memory>

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_test_util.h"
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
    test_api_.reset();
    GameDashboardTestBase::TearDown();
  }

  int GetToolbarHeight() {
    CHECK(test_api_->GetToolbarWidget())
        << "The toolbar must be opened first before trying to retrieve its "
           "height.";
    return test_api_->GetToolbarWidget()->GetWindowBoundsInScreen().height();
  }

  // If `is_arc_window` is true, this function creates the window as an ARC
  // game window. Otherwise, it creates the window as a GeForceNow window.
  void CreateGameWindow(bool is_arc_window) {
    ASSERT_FALSE(game_window_);
    ASSERT_FALSE(test_api_);
    game_window_ =
        CreateAppWindow((is_arc_window ? TestGameDashboardDelegate::kGameAppId
                                       : extension_misc::kGeForceNowAppId),
                        (is_arc_window ? AppType::ARC_APP : AppType::NON_APP),
                        gfx::Rect(0, 0, 400, 200));
    auto* context = GameDashboardController::Get()->GetGameDashboardContext(
        game_window_.get());
    ASSERT_TRUE(context);
    test_api_ = std::make_unique<GameDashboardContextTestApi>(
        context, GetEventGenerator());
    ASSERT_TRUE(test_api_);
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

 protected:
  std::unique_ptr<aura::Window> game_window_;
  std::unique_ptr<GameDashboardContextTestApi> test_api_;
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
TEST_P(GameTypeGameDashboardContextTest, ScreenCaptureFromMainMenu) {
  if (IsArcGame()) {
    game_window_->SetProperty(kArcGameControlsFlagsKey,
                              ArcGameControlsFlag::kKnown);
  }
  test_api_->OpenTheMainMenu();

  // Retrieve the video record tile and verify the initial state.
  auto* record_game_tile = test_api_->GetMainMenuRecordGameTile();
  ASSERT_TRUE(record_game_tile);

  LeftClickOn(record_game_tile);

  // Start video recording from `CaptureModeBarView`.
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

  test_api_->OpenTheMainMenu();

  // Retrieve the toolbar button and verify the toolbar widget is not available.
  auto* toolbar_tile = test_api_->GetMainMenuToolbarTile();
  ASSERT_TRUE(toolbar_tile);
  EXPECT_FALSE(toolbar_tile->IsToggled());

  test_api_->OpenTheToolbar();

  // Verify that the toolbar widget is now available.
  EXPECT_TRUE(test_api_->GetToolbarWidget());

  // Verify available feature buttons.
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

INSTANTIATE_TEST_SUITE_P(All,
                         GameTypeGameDashboardContextTest,
                         ::testing::Bool());

}  // namespace ash
