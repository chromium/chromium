// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_move_window_util.h"

#include "ash/accelerators/accelerator_commands.h"
#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accelerators/accelerator_table.h"
#include "ash/accessibility/test_accessibility_controller_client.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_window_builder.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/test/test_windows.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/display_layout_builder.h"
#include "ui/display/manager/display_layout_store.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ash::display_move_window_util {

namespace {

// Get the default left snapped window bounds which has snapped width ratio
// `chromeos::kDefaultSnapRatio`.
gfx::Rect GetDefaultLeftSnappedBoundsInDisplay(
    const display::Display& display) {
  auto work_area = display.work_area();
  work_area.set_width(work_area.width() * chromeos::kDefaultSnapRatio);
  return work_area;
}

views::Widget* CreateTestWidgetWithParent(views::Widget::InitParams::Type type,
                                          gfx::NativeView parent,
                                          const gfx::Rect& bounds,
                                          bool child) {
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, type);
  params.delegate = nullptr;
  params.parent = parent;
  params.bounds = bounds;
  params.child = child;
  views::Widget* widget = new views::Widget;
  widget->Init(std::move(params));
  widget->Show();
  return widget;
}

void PerformMoveWindowAccel() {
  Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
      AcceleratorAction::kMoveActiveWindowBetweenDisplays, {});
}

}  // namespace

class DisplayMoveWindowUtilTest : public AshTestBase {
 public:
  DisplayMoveWindowUtilTest() = default;
  DisplayMoveWindowUtilTest(const DisplayMoveWindowUtilTest&) = delete;
  DisplayMoveWindowUtilTest& operator=(const DisplayMoveWindowUtilTest&) =
      delete;
  ~DisplayMoveWindowUtilTest() override = default;
};

TEST_F(DisplayMoveWindowUtilTest, SingleDisplay) {
  aura::Window* window =
      CreateTestWindowInShellWithBounds(gfx::Rect(10, 20, 200, 100));
  wm::ActivateWindow(window);
  EXPECT_FALSE(CanHandleMoveActiveWindowBetweenDisplays());
}

// Tests that window bounds are not changing after moving to another display. It
// is added as a child window to another root window with the same origin.
TEST_F(DisplayMoveWindowUtilTest, WindowBounds) {
  // Layout: [p][1]
  UpdateDisplay("400x300,400x300");
  aura::Window* window =
      CreateTestWindowInShellWithBounds(gfx::Rect(10, 20, 200, 100));
  wm::ActivateWindow(window);
  PerformMoveWindowAccel();
  EXPECT_EQ(gfx::Rect(410, 20, 200, 100), window->GetBoundsInScreen());
}

