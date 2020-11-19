// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle_controller.h"

#include <algorithm>
#include <memory>

#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/focus_cycler.h"
#include "ash/frame_throttler/mock_frame_throttling_observer.h"
#include "ash/home_screen/home_screen_controller.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_cycle_list.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/display/display_layout_builder.h"
#include "ui/display/manager/display_layout_store.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/scoped_display_for_new_windows.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/event_handler.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

namespace {

class EventCounter : public ui::EventHandler {
 public:
  EventCounter() : key_events_(0), mouse_events_(0) {}
  ~EventCounter() override = default;

  int GetKeyEventCountAndReset() {
    int count = key_events_;
    key_events_ = 0;
    return count;
  }

  int GetMouseEventCountAndReset() {
    int count = mouse_events_;
    mouse_events_ = 0;
    return count;
  }

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override { key_events_++; }
  void OnMouseEvent(ui::MouseEvent* event) override { mouse_events_++; }

 private:
  int key_events_;
  int mouse_events_;

  DISALLOW_COPY_AND_ASSIGN(EventCounter);
};

bool IsWindowMinimized(aura::Window* window) {
  return WindowState::Get(window)->IsMinimized();
}

}  // namespace

using aura::test::CreateTestWindowWithId;
using aura::test::TestWindowDelegate;
using aura::Window;

class WindowCycleControllerTest : public AshTestBase {
 public:
  WindowCycleControllerTest() = default;
  ~WindowCycleControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    WindowCycleList::DisableInitialDelayForTesting();

    shelf_view_test_.reset(
        new ShelfViewTestAPI(GetPrimaryShelf()->GetShelfViewForTesting()));
    shelf_view_test_->SetAnimationDuration(
        base::TimeDelta::FromMilliseconds(1));
  }

  const aura::Window::Windows GetWindows(WindowCycleController* controller) {
    return controller->window_cycle_list()->windows();
  }

  const views::Widget* GetWindowCycleListWidget() const {
    return Shell::Get()
        ->window_cycle_controller()
        ->window_cycle_list()
        ->widget();
  }

  const views::View::Views& GetWindowCycleItemViews() const {
    return Shell::Get()
        ->window_cycle_controller()
        ->window_cycle_list()
        ->GetWindowCycleItemViewsForTesting();
  }

  const aura::Window* GetTargetWindow() const {
    return Shell::Get()
        ->window_cycle_controller()
        ->window_cycle_list()
        ->GetTargetWindowForTesting();
  }

  bool CycleViewExists() const {
    return Shell::Get()
        ->window_cycle_controller()
        ->window_cycle_list()
        ->cycle_view_for_testing();
  }

  int GetCurrentIndex() const {
    return Shell::Get()
        ->window_cycle_controller()
        ->window_cycle_list()
        ->current_index_for_testing();
  }

 private:
  std::unique_ptr<ShelfViewTestAPI> shelf_view_test_;

  DISALLOW_COPY_AND_ASSIGN(WindowCycleControllerTest);
};

