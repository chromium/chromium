// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/compat_mode/resize_toggle_menu.h"

#include <memory>

#include "ash/components/arc/compat_mode/arc_resize_lock_pref_delegate.h"
#include "ash/components/arc/compat_mode/metrics.h"
#include "ash/components/arc/compat_mode/test/compat_mode_test_base.h"
#include "base/test/bind.h"
#include "base/test/metrics/user_action_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/button/button.h"

namespace arc {
namespace {

constexpr char kTestAppId[] = "123";

}  // namespace

class ResizeToggleMenuTest : public CompatModeTestBase {
 public:
  // Overridden from test::Test.
  void SetUp() override {
    CompatModeTestBase::SetUp();
    widget_ = CreateArcWidget(std::string(kTestAppId));
    // Resizable mode by default.
    pref_delegate()->SetResizeLockState(kTestAppId,
                                        mojom::ArcResizeLockState::OFF);
    SyncResizeLockPropertyWithMojoState(widget());
    resize_toggle_menu_ = std::make_unique<ResizeToggleMenu>(
        on_bubble_widget_closing_callback_, widget_.get(), pref_delegate());
  }
  void TearDown() override {
    widget_->CloseNow();
    CompatModeTestBase::TearDown();
  }

  bool IsMenuRunning() {
    return resize_toggle_menu_->bubble_widget_ &&
           resize_toggle_menu_->bubble_widget_->IsVisible();
  }

  // Re-show the menu. This might close the running menu if any.
  void ReshowMenu() {
    resize_toggle_menu_.reset();
    resize_toggle_menu_ = std::make_unique<ResizeToggleMenu>(
        on_bubble_widget_closing_callback_, widget_.get(), pref_delegate());
  }

  bool IsCommandButtonDisabled(ash::ResizeCompatMode command_id) {
    return GetButtonByCommandId(command_id)->GetState() ==
           views::Button::ButtonState::STATE_DISABLED;
  }

  bool on_bubble_widget_closing_callback_called() const {
    return on_bubble_widget_closing_callback_called_;
  }

  void ClickButton(ash::ResizeCompatMode command_id) {
    const auto* button = GetButtonByCommandId(command_id);
    LeftClickOnView(widget_.get(), button);
    SyncResizeLockPropertyWithMojoState(widget());
  }

  void CloseBubble() { resize_toggle_menu_->CloseBubble(); }

  views::Widget* widget() { return widget_.get(); }
  ResizeToggleMenu* resize_toggle_menu() { return resize_toggle_menu_.get(); }

 private:
  views::Button* GetButtonByCommandId(ash::ResizeCompatMode command_id) {
    switch (command_id) {
      case ash::ResizeCompatMode::kPhone:
        return resize_toggle_menu_->phone_button_;
      case ash::ResizeCompatMode::kTablet:
        return resize_toggle_menu_->tablet_button_;
      case ash::ResizeCompatMode::kResizable:
        return resize_toggle_menu_->resizable_button_;
    }
  }