// Tests window state (maximized/fullscreen/snapped) and its bounds.
TEST_F(DisplayMoveWindowUtilTest, WindowState) {
  // Layout: [p][ 1 ]
  UpdateDisplay("400x300,800x300");

  aura::Window* window =
      CreateTestWindowInShellWithBounds(gfx::Rect(10, 20, 200, 100));
  wm::ActivateWindow(window);
  display::Screen* screen = display::Screen::GetScreen();
  ASSERT_EQ(display_manager()->GetDisplayAt(0).id(),
            screen->GetDisplayNearestWindow(window).id());
  WindowState* window_state = WindowState::Get(window);
  // Set window to maximized state.
  window_state->Maximize();
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_EQ(screen_util::GetMaximizedWindowBoundsInParent(window),
            window->bounds());
  PerformMoveWindowAccel();
  EXPECT_EQ(display_manager()->GetDisplayAt(1).id(),
            screen->GetDisplayNearestWindow(window).id());
  // Check that window state is maximized and has updated maximized bounds.
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_EQ(screen_util::GetMaximizedWindowBoundsInParent(window),
            window->bounds());

  // Set window to fullscreen state.
  PerformMoveWindowAccel();
  const WMEvent fullscreen(WM_EVENT_TOGGLE_FULLSCREEN);
  window_state->OnWMEvent(&fullscreen);
  EXPECT_EQ(display_manager()->GetDisplayAt(0).id(),
            screen->GetDisplayNearestWindow(window).id());
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_EQ(display_manager()->GetDisplayAt(0).bounds(),
            window->GetBoundsInScreen());
  PerformMoveWindowAccel();
  EXPECT_EQ(display_manager()->GetDisplayAt(1).id(),
            screen->GetDisplayNearestWindow(window).id());
  // Check that window state is fullscreen and has updated fullscreen bounds.
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_EQ(display_manager()->GetDisplayAt(1).bounds(),
            window->GetBoundsInScreen());

  // Set window to primary snapped state.
  PerformMoveWindowAccel();
  const WindowSnapWMEvent snap_primary(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_primary);
  EXPECT_EQ(display_manager()->GetDisplayAt(0).id(),
            screen->GetDisplayNearestWindow(window).id());
  EXPECT_TRUE(window_state->IsSnapped());
  EXPECT_EQ(GetDefaultLeftSnappedBoundsInDisplay(
                screen->GetDisplayNearestWindow(window)),
            window->GetBoundsInScreen());
  EXPECT_EQ(chromeos::kDefaultSnapRatio, *window_state->snap_ratio());
  PerformMoveWindowAccel();
  EXPECT_EQ(display_manager()->GetDisplayAt(1).id(),
            screen->GetDisplayNearestWindow(window).id());
  // Check that window state is snapped and has updated snapped bounds.
  EXPECT_TRUE(window_state->IsSnapped());
  EXPECT_EQ(GetDefaultLeftSnappedBoundsInDisplay(
                screen->GetDisplayNearestWindow(window)),
            window->GetBoundsInScreen());
  EXPECT_EQ(chromeos::kDefaultSnapRatio, *window_state->snap_ratio());
}

// Tests that movement follows cycling through sorted display id list.
TEST_F(DisplayMoveWindowUtilTest, FourDisplays) {
  display::Screen* screen = display::Screen::GetScreen();
  int64_t primary_id = screen->GetPrimaryDisplay().id();
  // Layout:
  // [3][2]
  // [1][p]
  display::DisplayIdList list =
      display::test::CreateDisplayIdListN(primary_id, 4);
  display::DisplayLayoutBuilder builder(primary_id);
  builder.AddDisplayPlacement(list[1], primary_id,
                              display::DisplayPlacement::LEFT, 0);
  builder.AddDisplayPlacement(list[2], primary_id,
                              display::DisplayPlacement::TOP, 0);
  builder.AddDisplayPlacement(list[3], list[2], display::DisplayPlacement::LEFT,
                              0);
  display_manager()->layout_store()->RegisterLayoutForDisplayIdList(
      list, builder.Build());
  UpdateDisplay("400x300,400x300,400x300,400x300");
  EXPECT_EQ(4U, display_manager()->GetNumDisplays());
  EXPECT_EQ(gfx::Rect(0, 0, 400, 300),
            display_manager()->GetDisplayAt(0).bounds());
  EXPECT_EQ(gfx::Rect(-400, 0, 400, 300),
            display_manager()->GetDisplayAt(1).bounds());
  EXPECT_EQ(gfx::Rect(0, -300, 400, 300),
            display_manager()->GetDisplayAt(2).bounds());
  EXPECT_EQ(gfx::Rect(-400, -300, 400, 300),
            display_manager()->GetDisplayAt(3).bounds());

  aura::Window* window =
      CreateTestWindowInShellWithBounds(gfx::Rect(10, 20, 200, 100));
  wm::ActivateWindow(window);
  ASSERT_EQ(list[0], screen->GetDisplayNearestWindow(window).id());

  PerformMoveWindowAccel();
  EXPECT_EQ(list[1], screen->GetDisplayNearestWindow(window).id());
  PerformMoveWindowAccel();
  EXPECT_EQ(list[2], screen->GetDisplayNearestWindow(window).id());
  PerformMoveWindowAccel();
  EXPECT_EQ(list[3], screen->GetDisplayNearestWindow(window).id());
  PerformMoveWindowAccel();
  EXPECT_EQ(list[0], screen->GetDisplayNearestWindow(window).id());
}