TEST_F(WindowCycleControllerTest, HandleCycleWindowBaseCases) {
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  // Cycling doesn't crash if there are no windows.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);

  // Create a single test window.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cycling works for a single window, even though nothing changes.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

// Verifies if there is only one window and it isn't active that cycling
// activates it.
TEST_F(WindowCycleControllerTest, SingleWindowNotActive) {
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  // Create a single test window.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Rotate focus, this should move focus to another window that isn't part of
  // the default container.
  Shell::Get()->focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_FALSE(wm::IsActiveWindow(window0.get()));

  // Cycling should activate the window.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

TEST_F(WindowCycleControllerTest, HandleCycleWindow) {
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  // Set up several windows to use to test cycling.  Create them in reverse
  // order so they are stacked 0 over 1 over 2.
  std::unique_ptr<Window> window2(CreateTestWindowInShellWithId(2));
  std::unique_ptr<Window> window1(CreateTestWindowInShellWithId(1));
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window0.get());

  // Simulate pressing and releasing Alt-tab.
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD);

  // Window lists should return the topmost window in front.
  ASSERT_TRUE(controller->window_cycle_list());
  ASSERT_EQ(3u, GetWindows(controller).size());
  ASSERT_EQ(window0.get(), GetWindows(controller)[0]);
  ASSERT_EQ(window1.get(), GetWindows(controller)[1]);
  ASSERT_EQ(window2.get(), GetWindows(controller)[2]);

  controller->CompleteCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  // Pressing and releasing Alt-tab again should cycle back to the most-
  // recently-used window in the current child order.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->CompleteCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cancelled cycling shouldn't move the active window.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->CancelCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Pressing Alt-tab multiple times without releasing Alt should cycle through
  // all the windows and wrap around.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(controller->IsCycling());

  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(controller->IsCycling());

  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(controller->IsCycling());

  controller->CompleteCycling();
  EXPECT_FALSE(controller->IsCycling());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Reset our stacking order.
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());
  wm::ActivateWindow(window0.get());

  // Likewise we can cycle backwards through the windows.
  controller->HandleCycleWindow(WindowCycleController::BACKWARD);
  controller->HandleCycleWindow(WindowCycleController::BACKWARD);
  controller->CompleteCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  // Reset our stacking order.
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());
  wm::ActivateWindow(window0.get());

  // When the screen is locked, cycling window does not take effect.
  GetSessionControllerClient()->LockScreen();
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_FALSE(controller->IsCycling());

  // Unlock, it works again.
  GetSessionControllerClient()->UnlockScreen();
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->CompleteCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  // When a modal window is active, cycling window does not take effect.
  aura::Window* modal_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_SystemModalContainer);
  std::unique_ptr<Window> modal_window(
      CreateTestWindowWithId(-2, modal_container));
  modal_window->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_SYSTEM);
  wm::ActivateWindow(modal_window.get());
  EXPECT_TRUE(wm::IsActiveWindow(modal_window.get()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(modal_window.get()));
  EXPECT_FALSE(controller->IsCycling());
  EXPECT_FALSE(wm::IsActiveWindow(window0.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window1.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window2.get()));
  controller->HandleCycleWindow(WindowCycleController::BACKWARD);
  EXPECT_TRUE(wm::IsActiveWindow(modal_window.get()));
  EXPECT_FALSE(controller->IsCycling());
  EXPECT_FALSE(wm::IsActiveWindow(window0.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window1.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window2.get()));

  modal_window.reset();
  std::unique_ptr<Window> skip_overview_window(
      CreateTestWindowInShellWithId(-3));
  skip_overview_window->SetProperty(kHideInOverviewKey, true);
  wm::ActivateWindow(window0.get());
  wm::ActivateWindow(skip_overview_window.get());
  wm::ActivateWindow(window1.get());
  EXPECT_FALSE(wm::IsActiveWindow(window0.get()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->CompleteCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  EXPECT_FALSE(wm::IsActiveWindow(skip_overview_window.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window1.get()));
}

TEST_F(WindowCycleControllerTest, Scroll) {
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  // Doesn't crash if there are no windows.
  controller->Scroll(WindowCycleController::FORWARD);

  // Create test windows.
  std::unique_ptr<Window> w5 = CreateTestWindow(gfx::Rect(0, 0, 200, 200));
  std::unique_ptr<Window> w4 = CreateTestWindow(gfx::Rect(0, 0, 200, 200));
  std::unique_ptr<Window> w3 = CreateTestWindow(gfx::Rect(0, 0, 200, 200));
  std::unique_ptr<Window> w2 = CreateTestWindow(gfx::Rect(0, 0, 200, 200));
  std::unique_ptr<Window> w1 = CreateTestWindow(gfx::Rect(0, 0, 200, 200));
  std::unique_ptr<Window> w0 = CreateTestWindow(gfx::Rect(0, 0, 200, 200));

  auto ScrollAndReturnCurrentIndex =
      [this](WindowCycleController::Direction direction, int num_of_scrolls) {
        WindowCycleController* controller =
            Shell::Get()->window_cycle_controller();
        for (int i = 0; i < num_of_scrolls; i++)
          controller->Scroll(direction);

        return GetCurrentIndex();
      };

  auto GetXOfCycleListCenterPoint = [this]() {
    return GetWindowCycleListWidget()
        ->GetWindowBoundsInScreen()
        .CenterPoint()
        .x();
  };

  auto GetXOfWindowCycleItemViewCenterPoint = [this](int index) {
    return GetWindowCycleItemViews()[index]
        ->GetBoundsInScreen()
        .CenterPoint()
        .x();
  };

  // Start cycling and scroll forward. The list should be not be centered around
  // w1. Since w1 is so close to the beginning of the list.
  controller->StartCycling();
  int current_index =
      ScrollAndReturnCurrentIndex(WindowCycleController::FORWARD, 1);
  EXPECT_EQ(1, current_index);
  EXPECT_GT(GetXOfCycleListCenterPoint(),
            GetXOfWindowCycleItemViewCenterPoint(current_index));

  // Scroll forward twice. The list should be centered around w3.
  current_index =
      ScrollAndReturnCurrentIndex(WindowCycleController::FORWARD, 2);
  EXPECT_EQ(3, current_index);
  EXPECT_EQ(GetXOfCycleListCenterPoint(),
            GetXOfWindowCycleItemViewCenterPoint(current_index));

  // Scroll backward once. The list should be centered around w2.
  current_index =
      ScrollAndReturnCurrentIndex(WindowCycleController::BACKWARD, 1);
  EXPECT_EQ(2, current_index);
  EXPECT_EQ(GetXOfCycleListCenterPoint(),
            GetXOfWindowCycleItemViewCenterPoint(current_index));

  // Scroll backward three times. The list should not be centered around w5.
  current_index =
      ScrollAndReturnCurrentIndex(WindowCycleController::BACKWARD, 3);
  EXPECT_EQ(5, current_index);
  EXPECT_LT(GetXOfCycleListCenterPoint(),
            GetXOfWindowCycleItemViewCenterPoint(current_index));

  // Cycle forward. Since the target window != current window, it should scroll
  // to target window then cycle. The target_window was w0 prior to cycling.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  current_index = GetCurrentIndex();
  EXPECT_EQ(1, current_index);
  EXPECT_GT(GetXOfCycleListCenterPoint(),
            GetXOfWindowCycleItemViewCenterPoint(current_index));
  controller->CompleteCycling();
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));

  // Start cycling, scroll backward once and complete cycling. Scroll should not
  // affect the selected window.
  controller->StartCycling();
  current_index =
      ScrollAndReturnCurrentIndex(WindowCycleController::BACKWARD, 1);
  EXPECT_EQ(5, current_index);
  controller->CompleteCycling();
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
}

// Cycles between a maximized and normal window.
TEST_F(WindowCycleControllerTest, MaximizedWindow) {
  // Create a couple of test windows.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<Window> window1(CreateTestWindowInShellWithId(1));
  WindowState* window1_state = WindowState::Get(window1.get());
  window1_state->Maximize();
  window1_state->Activate();
  EXPECT_TRUE(window1_state->IsActive());

  // Rotate focus, this should move focus to window0.
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->CompleteCycling();
  EXPECT_TRUE(WindowState::Get(window0.get())->IsActive());
  EXPECT_FALSE(window1_state->IsActive());

  // One more time.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->CompleteCycling();
  EXPECT_TRUE(window1_state->IsActive());
}

// Cycles to a minimized window.
TEST_F(WindowCycleControllerTest, Minimized) {
  // Create a couple of test windows.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<Window> window1(CreateTestWindowInShellWithId(1));
  WindowState* window0_state = WindowState::Get(window0.get());
  WindowState* window1_state = WindowState::Get(window1.get());

  window1_state->Minimize();
  window0_state->Activate();
  EXPECT_TRUE(window0_state->IsActive());

  // Rotate focus, this should move focus to window1 and unminimize it.
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->CompleteCycling();
  EXPECT_FALSE(window0_state->IsActive());
  EXPECT_FALSE(window1_state->IsMinimized());
  EXPECT_TRUE(window1_state->IsActive());

  // One more time back to w0.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->CompleteCycling();
  EXPECT_TRUE(window0_state->IsActive());
}