  bool on_bubble_widget_closing_callback_called_ = false;
  base::RepeatingClosure on_bubble_widget_closing_callback_ =
      base::BindLambdaForTesting(
          [&]() { on_bubble_widget_closing_callback_called_ = true; });
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ResizeToggleMenu> resize_toggle_menu_;
};

TEST_F(ResizeToggleMenuTest, ConstructDestruct) {
  EXPECT_TRUE(IsMenuRunning());
}

// Test that on_bubble_widget_closing_callback_ is called after closing bubble.
TEST_F(ResizeToggleMenuTest, TestCallback) {
  EXPECT_TRUE(IsMenuRunning());
  EXPECT_FALSE(on_bubble_widget_closing_callback_called());
  CloseBubble();
  EXPECT_TRUE(on_bubble_widget_closing_callback_called());
}

TEST_F(ResizeToggleMenuTest, TestResizePhone) {
  // Verify pre-conditions.
  EXPECT_TRUE(IsMenuRunning());

  // Test that resize command is properly handled.
  ClickButton(ash::ResizeCompatMode::kPhone);
  EXPECT_LT(widget()->GetWindowBoundsInScreen().width(),
            widget()->GetWindowBoundsInScreen().height());

  // Test that the selected item is changed dynamically after the resize.
  EXPECT_TRUE(IsCommandButtonDisabled(ash::ResizeCompatMode::kPhone));
  EXPECT_FALSE(IsCommandButtonDisabled(ash::ResizeCompatMode::kTablet));
  EXPECT_FALSE(IsCommandButtonDisabled(ash::ResizeCompatMode::kResizable));

  // Test that the item is selected after re-showing.
  ReshowMenu();
  EXPECT_TRUE(IsMenuRunning());
  EXPECT_TRUE(IsCommandButtonDisabled(ash::ResizeCompatMode::kPhone));
  EXPECT_FALSE(IsCommandButtonDisabled(ash::ResizeCompatMode::kTablet));
  EXPECT_FALSE(IsCommandButtonDisabled(ash::ResizeCompatMode::kResizable));
}

TEST_F(ResizeToggleMenuTest, TestResizeTablet) {
  // Verify pre-conditions.
  EXPECT_TRUE(IsMenuRunning());

  // Test that resize command is properly handled.
  ClickButton(ash::ResizeCompatMode::kTablet);
  EXPECT_GT(widget()->GetWindowBoundsInScreen().width(),
            widget()->GetWindowBoundsInScreen().height());

  // Test that the selected item is changed dynamically after the resize.
  EXPECT_FALSE(IsCommandButtonDisabled(ash::ResizeCompatMode::kPhone));
  EXPECT_TRUE(IsCommandButtonDisabled(ash::ResizeCompatMode::kTablet));
  EXPECT_FALSE(IsCommandButtonDisabled(ash::ResizeCompatMode::kResizable));

  // Test that the item is selected after re-showing.
  ReshowMenu();
  EXPECT_TRUE(IsMenuRunning());
  EXPECT_FALSE(IsCommandButtonDisabled(ash::ResizeCompatMode::kPhone));
  EXPECT_TRUE(IsCommandButtonDisabled(ash::ResizeCompatMode::kTablet));
  EXPECT_FALSE(IsCommandButtonDisabled(ash::ResizeCompatMode::kResizable));
}

TEST_F(ResizeToggleMenuTest, TestResizable) {
  // Verify pre-conditions.
  EXPECT_TRUE(IsMenuRunning());
  // Set resize locked mode to enable Resizable button.
  pref_delegate()->SetResizeLockState(kTestAppId,
                                      mojom::ArcResizeLockState::ON);
  SyncResizeLockPropertyWithMojoState(widget());

  // Test that resize command is properly handled.
  ClickButton(ash::ResizeCompatMode::kResizable);
  EXPECT_EQ(pref_delegate()->GetResizeLockState(kTestAppId),
            mojom::ArcResizeLockState::OFF);

  // Test that the selected item is changed dynamically.
  EXPECT_FALSE(IsCommandButtonDisabled(ash::ResizeCompatMode::kPhone));
  EXPECT_FALSE(IsCommandButtonDisabled(ash::ResizeCompatMode::kTablet));
  EXPECT_TRUE(IsCommandButtonDisabled(ash::ResizeCompatMode::kResizable));

  // Test that the item is selected after the resize.
  ReshowMenu();
  EXPECT_TRUE(IsMenuRunning());
  EXPECT_FALSE(IsCommandButtonDisabled(ash::ResizeCompatMode::kPhone));
  EXPECT_FALSE(IsCommandButtonDisabled(ash::ResizeCompatMode::kTablet));
  EXPECT_TRUE(IsCommandButtonDisabled(ash::ResizeCompatMode::kResizable));
}

// Test that the button state is dynamically changed even if no bounds change
// happens.
TEST_F(ResizeToggleMenuTest, TestButtonStateChangeWithoutBoundsChange) {
  // Verify pre-conditions.
  EXPECT_TRUE(IsMenuRunning());

  ClickButton(ash::ResizeCompatMode::kPhone);
  EXPECT_TRUE(IsCommandButtonDisabled(ash::ResizeCompatMode::kPhone));
  EXPECT_FALSE(IsCommandButtonDisabled(ash::ResizeCompatMode::kTablet));
  EXPECT_FALSE(IsCommandButtonDisabled(ash::ResizeCompatMode::kResizable));

  ClickButton(ash::ResizeCompatMode::kResizable);
  EXPECT_FALSE(IsCommandButtonDisabled(ash::ResizeCompatMode::kPhone));
  EXPECT_FALSE(IsCommandButtonDisabled(ash::ResizeCompatMode::kTablet));
  EXPECT_TRUE(IsCommandButtonDisabled(ash::ResizeCompatMode::kResizable));

  ClickButton(ash::ResizeCompatMode::kPhone);
  EXPECT_TRUE(IsCommandButtonDisabled(ash::ResizeCompatMode::kPhone));
  EXPECT_FALSE(IsCommandButtonDisabled(ash::ResizeCompatMode::kTablet));
  EXPECT_FALSE(IsCommandButtonDisabled(ash::ResizeCompatMode::kResizable));
}

// Test that the menu is closed with delay when the button is clicked.
TEST_F(ResizeToggleMenuTest, TestDelayedAutoClose) {
  EXPECT_TRUE(IsMenuRunning());

  ClickButton(ash::ResizeCompatMode::kPhone);
  EXPECT_TRUE(IsMenuRunning());
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(IsMenuRunning());
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(IsMenuRunning());
}

// Test that the delayed auto close is canceled when another button is clicked.
TEST_F(ResizeToggleMenuTest, TestDelayedAutoCloseCancel) {
  EXPECT_TRUE(IsMenuRunning());

  ClickButton(ash::ResizeCompatMode::kPhone);
  EXPECT_TRUE(IsMenuRunning());
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(IsMenuRunning());
  ClickButton(ash::ResizeCompatMode::kTablet);
  EXPECT_TRUE(IsMenuRunning());
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(IsMenuRunning());
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(IsMenuRunning());
}

// Tests that user action metrics are recorded correctly.
TEST_F(ResizeToggleMenuTest, TestUserActionMetrics) {
  base::UserActionTester user_action_tester;

  ClickButton(ash::ResizeCompatMode::kPhone);
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(GetResizeLockActionNameForTesting(
                ResizeLockActionType::ResizeToPhone)));
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(GetResizeLockActionNameForTesting(
                ResizeLockActionType::TurnOnResizeLock)));

  ClickButton(ash::ResizeCompatMode::kTablet);
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(GetResizeLockActionNameForTesting(
                ResizeLockActionType::ResizeToTablet)));
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(GetResizeLockActionNameForTesting(
                ResizeLockActionType::TurnOnResizeLock)));

  ClickButton(ash::ResizeCompatMode::kResizable);
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(GetResizeLockActionNameForTesting(
                ResizeLockActionType::TurnOffResizeLock)));

  ClickButton(ash::ResizeCompatMode::kPhone);
  EXPECT_EQ(2,
            user_action_tester.GetActionCount(GetResizeLockActionNameForTesting(
                ResizeLockActionType::ResizeToPhone)));
  EXPECT_EQ(2,
            user_action_tester.GetActionCount(GetResizeLockActionNameForTesting(
                ResizeLockActionType::TurnOnResizeLock)));
}

