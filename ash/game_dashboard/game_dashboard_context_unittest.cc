// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/game_dashboard/game_dashboard_context.h"

#include "ash/game_dashboard/game_dashboard_controller.h"
#include "ash/game_dashboard/game_dashboard_test_base.h"
#include "ash/game_dashboard/test_game_dashboard_delegate.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/frame/frame_header.h"
#include "chromeos/ui/wm/window_util.h"
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

 protected:
  std::unique_ptr<aura::Window> game_window_;
  const GameDashboardContext* game_context_;
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

}  // namespace ash