// Tests that when all windows are minimized, cycling starts with the first one
// rather than the second.
TEST_F(WindowCycleControllerTest, AllAreMinimized) {
  // Create a couple of test windows.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<Window> window1(CreateTestWindowInShellWithId(1));
  WindowState* window0_state = WindowState::Get(window0.get());
  WindowState* window1_state = WindowState::Get(window1.get());

  window0_state->Minimize();
  window1_state->Minimize();

  WindowCycleController* controller = Shell::Get()->window_cycle_controller();
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->CompleteCycling();
  EXPECT_TRUE(window0_state->IsActive());
  EXPECT_FALSE(window0_state->IsMinimized());
  EXPECT_TRUE(window1_state->IsMinimized());

  // But it's business as usual when cycling backwards.
  window0_state->Minimize();
  window1_state->Minimize();
  controller->HandleCycleWindow(WindowCycleController::BACKWARD);
  controller->CompleteCycling();
  EXPECT_TRUE(window0_state->IsMinimized());
  EXPECT_TRUE(window1_state->IsActive());
  EXPECT_FALSE(window1_state->IsMinimized());
}

TEST_F(WindowCycleControllerTest, AlwaysOnTopWindow) {
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  // Set up several windows to use to test cycling.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<Window> window1(CreateTestWindowInShellWithId(1));

  Window* top_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_AlwaysOnTopContainer);
  std::unique_ptr<Window> window2(CreateTestWindowWithId(2, top_container));
  wm::ActivateWindow(window0.get());

  // Simulate pressing and releasing Alt-tab.
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD);

  // Window lists should return the topmost window in front.
  ASSERT_TRUE(controller->window_cycle_list());
  ASSERT_EQ(3u, GetWindows(controller).size());
  EXPECT_EQ(window0.get(), GetWindows(controller)[0]);
  EXPECT_EQ(window2.get(), GetWindows(controller)[1]);
  EXPECT_EQ(window1.get(), GetWindows(controller)[2]);

  controller->CompleteCycling();
}

TEST_F(WindowCycleControllerTest, AlwaysOnTopMultiWindow) {
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  // Set up several windows to use to test cycling.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<Window> window1(CreateTestWindowInShellWithId(1));

  Window* top_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_AlwaysOnTopContainer);
  std::unique_ptr<Window> window2(CreateTestWindowWithId(2, top_container));
  std::unique_ptr<Window> window3(CreateTestWindowWithId(3, top_container));
  wm::ActivateWindow(window0.get());

  // Simulate pressing and releasing Alt-tab.
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD);

  // Window lists should return the topmost window in front.
  ASSERT_TRUE(controller->window_cycle_list());
  ASSERT_EQ(4u, GetWindows(controller).size());
  EXPECT_EQ(window0.get(), GetWindows(controller)[0]);
  EXPECT_EQ(window3.get(), GetWindows(controller)[1]);
  EXPECT_EQ(window2.get(), GetWindows(controller)[2]);
  EXPECT_EQ(window1.get(), GetWindows(controller)[3]);

  controller->CompleteCycling();
}

TEST_F(WindowCycleControllerTest, AlwaysOnTopMultipleRootWindows) {
  // Set up a second root window
  UpdateDisplay("1000x600,600x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());

  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  // Create two windows in the primary root.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  EXPECT_EQ(root_windows[0], window0->GetRootWindow());
  Window* top_container0 =
      Shell::GetContainer(root_windows[0], kShellWindowId_AlwaysOnTopContainer);
  std::unique_ptr<Window> window1(CreateTestWindowWithId(1, top_container0));
  EXPECT_EQ(root_windows[0], window1->GetRootWindow());

  // Move the active root window to the secondary root and create two windows.
  display::ScopedDisplayForNewWindows display_for_new_windows(root_windows[1]);
  std::unique_ptr<Window> window2(CreateTestWindowInShellWithId(2));
  EXPECT_EQ(root_windows[1], window2->GetRootWindow());

  Window* top_container1 =
      Shell::GetContainer(root_windows[1], kShellWindowId_AlwaysOnTopContainer);
  std::unique_ptr<Window> window3(CreateTestWindowWithId(3, top_container1));
  EXPECT_EQ(root_windows[1], window3->GetRootWindow());

  wm::ActivateWindow(window2.get());

  EXPECT_EQ(root_windows[0], window0->GetRootWindow());
  EXPECT_EQ(root_windows[0], window1->GetRootWindow());
  EXPECT_EQ(root_windows[1], window2->GetRootWindow());
  EXPECT_EQ(root_windows[1], window3->GetRootWindow());

  // Simulate pressing and releasing Alt-tab.
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD);

  // Window lists should return the topmost window in front.
  ASSERT_TRUE(controller->window_cycle_list());
  ASSERT_EQ(4u, GetWindows(controller).size());
  EXPECT_EQ(window2.get(), GetWindows(controller)[0]);
  EXPECT_EQ(window3.get(), GetWindows(controller)[1]);
  EXPECT_EQ(window1.get(), GetWindows(controller)[2]);
  EXPECT_EQ(window0.get(), GetWindows(controller)[3]);

  controller->CompleteCycling();
}

TEST_F(WindowCycleControllerTest, MostRecentlyUsed) {
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  // Set up several windows to use to test cycling.
  std::unique_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<Window> window1(CreateTestWindowInShellWithId(1));
  std::unique_ptr<Window> window2(CreateTestWindowInShellWithId(2));

  wm::ActivateWindow(window0.get());

  // Simulate pressing and releasing Alt-tab.
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  controller->HandleCycleWindow(WindowCycleController::FORWARD);

  // Window lists should return the topmost window in front.
  ASSERT_TRUE(controller->window_cycle_list());
  ASSERT_EQ(3u, GetWindows(controller).size());
  EXPECT_EQ(window0.get(), GetWindows(controller)[0]);
  EXPECT_EQ(window2.get(), GetWindows(controller)[1]);
  EXPECT_EQ(window1.get(), GetWindows(controller)[2]);

  // Cycling through then stopping the cycling will activate a window.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->CompleteCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  // Cycling alone (without CompleteCycling()) doesn't activate.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_FALSE(wm::IsActiveWindow(window0.get()));

  controller->CompleteCycling();
}