// Test that the menu is not shown if the window is maximized or fullscreen.
TEST_F(ResizeToggleMenuTest, TestMaximizedOrFullscreen) {
  // Test maximized after shown.
  EXPECT_TRUE(IsMenuRunning());
  widget()->Maximize();
  EXPECT_FALSE(IsMenuRunning());
  widget()->Restore();

  // Test fullscreen after shown.
  ReshowMenu();
  EXPECT_TRUE(IsMenuRunning());
  widget()->SetFullscreen(true);
  EXPECT_FALSE(IsMenuRunning());
  widget()->SetFullscreen(false);

  // Test maximized before shown.
  widget()->Maximize();
  ReshowMenu();
  EXPECT_FALSE(IsMenuRunning());
  widget()->Restore();

  // Test fullscreen before shown.
  widget()->SetFullscreen(true);
  ReshowMenu();
  EXPECT_FALSE(IsMenuRunning());
  widget()->SetFullscreen(false);
}

// Test that IsBubbleOpen returns the correct state of the bubble
TEST_F(ResizeToggleMenuTest, TestIsBubbleShown) {
  EXPECT_TRUE(IsMenuRunning());
  EXPECT_TRUE(resize_toggle_menu()->IsBubbleShown());
  CloseBubble();
  RunPendingMessages();
  EXPECT_FALSE(resize_toggle_menu()->IsBubbleShown());
}

}  // namespace arc
