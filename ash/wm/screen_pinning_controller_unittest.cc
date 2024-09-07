// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/screen_pinning_controller.h"

#include <vector>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/client_controlled_state.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "ui/aura/window.h"

namespace ash {
namespace {

int FindIndex(
    const std::vector<raw_ptr<aura::Window, VectorExperimental>>& windows,
    const aura::Window* target) {
  auto iter = base::ranges::find(windows, target);
  return iter != windows.end() ? iter - windows.begin() : -1;
}

class TestClientControlledStateDelegate
    : public ClientControlledState::Delegate {
 public:
  ~TestClientControlledStateDelegate() override = default;

  void HandleWindowStateRequest(WindowState* state,
                                chromeos::WindowStateType type) override {}
  void HandleBoundsRequest(WindowState* state,
                           chromeos::WindowStateType type,
                           const gfx::Rect& requested_bounds,
                           int64_t display_id) override {}
};

}  // namespace

using ScreenPinningControllerTest = AshTestBase;

TEST_F(ScreenPinningControllerTest, IsPinned) {
  aura::Window* w1 = CreateTestWindowInShellWithId(0);
  wm::ActivateWindow(w1);

  window_util::PinWindow(w1, /* trusted */ false);
  EXPECT_TRUE(Shell::Get()->screen_pinning_controller()->IsPinned());
}

TEST_F(ScreenPinningControllerTest, OnlyOnePinnedWindow) {
  aura::Window* w1 = CreateTestWindowInShellWithId(0);
  aura::Window* w2 = CreateTestWindowInShellWithId(1);
  wm::ActivateWindow(w1);

  window_util::PinWindow(w1, /* trusted */ false);
  EXPECT_TRUE(WindowState::Get(w1)->IsPinned());
  EXPECT_FALSE(WindowState::Get(w2)->IsPinned());

  // Prohibit to pin two (or more) windows.
  window_util::PinWindow(w2, /* trusted */ false);
  EXPECT_TRUE(WindowState::Get(w1)->IsPinned());
  EXPECT_FALSE(WindowState::Get(w2)->IsPinned());
}

TEST_F(ScreenPinningControllerTest, FullscreenInPinnedMode) {
  aura::Window* w1 = CreateTestWindowInShellWithId(0);
  aura::Window* w2 = CreateTestWindowInShellWithId(1);
  wm::ActivateWindow(w1);

  window_util::PinWindow(w1, /* trusted */ false);
  {
    // Window w1 should be in front of w2.
    std::vector<raw_ptr<aura::Window, VectorExperimental>> siblings =
        w1->parent()->children();
    int index1 = FindIndex(siblings, w1);
    int index2 = FindIndex(siblings, w2);
    EXPECT_NE(-1, index1);
    EXPECT_NE(-1, index2);
    EXPECT_GT(index1, index2);
  }

  // Set w2 to fullscreen.
  {
    wm::ActivateWindow(w2);
    const WMEvent event(WM_EVENT_TOGGLE_FULLSCREEN);
    WindowState::Get(w2)->OnWMEvent(&event);
  }
  {
    // Verify that w1 is still in front of w2.
    std::vector<raw_ptr<aura::Window, VectorExperimental>> siblings =
        w1->parent()->children();
    int index1 = FindIndex(siblings, w1);
    int index2 = FindIndex(siblings, w2);
    EXPECT_NE(-1, index1);
    EXPECT_NE(-1, index2);
    EXPECT_GT(index1, index2);
  }

  // Unset w2's fullscreen.
  {
    wm::ActivateWindow(w2);
    const WMEvent event(WM_EVENT_TOGGLE_FULLSCREEN);
    WindowState::Get(w2)->OnWMEvent(&event);
  }
  {
    // Verify that w1 is still in front of w2.
    std::vector<raw_ptr<aura::Window, VectorExperimental>> siblings =
        w1->parent()->children();
    int index1 = FindIndex(siblings, w1);
    int index2 = FindIndex(siblings, w2);
    EXPECT_NE(-1, index1);
    EXPECT_NE(-1, index2);
    EXPECT_GT(index1, index2);
  }

  // Maximize w2.
  {
    wm::ActivateWindow(w2);
    const WMEvent event(WM_EVENT_TOGGLE_MAXIMIZE);
    WindowState::Get(w2)->OnWMEvent(&event);
  }
  {
    // Verify that w1 is still in front of w2.
    std::vector<raw_ptr<aura::Window, VectorExperimental>> siblings =
        w1->parent()->children();
    int index1 = FindIndex(siblings, w1);
    int index2 = FindIndex(siblings, w2);
    EXPECT_NE(-1, index1);
    EXPECT_NE(-1, index2);
    EXPECT_GT(index1, index2);
  }

  // Unset w2's maximize.
  {
    wm::ActivateWindow(w2);
    const WMEvent event(WM_EVENT_TOGGLE_MAXIMIZE);
    WindowState::Get(w2)->OnWMEvent(&event);
  }
  {
    // Verify that w1 is still in front of w2.
    std::vector<raw_ptr<aura::Window, VectorExperimental>> siblings =
        w1->parent()->children();
    int index1 = FindIndex(siblings, w1);
    int index2 = FindIndex(siblings, w2);
    EXPECT_NE(-1, index1);
    EXPECT_NE(-1, index2);
    EXPECT_GT(index1, index2);
  }

  // Restore w1.
  WindowState::Get(w1)->Restore();

  // Now, fullscreen-ize w2 should put it in front of w1.
  {
    wm::ActivateWindow(w2);
    const WMEvent event(WM_EVENT_TOGGLE_FULLSCREEN);
    WindowState::Get(w2)->OnWMEvent(&event);
  }
  {
    // Verify that w1 is still in front of w2.
    std::vector<raw_ptr<aura::Window, VectorExperimental>> siblings =
        w1->parent()->children();
    int index1 = FindIndex(siblings, w1);
    int index2 = FindIndex(siblings, w2);
    EXPECT_NE(-1, index1);
    EXPECT_NE(-1, index2);
    EXPECT_GT(index2, index1);
  }
}

TEST_F(ScreenPinningControllerTest, TrustedPinnedWithAccelerator) {
  aura::Window* w1 = CreateTestWindowInShellWithId(0);
  wm::ActivateWindow(w1);

  window_util::PinWindow(w1, /* trusted */ true);
  EXPECT_TRUE(Shell::Get()->screen_pinning_controller()->IsPinned());

  Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
      AcceleratorAction::kUnpin, {});
  // The AcceleratorAction::kUnpin accelerator key is disabled for trusted
  // pinned and the window must be still pinned.
  EXPECT_TRUE(Shell::Get()->screen_pinning_controller()->IsPinned());
}