// Tests that beginning window selection hides the app list.
TEST_F(WindowCycleControllerTest, SelectingHidesAppList) {
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  std::unique_ptr<aura::Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(1));
  wm::ActivateWindow(window0.get());

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplay().id());
  GetAppListTestHelper()->CheckVisibility(true);
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Make sure that dismissing the app list this way doesn't pass activation
  // to a different window.
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window1.get()));

  controller->CompleteCycling();
}

// Tests that beginning window selection doesn't hide the app list in tablet
// mode.
TEST_F(WindowCycleControllerTest, SelectingDoesNotHideAppListInTabletMode) {
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(TabletModeControllerTestApi().IsTabletModeStarted());
  EXPECT_TRUE(Shell::Get()->home_screen_controller()->IsHomeScreenVisible());

  std::unique_ptr<aura::Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(1));
  wm::ActivateWindow(window0.get());

  WindowCycleController* controller = Shell::Get()->window_cycle_controller();
  controller->HandleCycleWindow(WindowCycleController::FORWARD);

  window0->Hide();
  window1->Hide();
  EXPECT_TRUE(Shell::Get()->home_screen_controller()->IsHomeScreenVisible());
}

// Tests that cycling through windows doesn't change their minimized state.
TEST_F(WindowCycleControllerTest, CyclePreservesMinimization) {
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  std::unique_ptr<aura::Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(1));
  wm::ActivateWindow(window1.get());
  WindowState::Get(window1.get())->Minimize();
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(IsWindowMinimized(window1.get()));

  // On window 2.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(IsWindowMinimized(window1.get()));

  // Back on window 1.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(IsWindowMinimized(window1.get()));

  controller->CompleteCycling();

  EXPECT_TRUE(IsWindowMinimized(window1.get()));
}

// Tests that the tab key events are not sent to the window.
TEST_F(WindowCycleControllerTest, TabKeyNotLeaked) {
  std::unique_ptr<Window> w0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<Window> w1(CreateTestWindowInShellWithId(1));
  EventCounter event_count;
  w0->AddPreTargetHandler(&event_count);
  w1->AddPreTargetHandler(&event_count);
  ui::test::EventGenerator* generator = GetEventGenerator();
  WindowState::Get(w0.get())->Activate();
  generator->PressKey(ui::VKEY_MENU, ui::EF_NONE);
  EXPECT_EQ(1, event_count.GetKeyEventCountAndReset());
  generator->PressKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);
  EXPECT_EQ(0, event_count.GetKeyEventCountAndReset());
  generator->ReleaseKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);
  EXPECT_EQ(0, event_count.GetKeyEventCountAndReset());
  generator->ReleaseKey(ui::VKEY_MENU, ui::EF_NONE);
  EXPECT_TRUE(WindowState::Get(w1.get())->IsActive());
  EXPECT_EQ(0, event_count.GetKeyEventCountAndReset());
}

// While the UI is active, mouse events are captured.
TEST_F(WindowCycleControllerTest, MouseEventsCaptured) {
  if (features::IsInteractiveWindowCycleListEnabled())
    return;

  // Set up a second root window
  UpdateDisplay("1000x600,600x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());

  // This delegate allows the window to receive mouse events.
  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<Window> w0(CreateTestWindowInShellWithDelegate(
      &delegate, 0, gfx::Rect(0, 0, 100, 100)));
  std::unique_ptr<Window> w1(CreateTestWindowInShellWithId(1));
  EventCounter event_count;
  w0->AddPreTargetHandler(&event_count);
  w1->SetTargetHandler(&event_count);
  ui::test::EventGenerator* generator = GetEventGenerator();
  wm::ActivateWindow(w0.get());

  // Events get through while not cycling.
  generator->MoveMouseToCenterOf(w0.get());
  generator->ClickLeftButton();
  EXPECT_LT(0, event_count.GetMouseEventCountAndReset());

  // Start cycling.
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();
  controller->HandleCycleWindow(WindowCycleController::FORWARD);

  // Mouse events not over the cycle view don't get through.
  generator->PressLeftButton();
  EXPECT_EQ(0, event_count.GetMouseEventCountAndReset());

  // Although releases do, regardless of mouse position.
  generator->ReleaseLeftButton();
  EXPECT_LT(0, event_count.GetMouseEventCountAndReset());

  // Stop cycling: once again, events get through.
  controller->CompleteCycling();
  generator->ClickLeftButton();
  EXPECT_LT(0, event_count.GetMouseEventCountAndReset());

  // Click somewhere on the second root window.
  generator->MoveMouseToCenterOf(root_windows[1]);
  generator->ClickLeftButton();
  EXPECT_EQ(0, event_count.GetMouseEventCountAndReset());
}

// Tests that we can cycle past fullscreen windows: https://crbug.com/622396.
// Fullscreen windows are special in that they are allowed to handle alt+tab
// keypresses, which means the window cycle event filter should not handle
// the tab press else it prevents cycling past that window.
TEST_F(WindowCycleControllerTest, TabPastFullscreenWindow) {
  std::unique_ptr<Window> w0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<Window> w1(CreateTestWindowInShellWithId(1));
  WMEvent maximize_event(WM_EVENT_FULLSCREEN);

  // To make this test work with or without the new alt+tab selector we make
  // both the initial window and the second window fullscreen.
  WindowState::Get(w0.get())->OnWMEvent(&maximize_event);
  WindowState::Get(w1.get())->Activate();
  WindowState::Get(w1.get())->OnWMEvent(&maximize_event);
  EXPECT_TRUE(WindowState::Get(w0.get())->IsFullscreen());
  EXPECT_TRUE(WindowState::Get(w1.get())->IsFullscreen());
  WindowState::Get(w0.get())->Activate();
  EXPECT_TRUE(WindowState::Get(w0.get())->IsActive());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::VKEY_MENU, ui::EF_NONE);

  generator->PressKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);
  generator->ReleaseKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);

  // Because w0 and w1 are full-screen, the event should be passed to the
  // browser window to handle it (which if the browser doesn't handle it will
  // pass on the alt+tab to continue cycling). To make this test work with or
  // without the new alt+tab selector we check for the event on either
  // fullscreen window.
  EventCounter event_count;
  w0->AddPreTargetHandler(&event_count);
  w1->AddPreTargetHandler(&event_count);
  generator->PressKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);
  EXPECT_EQ(1, event_count.GetKeyEventCountAndReset());
}

