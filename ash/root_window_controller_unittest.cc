// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/root_window_controller.h"

#include <memory>

#include "ash/keyboard/ui/keyboard_ui.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/system_modal_container_layout_manager.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/test_event_handler.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

using aura::Window;
using views::Widget;

namespace ash {
namespace {

class DeleteOnBlurDelegate : public aura::test::TestWindowDelegate,
                             public aura::client::FocusChangeObserver {
 public:
  DeleteOnBlurDelegate() = default;

  DeleteOnBlurDelegate(const DeleteOnBlurDelegate&) = delete;
  DeleteOnBlurDelegate& operator=(const DeleteOnBlurDelegate&) = delete;

  ~DeleteOnBlurDelegate() override = default;

  void SetWindow(aura::Window* window) {
    window_ = window;
    aura::client::SetFocusChangeObserver(window_, this);
  }

 private:
  // aura::test::TestWindowDelegate overrides:
  bool CanFocus() override { return true; }

  // aura::client::FocusChangeObserver implementation:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override {
    if (window_ == lost_focus) {
      delete window_;
    }
  }

  raw_ptr<aura::Window, DanglingUntriaged> window_{nullptr};
};

aura::LayoutManager* GetLayoutManager(RootWindowController* controller,
                                      int id) {
  return controller->GetContainer(id)->layout_manager();
}

}  // namespace

class RootWindowControllerTest : public AshTestBase {
 public:
  views::Widget* CreateTestWidget(const gfx::Rect& bounds) {
    views::Widget* widget =
        views::Widget::CreateWindowWithContext(nullptr, GetContext());
    // Any initial bounds are constrained to the screen work area or the parent.
    // See Widget::InitialBounds() & Widget::SetBoundsConstrained(). Explicitly
    // setting the bounds here will allow the view to be positioned such that it
    // can extend outside the screen work area.
    widget->SetBounds(bounds);
    widget->Show();
    return widget;
  }

  views::WidgetDelegate* CreateModalWidgetDelegate() {
    auto delegate = std::make_unique<views::WidgetDelegateView>();
    delegate->SetModalType(ui::mojom::ModalType::kSystem);
    return delegate.release();
  }

  views::Widget* CreateModalWidget(const gfx::Rect& bounds) {
    views::Widget* widget = views::Widget::CreateWindowWithContext(
        CreateModalWidgetDelegate(), GetContext());
    // See the above comment.
    widget->SetBounds(bounds);
    widget->Show();
    return widget;
  }

  views::Widget* CreateModalWidgetWithParent(const gfx::Rect& bounds,
                                             aura::Window* parent) {
    views::Widget* widget = views::Widget::CreateWindowWithParent(
        CreateModalWidgetDelegate(), parent);
    // See the above comment.
    widget->SetBounds(bounds);
    widget->Show();
    return widget;
  }

  aura::Window* GetModalContainer(aura::Window* root_window) {
    return Shell::GetContainer(root_window,
                               kShellWindowId_SystemModalContainer);
  }
};

