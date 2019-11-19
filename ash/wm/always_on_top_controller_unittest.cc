// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/always_on_top_controller.h"

#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/keyboard/ui/keyboard_ui.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "ui/aura/client/aura_constants.h"

namespace ash {

class AlwaysOnTopControllerTest : public AshTestBase {
 public:
  AlwaysOnTopControllerTest() = default;
  ~AlwaysOnTopControllerTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    AshTestBase::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AlwaysOnTopControllerTest);
};

class TestLayoutManager : public WorkspaceLayoutManager {
 public:
  explicit TestLayoutManager(aura::Window* window)
      : WorkspaceLayoutManager(window), keyboard_bounds_changed_(false) {}

  ~TestLayoutManager() override = default;

  void OnKeyboardDisplacingBoundsChanged(const gfx::Rect& bounds) override {
    keyboard_bounds_changed_ = true;
    WorkspaceLayoutManager::OnKeyboardDisplacingBoundsChanged(bounds);
  }

  bool keyboard_bounds_changed() const { return keyboard_bounds_changed_; }

 private:
  bool keyboard_bounds_changed_;
  DISALLOW_COPY_AND_ASSIGN(TestLayoutManager);
};

// Verifies that the always on top controller is notified of keyboard bounds
// changing events.
TEST_F(AlwaysOnTopControllerTest, NotifyKeyboardBoundsChanging) {
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  aura::Window* always_on_top_container =
      Shell::GetContainer(root_window, kShellWindowId_AlwaysOnTopContainer);
  // Install test layout manager.
  TestLayoutManager* manager = new TestLayoutManager(always_on_top_container);
  RootWindowController* controller = Shell::GetPrimaryRootWindowController();
  AlwaysOnTopController* always_on_top_controller =
      controller->always_on_top_controller();
  always_on_top_controller->SetLayoutManagerForTest(base::WrapUnique(manager));

  // Show the keyboard.
  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->ShowKeyboard(false /* locked */);
  ASSERT_TRUE(keyboard::WaitUntilShown());

  // Verify that test manager was notified of bounds change.
  ASSERT_TRUE(manager->keyboard_bounds_changed());
}

TEST_F(AlwaysOnTopControllerTest,
       AlwaysOnTopContainerReturnedForFloatingWindow) {
  RootWindowController* controller = Shell::GetPrimaryRootWindowController();
  AlwaysOnTopController* always_on_top_controller =
      controller->always_on_top_controller();

  const gfx::Rect bounds(100, 100, 200, 200);
  std::unique_ptr<aura::Window> always_on_top_window(
      CreateTestWindowInShellWithBounds(bounds));
  always_on_top_window->SetProperty(aura::client::kZOrderingKey,
                                    ui::ZOrderLevel::kFloatingWindow);

  aura::Window* container =
      always_on_top_controller->GetContainer(always_on_top_window.get());
  ASSERT_TRUE(container);
  EXPECT_EQ(kShellWindowId_AlwaysOnTopContainer, container->id());
}

TEST_F(AlwaysOnTopControllerTest, PipContainerReturnedForFloatingPipWindow) {
  RootWindowController* controller = Shell::GetPrimaryRootWindowController();
  AlwaysOnTopController* always_on_top_controller =
      controller->always_on_top_controller();

  const gfx::Rect bounds(100, 100, 200, 200);
  std::unique_ptr<aura::Window> pip_window(
      CreateTestWindowInShellWithBounds(bounds));

  WindowState* window_state = WindowState::Get(pip_window.get());
  const WMEvent enter_pip(WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);
  pip_window->SetProperty(aura::client::kZOrderingKey,
                          ui::ZOrderLevel::kFloatingWindow);
  EXPECT_TRUE(window_state->IsPip());

  aura::Window* container =
      always_on_top_controller->GetContainer(pip_window.get());
  ASSERT_TRUE(container);
  EXPECT_EQ(kShellWindowId_PipContainer, container->id());
}

TEST_F(AlwaysOnTopControllerTest,
       DefaultContainerReturnedForWindowNotAlwaysOnTop) {
  RootWindowController* controller = Shell::GetPrimaryRootWindowController();
  AlwaysOnTopController* always_on_top_controller =
      controller->always_on_top_controller();

  const gfx::Rect bounds(100, 100, 200, 200);
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(bounds));

  aura::Window* container =
      always_on_top_controller->GetContainer(window.get());
  ASSERT_TRUE(container);
  EXPECT_EQ(desks_util::GetActiveDeskContainerId(), container->id());
}

TEST_F(AlwaysOnTopControllerTest,
       FloatingWindowMovedBetweenContainersWhenPipStateChanges) {
  const gfx::Rect bounds(100, 100, 200, 200);
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(bounds));
  window->SetProperty(aura::client::kZOrderingKey,
                      ui::ZOrderLevel::kFloatingWindow);

  EXPECT_EQ(kShellWindowId_AlwaysOnTopContainer, window->parent()->id());

  WindowState* window_state = WindowState::Get(window.get());
  const WMEvent enter_pip(WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);
  EXPECT_TRUE(window_state->IsPip());

  EXPECT_EQ(kShellWindowId_PipContainer, window->parent()->id());

  const WMEvent enter_normal(WM_EVENT_NORMAL);
  window_state->OnWMEvent(&enter_normal);
  EXPECT_FALSE(window_state->IsPip());

  EXPECT_EQ(kShellWindowId_AlwaysOnTopContainer, window->parent()->id());
}

}  // namespace ash
