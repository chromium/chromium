// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/game_dashboard/game_dashboard_context.h"

#include "ash/capture_mode/capture_mode_test_util.h"
#include "ash/game_dashboard/game_dashboard_controller.h"
#include "ash/game_dashboard/game_dashboard_test_base.h"
#include "ash/game_dashboard/test_game_dashboard_delegate.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/window_properties.h"
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

  void SetUp() override {
    GameDashboardTestBase::SetUp();
    game_window_ = CreateAppWindow(TestGameDashboardDelegate::kGameAppId,
                                   AppType::ARC_APP, gfx::Rect(0, 0, 400, 200));
    game_context_ = GameDashboardController::Get()->GetGameDashboardContext(
        game_window_.get());
    DCHECK(game_context_);
  }

  void SetUpGeForceNowApp() {
    game_window_->SetProperty(
        kAppIDKey, static_cast<std::string>(extension_misc::kGeForceNowAppId));
    game_window_->SetProperty(aura::client::kAppType,
                              static_cast<int>(AppType::NON_APP));
  }

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

 protected:
  std::unique_ptr<aura::Window> game_window_;
  raw_ptr<const GameDashboardContext, ExperimentalAsh> game_context_;
  const gfx::Rect game_window_bounds_ = gfx::Rect(0, 0, 400, 200);
};

// Tests
// -----------------------------------------------------------------------
// Verifies the initial location of the main menu button widget relative to the
// game window.
TEST_F(GameDashboardContextTest, MainMenuButtonWidget_InitialLocation) {
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
TEST_F(GameDashboardContextTest,
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
TEST_F(GameDashboardContextTest, OpenMainMenuButtonWidget) {
  // Verifies the initial state.
  EXPECT_FALSE(GetMainMenuDialogWidget());

  // Opens main menu dialog.
  LeftClickOn(GetMainMenuButtonWidget()->GetContentsView());

  // Verifies that the menu is visible.
  EXPECT_TRUE(GetMainMenuDialogWidget());
}

// Verifies clicking the main menu button will close the main menu widget if
// it's already open.
TEST_F(GameDashboardContextTest, CloseMainMenuButtonWidget) {
  // Opens the main menu widget and Verifies the initial state.
  LeftClickOn(GetMainMenuButtonWidget()->GetContentsView());
  EXPECT_TRUE(GetMainMenuDialogWidget());

  // Closes the main menu dialog.
  LeftClickOn(GetMainMenuButtonWidget()->GetContentsView());

  // Verifies that the menu is no longer visible.
  EXPECT_FALSE(GetMainMenuDialogWidget());
}

// Verifies the main menu shows all items allowed for ARC games.
TEST_F(GameDashboardContextTest, MainMenuDialogWidget_ARCGame) {
  // Open the main menu.
  LeftClickOn(GetMainMenuButtonWidget()->GetContentsView());
  ASSERT_TRUE(GetMainMenuDialogWidget());

  // Verify whether each element available in the main menu is available as
  // expected.
  EXPECT_TRUE(GetMainMenuViewById(VIEW_ID_GD_TOOLBAR_TILE));
  // TODO(b/273641402): Update Game Controls visibility once implemented.
  EXPECT_FALSE(GetMainMenuViewById(VIEW_ID_GD_CONTROLS_TILE));
  EXPECT_TRUE(GetMainMenuViewById(VIEW_ID_GD_RECORD_TILE));
  EXPECT_TRUE(GetMainMenuViewById(VIEW_ID_GD_SCREENSHOT_TILE));
  EXPECT_TRUE(GetMainMenuViewById(VIEW_ID_GD_SCREEN_SIZE_TILE));
  EXPECT_TRUE(GetMainMenuViewById(VIEW_ID_GD_FEEDBACK_BUTTON));
  EXPECT_TRUE(GetMainMenuViewById(VIEW_ID_GD_HELP_BUTTON));
  EXPECT_TRUE(GetMainMenuViewById(VIEW_ID_GD_GENERAL_SETTINGS_BUTTON));
}

// Verifies the main menu doesn't show items only allowed for ARC games on
// non-ARC apps.
TEST_F(GameDashboardContextTest, MainMenuDialogWidget_NonARCGame) {
  // Override the default `game_window_` to reflect GeForce Now and open the
  // main menu.
  SetUpGeForceNowApp();
  LeftClickOn(GetMainMenuButtonWidget()->GetContentsView());
  ASSERT_TRUE(GetMainMenuDialogWidget());

  // Verify whether each element available in the main menu is available as
  // expected.
  EXPECT_TRUE(GetMainMenuViewById(VIEW_ID_GD_TOOLBAR_TILE));
  EXPECT_FALSE(GetMainMenuViewById(VIEW_ID_GD_CONTROLS_TILE));
  EXPECT_TRUE(GetMainMenuViewById(VIEW_ID_GD_RECORD_TILE));
  EXPECT_TRUE(GetMainMenuViewById(VIEW_ID_GD_SCREENSHOT_TILE));
  EXPECT_FALSE(GetMainMenuViewById(VIEW_ID_GD_SCREEN_SIZE_TILE));
  EXPECT_TRUE(GetMainMenuViewById(VIEW_ID_GD_FEEDBACK_BUTTON));
  EXPECT_TRUE(GetMainMenuViewById(VIEW_ID_GD_HELP_BUTTON));
  EXPECT_TRUE(GetMainMenuViewById(VIEW_ID_GD_GENERAL_SETTINGS_BUTTON));
}

TEST_F(GameDashboardContextTest, TakeScreenshot) {
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

}  // namespace ash