// Tests that a11y alert is sent on handling move window to display.
TEST_F(DisplayMoveWindowUtilTest, A11yAlert) {
  // Layout: [p][1]
  UpdateDisplay("400x300,400x300");
  TestAccessibilityControllerClient client;

  aura::Window* window =
      CreateTestWindowInShellWithBounds(gfx::Rect(10, 20, 200, 100));
  wm::ActivateWindow(window);
  PerformMoveWindowAccel();
  EXPECT_EQ(AccessibilityAlert::WINDOW_MOVED_TO_ANOTHER_DISPLAY,
            client.last_a11y_alert());
}

// Tests that moving window between displays is no-op if active window is not in
// window cycle list.
TEST_F(DisplayMoveWindowUtilTest, NoMovementIfNotInCycleWindowList) {
  // Layout: [p][1]
  UpdateDisplay("400x300,400x300");
  // Create a window in app list container, which would be excluded in cycle
  // window list.
  std::unique_ptr<aura::Window> window =
      ChildTestWindowBuilder(Shell::GetPrimaryRootWindow(),
                             gfx::Rect(10, 20, 200, 100),
                             kShellWindowId_AppListContainer)
          .Build();

  wm::ActivateWindow(window.get());
  display::Screen* screen = display::Screen::GetScreen();
  EXPECT_EQ(display_manager()->GetDisplayAt(0).id(),
            screen->GetDisplayNearestWindow(window.get()).id());

  MruWindowTracker::WindowList cycle_window_list =
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kActiveDesk);
  EXPECT_TRUE(cycle_window_list.empty());
  PerformMoveWindowAccel();
  EXPECT_EQ(display_manager()->GetDisplayAt(0).id(),
            screen->GetDisplayNearestWindow(window.get()).id());
}

// Tests that window bounds should be kept if it is not changed by user, i.e.
// if it is changed by display area difference, it should restore to the
// original bounds when it is moved between displays and there is enough work
// area to show this window.
TEST_F(DisplayMoveWindowUtilTest, KeepWindowBoundsIfNotChangedByUser) {
  // Layout:
  // +---+---+
  // | p |   |
  // +---+ 1 |
  //     |   |
  //     +---+
  UpdateDisplay("400x300,400x600");
  const int shelf_inset = 300 - ShelfConfig::Get()->shelf_size();
  // Create and activate window on display [1].
  aura::Window* window =
      CreateTestWindowInShellWithBounds(gfx::Rect(410, 20, 200, 400));
  wm::ActivateWindow(window);
  display::Screen* screen = display::Screen::GetScreen();
  EXPECT_EQ(display_manager()->GetDisplayAt(1).id(),
            screen->GetDisplayNearestWindow(window).id());
  // Move window to display [p]. Its window bounds is adjusted by available work
  // area.
  PerformMoveWindowAccel();
  EXPECT_EQ(display_manager()->GetDisplayAt(0).id(),
            screen->GetDisplayNearestWindow(window).id());
  EXPECT_EQ(gfx::Rect(10, 20, 200, shelf_inset), window->GetBoundsInScreen());
  // Move window back to display [1]. Its window bounds should be restored.
  PerformMoveWindowAccel();
  EXPECT_EQ(display_manager()->GetDisplayAt(1).id(),
            screen->GetDisplayNearestWindow(window).id());
  EXPECT_EQ(gfx::Rect(410, 20, 200, 400), window->GetBoundsInScreen());

  // Move window to display [p] and set that its bounds is changed by user.
  WindowState* window_state = WindowState::Get(window);
  PerformMoveWindowAccel();
  window_state->SetBoundsChangedByUser(true);
  // Move window back to display [1], but its bounds has been changed by user.
  // Then window bounds should be kept the same as that in display [p].
  PerformMoveWindowAccel();
  EXPECT_EQ(display_manager()->GetDisplayAt(1).id(),
            screen->GetDisplayNearestWindow(window).id());
  EXPECT_EQ(gfx::Rect(410, 20, 200, shelf_inset), window->GetBoundsInScreen());
}

