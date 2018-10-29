// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/always_on_top_controller.h"

#include "ash/keyboard/ash_keyboard_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_ui.h"
#include "ui/keyboard/test/keyboard_test_util.h"

namespace ash {

class VirtualKeyboardAlwaysOnTopControllerTest : public AshTestBase {
 public:
  VirtualKeyboardAlwaysOnTopControllerTest() = default;
  ~VirtualKeyboardAlwaysOnTopControllerTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    AshTestBase::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardAlwaysOnTopControllerTest);
};

class TestLayoutManager : public WorkspaceLayoutManager {
 public:
  explicit TestLayoutManager(aura::Window* window)
      : WorkspaceLayoutManager(window), keyboard_bounds_changed_(false) {}

  ~TestLayoutManager() override = default;

  void OnKeyboardWorkspaceDisplacingBoundsChanged(
      const gfx::Rect& bounds) override {
    keyboard_bounds_changed_ = true;
    WorkspaceLayoutManager::OnKeyboardWorkspaceDisplacingBoundsChanged(bounds);
  }

  bool keyboard_bounds_changed() const { return keyboard_bounds_changed_; }

 private:
  bool keyboard_bounds_changed_;
  DISALLOW_COPY_AND_ASSIGN(TestLayoutManager);
};

// Verifies that the always on top controller is notified of keyboard bounds
// changing events.
TEST_F(VirtualKeyboardAlwaysOnTopControllerTest, NotifyKeyboardBoundsChanging) {
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  aura::Window* always_on_top_container =
      Shell::GetContainer(root_window, kShellWindowId_AlwaysOnTopContainer);
  // Install test layout manager.
  TestLayoutManager* manager = new TestLayoutManager(always_on_top_container);
  RootWindowController* controller = Shell::GetPrimaryRootWindowController();
  // Deactivates keyboard to unregister existing listeners.
  Shell::Get()->ash_keyboard_controller()->DeactivateKeyboard();
  AlwaysOnTopController* always_on_top_controller =
      controller->always_on_top_controller();
  always_on_top_controller->SetLayoutManagerForTest(base::WrapUnique(manager));
  // Activate keyboard. This triggers keyboard listeners to be registered.
  Shell::Get()->ash_keyboard_controller()->ActivateKeyboard();

  // Show the keyboard.
  auto* keyboard_controller = keyboard::KeyboardController::Get();
  keyboard_controller->ShowKeyboard(false /* locked */);
  const int kKeyboardHeight = 200;
  gfx::Rect keyboard_bounds = keyboard::KeyboardBoundsFromRootBounds(
      root_window->bounds(), kKeyboardHeight);
  keyboard_controller->GetKeyboardWindow()->SetBounds(keyboard_bounds);
  keyboard_controller->NotifyKeyboardWindowLoaded();

  // Verify that test manager was notified of bounds change.
  ASSERT_TRUE(manager->keyboard_bounds_changed());
}

}  // namespace ash