// Tests that the Alt+Tab UI's position isn't affected by the origin of the
// display it's on. See crbug.com/675718
TEST_F(WindowCycleControllerTest, MultiDisplayPositioning) {
  int64_t primary_id = GetPrimaryDisplay().id();
  display::DisplayIdList list =
      display::test::CreateDisplayIdListN(primary_id, 2);

  auto placements = {
      display::DisplayPlacement::BOTTOM, display::DisplayPlacement::TOP,
      display::DisplayPlacement::LEFT, display::DisplayPlacement::RIGHT,
  };

  gfx::Rect expected_bounds;
  for (auto placement : placements) {
    SCOPED_TRACE(placement);

    display::DisplayLayoutBuilder builder(primary_id);
    builder.AddDisplayPlacement(list[1], primary_id, placement, 0);
    display_manager()->layout_store()->RegisterLayoutForDisplayIdList(
        list, builder.Build());

    // Use two displays.
    UpdateDisplay("500x500,600x600");

    gfx::Rect second_display_bounds =
        display_manager()->GetDisplayAt(1).bounds();
    std::unique_ptr<Window> window0(
        CreateTestWindowInShellWithBounds(second_display_bounds));
    // Activate this window so that the secondary display becomes the one where
    // the Alt+Tab UI is shown.
    wm::ActivateWindow(window0.get());
    std::unique_ptr<Window> window1(
        CreateTestWindowInShellWithBounds(second_display_bounds));

    WindowCycleController* controller = Shell::Get()->window_cycle_controller();
    controller->HandleCycleWindow(WindowCycleController::FORWARD);

    const gfx::Rect bounds =
        GetWindowCycleListWidget()->GetWindowBoundsInScreen();
    EXPECT_TRUE(second_display_bounds.Contains(bounds));
    EXPECT_FALSE(
        display_manager()->GetDisplayAt(0).bounds().Intersects(bounds));
    const gfx::Rect display_relative_bounds =
        bounds - second_display_bounds.OffsetFromOrigin();
    // Base case sets the expectation for other cases.
    if (expected_bounds.IsEmpty())
      expected_bounds = display_relative_bounds;
    else
      EXPECT_EQ(expected_bounds, display_relative_bounds);
    controller->CompleteCycling();
  }
}

TEST_F(WindowCycleControllerTest, CycleShowsAllDesksWindows) {
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(3u, desks_controller->desks().size());
  const Desk* desk_2 = desks_controller->desks()[1].get();
  ActivateDesk(desk_2);
  EXPECT_EQ(desk_2, desks_controller->active_desk());
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 300, 200));
  const Desk* desk_3 = desks_controller->desks()[2].get();
  ActivateDesk(desk_3);
  EXPECT_EQ(desk_3, desks_controller->active_desk());
  auto win3 = CreateAppWindow(gfx::Rect(10, 30, 400, 200));

  WindowCycleController* cycle_controller =
      Shell::Get()->window_cycle_controller();
  cycle_controller->HandleCycleWindow(WindowCycleController::FORWARD);
  // All desks' windows are included in the cycle list.
  auto cycle_windows = GetWindows(cycle_controller);
  EXPECT_EQ(4u, cycle_windows.size());
  EXPECT_TRUE(base::Contains(cycle_windows, win0.get()));
  EXPECT_TRUE(base::Contains(cycle_windows, win1.get()));
  EXPECT_TRUE(base::Contains(cycle_windows, win2.get()));
  EXPECT_TRUE(base::Contains(cycle_windows, win3.get()));

  // The MRU order is {win3, win2, win1, win0}. We're now at win2. Cycling one
  // more time and completing the cycle, will activate win1 which exists on a
  // desk_1. This should activate desk_1.
  {
    base::HistogramTester histogram_tester;
    DeskSwitchAnimationWaiter waiter;
    cycle_controller->HandleCycleWindow(WindowCycleController::FORWARD);
    cycle_controller->CompleteCycling();
    waiter.Wait();
    Desk* desk_1 = desks_controller->desks()[0].get();
    EXPECT_EQ(desk_1, desks_controller->active_desk());
    EXPECT_EQ(win1.get(), window_util::GetActiveWindow());
    histogram_tester.ExpectUniqueSample(
        "Ash.WindowCycleController.DesksSwitchDistance",
        /*desk distance of 3 - 1 = */ 2, /*expected_count=*/1);
  }

  // Cycle again and activate win2, which exist on desk_2. Expect that desk to
  // be activated, and a histogram sample of distance of 1 is recorded.
  // MRU is {win1, win3, win2, win0}.
  {
    base::HistogramTester histogram_tester;
    DeskSwitchAnimationWaiter waiter;
    cycle_controller->HandleCycleWindow(WindowCycleController::FORWARD);
    cycle_controller->HandleCycleWindow(WindowCycleController::FORWARD);
    cycle_controller->CompleteCycling();
    waiter.Wait();
    EXPECT_EQ(desk_2, desks_controller->active_desk());
    EXPECT_EQ(win2.get(), window_util::GetActiveWindow());
    histogram_tester.ExpectUniqueSample(
        "Ash.WindowCycleController.DesksSwitchDistance",
        /*desk distance of 2 - 1 = */ 1, /*expected_count=*/1);
  }
}

class LimitedWindowCycleControllerTest : public WindowCycleControllerTest {
 public:
  LimitedWindowCycleControllerTest() = default;
  LimitedWindowCycleControllerTest(const LimitedWindowCycleControllerTest&) =
      delete;
  LimitedWindowCycleControllerTest& operator=(
      const LimitedWindowCycleControllerTest&) = delete;
  ~LimitedWindowCycleControllerTest() override = default;

