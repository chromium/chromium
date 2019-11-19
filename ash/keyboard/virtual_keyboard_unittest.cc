// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/events/test/event_generator.h"

namespace ash {

class VirtualKeyboardTest : public AshTestBase {
 public:
  VirtualKeyboardTest() = default;
  ~VirtualKeyboardTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    AshTestBase::SetUp();
    keyboard::SetTouchKeyboardEnabled(true);
  }

  void TearDown() override {
    keyboard::SetTouchKeyboardEnabled(false);
    AshTestBase::TearDown();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardTest);
};

TEST_F(VirtualKeyboardTest, EventsAreHandledBasedOnHitTestBounds) {
  aura::Window* root_window = Shell::GetPrimaryRootWindow();

  // Create a test window in the background with the same size as the screen.
  aura::test::EventCountDelegate delegate;
  std::unique_ptr<aura::Window> background_window(
      CreateTestWindowInShellWithDelegate(&delegate, 0, root_window->bounds()));

  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->ShowKeyboard(false);
  ASSERT_TRUE(keyboard::WaitUntilShown());

  // Add two hit test bounds (coordinates relative to keyboard window).
  // Both are 10x10 squares, but placed in different locations.
  std::vector<gfx::Rect> hit_test_bounds;
  hit_test_bounds.emplace_back(0, 0, 10, 10);
  hit_test_bounds.emplace_back(20, 20, 10, 10);
  keyboard_controller->SetHitTestBounds(hit_test_bounds);

  // Click at various places within the keyboard window and check whether the
  // event passes through the keyboard window to the background window.
  ui::test::EventGenerator generator(root_window);
  const gfx::Point origin =
      keyboard_controller->GetVisualBoundsInScreen().origin();

  // (0, 0) is inside the first hit rect, so the event is handled by the window
  // and is not received by the background window.
  generator.MoveMouseTo(origin);
  generator.ClickLeftButton();
  EXPECT_EQ("0 0", delegate.GetMouseButtonCountsAndReset());

  // (25, 25) is inside the second hit rect, so the background window does not
  // receive the event.
  generator.MoveMouseTo(origin + gfx::Vector2d(25, 25));
  generator.ClickLeftButton();
  EXPECT_EQ("0 0", delegate.GetMouseButtonCountsAndReset());

  // (5, 25) is not inside any hit rect, so the background window receives the
  // event.
  generator.MoveMouseTo(origin + gfx::Vector2d(5, 25));
  generator.ClickLeftButton();
  EXPECT_EQ("1 1", delegate.GetMouseButtonCountsAndReset());

  // (25, 5) is not inside any hit rect, so the background window receives the
  // event.
  generator.MoveMouseTo(origin + gfx::Vector2d(25, 5));
  generator.ClickLeftButton();
  EXPECT_EQ("1 1", delegate.GetMouseButtonCountsAndReset());
}

TEST_F(VirtualKeyboardTest, HitTestBoundsAreResetWhenContainerTypeChanges) {
  aura::Window* root_window = Shell::GetPrimaryRootWindow();

  // Create a test window in the background with the same size as the screen.
  aura::test::EventCountDelegate delegate;
  std::unique_ptr<aura::Window> background_window(
      CreateTestWindowInShellWithDelegate(&delegate, 0, root_window->bounds()));

  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->ShowKeyboard(false);
  ASSERT_TRUE(keyboard::WaitUntilShown());

  // Set empty hit test bounds, so all events pass through to the background.
  keyboard_controller->SetHitTestBounds(std::vector<gfx::Rect>());

  ui::test::EventGenerator generator(root_window);
  aura::Window* keyboard_window = keyboard_controller->GetKeyboardWindow();

  // (0, 0) passes through and is received by background window.
  generator.MoveMouseTo(keyboard_window->bounds().origin());
  generator.ClickLeftButton();
  EXPECT_EQ("1 1", delegate.GetMouseButtonCountsAndReset());

  // Change the container behavior, which should reset the hit test bounds to
  // the whole keyboard window.
  keyboard_controller->HideKeyboardExplicitlyBySystem();
  keyboard_controller->SetContainerType(keyboard::ContainerType::kFloating,
                                        base::nullopt, base::DoNothing());
  keyboard_controller->ShowKeyboard(false);

  // (0, 0) should no longer pass through the keyboard window.
  generator.MoveMouseTo(keyboard_window->bounds().origin());
  generator.ClickLeftButton();
  EXPECT_EQ("0 0", delegate.GetMouseButtonCountsAndReset());
}

}  // namespace ash