// Tests auto window management on moving window between displays.
TEST_F(DisplayMoveWindowUtilTest, AutoManaged) {
  // Layout: [p][1]
  UpdateDisplay("400x300,400x300");
  // Create and show window on display [p]. Enable auto window position managed,
  // which will center the window on display [p].
  aura::Window* window1 =
      CreateTestWindowInShellWithBounds(gfx::Rect(10, 20, 200, 100));
  WindowState* window1_state = WindowState::Get(window1);
  window1_state->SetWindowPositionManaged(true);
  window1->Hide();
  window1->Show();
  EXPECT_EQ(gfx::Rect(100, 20, 200, 100), window1->GetBoundsInScreen());
  display::Screen* screen = display::Screen::GetScreen();
  EXPECT_EQ(display_manager()->GetDisplayAt(0).id(),
            screen->GetDisplayNearestWindow(window1).id());

  // Create and show window on display [p]. Enable auto window position managed,
  // which will do auto window management (pushing the other window to side).
  aura::Window* window2 =
      CreateTestWindowInShellWithBounds(gfx::Rect(10, 20, 200, 100));
  WindowState* window2_state = WindowState::Get(window2);
  window2_state->SetWindowPositionManaged(true);
  window2->Hide();
  window2->Show();
  EXPECT_EQ(gfx::Rect(0, 20, 200, 100), window1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(200, 20, 200, 100), window2->GetBoundsInScreen());
  // Activate window2.
  wm::ActivateWindow(window2);
  EXPECT_EQ(display_manager()->GetDisplayAt(0).id(),
            screen->GetDisplayNearestWindow(window2).id());
  EXPECT_TRUE(window2_state->pre_auto_manage_window_bounds());
  EXPECT_EQ(gfx::Rect(10, 20, 200, 100),
            *window2_state->pre_auto_manage_window_bounds());

  // Move window2 to display [1] and check auto window management.
  PerformMoveWindowAccel();
  EXPECT_EQ(display_manager()->GetDisplayAt(1).id(),
            screen->GetDisplayNearestWindow(window2).id());
  // Check |pre_added_to_workspace_window_bounds()|, which should be equal to
  // |pre_auto_manage_window_bounds()| in this case.
  EXPECT_EQ(*window2_state->pre_auto_manage_window_bounds(),
            *window2_state->pre_added_to_workspace_window_bounds());
  // Window 1 centers on display [p] again.
  EXPECT_EQ(gfx::Rect(100, 20, 200, 100), window1->GetBoundsInScreen());
  // Window 2 is positioned by |pre_added_to_workspace_window_bounds()|.
  EXPECT_EQ(gfx::Rect(410, 20, 200, 100), window2->GetBoundsInScreen());
}

TEST_F(DisplayMoveWindowUtilTest, WindowWithTransientChild) {
  UpdateDisplay("400x300,400x300");
  aura::Window* window =
      CreateTestWindowInShellWithBounds(gfx::Rect(10, 20, 200, 100));
  wm::ActivateWindow(window);

  // Create a |child| window and make it a transient child of |window|.
  std::unique_ptr<aura::Window> child =
      ChildTestWindowBuilder(window, gfx::Rect(20, 30, 40, 50)).Build();
  ::wm::AddTransientChild(window, child.get());
  display::Screen* screen = display::Screen::GetScreen();
  EXPECT_EQ(display_manager()->GetDisplayAt(0).id(),
            screen->GetDisplayNearestWindow(window).id());
  EXPECT_EQ(display_manager()->GetDisplayAt(0).id(),
            screen->GetDisplayNearestWindow(child.get()).id());

  // Operate moving window to right display. Check display and bounds.
  PerformMoveWindowAccel();
  EXPECT_EQ(display_manager()->GetDisplayAt(1).id(),
            screen->GetDisplayNearestWindow(window).id());
  EXPECT_EQ(display_manager()->GetDisplayAt(1).id(),
            screen->GetDisplayNearestWindow(child.get()).id());
  EXPECT_EQ(gfx::Rect(410, 20, 200, 100), window->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(430, 50, 40, 50), child->GetBoundsInScreen());
}