  // WindowCycleControllerTest:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kLimitAltTabToActiveDesk);
    WindowCycleControllerTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(LimitedWindowCycleControllerTest, CycleShowsActiveDeskWindows) {
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(3u, desks_controller->desks().size());
  const Desk* desk_2 = desks_controller->desks()[1].get();
  ActivateDesk(desk_2);
  EXPECT_EQ(desk_2, desks_controller->active_desk());
  auto win2 = CreateAppWindow(gfx::Rect(0, 0, 300, 200));
  const Desk* desk_3 = desks_controller->desks()[2].get();
  ActivateDesk(desk_3);
  EXPECT_EQ(desk_3, desks_controller->active_desk());
  auto win3 = CreateAppWindow(gfx::Rect(10, 30, 400, 200));

  WindowCycleController* cycle_controller =
      Shell::Get()->window_cycle_controller();

  // Should contain only windows from |desk_3|.
  cycle_controller->HandleCycleWindow(WindowCycleController::FORWARD);
  auto cycle_windows = GetWindows(cycle_controller);
  EXPECT_EQ(1u, cycle_windows.size());
  EXPECT_TRUE(base::Contains(cycle_windows, win3.get()));
  cycle_controller->CompleteCycling();
  EXPECT_EQ(win3.get(), window_util::GetActiveWindow());

  // Should contain only windows from |desk_2|.
  ActivateDesk(desk_2);
  cycle_controller->HandleCycleWindow(WindowCycleController::FORWARD);
  cycle_windows = GetWindows(cycle_controller);
  EXPECT_EQ(1u, cycle_windows.size());
  EXPECT_TRUE(base::Contains(cycle_windows, win2.get()));
  cycle_controller->CompleteCycling();
  EXPECT_EQ(win2.get(), window_util::GetActiveWindow());

  // Should contain only windows from |desk_1|.
  const Desk* desk_1 = desks_controller->desks()[0].get();
  ActivateDesk(desk_1);
  cycle_controller->HandleCycleWindow(WindowCycleController::FORWARD);
  cycle_windows = GetWindows(cycle_controller);
  EXPECT_EQ(2u, cycle_windows.size());
  EXPECT_TRUE(base::Contains(cycle_windows, win0.get()));
  EXPECT_TRUE(base::Contains(cycle_windows, win1.get()));
  cycle_controller->CompleteCycling();
  EXPECT_EQ(win0.get(), window_util::GetActiveWindow());

  // Swap desks while cycling, contents should update.
  cycle_controller->HandleCycleWindow(WindowCycleController::FORWARD);
  cycle_windows = GetWindows(cycle_controller);
  EXPECT_EQ(2u, cycle_windows.size());
  EXPECT_TRUE(base::Contains(cycle_windows, win0.get()));
  EXPECT_TRUE(base::Contains(cycle_windows, win1.get()));
  ActivateDesk(desk_2);
  EXPECT_TRUE(cycle_controller->IsCycling());
  cycle_windows = GetWindows(cycle_controller);
  EXPECT_EQ(1u, cycle_windows.size());
  EXPECT_TRUE(base::Contains(cycle_windows, win2.get()));
  cycle_controller->CompleteCycling();
  EXPECT_EQ(win2.get(), window_util::GetActiveWindow());
}

class InteractiveWindowCycleControllerTest : public WindowCycleControllerTest {
 public:
  InteractiveWindowCycleControllerTest() = default;
  InteractiveWindowCycleControllerTest(const InteractiveWindowCycleControllerTest&) =
      delete;
  InteractiveWindowCycleControllerTest& operator=(
      const InteractiveWindowCycleControllerTest&) = delete;
  ~InteractiveWindowCycleControllerTest() override = default;