TEST_F(ScreenPinningControllerTest, ExitUnifiedDisplay) {
  display_manager()->SetUnifiedDesktopEnabled(true);

  UpdateDisplay("400x300, 500x400");

  aura::Window* w1 = CreateTestWindowInShellWithId(0);
  wm::ActivateWindow(w1);
  auto* window_state = WindowState::Get(w1);

  window_util::PinWindow(w1, /*trusted=*/true);

  EXPECT_TRUE(window_state->IsPinned());
  EXPECT_TRUE(Shell::Get()->screen_pinning_controller()->IsPinned());

  UpdateDisplay("300x200");

  EXPECT_TRUE(window_state->IsPinned());
  EXPECT_TRUE(Shell::Get()->screen_pinning_controller()->IsPinned());
}

TEST_F(ScreenPinningControllerTest, CleanUpObserversAndDimmer) {
  // Create a window with ClientControlledState.
  auto w = CreateAppWindow(gfx::Rect(), chromeos::AppType::CHROME_APP, 0);
  ash::WindowState* ws = ash::WindowState::Get(w.get());
  auto delegate = std::make_unique<TestClientControlledStateDelegate>();
  auto state = std::make_unique<ClientControlledState>(std::move(delegate));
  auto* state_raw = state.get();
  ws->SetStateObject(std::move(state));

  wm::ActivateWindow(w.get());

  // Observer should be added to |w|, and |w->parent()|.
  state_raw->EnterNextState(ws, chromeos::WindowStateType::kPinned);
  EXPECT_TRUE(WindowState::Get(w.get())->IsPinned());

  const aura::Window* container = w->parent();
  // Destroying |w| clears |pinned_window_|. The observers should be removed
  // even if ClientControlledState doesn't call SetPinnedWindow when
  // WindowState::Restore() is called.
  w.reset();

  // It should clear all child windows in |container| when the pinned window is
  // destroyed.
  EXPECT_EQ(container->children().size(), 0u);

  // Add a sibling window. It should not crash.
  CreateTestWindowInShellWithId(2);
}

TEST_F(ScreenPinningControllerTest, AllowWindowOnTopOfPinnedWindowForOnTask) {
  aura::Window* const w1 = CreateTestWindowInShellWithId(0);
  aura::Window* const w2 = CreateTestWindowInShellWithId(1);
  wm::ActivateWindow(w1);

  window_util::PinWindow(w1, /*trusted=*/false);
  EXPECT_TRUE(WindowState::Get(w1)->IsPinned());
  EXPECT_FALSE(WindowState::Get(w2)->IsPinned());
  Shell::Get()
      ->screen_pinning_controller()
      ->SetAllowWindowStackingWithPinnedWindow(true);
  aura::Window* const top_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_AlwaysOnTopContainer);
  top_container->StackChildAtTop(w2);
  EXPECT_TRUE(WindowState::Get(w1)->IsPinned());

  // Verify that w2 is in front of w1.
  aura::Window::Windows siblings = w2->parent()->children();
  int index1 = FindIndex(siblings, w1);
  int index2 = FindIndex(siblings, w2);
  EXPECT_NE(-1, index1);
  EXPECT_NE(-1, index2);
  EXPECT_GT(index1, index2);
}

}  // namespace ash