// Test that when operating move window between displays on activated transient
// child window, its first non-transient transient-parent window should be the
// target instead.
TEST_F(DisplayMoveWindowUtilTest, ActiveTransientChildWindow) {
  UpdateDisplay("400x300,400x300");
  std::unique_ptr<views::Widget> window =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  window->SetBounds(gfx::Rect(10, 20, 200, 100));

  // Create a |child| transient widget of |window|. When |child| is shown, it is
  // activated.
  std::unique_ptr<views::Widget> child(CreateTestWidgetWithParent(
      views::Widget::InitParams::TYPE_WINDOW, window->GetNativeView(),
      gfx::Rect(20, 30, 40, 50), false));
  display::Screen* screen = display::Screen::GetScreen();
  EXPECT_EQ(display_manager()->GetDisplayAt(0).id(),
            screen->GetDisplayNearestWindow(window->GetNativeWindow()).id());
  EXPECT_EQ(display_manager()->GetDisplayAt(0).id(),
            screen->GetDisplayNearestWindow(child->GetNativeWindow()).id());
  // Ensure |child| window is activated.
  EXPECT_FALSE(wm::IsActiveWindow(window->GetNativeWindow()));
  EXPECT_TRUE(wm::IsActiveWindow(child->GetNativeWindow()));

  // Operate moving window to right display. Check display and bounds.
  PerformMoveWindowAccel();
  EXPECT_EQ(display_manager()->GetDisplayAt(1).id(),
            screen->GetDisplayNearestWindow(window->GetNativeWindow()).id());
  EXPECT_EQ(display_manager()->GetDisplayAt(1).id(),
            screen->GetDisplayNearestWindow(child->GetNativeWindow()).id());
  EXPECT_EQ(gfx::Rect(410, 20, 200, 100),
            window->GetNativeWindow()->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(420, 30, 40, 50),
            child->GetNativeWindow()->GetBoundsInScreen());
}

// Test that when active window is transient child window, no movement if its
// first non-transient transient-parent window is not in window cycle list.
TEST_F(DisplayMoveWindowUtilTest, TransientParentNotInCycleWindowList) {
  UpdateDisplay("400x300,400x300");
  aura::Window* w1 =
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 50, 50));
  wm::ActivateWindow(w1);

  // Create a window |w2| in non-switchable window container.
  aura::Window* setting_bubble_container =
      Shell::GetPrimaryRootWindowController()->GetContainer(
          kShellWindowId_SettingBubbleContainer);
  std::unique_ptr<aura::Window> w2 =
      ChildTestWindowBuilder(setting_bubble_container,
                             gfx::Rect(10, 20, 200, 100))
          .Build();
  wm::ActivateWindow(w2.get());

  // Create a |child| transient widget of |w2|. When |child| is shown, it is
  // activated.
  std::unique_ptr<views::Widget> child(
      CreateTestWidgetWithParent(views::Widget::InitParams::TYPE_WINDOW,
                                 w2.get(), gfx::Rect(20, 30, 40, 50), false));
  display::Screen* screen = display::Screen::GetScreen();
  EXPECT_EQ(display_manager()->GetDisplayAt(0).id(),
            screen->GetDisplayNearestWindow(w1).id());
  EXPECT_EQ(display_manager()->GetDisplayAt(0).id(),
            screen->GetDisplayNearestWindow(w2.get()).id());
  EXPECT_EQ(display_manager()->GetDisplayAt(0).id(),
            screen->GetDisplayNearestWindow(child->GetNativeWindow()).id());
  // Ensure |child| window is activated.
  EXPECT_FALSE(wm::IsActiveWindow(w1));
  EXPECT_FALSE(wm::IsActiveWindow(w2.get()));
  EXPECT_TRUE(wm::IsActiveWindow(child->GetNativeWindow()));

  PerformMoveWindowAccel();
  EXPECT_EQ(display_manager()->GetDisplayAt(0).id(),
            screen->GetDisplayNearestWindow(w1).id());
  EXPECT_EQ(display_manager()->GetDisplayAt(0).id(),
            screen->GetDisplayNearestWindow(w2.get()).id());
  EXPECT_EQ(display_manager()->GetDisplayAt(0).id(),
            screen->GetDisplayNearestWindow(child->GetNativeWindow()).id());
}

