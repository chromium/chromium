// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/switch_access_menu_bubble_controller.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/accessibility/switch_access_back_button_bubble_controller.h"
#include "ash/system/accessibility/switch_access_back_button_view.h"
#include "ash/system/accessibility/switch_access_menu_button.h"
#include "ash/system/accessibility/switch_access_menu_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "ui/accessibility/accessibility_switches.h"

namespace ash {

class SwitchAccessMenuBubbleControllerTest : public AshTestBase {
 public:
  SwitchAccessMenuBubbleControllerTest() = default;
  ~SwitchAccessMenuBubbleControllerTest() override = default;

  SwitchAccessMenuBubbleControllerTest(
      const SwitchAccessMenuBubbleControllerTest&) = delete;
  SwitchAccessMenuBubbleControllerTest& operator=(
      const SwitchAccessMenuBubbleControllerTest&) = delete;

  // AshTestBase:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kEnableExperimentalAccessibilitySwitchAccess);
    AshTestBase::SetUp();
    Shell::Get()->accessibility_controller()->SetSwitchAccessEnabled(true);
  }

  SwitchAccessMenuBubbleController* GetBubbleController() {
    return Shell::Get()
        ->accessibility_controller()
        ->GetSwitchAccessBubbleControllerForTest();
  }

  SwitchAccessMenuView* GetMenuView() {
    return GetBubbleController()->menu_view_;
  }

  std::vector<SwitchAccessMenuButton*> GetMenuButtons() {
    std::vector<SwitchAccessMenuButton*> buttons;
    for (views::View* button : GetMenuView()->children())
      buttons.push_back(static_cast<SwitchAccessMenuButton*>(button));
    return buttons;
  }

  std::string GetName(SwitchAccessMenuButton* button) {
    return button->action_name_;
  }

  gfx::Rect GetBackButtonBounds() {
    SwitchAccessMenuBubbleController* bubble_controller = GetBubbleController();
    if (bubble_controller && bubble_controller->back_button_controller_ &&
        bubble_controller->back_button_controller_->back_button_view_) {
      return bubble_controller->back_button_controller_->back_button_view_
          ->GetBoundsInScreen();
    }
    return gfx::Rect();
  }

  int GetExpectedButtonWidth() { return SwitchAccessMenuButton::kWidthDip; }
  int GetExpectedBubbleWidth() {
    return 288;  // From the Switch Access spec.
  }
};

// TODO(anastasi): Add more tests for closing and repositioning the button.
TEST_F(SwitchAccessMenuBubbleControllerTest, ShowBackButton) {
  gfx::Rect anchor_rect(100, 100, 0, 0);
  GetBubbleController()->ShowBackButton(anchor_rect);

  gfx::Rect bounds = GetBackButtonBounds();
  EXPECT_GT(bounds.width(), 36);
  EXPECT_GT(bounds.height(), 36);
}

TEST_F(SwitchAccessMenuBubbleControllerTest, ShowMenu) {
  gfx::Rect anchor_rect(10, 10, 0, 0);
  GetBubbleController()->ShowMenu(anchor_rect,
                                  {"select", "scrollDown", "settings"});
  EXPECT_TRUE(GetMenuView());

  for (SwitchAccessMenuButton* button : GetMenuButtons()) {
    EXPECT_TRUE(button);
    EXPECT_EQ(button->width(), GetExpectedButtonWidth());
  }

  EXPECT_EQ(GetMenuView()->width(), GetExpectedBubbleWidth());
}

TEST_F(SwitchAccessMenuBubbleControllerTest, SetActions) {
  gfx::Rect anchor_rect(10, 10, 0, 0);
  GetBubbleController()->ShowMenu(anchor_rect,
                                  {"select", "scrollDown", "settings"});
  EXPECT_TRUE(GetMenuView());

  std::vector<SwitchAccessMenuButton*> buttons = GetMenuButtons();
  EXPECT_EQ(3ul, buttons.size());
  EXPECT_EQ("select", GetName(buttons[0]));
  EXPECT_EQ("scrollDown", GetName(buttons[1]));
  EXPECT_EQ("settings", GetName(buttons[2]));

  GetBubbleController()->ShowMenu(
      anchor_rect,
      {"keyboard", "dictation", "increment", "decrement", "settings"});

  buttons = GetMenuButtons();
  EXPECT_EQ(5ul, buttons.size());
  EXPECT_EQ("keyboard", GetName(buttons[0]));
  EXPECT_EQ("dictation", GetName(buttons[1]));
  EXPECT_EQ("increment", GetName(buttons[2]));
  EXPECT_EQ("decrement", GetName(buttons[3]));
  EXPECT_EQ("settings", GetName(buttons[4]));
}

TEST_F(SwitchAccessMenuBubbleControllerTest, AvoidsShelfBubble) {
  const struct {
    session_manager::SessionState session_state;
    std::string name;
  } kTestCases[]{
      {session_manager::SessionState::OOBE, "OOBE"},
      {session_manager::SessionState::LOGIN_PRIMARY, "LOGIN_PRIMARY"},
      {session_manager::SessionState::ACTIVE, "ACTIVE"},
      {session_manager::SessionState::LOCKED, "LOCKED"},
  };

  for (auto test : kTestCases) {
    GetSessionControllerClient()->SetSessionState(test.session_state);
    // Set up the switch access menu and the shelf
    auto* system_tray = GetPrimaryUnifiedSystemTray();
    system_tray->ShowBubble(true /* show_by_click */);
    gfx::Rect tray_bubble_bounds = system_tray->GetBubbleBoundsInScreen();

    // Try to show the menu in the center of the tray bubble.
    gfx::Rect anchor(tray_bubble_bounds.CenterPoint(), gfx::Size());
    GetBubbleController()->ShowMenu(anchor,
                                    {"select", "scrollDown", "settings"});
    SwitchAccessMenuView* menu = GetMenuView();
    ASSERT_TRUE(menu) << test.name;
    gfx::Rect menu_bounds = menu->GetBoundsInScreen();

    // Expect that the menu is shown somewhere that doesn't overlap the system
    // tray bubble.
    EXPECT_FALSE(menu_bounds.Intersects(tray_bubble_bounds)) << test.name;
  }
}

}  // namespace ash