  // WindowCycleControllerTest:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kInteractiveWindowCycleList);
    WindowCycleControllerTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that when the cycle view is not open, the event filter does not check
// whether events occur within the cycle view.
// TODO(chinsenj): Add this to WindowCycleControllerTest.MouseEventsCaptured
// after feature launch.
TEST_F(InteractiveWindowCycleControllerTest,
       MouseEventWhenCycleViewDoesNotExist) {
  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<Window> w0(CreateTestWindowInShellWithDelegate(
      &delegate, 0, gfx::Rect(0, 0, 100, 100)));
  EventCounter event_count;
  w0->AddPreTargetHandler(&event_count);
  ui::test::EventGenerator* generator = GetEventGenerator();
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  // Mouse events get through if the cycle view is not open.
  // Cycling with one window open ensures the UI doesn't show but the event
  // filter is.
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  generator->MoveMouseToCenterOf(w0.get());
  generator->ClickLeftButton();
  EXPECT_TRUE(controller->IsCycling());
  EXPECT_FALSE(CycleViewExists());
  EXPECT_LT(0, event_count.GetMouseEventCountAndReset());
  controller->CompleteCycling();
}

// When a user hovers their mouse over an item, it should cycle to it.
// The items in the list should not move, only the focus ring.
// If a user clicks on an item, it should complete cycling and activate
// the hovered item.
TEST_F(InteractiveWindowCycleControllerTest, MouseHoverAndSelect) {
  std::unique_ptr<Window> w0 = CreateTestWindow();
  std::unique_ptr<Window> w1 = CreateTestWindow();
  std::unique_ptr<Window> w2 = CreateTestWindow();
  std::unique_ptr<Window> w3 = CreateTestWindow();
  std::unique_ptr<Window> w4 = CreateTestWindow();
  std::unique_ptr<Window> w5 = CreateTestWindow();
  std::unique_ptr<Window> w6 = CreateTestWindow();
  ui::test::EventGenerator* generator = GetEventGenerator();
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  // Cycle to the third item, mouse over second item, and release alt-tab.
  // Starting order of windows in cycle list is [6,5,4,3,2,1,0].
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  gfx::Point target_item_center =
      GetWindowCycleItemViews()[1]->GetBoundsInScreen().CenterPoint();
  generator->MoveMouseTo(target_item_center);
  EXPECT_EQ(target_item_center,
            GetWindowCycleItemViews()[1]->GetBoundsInScreen().CenterPoint());
  controller->CompleteCycling();
  EXPECT_TRUE(wm::IsActiveWindow(w5.get()));

  // Start cycle, mouse over third item, and release alt-tab.
  // Starting order of windows in cycle list is [5,6,4,3,2,1,0].
  controller->StartCycling();
  target_item_center =
      GetWindowCycleItemViews()[2]->GetBoundsInScreen().CenterPoint();
  generator->MoveMouseTo(target_item_center);
  EXPECT_EQ(target_item_center,
            GetWindowCycleItemViews()[2]->GetBoundsInScreen().CenterPoint());
  controller->CompleteCycling();
  EXPECT_TRUE(wm::IsActiveWindow(w4.get()));

  // Start cycle, cycle to the fifth item, mouse over seventh item, and click.
  // Starting order of windows in cycle list is [4,5,6,3,2,1,0].
  controller->StartCycling();
  for (int i = 0; i < 5; i++)
    controller->HandleCycleWindow(WindowCycleController::FORWARD);
  target_item_center =
      GetWindowCycleItemViews()[6]->GetBoundsInScreen().CenterPoint();
  generator->MoveMouseTo(target_item_center);
  EXPECT_EQ(target_item_center,
            GetWindowCycleItemViews()[6]->GetBoundsInScreen().CenterPoint());
  generator->PressLeftButton();
  EXPECT_TRUE(wm::IsActiveWindow(w0.get()));
}

// Tests that the left and right keys cycle after the cycle list has been
// initialized.
TEST_F(InteractiveWindowCycleControllerTest, LeftRightCycle) {
  std::unique_ptr<Window> w0 = CreateTestWindow();
  std::unique_ptr<Window> w1 = CreateTestWindow();
  std::unique_ptr<Window> w2 = CreateTestWindow();
  ui::test::EventGenerator* generator = GetEventGenerator();
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  // Start cycle, simulating alt button being held down. Cycle right to the
  // third item.
  // Starting order of windows in cycle list is [2,1,0].
  controller->StartCycling();
  generator->PressKey(ui::VKEY_RIGHT, ui::EF_NONE);
  generator->PressKey(ui::VKEY_RIGHT, ui::EF_NONE);
  controller->CompleteCycling();
  EXPECT_TRUE(wm::IsActiveWindow(w0.get()));

  // Start cycle. Cycle right once, then left two times.
  // Starting order of windows in cycle list is [0,2,1].
  controller->StartCycling();
  generator->PressKey(ui::VKEY_RIGHT, ui::EF_NONE);
  generator->PressKey(ui::VKEY_LEFT, ui::EF_NONE);
  generator->PressKey(ui::VKEY_LEFT, ui::EF_NONE);
  controller->CompleteCycling();
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));

  // Start cycle. Cycle right once, then left once, then right once.
  // Starting order of windows in cycle list is [0,2,1].
  controller->StartCycling();
  generator->PressKey(ui::VKEY_LEFT, ui::EF_ALT_DOWN);
  generator->PressKey(ui::VKEY_RIGHT, ui::EF_ALT_DOWN);
  generator->PressKey(ui::VKEY_LEFT, ui::EF_ALT_DOWN);
  controller->CompleteCycling();
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));
}

// Tests that pressing the space key, pressing the enter key, or releasing the
// alt key during window cycle confirms a selection.
TEST_F(InteractiveWindowCycleControllerTest, KeysConfirmSelection) {
  std::unique_ptr<Window> w0 = CreateTestWindow();
  std::unique_ptr<Window> w1 = CreateTestWindow();
  std::unique_ptr<Window> w2 = CreateTestWindow();
  ui::test::EventGenerator* generator = GetEventGenerator();
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  // Start cycle, simulating alt button being held down. Cycle right once and
  // complete cycle using space.
  // Starting order of windows in cycle list is [2,1,0].
  controller->StartCycling();
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  generator->PressKey(ui::VKEY_SPACE, ui::EF_NONE);
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));

  // Start cycle, simulating alt button being held down. Cycle right once and
  // complete cycle using enter.
  // Starting order of windows in cycle list is [1,2,0].
  controller->StartCycling();
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  generator->PressKey(ui::VKEY_RETURN, ui::EF_NONE);
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));

  // Start cycle, simulating alt button being held down. Cycle right once and
  // complete cycle by releasing alt key (Views uses VKEY_MENU for both left and
  // right alt keys).
  // Starting order of windows in cycle list is [2,1,0].
  controller->StartCycling();
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  generator->ReleaseKey(ui::VKEY_MENU, ui::EF_NONE);
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
}