// Tests that restore bounds is updated with window movement to another display.
TEST_F(DisplayMoveWindowUtilTest, RestoreMaximizedWindowAfterMovement) {
  UpdateDisplay("400x300,400x300");
  aura::Window* w =
      CreateTestWindowInShellWithBounds(gfx::Rect(10, 20, 200, 100));
  wm::ActivateWindow(w);

  WindowState* window_state = WindowState::Get(w);
  window_state->Maximize();
  EXPECT_EQ(gfx::Rect(0, 0, 400, 300 - ShelfConfig::Get()->shelf_size()),
            w->GetBoundsInScreen());

  PerformMoveWindowAccel();
  EXPECT_EQ(gfx::Rect(400, 0, 400, 300 - ShelfConfig::Get()->shelf_size()),
            w->GetBoundsInScreen());
  window_state->Restore();
  EXPECT_EQ(gfx::Rect(410, 20, 200, 100), w->GetBoundsInScreen());
}

// Tests that the restore history stack will be updated correctly on the restore
// bounds updates.
TEST_F(DisplayMoveWindowUtilTest, RestoreHistoryOnUpdatedRestoreBounds) {
  UpdateDisplay("400x300,400x300");
  aura::Window* w =
      CreateTestWindowInShellWithBounds(gfx::Rect(10, 20, 200, 100));
  wm::ActivateWindow(w);

  const gfx::Rect restore_bounds_in_second_display(410, 20, 200, 100);
  WindowState* window_state = WindowState::Get(w);
  window_state->Maximize();

  using chromeos::WindowStateType;
  const std::vector<chromeos::WindowStateType>& restore_stack =
      window_state->window_state_restore_history();
  EXPECT_EQ(gfx::Rect(10, 20, 200, 100),
            window_state->GetRestoreBoundsInScreen());

  // Moving the window to the second display through shortcut should update both
  // the restore bounds and the restore history stack.
  PerformMoveWindowAccel();
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_EQ(restore_bounds_in_second_display,
            window_state->GetRestoreBoundsInScreen());
  EXPECT_EQ(1u, restore_stack.size());
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);

  // Verify the restore bounds and restore history after toggling to fullscreen
  // the window.
  accelerators::ToggleFullscreen();
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_EQ(gfx::Rect(400, 0, 400, 300), w->GetBoundsInScreen());
  EXPECT_EQ(restore_bounds_in_second_display,
            window_state->GetRestoreBoundsInScreen());
  EXPECT_EQ(2u, restore_stack.size());
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(restore_stack[1], WindowStateType::kMaximized);

  // Verify the restore bounds and restore history after toggling to
  // restore the window to maxmized.
  accelerators::ToggleFullscreen();
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_EQ(restore_bounds_in_second_display,
            window_state->GetRestoreBoundsInScreen());
  EXPECT_EQ(gfx::Rect(400, 0, 400, 300 - ShelfConfig::Get()->shelf_size()),
            w->GetBoundsInScreen());
  EXPECT_EQ(1u, restore_stack.size());
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);

  // Verify the restore bounds and restore history after toggling to fullscreen
  // the window again. And the window should stay in the second display with
  // correct restore bounds.
  accelerators::ToggleFullscreen();
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_EQ(restore_bounds_in_second_display,
            window_state->GetRestoreBoundsInScreen());
  EXPECT_EQ(gfx::Rect(400, 0, 400, 300), w->GetBoundsInScreen());
  EXPECT_EQ(2u, restore_stack.size());
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(restore_stack[1], WindowStateType::kMaximized);
}

}  // namespace ash::display_move_window_util