TEST_F(RootWindowControllerTest, MoveWindows_Basic) {
  // Windows origin should be doubled when moved to the 1st display.
  UpdateDisplay("600x500,300x250");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  int bottom_inset = 250 - ShelfConfig::Get()->shelf_size();
  views::Widget* normal = CreateTestWidget(gfx::Rect(650, 10, 100, 100));
  EXPECT_EQ(root_windows[1], normal->GetNativeView()->GetRootWindow());
  EXPECT_EQ("650,10 100x100", normal->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("50,10 100x100",
            normal->GetNativeView()->GetBoundsInRootWindow().ToString());

  views::Widget* maximized = CreateTestWidget(gfx::Rect(700, 10, 100, 100));
  maximized->Maximize();
  EXPECT_EQ(root_windows[1], maximized->GetNativeView()->GetRootWindow());
  EXPECT_EQ(gfx::Rect(600, 0, 300, bottom_inset).ToString(),
            maximized->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ(gfx::Rect(0, 0, 300, bottom_inset).ToString(),
            maximized->GetNativeView()->GetBoundsInRootWindow().ToString());

  views::Widget* minimized = CreateTestWidget(gfx::Rect(550, 10, 200, 200));
  minimized->Minimize();
  EXPECT_EQ(root_windows[1], minimized->GetNativeView()->GetRootWindow());
  EXPECT_EQ("550,10 200x200", minimized->GetWindowBoundsInScreen().ToString());

  views::Widget* fullscreen = CreateTestWidget(gfx::Rect(850, 10, 200, 200));
  display::Display secondary_display = GetSecondaryDisplay();
  gfx::Rect orig_bounds = fullscreen->GetWindowBoundsInScreen();
  EXPECT_TRUE(secondary_display.work_area().Intersects(orig_bounds));
  EXPECT_FALSE(secondary_display.work_area().Contains(orig_bounds));

  fullscreen->SetFullscreen(true);
  EXPECT_EQ(root_windows[1], fullscreen->GetNativeView()->GetRootWindow());

  EXPECT_EQ("600,0 300x250", fullscreen->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("0,0 300x250",
            fullscreen->GetNativeView()->GetBoundsInRootWindow().ToString());

  views::Widget* unparented_control = new Widget;
  Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      Widget::InitParams::TYPE_CONTROL);
  params.bounds = gfx::Rect(650, 10, 100, 100);
  params.context = GetContext();
  unparented_control->Init(std::move(params));
  EXPECT_EQ(root_windows[1],
            unparented_control->GetNativeView()->GetRootWindow());
  EXPECT_EQ(kShellWindowId_UnparentedContainer,
            unparented_control->GetNativeView()->parent()->GetId());

  // Make sure a window that will delete itself when losing focus
  // will not crash.
  aura::WindowTracker tracker;
  DeleteOnBlurDelegate delete_on_blur_delegate;
  aura::Window* d2 = CreateTestWindowInShellWithDelegate(
      &delete_on_blur_delegate, 0, gfx::Rect(50, 50, 100, 100));
  delete_on_blur_delegate.SetWindow(d2);
  aura::client::GetFocusClient(root_windows[0])->FocusWindow(d2);
  tracker.Add(d2);

  UpdateDisplay("600x500");

  // d2 must have been deleted.
  EXPECT_FALSE(tracker.Contains(d2));

  EXPECT_EQ(root_windows[0], normal->GetNativeView()->GetRootWindow());
  EXPECT_EQ("100,20 100x100", normal->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("100,20 100x100",
            normal->GetNativeView()->GetBoundsInRootWindow().ToString());

  bottom_inset = 500 - ShelfConfig::Get()->shelf_size();

  // First clear fullscreen status, since both fullscreen and maximized windows
  // share the same desktop workspace, which cancels the shelf status.
  fullscreen->SetFullscreen(false);
  EXPECT_EQ(root_windows[0], maximized->GetNativeView()->GetRootWindow());
  EXPECT_EQ(gfx::Rect(0, 0, 600, bottom_inset).ToString(),
            maximized->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ(gfx::Rect(0, 0, 600, bottom_inset).ToString(),
            maximized->GetNativeView()->GetBoundsInRootWindow().ToString());

  // Set fullscreen to true, but maximized window's size won't change because
  // it's not visible. see crbug.com/504299.
  fullscreen->SetFullscreen(true);
  EXPECT_EQ(root_windows[0], maximized->GetNativeView()->GetRootWindow());
  EXPECT_EQ(gfx::Rect(0, 0, 600, bottom_inset).ToString(),
            maximized->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ(gfx::Rect(0, 0, 600, bottom_inset).ToString(),
            maximized->GetNativeView()->GetBoundsInRootWindow().ToString());

  EXPECT_EQ(root_windows[0], minimized->GetNativeView()->GetRootWindow());
  EXPECT_EQ("0,20 200x200", minimized->GetWindowBoundsInScreen().ToString());

  EXPECT_EQ(root_windows[0], fullscreen->GetNativeView()->GetRootWindow());
  EXPECT_TRUE(fullscreen->IsFullscreen());
  EXPECT_EQ("0,0 600x500", fullscreen->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("0,0 600x500",
            fullscreen->GetNativeView()->GetBoundsInRootWindow().ToString());

  // Test if the restore bounds are correctly updated.
  WindowState::Get(maximized->GetNativeView())->Restore();
  EXPECT_EQ("200,20 100x100", maximized->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("200,20 100x100",
            maximized->GetNativeView()->GetBoundsInRootWindow().ToString());

  fullscreen->SetFullscreen(false);
  EXPECT_EQ("400,20 200x200", fullscreen->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ("400,20 200x200",
            fullscreen->GetNativeView()->GetBoundsInRootWindow().ToString());

  // Test if the unparented widget has moved.
  EXPECT_EQ(root_windows[0],
            unparented_control->GetNativeView()->GetRootWindow());
  EXPECT_EQ(kShellWindowId_UnparentedContainer,
            unparented_control->GetNativeView()->parent()->GetId());
}

TEST_F(RootWindowControllerTest, MoveWindows_Modal) {
  UpdateDisplay("500x400,500x600");

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  // Emulate virtual screen coordinate system.
  root_windows[0]->SetBounds(gfx::Rect(0, 0, 500, 400));
  root_windows[1]->SetBounds(gfx::Rect(500, 0, 500, 600));

  views::Widget* normal = CreateTestWidget(gfx::Rect(300, 10, 100, 100));
  EXPECT_EQ(root_windows[0], normal->GetNativeView()->GetRootWindow());
  EXPECT_TRUE(wm::IsActiveWindow(normal->GetNativeView()));

  views::Widget* modal = CreateModalWidget(gfx::Rect(650, 10, 100, 100));
  EXPECT_EQ(root_windows[1], modal->GetNativeView()->GetRootWindow());
  EXPECT_TRUE(
      GetModalContainer(root_windows[1])->Contains(modal->GetNativeView()));
  EXPECT_TRUE(wm::IsActiveWindow(modal->GetNativeView()));

  ui::test::EventGenerator generator_1st(root_windows[0]);
  generator_1st.ClickLeftButton();
  EXPECT_TRUE(wm::IsActiveWindow(modal->GetNativeView()));

  UpdateDisplay("500x400");
  EXPECT_EQ(root_windows[0], modal->GetNativeView()->GetRootWindow());
  EXPECT_TRUE(wm::IsActiveWindow(modal->GetNativeView()));
  generator_1st.ClickLeftButton();
  EXPECT_TRUE(wm::IsActiveWindow(modal->GetNativeView()));
}

// Make sure lock related windows moves.
TEST_F(RootWindowControllerTest, MoveWindows_LockWindowsInUnified) {
  display_manager()->SetUnifiedDesktopEnabled(true);

  UpdateDisplay("500x400");
  const int kLockScreenWindowId = 1000;

  RootWindowController* controller = Shell::GetPrimaryRootWindowController();

  aura::Window* lock_container =
      controller->GetContainer(kShellWindowId_LockScreenContainer);

  views::Widget* lock_screen =
      CreateModalWidgetWithParent(gfx::Rect(10, 10, 100, 100), lock_container);
  lock_screen->GetNativeWindow()->SetId(kLockScreenWindowId);
  lock_screen->SetFullscreen(true);

  ASSERT_EQ(lock_screen->GetNativeWindow(),
            controller->GetRootWindow()->GetChildById(kLockScreenWindowId));
  EXPECT_EQ("0,0 500x400", lock_screen->GetNativeWindow()->bounds().ToString());

  // Switch to unified.
  UpdateDisplay("500x400,500x400");

  // In unified mode, RWC is created
  controller = Shell::GetPrimaryRootWindowController();

  ASSERT_EQ(lock_screen->GetNativeWindow(),
            controller->GetRootWindow()->GetChildById(kLockScreenWindowId));
  EXPECT_EQ("0,0 500x400", lock_screen->GetNativeWindow()->bounds().ToString());

  // Switch to mirror.
  display_manager()->SetMirrorMode(display::MirrorMode::kNormal, std::nullopt);
  EXPECT_TRUE(display_manager()->IsInMirrorMode());

  controller = Shell::GetPrimaryRootWindowController();
  ASSERT_EQ(lock_screen->GetNativeWindow(),
            controller->GetRootWindow()->GetChildById(kLockScreenWindowId));
  EXPECT_EQ("0,0 500x400", lock_screen->GetNativeWindow()->bounds().ToString());

  // Switch to unified.
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, std::nullopt);
  EXPECT_TRUE(display_manager()->IsInUnifiedMode());

  controller = Shell::GetPrimaryRootWindowController();

  ASSERT_EQ(lock_screen->GetNativeWindow(),
            controller->GetRootWindow()->GetChildById(kLockScreenWindowId));
  EXPECT_EQ("0,0 500x400", lock_screen->GetNativeWindow()->bounds().ToString());

  // Switch to single display.
  UpdateDisplay("600x500");
  EXPECT_FALSE(display_manager()->IsInUnifiedMode());
  EXPECT_FALSE(display_manager()->IsInMirrorMode());

  controller = Shell::GetPrimaryRootWindowController();

  ASSERT_EQ(lock_screen->GetNativeWindow(),
            controller->GetRootWindow()->GetChildById(kLockScreenWindowId));
  EXPECT_EQ("0,0 600x500", lock_screen->GetNativeWindow()->bounds().ToString());
}

// Tests that the moved windows maintain MRU ordering.
TEST_F(RootWindowControllerTest, MoveWindows_MaintainMRUordering) {
  UpdateDisplay("600x500,300x250");

  display::Screen* screen = display::Screen::GetScreen();
  const display::Display primary_display = screen->GetPrimaryDisplay();
  const display::Display secondary_display = GetSecondaryDisplay();

  views::Widget* existing1 = CreateTestWidget(gfx::Rect(0, 10, 100, 100));
  ASSERT_EQ(primary_display.id(),
            screen->GetDisplayNearestWindow(existing1->GetNativeWindow()).id());

  views::Widget* moved = CreateTestWidget(gfx::Rect(650, 10, 100, 100));
  ASSERT_EQ(secondary_display.id(),
            screen->GetDisplayNearestWindow(moved->GetNativeWindow()).id());

  views::Widget* existing2 = CreateTestWidget(gfx::Rect(0, 10, 100, 100));
  ASSERT_EQ(primary_display.id(),
            screen->GetDisplayNearestWindow(existing2->GetNativeWindow()).id());

  views::Widget* active = CreateTestWidget(gfx::Rect(650, 10, 100, 100));
  ASSERT_TRUE(active->IsActive());
  ASSERT_EQ(secondary_display.id(),
            screen->GetDisplayNearestWindow(active->GetNativeWindow()).id());

  // Switch to single display.
  UpdateDisplay("600x500");

  // |active| is still active.
  ASSERT_TRUE(active->IsActive());

  // |moved| should be put between |existing2| and |existing1| to maintain MRU
  // ordering.
  aura::Window* parent = moved->GetNativeWindow()->parent();
  ASSERT_EQ(parent, existing1->GetNativeWindow()->parent());
  const std::vector<raw_ptr<aura::Window, VectorExperimental>> expected_order =
      {existing1->GetNativeWindow(), moved->GetNativeWindow(),
       existing2->GetNativeWindow(), active->GetNativeWindow()};
  EXPECT_EQ(expected_order, parent->children());
}

TEST_F(RootWindowControllerTest, ModalContainer) {
  UpdateDisplay("600x500");
  RootWindowController* controller = Shell::GetPrimaryRootWindowController();
  EXPECT_TRUE(Shell::Get()->session_controller()->IsActiveUserSessionStarted());
  EXPECT_EQ(GetLayoutManager(controller, kShellWindowId_SystemModalContainer),
            controller->GetSystemModalLayoutManager(nullptr));

  views::Widget* session_modal_widget =
      CreateModalWidget(gfx::Rect(300, 10, 100, 100));
  EXPECT_EQ(GetLayoutManager(controller, kShellWindowId_SystemModalContainer),
            controller->GetSystemModalLayoutManager(
                session_modal_widget->GetNativeWindow()));

  GetSessionControllerClient()->LockScreen();
  EXPECT_TRUE(Shell::Get()->session_controller()->IsScreenLocked());
  EXPECT_EQ(
      GetLayoutManager(controller, kShellWindowId_LockSystemModalContainer),
      controller->GetSystemModalLayoutManager(nullptr));

  aura::Window* lock_container =
      controller->GetContainer(kShellWindowId_LockScreenContainer);
  views::Widget* lock_modal_widget =
      CreateModalWidgetWithParent(gfx::Rect(300, 10, 100, 100), lock_container);
  EXPECT_EQ(
      GetLayoutManager(controller, kShellWindowId_LockSystemModalContainer),
      controller->GetSystemModalLayoutManager(
          lock_modal_widget->GetNativeWindow()));
  EXPECT_EQ(GetLayoutManager(controller, kShellWindowId_SystemModalContainer),
            controller->GetSystemModalLayoutManager(
                session_modal_widget->GetNativeWindow()));

  GetSessionControllerClient()->UnlockScreen();
}

TEST_F(RootWindowControllerTest, ModalContainerNotLoggedInLoggedIn) {
  UpdateDisplay("600x500");

  // Configure login screen environment.
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  ClearLogin();
  EXPECT_EQ(0, session_controller->NumberOfLoggedInUsers());
  EXPECT_FALSE(session_controller->IsActiveUserSessionStarted());

  RootWindowController* controller = Shell::GetPrimaryRootWindowController();
  EXPECT_EQ(
      GetLayoutManager(controller, kShellWindowId_LockSystemModalContainer),
      controller->GetSystemModalLayoutManager(nullptr));

  aura::Window* lock_container =
      controller->GetContainer(kShellWindowId_LockScreenContainer);
  views::Widget* login_modal_widget =
      CreateModalWidgetWithParent(gfx::Rect(300, 10, 100, 100), lock_container);
  EXPECT_EQ(
      GetLayoutManager(controller, kShellWindowId_LockSystemModalContainer),
      controller->GetSystemModalLayoutManager(
          login_modal_widget->GetNativeWindow()));
  login_modal_widget->Close();

  // Configure user session environment.
  CreateUserSessions(1);
  EXPECT_EQ(1, session_controller->NumberOfLoggedInUsers());
  EXPECT_TRUE(session_controller->IsActiveUserSessionStarted());
  EXPECT_EQ(GetLayoutManager(controller, kShellWindowId_SystemModalContainer),
            controller->GetSystemModalLayoutManager(nullptr));

  views::Widget* session_modal_widget =
      CreateModalWidget(gfx::Rect(300, 10, 100, 100));
  EXPECT_EQ(GetLayoutManager(controller, kShellWindowId_SystemModalContainer),
            controller->GetSystemModalLayoutManager(
                session_modal_widget->GetNativeWindow()));
}

TEST_F(RootWindowControllerTest, ModalContainerBlockedSession) {
  UpdateDisplay("600x500");
  RootWindowController* controller = Shell::GetPrimaryRootWindowController();
  aura::Window* lock_container =
      controller->GetContainer(kShellWindowId_LockScreenContainer);
  for (int block_reason = FIRST_BLOCK_REASON;
       block_reason < NUMBER_OF_BLOCK_REASONS; ++block_reason) {
    views::Widget* session_modal_widget =
        CreateModalWidget(gfx::Rect(300, 10, 100, 100));
    EXPECT_EQ(GetLayoutManager(controller, kShellWindowId_SystemModalContainer),
              controller->GetSystemModalLayoutManager(
                  session_modal_widget->GetNativeWindow()));
    EXPECT_EQ(GetLayoutManager(controller, kShellWindowId_SystemModalContainer),
              controller->GetSystemModalLayoutManager(nullptr));
    session_modal_widget->Close();

    BlockUserSession(static_cast<UserSessionBlockReason>(block_reason));

    EXPECT_EQ(
        GetLayoutManager(controller, kShellWindowId_LockSystemModalContainer),
        controller->GetSystemModalLayoutManager(nullptr));

    views::Widget* lock_modal_widget = CreateModalWidgetWithParent(
        gfx::Rect(300, 10, 100, 100), lock_container);
    EXPECT_EQ(
        GetLayoutManager(controller, kShellWindowId_LockSystemModalContainer),
        controller->GetSystemModalLayoutManager(
            lock_modal_widget->GetNativeWindow()));

    session_modal_widget = CreateModalWidget(gfx::Rect(300, 10, 100, 100));
    EXPECT_EQ(GetLayoutManager(controller, kShellWindowId_SystemModalContainer),
              controller->GetSystemModalLayoutManager(
                  session_modal_widget->GetNativeWindow()));
    session_modal_widget->Close();

    lock_modal_widget->Close();
    UnblockUserSession();
  }
}

TEST_F(RootWindowControllerTest, GetWindowForFullscreenMode) {
  UpdateDisplay("600x500");
  RootWindowController* controller = Shell::GetPrimaryRootWindowController();

  Widget* w1 = CreateTestWidget(gfx::Rect(0, 0, 100, 100));
  w1->Maximize();
  Widget* w2 = CreateTestWidget(gfx::Rect(0, 0, 100, 100));
  w2->SetFullscreen(true);
  // |w3| is a transient child of |w2|.
  Widget* w3 = Widget::CreateWindowWithParent(nullptr, w2->GetNativeWindow(),
                                              gfx::Rect(0, 0, 100, 100));

  // Test that GetWindowForFullscreenMode() finds the fullscreen window when one
  // of its transient children is active.
  w3->Activate();
  EXPECT_EQ(w2->GetNativeWindow(), controller->GetWindowForFullscreenMode());

  // If the topmost window is not fullscreen, it returns nullptr.
  w1->Activate();
  EXPECT_EQ(nullptr, controller->GetWindowForFullscreenMode());
  w1->Close();
  w3->Close();

  // Only w2 remains, if minimized GetWindowForFullscreenMode should return
  // nullptr.
  w2->Activate();
  EXPECT_EQ(w2->GetNativeWindow(), controller->GetWindowForFullscreenMode());
  w2->Minimize();
  EXPECT_EQ(nullptr, controller->GetWindowForFullscreenMode());
}

TEST_F(RootWindowControllerTest, MultipleDisplaysGetWindowForFullscreenMode) {
  UpdateDisplay("600x500,600x500");
  Shell::RootWindowControllerList controllers =
      Shell::Get()->GetAllRootWindowControllers();

  Widget* w1 = CreateTestWidget(gfx::Rect(0, 0, 100, 100));
  w1->Maximize();
  Widget* w2 = CreateTestWidget(gfx::Rect(0, 0, 100, 100));
  w2->SetFullscreen(true);
  Widget* w3 = CreateTestWidget(gfx::Rect(600, 0, 100, 100));

  EXPECT_EQ(w1->GetNativeWindow()->GetRootWindow(),
            controllers[0]->GetRootWindow());
  EXPECT_EQ(w2->GetNativeWindow()->GetRootWindow(),
            controllers[0]->GetRootWindow());
  EXPECT_EQ(w3->GetNativeWindow()->GetRootWindow(),
            controllers[1]->GetRootWindow());

  w1->Activate();
  EXPECT_EQ(nullptr, controllers[0]->GetWindowForFullscreenMode());
  EXPECT_EQ(nullptr, controllers[1]->GetWindowForFullscreenMode());

  w2->Activate();
  EXPECT_EQ(w2->GetNativeWindow(),
            controllers[0]->GetWindowForFullscreenMode());
  EXPECT_EQ(nullptr, controllers[1]->GetWindowForFullscreenMode());

  // Verify that the first root window controller remains in fullscreen mode
  // when a window on the other display is activated.
  w3->Activate();
  EXPECT_EQ(w2->GetNativeWindow(),
            controllers[0]->GetWindowForFullscreenMode());
  EXPECT_EQ(nullptr, controllers[1]->GetWindowForFullscreenMode());
}

// Test that ForWindow() works with multiple displays and child widgets.
TEST_F(RootWindowControllerTest, ForWindow) {
  UpdateDisplay("600x500,600x500");
  Shell::RootWindowControllerList controllers =
      Shell::Get()->GetAllRootWindowControllers();
  ASSERT_EQ(2u, controllers.size());

  // Test a root window.
  EXPECT_EQ(controllers[0],
            RootWindowController::ForWindow(Shell::GetPrimaryRootWindow()));

  // Test a widget on the first display.
  Widget* w1 = CreateTestWidget(gfx::Rect(0, 0, 100, 100));
  EXPECT_EQ(controllers[0],
            RootWindowController::ForWindow(w1->GetNativeWindow()));

  // Test a child widget.
  Widget* w2 = Widget::CreateWindowWithParent(nullptr, w1->GetNativeWindow(),
                                              gfx::Rect(0, 0, 100, 100));
  EXPECT_EQ(controllers[0],
            RootWindowController::ForWindow(w2->GetNativeWindow()));

  // Test a widget on the second display.
  Widget* w3 = CreateTestWidget(gfx::Rect(600, 0, 100, 100));
  EXPECT_EQ(controllers[1],
            RootWindowController::ForWindow(w3->GetNativeWindow()));
}

// Test that user session window can't be focused if user session blocked by
// some overlapping UI.
TEST_F(RootWindowControllerTest, FocusBlockedWindow) {
  UpdateDisplay("600x500");
  RootWindowController* controller = Shell::GetPrimaryRootWindowController();
  aura::Window* lock_container =
      controller->GetContainer(kShellWindowId_LockScreenContainer);
  aura::Window* lock_window =
      Widget::CreateWindowWithParent(nullptr, lock_container,
                                     gfx::Rect(0, 0, 100, 100))
          ->GetNativeView();
  lock_window->Show();
  aura::Window* session_window =
      CreateTestWidget(gfx::Rect(0, 0, 100, 100))->GetNativeView();
  session_window->Show();

  for (int block_reason = FIRST_BLOCK_REASON;
       block_reason < NUMBER_OF_BLOCK_REASONS; ++block_reason) {
    BlockUserSession(static_cast<UserSessionBlockReason>(block_reason));
    lock_window->Focus();
    EXPECT_TRUE(lock_window->HasFocus());
    session_window->Focus();
    EXPECT_FALSE(session_window->HasFocus());
    UnblockUserSession();
  }
}

// Tracks whether OnWindowDestroying() has been invoked.
class DestroyedWindowObserver : public aura::WindowObserver {
 public:
  DestroyedWindowObserver() = default;

  DestroyedWindowObserver(const DestroyedWindowObserver&) = delete;
  DestroyedWindowObserver& operator=(const DestroyedWindowObserver&) = delete;

  ~DestroyedWindowObserver() override { Shutdown(); }

  void SetWindow(Window* window) {
    window_ = window;
    window->AddObserver(this);
  }

  bool destroyed() const { return destroyed_; }

  // WindowObserver overrides:
  void OnWindowDestroying(Window* window) override {
    destroyed_ = true;
    Shutdown();
  }

 private:
  void Shutdown() {
    if (!window_) {
      return;
    }
    window_->RemoveObserver(this);
    window_ = nullptr;
  }

  bool destroyed_ = false;
  raw_ptr<Window> window_{nullptr};
};

// Verifies shutdown doesn't delete windows that are not owned by the parent.
TEST_F(RootWindowControllerTest, DontDeleteWindowsNotOwnedByParent) {
  DestroyedWindowObserver observer1;
  aura::test::TestWindowDelegate delegate1;
  std::unique_ptr<aura::Window> window1 = std::make_unique<aura::Window>(
      &delegate1, aura::client::WINDOW_TYPE_CONTROL);
  window1->set_owned_by_parent(false);
  observer1.SetWindow(window1.get());
  window1->Init(ui::LAYER_NOT_DRAWN);
  aura::client::ParentWindowWithContext(
      window1.get(), Shell::GetPrimaryRootWindow(), gfx::Rect(),
      display::kInvalidDisplayId);

  DestroyedWindowObserver observer2;
  std::unique_ptr<aura::Window> window2 =
      std::make_unique<aura::Window>(nullptr);
  window2->set_owned_by_parent(false);
  observer2.SetWindow(window2.get());
  window2->Init(ui::LAYER_NOT_DRAWN);
  Shell::GetPrimaryRootWindow()->AddChild(window2.get());

  Shell::GetPrimaryRootWindowController()->CloseChildWindows();

  ASSERT_FALSE(observer1.destroyed());
  window1.reset();

  ASSERT_FALSE(observer2.destroyed());
  window2.reset();
}

// Verify that the context menu gets hidden when entering or exiting tablet
// mode.
TEST_F(RootWindowControllerTest, ContextMenuDisappearsInTabletMode) {
  RootWindowController* controller = Shell::GetPrimaryRootWindowController();

  // Open context menu.
  ui::test::EventGenerator generator(controller->GetRootWindow());
  generator.PressRightButton();
  generator.ReleaseRightButton();
  EXPECT_TRUE(controller->root_window_menu_model_adapter_);

  // Verify menu closes on entering tablet mode.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_FALSE(controller->root_window_menu_model_adapter_);

  // Open context menu.
  generator.PressRightButton();
  generator.ReleaseRightButton();
  EXPECT_TRUE(controller->root_window_menu_model_adapter_);

  // Verify menu closes on exiting tablet mode.
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_FALSE(controller->root_window_menu_model_adapter_);
}

class VirtualKeyboardRootWindowControllerTest
    : public RootWindowControllerTest {
 public:
  VirtualKeyboardRootWindowControllerTest() = default;

  VirtualKeyboardRootWindowControllerTest(
      const VirtualKeyboardRootWindowControllerTest&) = delete;
  VirtualKeyboardRootWindowControllerTest& operator=(
      const VirtualKeyboardRootWindowControllerTest&) = delete;

  ~VirtualKeyboardRootWindowControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    SetVirtualKeyboardEnabled(true);
  }

  void TearDown() override {
    SetVirtualKeyboardEnabled(false);
    AshTestBase::TearDown();
  }

  void EnsureCaretInWorkArea(const gfx::Rect& occluded_bounds) {
    keyboard::KeyboardUIController::Get()->EnsureCaretInWorkAreaForTest(
        occluded_bounds);
  }
};

class MockTextInputClient : public ui::DummyTextInputClient {
 public:
  MockTextInputClient() : ui::DummyTextInputClient(ui::TEXT_INPUT_TYPE_TEXT) {}

  MockTextInputClient(const MockTextInputClient&) = delete;
  MockTextInputClient& operator=(const MockTextInputClient&) = delete;

  void EnsureCaretNotInRect(const gfx::Rect& rect) override {
    caret_exclude_rect_ = rect;
  }

  const gfx::Rect& caret_exclude_rect() const { return caret_exclude_rect_; }

 private:
  gfx::Rect caret_exclude_rect_;
};

class TargetHitTestEventHandler : public ui::test::TestEventHandler {
 public:
  TargetHitTestEventHandler() = default;

  TargetHitTestEventHandler(const TargetHitTestEventHandler&) = delete;
  TargetHitTestEventHandler& operator=(const TargetHitTestEventHandler&) =
      delete;

  // ui::test::TestEventHandler overrides.
  void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->type() == ui::EventType::kMousePressed) {
      ui::test::TestEventHandler::OnMouseEvent(event);
    }
    event->StopPropagation();
  }
};

// Test for http://crbug.com/263599. Virtual keyboard should be able to receive
// events at blocked user session.
TEST_F(VirtualKeyboardRootWindowControllerTest,
       ClickVirtualKeyboardInBlockedWindow) {
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  aura::Window* keyboard_container =
      Shell::GetContainer(root_window, kShellWindowId_VirtualKeyboardContainer);
  ASSERT_TRUE(keyboard_container);
  keyboard_container->Show();

  aura::Window* contents_window =
      keyboard::KeyboardUIController::Get()->GetKeyboardWindow();
  contents_window->Show();
  keyboard::KeyboardUIController::Get()->ShowKeyboard(false);

  // Make sure no pending mouse events in the queue.
  base::RunLoop().RunUntilIdle();

  // TODO(oshima|yhanada): This simply make sure that targeting logic works, but
  // doesn't mean it'll deliver the event to the target. Fix this to make this
  // more reliable.
  ui::test::TestEventHandler handler;
  root_window->AddPreTargetHandler(&handler);

  ui::test::EventGenerator event_generator(root_window, contents_window);
  event_generator.ClickLeftButton();
  EXPECT_EQ(2, handler.num_mouse_events());

  for (int block_reason = FIRST_BLOCK_REASON;
       block_reason < NUMBER_OF_BLOCK_REASONS; ++block_reason) {
    SCOPED_TRACE(base::StringPrintf("Reason: %d", block_reason));
    BlockUserSession(static_cast<UserSessionBlockReason>(block_reason));
    handler.Reset();
    event_generator.ClickLeftButton();
    // Click may generate CAPTURE_CHANGED event so make sure it's more than
    // 2 (press,release);
    EXPECT_LE(2, handler.num_mouse_events());
    UnblockUserSession();
  }
  root_window->RemovePreTargetHandler(&handler);
}

// Test for crbug.com/342524. After user login, the work space should restore to
// full screen.
TEST_F(VirtualKeyboardRootWindowControllerTest, RestoreWorkspaceAfterLogin) {
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  aura::Window* keyboard_container =
      Shell::GetContainer(root_window, kShellWindowId_VirtualKeyboardContainer);
  keyboard_container->Show();
  auto* controller = keyboard::KeyboardUIController::Get();
  aura::Window* contents_window = controller->GetKeyboardWindow();
  contents_window->SetBounds(
      keyboard::test::KeyboardBoundsFromRootBounds(root_window->bounds(), 100));
  contents_window->Show();

  gfx::Rect before =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();

  if (!controller->IsKeyboardOverscrollEnabled()) {
    gfx::Rect after =
        display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
    EXPECT_LT(after, before);
  }

  // Mock a login user profile change to reinitialize the keyboard.
  SessionInfo info;
  info.state = session_manager::SessionState::ACTIVE;
  Shell::Get()->session_controller()->SetSessionInfo(info);
  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().work_area(),
            before);
}

// Ensure that system modal dialogs do not block events targeted at the virtual
// keyboard.
TEST_F(VirtualKeyboardRootWindowControllerTest, ClickWithActiveModalDialog) {
  auto* controller = keyboard::KeyboardUIController::Get();
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  ASSERT_EQ(root_window, controller->GetRootWindow());

  controller->ShowKeyboard(false /* locked */);
  ASSERT_TRUE(keyboard::test::WaitUntilShown());

  ui::test::TestEventHandler handler;
  root_window->AddPreTargetHandler(&handler);
  ui::test::EventGenerator root_window_event_generator(root_window);
  ui::test::EventGenerator keyboard_event_generator(
      root_window, controller->GetKeyboardWindow());

  views::Widget* modal_widget = CreateModalWidget(gfx::Rect(300, 10, 100, 100));

  // Verify that mouse events to the root window are block with a visble modal
  // dialog.
  root_window_event_generator.ClickLeftButton();
  EXPECT_EQ(0, handler.num_mouse_events());

  // Verify that event dispatch to the virtual keyboard is unblocked.
  keyboard_event_generator.ClickLeftButton();
  EXPECT_EQ(1, handler.num_mouse_events() / 2);

  modal_widget->Close();

  // Verify that mouse events are now unblocked to the root window.
  root_window_event_generator.ClickLeftButton();
  EXPECT_EQ(2, handler.num_mouse_events() / 2);
  root_window->RemovePreTargetHandler(&handler);
}

// Ensure that the visible area for scrolling the text caret excludes the
// region occluded by the on-screen keyboard.
TEST_F(VirtualKeyboardRootWindowControllerTest, EnsureCaretInWorkArea) {
  auto* keyboard_controller = keyboard::KeyboardUIController::Get();

  MockTextInputClient text_input_client;
  ui::InputMethod* input_method = keyboard_controller->GetInputMethodForTest();
  ASSERT_TRUE(input_method);
  input_method->SetFocusedTextInputClient(&text_input_client);

  aura::Window* root_window = Shell::GetPrimaryRootWindow();

  const int keyboard_height = 100;
  aura::Window* contents_window = keyboard_controller->GetKeyboardWindow();
  contents_window->SetBounds(keyboard::test::KeyboardBoundsFromRootBounds(
      root_window->bounds(), keyboard_height));
  contents_window->Show();

  EnsureCaretInWorkArea(contents_window->GetBoundsInScreen());
  ASSERT_EQ(root_window->bounds().width(),
            text_input_client.caret_exclude_rect().width());
  ASSERT_EQ(keyboard_height, text_input_client.caret_exclude_rect().height());

  input_method->SetFocusedTextInputClient(nullptr);
}

TEST_F(VirtualKeyboardRootWindowControllerTest,
       EnsureCaretInWorkAreaWithMultipleDisplays) {
  UpdateDisplay("600x500,600x500");
  const int64_t primary_display_id =
      display::Screen::GetScreen()->GetPrimaryDisplay().id();
  const int64_t secondary_display_id = GetSecondaryDisplay().id();
  ASSERT_NE(primary_display_id, secondary_display_id);

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(static_cast<size_t>(2), root_windows.size());
  aura::Window* primary_root_window = root_windows[0];
  aura::Window* secondary_root_window = root_windows[1];

  auto* keyboard_controller = keyboard::KeyboardUIController::Get();

  MockTextInputClient text_input_client;
  ui::InputMethod* input_method = keyboard_controller->GetInputMethodForTest();
  ASSERT_TRUE(input_method);
  input_method->SetFocusedTextInputClient(&text_input_client);

  const int keyboard_height = 100;
  // Check that the keyboard on the primary screen doesn't cover the window on
  // the secondary screen.
  aura::Window* contents_window = keyboard_controller->GetKeyboardWindow();
  contents_window->SetBounds(keyboard::test::KeyboardBoundsFromRootBounds(
      primary_root_window->bounds(), keyboard_height));
  contents_window->Show();

  EnsureCaretInWorkArea(contents_window->GetBoundsInScreen());
  EXPECT_TRUE(primary_root_window->GetBoundsInScreen().Contains(
      text_input_client.caret_exclude_rect()));
  EXPECT_EQ(primary_root_window->GetBoundsInScreen().width(),
            text_input_client.caret_exclude_rect().width());
  EXPECT_FALSE(secondary_root_window->GetBoundsInScreen().Contains(
      text_input_client.caret_exclude_rect()));

  // Move the keyboard into the secondary display and check that the keyboard
  // doesn't cover the window on the primary screen.
  keyboard_controller->ShowKeyboardInDisplay(GetSecondaryDisplay());
  contents_window->SetBounds(keyboard::test::KeyboardBoundsFromRootBounds(
      secondary_root_window->bounds(), keyboard_height));

  EnsureCaretInWorkArea(contents_window->GetBoundsInScreen());
  EXPECT_FALSE(primary_root_window->GetBoundsInScreen().Contains(
      text_input_client.caret_exclude_rect()));
  EXPECT_TRUE(secondary_root_window->GetBoundsInScreen().Contains(
      text_input_client.caret_exclude_rect()));
  EXPECT_EQ(secondary_root_window->GetBoundsInScreen().width(),
            text_input_client.caret_exclude_rect().width());

  input_method->SetFocusedTextInputClient(nullptr);
}

// Tests that the virtual keyboard does not block context menus. The virtual
// keyboard should appear in front of most content, but not context menus. See
// crbug/377180.
TEST_F(VirtualKeyboardRootWindowControllerTest, ZOrderTest) {
  UpdateDisplay("800x600");
  auto* keyboard_controller = keyboard::KeyboardUIController::Get();

  aura::Window* root_window = Shell::GetPrimaryRootWindow();

  const int keyboard_height = 200;
  aura::Window* contents_window = keyboard_controller->GetKeyboardWindow();
  gfx::Rect keyboard_bounds = keyboard::test::KeyboardBoundsFromRootBounds(
      root_window->bounds(), keyboard_height);
  contents_window->SetBounds(keyboard_bounds);
  contents_window->Show();

  ui::test::EventGenerator generator(root_window);

  // Cover the screen with two windows: a normal window on the left side and a
  // context menu on the right side. When the virtual keyboard is displayed it
  // partially occludes the normal window, but not the context menu. Compute
  // positions for generating synthetic click events to perform hit tests,
  // ensuring the correct window layering. 'top' is above the VK, whereas
  // 'bottom' lies within the VK. 'left' is centered in the normal window, and
  // 'right' is centered in the context menu.
  int window_height = keyboard_bounds.bottom();
  int window_width = keyboard_bounds.width() / 2;
  int left = window_width / 2;
  int right = 3 * window_width / 2;
  int top = keyboard_bounds.y() / 2;
  int bottom = window_height - keyboard_height / 2;

  // Normal window is partially occluded by the virtual keyboard.
  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> normal(
      CreateTestWindowInShellWithDelegateAndType(
          &delegate, aura::client::WINDOW_TYPE_NORMAL, 0,
          gfx::Rect(0, 0, window_width, window_height)));
  normal->set_owned_by_parent(false);
  normal->Show();
  TargetHitTestEventHandler normal_handler;
  normal->AddPreTargetHandler(&normal_handler);

  // Test that only the click on the top portion of the window is picked up. The
  // click on the bottom hits the virtual keyboard instead.
  generator.MoveMouseTo(left, top);
  generator.ClickLeftButton();
  EXPECT_EQ(1, normal_handler.num_mouse_events());
  generator.MoveMouseTo(left, bottom);
  generator.ClickLeftButton();
  EXPECT_EQ(1, normal_handler.num_mouse_events());

  // Menu overlaps virtual keyboard.
  aura::test::TestWindowDelegate delegate2;
  std::unique_ptr<aura::Window> menu(CreateTestWindowInShellWithDelegateAndType(
      &delegate2, aura::client::WINDOW_TYPE_MENU, 0,
      gfx::Rect(window_width, 0, window_width, window_height)));
  menu->set_owned_by_parent(false);
  menu->Show();
  TargetHitTestEventHandler menu_handler;
  menu->AddPreTargetHandler(&menu_handler);

  // Test that both clicks register.
  generator.MoveMouseTo(right, top);
  generator.ClickLeftButton();
  EXPECT_EQ(1, menu_handler.num_mouse_events());
  generator.MoveMouseTo(right, bottom);
  generator.ClickLeftButton();
  EXPECT_EQ(2, menu_handler.num_mouse_events());

  // Cleanup to ensure that the test windows are destroyed before their
  // delegates.
  normal.reset();
  menu.reset();
}

// Tests that the virtual keyboard correctly resizes with a change to display
// orientation. See crbug/417612.
TEST_F(VirtualKeyboardRootWindowControllerTest, DisplayRotation) {
  UpdateDisplay("800x600");
  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->ShowKeyboard(false);

  aura::Window* keyboard_window = keyboard_controller->GetKeyboardWindow();

  keyboard_window->SetBounds(gfx::Rect(0, 400, 800, 200));
  EXPECT_EQ("0,400 800x200", keyboard_window->bounds().ToString());

  UpdateDisplay("600x800");
  EXPECT_EQ("0,600 600x200", keyboard_window->bounds().ToString());
}

// Keeps a count of all the events a window receives.
class EventObserver : public ui::EventHandler {
 public:
  EventObserver() = default;

  EventObserver(const EventObserver&) = delete;
  EventObserver& operator=(const EventObserver&) = delete;

  ~EventObserver() override = default;

  int GetEventCount(ui::EventType type) { return event_counts_[type]; }
  void ResetAllEventCounts() { event_counts_.clear(); }

 private:
  // Overridden from ui::EventHandler:
  void OnEvent(ui::Event* event) override {
    ui::EventHandler::OnEvent(event);
    event_counts_[event->type()]++;
  }

  std::map<ui::EventType, int> event_counts_;
};

// Tests that tapping/clicking inside the keyboard does not give it focus.
TEST_F(VirtualKeyboardRootWindowControllerTest, ClickDoesNotFocusKeyboard) {
  aura::Window* root_window = Shell::GetPrimaryRootWindow();

  // Create a test window in the background with the same size as the screen.
  aura::test::EventCountDelegate delegate;
  std::unique_ptr<aura::Window> background_window(
      CreateTestWindowInShellWithDelegate(&delegate, 0, root_window->bounds()));
  background_window->Focus();
  EXPECT_TRUE(background_window->IsVisible());
  EXPECT_TRUE(background_window->HasFocus());

  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->ShowKeyboard(false);
  ASSERT_TRUE(keyboard::test::WaitUntilShown());
  aura::Window* keyboard_window = keyboard_controller->GetKeyboardWindow();
  EXPECT_FALSE(keyboard_window->HasFocus());

  // Click on the keyboard. Make sure the keyboard receives the event, but does
  // not get focus.
  EventObserver observer;
  keyboard_window->AddPreTargetHandler(&observer);

  ui::test::EventGenerator generator(root_window);
  generator.MoveMouseTo(keyboard_window->bounds().CenterPoint());
  generator.ClickLeftButton();
  EXPECT_TRUE(background_window->HasFocus());
  EXPECT_FALSE(keyboard_window->HasFocus());
  EXPECT_EQ("0 0", delegate.GetMouseButtonCountsAndReset());
  EXPECT_EQ(1, observer.GetEventCount(ui::EventType::kMousePressed));
  EXPECT_EQ(1, observer.GetEventCount(ui::EventType::kMouseReleased));

  // Click outside of the keyboard. It should reach the window behind.
  observer.ResetAllEventCounts();
  generator.MoveMouseTo(gfx::Point());
  generator.ClickLeftButton();
  EXPECT_EQ("1 1", delegate.GetMouseButtonCountsAndReset());
  EXPECT_EQ(0, observer.GetEventCount(ui::EventType::kMousePressed));
  EXPECT_EQ(0, observer.GetEventCount(ui::EventType::kMouseReleased));
  keyboard_window->RemovePreTargetHandler(&observer);
}

}  // namespace ash