// When a user taps on an item, it should set the focus ring to that item.
TEST_F(InteractiveWindowCycleControllerTest, TapSelect) {
  std::unique_ptr<Window> w0 = CreateTestWindow();
  std::unique_ptr<Window> w1 = CreateTestWindow();
  std::unique_ptr<Window> w2 = CreateTestWindow();
  ui::test::EventGenerator* generator = GetEventGenerator();
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  // Start cycle and tap third item.
  // Starting order of windows in cycle list is [2,1,0].
  controller->StartCycling();
  generator->GestureTapAt(
      GetWindowCycleItemViews()[2]->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(controller->IsCycling());
  EXPECT_EQ(GetTargetWindow(), w0.get());

  // Start cycle and tap second item.
  // Starting order of windows in cycle list is [2,1,0].
  controller->StartCycling();
  generator->GestureTapAt(
      GetWindowCycleItemViews()[1]->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(controller->IsCycling());
  EXPECT_EQ(GetTargetWindow(), w1.get());
  controller->CompleteCycling();
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
}

// Tests that mouse events are filtered until the mouse is actually used,
// preventing the mouse from unexpectedly triggering events.
// See crbug.com/1143275.
TEST_F(InteractiveWindowCycleControllerTest, FilterMouseEventsUntilUsed) {
  std::unique_ptr<Window> w0 = CreateTestWindow();
  std::unique_ptr<Window> w1 = CreateTestWindow();
  std::unique_ptr<Window> w2 = CreateTestWindow();
  EventCounter event_count;
  ui::test::EventGenerator* generator = GetEventGenerator();
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  // Start cycling.
  // Current window order is [2,1,0].
  controller->StartCycling();
  auto item_views = GetWindowCycleItemViews();
  item_views[2]->AddPreTargetHandler(&event_count);

  // Move the mouse over to the third item and complete cycling. These mouse
  // events shouldn't be filtered since the user has moved their mouse.
  generator->MoveMouseTo(gfx::Point(0, 0));
  const gfx::Point third_item_center =
      GetWindowCycleItemViews()[2]->GetBoundsInScreen().CenterPoint();
  generator->MoveMouseTo(third_item_center);
  controller->CompleteCycling();
  EXPECT_TRUE(wm::IsActiveWindow(w0.get()));
  EXPECT_LT(0, event_count.GetMouseEventCountAndReset());

  // Start cycling again while the mouse is over where the third item will be
  // when cycling starts.
  // Current window order is [0,2,1].
  controller->StartCycling();
  item_views = GetWindowCycleItemViews();
  item_views[2]->AddPreTargetHandler(&event_count);

  // Generate mouse events at the cursor's initial position. These mouse events
  // should be filtered because the user hasn't moved their mouse yet.
  generator->MoveMouseTo(third_item_center);
  controller->CompleteCycling();
  EXPECT_TRUE(wm::IsActiveWindow(w0.get()));
  EXPECT_EQ(0, event_count.GetMouseEventCountAndReset());

  // Start cycling again and click. This should not be filtered out.
  // Current window order is [0,2,1].
  controller->StartCycling();
  generator->PressLeftButton();
  EXPECT_FALSE(controller->IsCycling());
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
}

// When a user has the window cycle list open and clicks outside of it, it
// should cancel cycling.
TEST_F(InteractiveWindowCycleControllerTest,
       MousePressOutsideOfListCancelsCycling) {
  std::unique_ptr<Window> w0 = CreateTestWindow();
  std::unique_ptr<Window> w1 = CreateTestWindow();
  std::unique_ptr<Window> w2 = CreateTestWindow();
  ui::test::EventGenerator* generator = GetEventGenerator();
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();

  // Cycle to second item, move to above the window cycle list, and click.
  controller->StartCycling();
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  gfx::Point above_window_cycle_list =
      GetWindowCycleListWidget()->GetWindowBoundsInScreen().top_center();
  above_window_cycle_list.Offset(0, 100);
  generator->MoveMouseTo(above_window_cycle_list);
  generator->ClickLeftButton();
  EXPECT_FALSE(controller->IsCycling());
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
}

// When the user has one window open, the window cycle view isn't shown. In this
// case we should not eat mouse events.
TEST_F(InteractiveWindowCycleControllerTest,
       MouseEventsNotEatenWhenCycleViewNotVisible) {
  std::unique_ptr<Window> w0 = CreateTestWindow();
  EventCounter event_count;
  w0->AddPreTargetHandler(&event_count);
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Start cycling. Since there's only one window the cycle view shouldn't be
  // visible.
  WindowCycleController* controller = Shell::Get()->window_cycle_controller();
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  ASSERT_TRUE(controller->IsCycling());
  ASSERT_FALSE(controller->IsWindowListVisible());

  generator->MoveMouseToCenterOf(w0.get());
  generator->ClickLeftButton();
  EXPECT_LT(0, event_count.GetMouseEventCountAndReset());
}

// Tests that frame throttling starts and ends accordingly when window cycling
// starts and ends.
TEST_F(WindowCycleControllerTest, FrameThrottling) {
  MockFrameThrottlingObserver observer;
  FrameThrottlingController* frame_throttling_controller =
      Shell::Get()->frame_throttling_controller();
  frame_throttling_controller->AddObserver(&observer);
  const int window_count = 5;
  std::unique_ptr<aura::Window> created_windows[window_count];
  std::vector<aura::Window*> windows(window_count, nullptr);
  for (int i = 0; i < window_count; ++i) {
    created_windows[i] = CreateTestWindow();
    windows[i] = created_windows[i].get();
  }

  WindowCycleController* controller = Shell::Get()->window_cycle_controller();
  EXPECT_CALL(observer,
              OnThrottlingStarted(testing::UnorderedElementsAreArray(windows)));
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_CALL(observer,
              OnThrottlingStarted(testing::UnorderedElementsAreArray(windows)))
      .Times(0);
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_CALL(observer, OnThrottlingEnded());
  controller->CompleteCycling();

  EXPECT_CALL(observer,
              OnThrottlingStarted(testing::UnorderedElementsAreArray(windows)));
  controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_CALL(observer, OnThrottlingEnded());
  controller->CancelCycling();
  frame_throttling_controller->RemoveObserver(&observer);
}

// Tests that pressing Alt+Tab while there is an on-going desk animation
// prevents a new window cycle from starting.
TEST_F(WindowCycleControllerTest, DoubleAltTabWithDeskSwitch) {
  WindowCycleController* cycle_controller =
      Shell::Get()->window_cycle_controller();

  auto win0 = CreateAppWindow(gfx::Rect(250, 100));
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());
  const Desk* desk_0 = desks_controller->desks()[0].get();
  const Desk* desk_1 = desks_controller->desks()[1].get();
  ActivateDesk(desk_1);
  EXPECT_EQ(desk_1, desks_controller->active_desk());
  auto win1 = CreateAppWindow(gfx::Rect(300, 200));
  ASSERT_EQ(win1.get(), window_util::GetActiveWindow());
  auto desk_1_windows = desk_1->windows();
  EXPECT_EQ(1u, desk_1_windows.size());
  EXPECT_TRUE(base::Contains(desk_1_windows, win1.get()));

  DeskSwitchAnimationWaiter waiter;
  cycle_controller->HandleCycleWindow(WindowCycleController::FORWARD);
  cycle_controller->CompleteCycling();
  EXPECT_FALSE(cycle_controller->CanCycle());
  cycle_controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_FALSE(cycle_controller->IsCycling());
  waiter.Wait();
  EXPECT_EQ(desk_0, desks_controller->active_desk());
  EXPECT_EQ(win0.get(), window_util::GetActiveWindow());
}

}  // namespace ash
