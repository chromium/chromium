// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/system_modal_container_layout_manager.h"

#include <memory>

#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/keyboard/ui/keyboard_ui.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/container_finder.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/run_loop.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/hit_test.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/test/capture_tracking_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

aura::Window* GetModalContainer() {
  return Shell::GetPrimaryRootWindowController()->GetContainer(
      ash::kShellWindowId_SystemModalContainer);
}

bool AllRootWindowsHaveModalBackgroundsForContainer(int container_id) {
  aura::Window::Windows containers =
      GetContainersForAllRootWindows(container_id);
  bool has_modal_screen = !containers.empty();
  for (aura::Window* container : containers) {
    has_modal_screen &= static_cast<SystemModalContainerLayoutManager*>(
                            container->layout_manager())
                            ->has_window_dimmer();
  }
  return has_modal_screen;
}

bool AllRootWindowsHaveLockedModalBackgrounds() {
  return AllRootWindowsHaveModalBackgroundsForContainer(
      kShellWindowId_LockSystemModalContainer);
}

bool AllRootWindowsHaveModalBackgrounds() {
  return AllRootWindowsHaveModalBackgroundsForContainer(
      kShellWindowId_SystemModalContainer);
}

class TestWindow : public views::WidgetDelegateView {
 public:
  explicit TestWindow(bool modal) {
    SetModalType(modal ? ui::mojom::ModalType::kSystem
                       : ui::mojom::ModalType::kNone);
    SetPreferredSize(gfx::Size(50, 50));
  }

  TestWindow(const TestWindow&) = delete;
  TestWindow& operator=(const TestWindow&) = delete;

  ~TestWindow() override = default;

  // The window needs be closed from widget in order for
  // aura::client::kModalKey property to be reset.
  static void CloseTestWindow(aura::Window* window) {
    views::Widget::GetWidgetForNativeWindow(window)->Close();
  }
};

class EventTestWindow : public TestWindow {
 public:
  explicit EventTestWindow(bool modal) : TestWindow(modal), mouse_presses_(0) {}

  EventTestWindow(const EventTestWindow&) = delete;
  EventTestWindow& operator=(const EventTestWindow&) = delete;

  ~EventTestWindow() override = default;

  aura::Window* ShowTestWindowWithContext(aura::Window* context) {
    views::Widget* widget =
        views::Widget::CreateWindowWithContext(this, context);
    widget->Show();
    return widget->GetNativeView();
  }

  aura::Window* ShowTestWindowWithParent(aura::Window* parent) {
    DCHECK(parent);
    views::Widget* widget = views::Widget::CreateWindowWithParent(this, parent);
    widget->Show();
    return widget->GetNativeView();
  }

  // Overridden from views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override {
    mouse_presses_++;
    return false;
  }

  int mouse_presses() const { return mouse_presses_; }

 private:
  int mouse_presses_;
};

class TransientWindowObserver : public aura::WindowObserver {
 public:
  TransientWindowObserver() : destroyed_(false) {}

  TransientWindowObserver(const TransientWindowObserver&) = delete;
  TransientWindowObserver& operator=(const TransientWindowObserver&) = delete;

  ~TransientWindowObserver() override = default;

  bool destroyed() const { return destroyed_; }

  // Overridden from aura::WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override { destroyed_ = true; }

 private:
  bool destroyed_;
};

}  // namespace

class SystemModalContainerLayoutManagerTest : public AshTestBase {
 public:
  void SetUp() override {
    // Allow a virtual keyboard (and initialize it per default).
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    AshTestBase::SetUp();
  }

  aura::Window* ShowToplevelTestWindow(bool modal) {
    views::Widget* widget = views::Widget::CreateWindowWithContext(
        new TestWindow(modal), GetContext());
    widget->Show();
    return widget->GetNativeView();
  }

  aura::Window* ShowTestWindowWithParent(aura::Window* parent, bool modal) {
    views::Widget* widget =
        views::Widget::CreateWindowWithParent(new TestWindow(modal), parent);
    widget->Show();
    return widget->GetNativeView();
  }

  // Show or hide the keyboard.
  void ShowKeyboard(bool show) {
    auto* keyboard = keyboard::KeyboardUIController::Get();
    ASSERT_TRUE(keyboard->IsEnabled());
    if (show == keyboard->IsKeyboardVisible())
      return;

    if (show) {
      keyboard->ShowKeyboard(true /* lock */);
      ASSERT_TRUE(keyboard::test::WaitUntilShown());
    } else {
      keyboard->HideKeyboardByUser();
    }

    DCHECK_EQ(show, keyboard->IsKeyboardVisible());
  }
};

TEST_F(SystemModalContainerLayoutManagerTest, NonModalTransient) {
  std::unique_ptr<aura::Window> parent(ShowToplevelTestWindow(false));
  aura::Window* transient = ShowTestWindowWithParent(parent.get(), false);
  TransientWindowObserver destruction_observer;
  transient->AddObserver(&destruction_observer);

  EXPECT_EQ(parent.get(), ::wm::GetTransientParent(transient));
  EXPECT_EQ(parent->parent(), transient->parent());

  // The transient should be destroyed with its parent.
  parent.reset();
  EXPECT_TRUE(destruction_observer.destroyed());
}

TEST_F(SystemModalContainerLayoutManagerTest, ModalTransient) {
  std::unique_ptr<aura::Window> parent(ShowToplevelTestWindow(false));
  // parent should be active.
  EXPECT_TRUE(wm::IsActiveWindow(parent.get()));
  aura::Window* t1 = ShowTestWindowWithParent(parent.get(), true);

  TransientWindowObserver do1;
  t1->AddObserver(&do1);

  EXPECT_EQ(parent.get(), ::wm::GetTransientParent(t1));
  EXPECT_EQ(GetModalContainer(), t1->parent());

  // t1 should now be active.
  EXPECT_TRUE(wm::IsActiveWindow(t1));

  // Attempting to click the parent should result in no activation change.
  ui::test::EventGenerator e1(Shell::GetPrimaryRootWindow(), parent.get());
  e1.ClickLeftButton();
  EXPECT_TRUE(wm::IsActiveWindow(t1));

  // Now open another modal transient parented to the original modal transient.
  aura::Window* t2 = ShowTestWindowWithParent(t1, true);
  TransientWindowObserver do2;
  t2->AddObserver(&do2);

  EXPECT_TRUE(wm::IsActiveWindow(t2));

  EXPECT_EQ(t1, ::wm::GetTransientParent(t2));
  EXPECT_EQ(GetModalContainer(), t2->parent());

  // t2 should still be active, even after clicking on t1.
  ui::test::EventGenerator e2(Shell::GetPrimaryRootWindow(), t1);
  e2.ClickLeftButton();
  EXPECT_TRUE(wm::IsActiveWindow(t2));

  // Both transients should be destroyed with parent.
  parent.reset();
  EXPECT_TRUE(do1.destroyed());
  EXPECT_TRUE(do2.destroyed());
}

TEST_F(SystemModalContainerLayoutManagerTest, ModalNonTransient) {
  std::unique_ptr<aura::Window> t1(ShowToplevelTestWindow(true));
  // parent should be active.
  EXPECT_TRUE(wm::IsActiveWindow(t1.get()));
  TransientWindowObserver do1;
  t1->AddObserver(&do1);

  EXPECT_EQ(NULL, ::wm::GetTransientParent(t1.get()));
  EXPECT_EQ(GetModalContainer(), t1->parent());

  // t1 should now be active.
  EXPECT_TRUE(wm::IsActiveWindow(t1.get()));

  // Attempting to click the parent should result in no activation change.
  ui::test::EventGenerator e1(Shell::GetPrimaryRootWindow(),
                              Shell::GetPrimaryRootWindow());
  e1.ClickLeftButton();
  EXPECT_TRUE(wm::IsActiveWindow(t1.get()));

  // Now open another modal transient parented to the original modal transient.
  aura::Window* t2 = ShowTestWindowWithParent(t1.get(), true);
  TransientWindowObserver do2;
  t2->AddObserver(&do2);

  EXPECT_TRUE(wm::IsActiveWindow(t2));

  EXPECT_EQ(t1.get(), ::wm::GetTransientParent(t2));
  EXPECT_EQ(GetModalContainer(), t2->parent());

  // t2 should still be active, even after clicking on t1.
  ui::test::EventGenerator e2(Shell::GetPrimaryRootWindow(), t1.get());
  e2.ClickLeftButton();
  EXPECT_TRUE(wm::IsActiveWindow(t2));

  // Both transients should be destroyed with parent.
  t1.reset();
  EXPECT_TRUE(do1.destroyed());
  EXPECT_TRUE(do2.destroyed());
}

// Tests that we can activate an unrelated window after a modal window is closed
// for a window.
TEST_F(SystemModalContainerLayoutManagerTest, CanActivateAfterEndModalSession) {
  std::unique_ptr<aura::Window> unrelated(ShowToplevelTestWindow(false));
  unrelated->SetBounds(gfx::Rect(100, 100, 50, 50));
  std::unique_ptr<aura::Window> parent(ShowToplevelTestWindow(false));
  // parent should be active.
  EXPECT_TRUE(wm::IsActiveWindow(parent.get()));

  std::unique_ptr<aura::Window> transient(
      ShowTestWindowWithParent(parent.get(), true));
  // t1 should now be active.
  EXPECT_TRUE(wm::IsActiveWindow(transient.get()));

  // Attempting to click the parent should result in no activation change.
  ui::test::EventGenerator e1(Shell::GetPrimaryRootWindow(), parent.get());
  e1.ClickLeftButton();
  EXPECT_TRUE(wm::IsActiveWindow(transient.get()));

  // Now close the transient.
  transient->Hide();
  TestWindow::CloseTestWindow(transient.release());

  base::RunLoop().RunUntilIdle();

  // parent should now be active again.
  EXPECT_TRUE(wm::IsActiveWindow(parent.get()));

  // Attempting to click unrelated should activate it.
  ui::test::EventGenerator e2(Shell::GetPrimaryRootWindow(), unrelated.get());
  e2.ClickLeftButton();
  EXPECT_TRUE(wm::IsActiveWindow(unrelated.get()));
}

TEST_F(SystemModalContainerLayoutManagerTest, EventFocusContainers) {
  // Create a normal window and attempt to receive a click event.
  EventTestWindow* main_delegate = new EventTestWindow(false);
  std::unique_ptr<aura::Window> main(
      main_delegate->ShowTestWindowWithContext(GetContext()));
  EXPECT_TRUE(wm::IsActiveWindow(main.get()));
  ui::test::EventGenerator e1(Shell::GetPrimaryRootWindow(), main.get());
  e1.ClickLeftButton();
  EXPECT_EQ(1, main_delegate->mouse_presses());

  EventTestWindow* status_delegate = new EventTestWindow(false);
  std::unique_ptr<aura::Window> status_control(
      status_delegate->ShowTestWindowWithParent(
          Shell::GetPrimaryRootWindowController()->GetContainer(
              kShellWindowId_ShelfContainer)));
  status_control->SetBounds(main->bounds());

  // Make sure that status window can receive event.
  e1.ClickLeftButton();
  EXPECT_EQ(1, status_delegate->mouse_presses());

  // Create a modal window for the main window and verify that the main window
  // no longer receives mouse events.
  EventTestWindow* transient_delegate = new EventTestWindow(true);
  aura::Window* transient =
      transient_delegate->ShowTestWindowWithParent(main.get());
  EXPECT_TRUE(wm::IsActiveWindow(transient));
  e1.ClickLeftButton();

  EXPECT_EQ(0, transient_delegate->mouse_presses());
  EXPECT_EQ(1, status_delegate->mouse_presses());

  status_control->Hide();
  e1.ClickLeftButton();
  EXPECT_EQ(1, transient_delegate->mouse_presses());
  EXPECT_EQ(1, status_delegate->mouse_presses());

  for (int block_reason = FIRST_BLOCK_REASON;
       block_reason < NUMBER_OF_BLOCK_REASONS; ++block_reason) {
    // Create a window in the lock screen container and ensure that it receives
    // the mouse event instead of the modal window (crbug.com/110920).
    BlockUserSession(static_cast<UserSessionBlockReason>(block_reason));

    EventTestWindow* lock_delegate = new EventTestWindow(false);
    std::unique_ptr<aura::Window> lock(lock_delegate->ShowTestWindowWithParent(
        Shell::GetPrimaryRootWindowController()->GetContainer(
            kShellWindowId_LockScreenContainer)));
    // BlockUserSession could change the workspace size. Make sure |lock| has
    // the same bounds as |main| so that |lock| gets the generated mouse events.
    lock->SetBounds(main->bounds());

    EXPECT_TRUE(wm::IsActiveWindow(lock.get()));
    e1.ClickLeftButton();
    EXPECT_EQ(1, lock_delegate->mouse_presses());
    EXPECT_EQ(1, status_delegate->mouse_presses());

    // Make sure that a modal container created by the lock screen can still
    // receive mouse events.
    EventTestWindow* lock_modal_delegate = new EventTestWindow(true);
    aura::Window* lock_modal =
        lock_modal_delegate->ShowTestWindowWithParent(lock.get());
    EXPECT_TRUE(wm::IsActiveWindow(lock_modal));
    e1.ClickLeftButton();

    // Verify that none of the other containers received any more mouse presses.
    EXPECT_EQ(1, lock_modal_delegate->mouse_presses());
    EXPECT_EQ(1, lock_delegate->mouse_presses());
    EXPECT_EQ(1, status_delegate->mouse_presses());
    EXPECT_EQ(1, main_delegate->mouse_presses());
    EXPECT_EQ(1, transient_delegate->mouse_presses());

    // Showing status will block events.
    status_control->Show();
    e1.ClickLeftButton();

    // Verify that none of the other containers received any more mouse presses.
    EXPECT_EQ(1, lock_modal_delegate->mouse_presses());
    EXPECT_EQ(1, lock_delegate->mouse_presses());
    EXPECT_EQ(1, status_delegate->mouse_presses());
    EXPECT_EQ(1, main_delegate->mouse_presses());
    EXPECT_EQ(1, transient_delegate->mouse_presses());
    status_control->Hide();

    // Close |lock| before unlocking so that Shell::OnLockStateChanged does
    // not DCHECK on finding a system modal in Lock layer when unlocked.
    lock.reset();

    UnblockUserSession();
  }
}

// Test if a user can still click the status area in lock screen
// even if the user session has system modal dialog.
TEST_F(SystemModalContainerLayoutManagerTest,
       ClickStatusWithModalDialogInLockedState) {
  // Create a normal window and attempt to receive a click event.
  EventTestWindow* main_delegate = new EventTestWindow(false);
  std::unique_ptr<aura::Window> main(
      main_delegate->ShowTestWindowWithContext(GetContext()));
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(), main.get());

  // A window for status area to test if it could receive an event
  // under each condition.
  EventTestWindow* status_delegate = new EventTestWindow(false);
  std::unique_ptr<aura::Window> status_control(
      status_delegate->ShowTestWindowWithParent(
          Shell::GetPrimaryRootWindowController()->GetContainer(
              kShellWindowId_ShelfContainer)));

  status_control->SetBounds(main->bounds());
  status_control->SetName("Status Control");

  // Events are blocked on all windows because status window is above the modal
  // window.
  EventTestWindow* modal_delegate = new EventTestWindow(true);
  aura::Window* modal = modal_delegate->ShowTestWindowWithParent(main.get());
  modal->SetName("Modal Window");
  EXPECT_TRUE(wm::IsActiveWindow(modal));
  generator.ClickLeftButton();

  EXPECT_EQ(0, main_delegate->mouse_presses());
  EXPECT_EQ(0, modal_delegate->mouse_presses());
  EXPECT_EQ(0, status_delegate->mouse_presses());

  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);

  // In lock screen, a window in status area container should be able to receive
  // the event even with a system modal dialog in the user session.
  generator.ClickLeftButton();
  EXPECT_EQ(0, main_delegate->mouse_presses());
  EXPECT_EQ(0, modal_delegate->mouse_presses());
  EXPECT_EQ(1, status_delegate->mouse_presses());
}

TEST_F(SystemModalContainerLayoutManagerTest, ModalTransientChildEvents) {
  // Create a normal window and attempt to receive a click event.
  EventTestWindow* main_delegate = new EventTestWindow(false);
  std::unique_ptr<aura::Window> main(
      main_delegate->ShowTestWindowWithContext(GetContext()));
  EXPECT_TRUE(wm::IsActiveWindow(main.get()));
  ui::test::EventGenerator e1(Shell::GetPrimaryRootWindow(), main.get());
  e1.ClickLeftButton();
  EXPECT_EQ(1, main_delegate->mouse_presses());

  // Create a modal window for the main window and verify that the main window
  // no longer receives mouse events.
  EventTestWindow* modal1_delegate = new EventTestWindow(true);
  aura::Window* modal1 = modal1_delegate->ShowTestWindowWithParent(main.get());
  EXPECT_TRUE(wm::IsActiveWindow(modal1));
  e1.ClickLeftButton();
  EXPECT_EQ(1, modal1_delegate->mouse_presses());
  EXPECT_EQ(1, main_delegate->mouse_presses());

  // Create a non-modal transient child of modal1 and verify it receives mouse
  // events instead of main and modal1.
  EventTestWindow* modal1_transient_delegate = new EventTestWindow(false);
  aura::Window* modal1_transient =
      modal1_transient_delegate->ShowTestWindowWithParent(modal1);
  EXPECT_TRUE(wm::IsActiveWindow(modal1_transient));
  e1.ClickLeftButton();
  EXPECT_EQ(1, modal1_transient_delegate->mouse_presses());
  EXPECT_EQ(1, modal1_delegate->mouse_presses());
  EXPECT_EQ(1, main_delegate->mouse_presses());

  // Create a child control for modal1_transient and it receives mouse events.
  aura::test::EventCountDelegate control_delegate;
  control_delegate.set_window_component(HTCLIENT);
  std::unique_ptr<aura::Window> child =
      std::make_unique<aura::Window>(&control_delegate);
  child->SetType(aura::client::WINDOW_TYPE_CONTROL);
  child->Init(ui::LAYER_TEXTURED);
  modal1_transient->AddChild(child.get());
  child->SetBounds(gfx::Rect(100, 100));
  child->Show();
  e1.ClickLeftButton();
  EXPECT_EQ("1 1", control_delegate.GetMouseButtonCountsAndReset());
  EXPECT_EQ(1, modal1_transient_delegate->mouse_presses());
  EXPECT_EQ(1, modal1_delegate->mouse_presses());
  EXPECT_EQ(1, main_delegate->mouse_presses());

  // Create another modal window for the main window and it receives mouse
  // events instead of all others.
  EventTestWindow* modal2_delegate = new EventTestWindow(true);
  aura::Window* modal2 = modal2_delegate->ShowTestWindowWithParent(main.get());
  EXPECT_TRUE(wm::IsActiveWindow(modal2));
  e1.ClickLeftButton();
  EXPECT_EQ(1, modal2_delegate->mouse_presses());
  EXPECT_EQ("0 0", control_delegate.GetMouseButtonCountsAndReset());
  EXPECT_EQ(1, modal1_transient_delegate->mouse_presses());
  EXPECT_EQ(1, modal1_delegate->mouse_presses());
  EXPECT_EQ(1, main_delegate->mouse_presses());

  // Create a non-modal transient child of modal2 and it receives events.
  EventTestWindow* modal2_transient_delegate = new EventTestWindow(false);
  aura::Window* modal2_transient =
      modal2_transient_delegate->ShowTestWindowWithParent(modal2);
  EXPECT_TRUE(wm::IsActiveWindow(modal2_transient));
  e1.ClickLeftButton();
  EXPECT_EQ(1, modal2_transient_delegate->mouse_presses());
  EXPECT_EQ(1, modal2_delegate->mouse_presses());
  EXPECT_EQ("0 0", control_delegate.GetMouseButtonCountsAndReset());
  EXPECT_EQ(1, modal1_transient_delegate->mouse_presses());
  EXPECT_EQ(1, modal1_delegate->mouse_presses());
  EXPECT_EQ(1, main_delegate->mouse_presses());

  // Create a non-modal transient grandchild of modal2 and it receives events.
  EventTestWindow* modal2_transient_grandchild_delegate =
      new EventTestWindow(false);
  aura::Window* modal2_transient_grandchild =
      modal2_transient_grandchild_delegate->ShowTestWindowWithParent(
          modal2_transient);
  EXPECT_TRUE(wm::IsActiveWindow(modal2_transient_grandchild));
  e1.ClickLeftButton();
  EXPECT_EQ(1, modal2_transient_grandchild_delegate->mouse_presses());
  EXPECT_EQ(1, modal2_transient_delegate->mouse_presses());
  EXPECT_EQ(1, modal2_delegate->mouse_presses());
  EXPECT_EQ("0 0", control_delegate.GetMouseButtonCountsAndReset());
  EXPECT_EQ(1, modal1_transient_delegate->mouse_presses());
  EXPECT_EQ(1, modal1_delegate->mouse_presses());
  EXPECT_EQ(1, main_delegate->mouse_presses());
}

// Makes sure we don't crash if a modal window is shown while the parent window
// is hidden.
TEST_F(SystemModalContainerLayoutManagerTest, ShowModalWhileHidden) {
  // Hide the lock screen.
  Shell::GetPrimaryRootWindowController()
      ->GetContainer(kShellWindowId_SystemModalContainer)
      ->layer()
      ->SetOpacity(0);

  // Create a modal window.
  std::unique_ptr<aura::Window> parent(ShowToplevelTestWindow(false));
  std::unique_ptr<aura::Window> modal_window(
      ShowTestWindowWithParent(parent.get(), true));
}

// Verifies we generate a capture lost when showing a modal window.
TEST_F(SystemModalContainerLayoutManagerTest, ChangeCapture) {
  std::unique_ptr<aura::Window> widget_window(ShowToplevelTestWindow(false));
  views::test::CaptureTrackingView* view = new views::test::CaptureTrackingView;
  views::View* client_view =
      views::Widget::GetWidgetForNativeView(widget_window.get())
          ->non_client_view()
          ->client_view();
  client_view->AddChildView(view);
  view->SetBoundsRect(client_view->bounds());

  gfx::Point center(view->width() / 2, view->height() / 2);
  views::View::ConvertPointToScreen(view, &center);
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(), center);
  generator.PressLeftButton();
  EXPECT_TRUE(view->got_press());
  std::unique_ptr<aura::Window> modal_window(
      ShowTestWindowWithParent(widget_window.get(), true));
  EXPECT_TRUE(view->got_capture_lost());
}

// Verifies that the window gets moved into the visible screen area upon screen
// resize.
TEST_F(SystemModalContainerLayoutManagerTest, KeepVisible) {
  GetModalContainer()->SetBounds(gfx::Rect(0, 0, 1024, 768));
  std::unique_ptr<aura::Window> main(
      ShowTestWindowWithParent(GetModalContainer(), true));
  const int shelf_height = ShelfConfig::Get()->shelf_size();
  main->SetBounds(gfx::Rect(924, 668 - shelf_height, 100, 100));
  // We set now the bounds of the root window to something new which will
  // Then trigger the repos operation.
  GetModalContainer()->SetBounds(gfx::Rect(0, 0, 800, 600));

  gfx::Rect bounds = main->bounds();
  EXPECT_EQ(bounds, gfx::Rect(700, 500 - shelf_height, 100, 100));
}

// Verifies that centered windows will remain centered after the visible screen
// area changed.
TEST_F(SystemModalContainerLayoutManagerTest, KeepCentered) {
  GetModalContainer()->SetBounds(gfx::Rect(0, 0, 800, 600));
  std::unique_ptr<aura::Window> main(
      ShowTestWindowWithParent(GetModalContainer(), true));
  // Center the window.
  main->SetBounds(gfx::Rect((800 - 512) / 2, (600 - 256) / 2, 512, 256));

  // We set now the bounds of the root window to something new which will
  // Then trigger the reposition operation.
  GetModalContainer()->SetBounds(gfx::Rect(0, 0, 600, 400));

  // The window should still be centered.
  gfx::Rect bounds = main->bounds();
  EXPECT_EQ(bounds.ToString(),
            gfx::Rect((600 - 512) / 2, (400 - 256) / 2, 512, 256).ToString());
}

// Verifies that centered windows will remain centered in the secondary screen
// with correct global position in screen coordinate system and local position
// relative to root window.
TEST_F(SystemModalContainerLayoutManagerTest, KeepCenteredSecondaryScreen) {
  UpdateDisplay("800x600,800+0-800x600");

  // Create a lock modal window in a lock state.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::OOBE);
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  aura::Window* secondary_display_modal_container = Shell::GetContainer(
      root_windows[1], kShellWindowId_LockSystemModalContainer);
  secondary_display_modal_container->SetBounds(gfx::Rect(0, 0, 800, 600));
  std::unique_ptr<aura::Window> modal(
      ShowTestWindowWithParent(secondary_display_modal_container, true));

  // Center the window.
  modal->SetBounds(gfx::Rect((800 - 512) / 2, (600 - 256) / 2, 512, 256));

  // We set now the bounds of the root window to something new which will
  // Then trigger the reposition operation.
  secondary_display_modal_container->SetBounds(gfx::Rect(0, 0, 600, 400));

  // The window should still be centered with global and local coordinates.
  gfx::Rect modal_bounds_in_screen = modal->GetBoundsInScreen();
  EXPECT_EQ(
      modal_bounds_in_screen.ToString(),
      gfx::Rect(800 + (600 - 512) / 2, (400 - 256) / 2, 512, 256).ToString());
  gfx::Rect modal_bounds_in_root = modal->bounds();
  EXPECT_EQ(modal_bounds_in_root.ToString(),
            gfx::Rect((600 - 512) / 2, (400 - 256) / 2, 512, 256).ToString());
}

TEST_F(SystemModalContainerLayoutManagerTest, ShowNormalBackgroundOrLocked) {
  std::unique_ptr<aura::Window> parent(ShowToplevelTestWindow(false));
  std::unique_ptr<aura::Window> modal_window(
      ShowTestWindowWithParent(parent.get(), true));

  // Normal system modal window.  Shows normal system modal background and not
  // locked.
  EXPECT_TRUE(AllRootWindowsHaveModalBackgrounds());
  EXPECT_FALSE(AllRootWindowsHaveLockedModalBackgrounds());

  TestWindow::CloseTestWindow(modal_window.release());
  EXPECT_FALSE(AllRootWindowsHaveModalBackgrounds());
  EXPECT_FALSE(AllRootWindowsHaveLockedModalBackgrounds());

  for (int block_reason = FIRST_BLOCK_REASON;
       block_reason < NUMBER_OF_BLOCK_REASONS; ++block_reason) {
    // Normal system modal window while blocked.  Shows blocked system modal
    // background.
    BlockUserSession(static_cast<UserSessionBlockReason>(block_reason));
    std::unique_ptr<aura::Window> lock_parent(ShowTestWindowWithParent(
        Shell::GetPrimaryRootWindowController()->GetContainer(
            kShellWindowId_LockScreenContainer),
        false));
    std::unique_ptr<aura::Window> lock_modal_window(
        ShowTestWindowWithParent(lock_parent.get(), true));
    EXPECT_FALSE(AllRootWindowsHaveModalBackgrounds());
    EXPECT_TRUE(AllRootWindowsHaveLockedModalBackgrounds());
    TestWindow::CloseTestWindow(lock_modal_window.release());

    // Normal system modal window while blocked, but it belongs to the normal
    // window.  Shouldn't show blocked system modal background, but normal.
    std::unique_ptr<aura::Window> modal_window_while_blocked(
        ShowTestWindowWithParent(parent.get(), true));
    EXPECT_TRUE(AllRootWindowsHaveModalBackgrounds());
    EXPECT_FALSE(AllRootWindowsHaveLockedModalBackgrounds());
    TestWindow::CloseTestWindow(modal_window_while_blocked.release());

    // Close |lock_parent| before unlocking so that Shell::OnLockStateChanged
    // does not DCHECK on finding a system modal in Lock layer when unlocked.
    lock_parent.reset();

    UnblockUserSession();
    // Here we should check the behavior of the locked system modal dialog when
    // unlocked, but such case isn't handled very well right now.
    // See crbug.com/157660
    // TODO(mukai): add the test case when the bug is fixed.
  }
}

TEST_F(SystemModalContainerLayoutManagerTest, MultiDisplays) {
  UpdateDisplay("600x500,600x500");

  std::unique_ptr<aura::Window> normal(ShowToplevelTestWindow(false));
  normal->SetBounds(gfx::Rect(100, 100, 50, 50));

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(2U, root_windows.size());
  aura::Window* container1 =
      Shell::GetContainer(root_windows[0], kShellWindowId_SystemModalContainer);
  aura::Window* container2 =
      Shell::GetContainer(root_windows[1], kShellWindowId_SystemModalContainer);

  std::unique_ptr<aura::Window> modal1(
      ShowTestWindowWithParent(container1, true));
  EXPECT_TRUE(AllRootWindowsHaveModalBackgrounds());
  EXPECT_TRUE(wm::IsActiveWindow(modal1.get()));

  std::unique_ptr<aura::Window> modal11(
      ShowTestWindowWithParent(container1, true));
  EXPECT_TRUE(wm::IsActiveWindow(modal11.get()));

  std::unique_ptr<aura::Window> modal2(
      ShowTestWindowWithParent(container2, true));
  EXPECT_TRUE(wm::IsActiveWindow(modal2.get()));

  // Sanity check if they're on the correct containers.
  EXPECT_EQ(container1, modal1->parent());
  EXPECT_EQ(container1, modal11->parent());
  EXPECT_EQ(container2, modal2->parent());

  TestWindow::CloseTestWindow(modal2.release());
  EXPECT_TRUE(AllRootWindowsHaveModalBackgrounds());
  EXPECT_TRUE(wm::IsActiveWindow(modal11.get()));

  TestWindow::CloseTestWindow(modal11.release());
  EXPECT_TRUE(AllRootWindowsHaveModalBackgrounds());
  EXPECT_TRUE(wm::IsActiveWindow(modal1.get()));

  UpdateDisplay("600x500");
  EXPECT_TRUE(AllRootWindowsHaveModalBackgrounds());
  EXPECT_TRUE(wm::IsActiveWindow(modal1.get()));

  UpdateDisplay("600x500,700x600");
  EXPECT_TRUE(AllRootWindowsHaveModalBackgrounds());
  EXPECT_TRUE(wm::IsActiveWindow(modal1.get()));

  // No more modal screen.
  modal1->Hide();
  TestWindow::CloseTestWindow(modal1.release());
  EXPECT_FALSE(AllRootWindowsHaveModalBackgrounds());
  EXPECT_TRUE(wm::IsActiveWindow(normal.get()));
}

// Test that with the visible keyboard, an existing system modal dialog gets
// positioned into the visible area.
TEST_F(SystemModalContainerLayoutManagerTest,
       SystemModalDialogGetPushedFromKeyboard) {
  const gfx::Rect& container_bounds = GetModalContainer()->bounds();
  // Place the window at the bottom of the screen.
  gfx::Size modal_size(100, 100);
  gfx::Point modal_origin = gfx::Point(
      (container_bounds.right() - modal_size.width()) / 2,  // X centered
      container_bounds.bottom() - modal_size.height());     // at bottom
  gfx::Rect modal_bounds = gfx::Rect(modal_origin, modal_size);

  // Create a modal window.
  std::unique_ptr<aura::Window> parent(ShowToplevelTestWindow(false));
  std::unique_ptr<aura::Window> modal_window(
      ShowTestWindowWithParent(parent.get(), true));
  modal_window->SetBounds(modal_bounds);

  EXPECT_EQ(modal_bounds.ToString(), modal_window->bounds().ToString());

  // The keyboard gets shown and the dialog should get pushed.
  ShowKeyboard(true);
  EXPECT_NE(modal_bounds.ToString(), modal_window->bounds().ToString());
  EXPECT_GT(modal_bounds.y(), modal_window->bounds().y());
  EXPECT_EQ(modal_size.ToString(), modal_window->bounds().size().ToString());
  EXPECT_EQ(modal_origin.x(), modal_window->bounds().x());

  // After the keyboard is gone, the window will remain where it was.
  ShowKeyboard(false);
  EXPECT_NE(modal_bounds.ToString(), modal_window->bounds().ToString());
  EXPECT_EQ(modal_size.ToString(), modal_window->bounds().size().ToString());
  EXPECT_EQ(modal_origin.x(), modal_window->bounds().x());
}

// Test that windows will not get cropped through the visible virtual keyboard -
// if centered.
TEST_F(SystemModalContainerLayoutManagerTest,
       SystemModalDialogGetPushedButNotCroppedFromKeyboard) {
  const gfx::Rect& container_bounds = GetModalContainer()->bounds();
  const gfx::Size screen_size = Shell::GetPrimaryRootWindow()->bounds().size();
  // Place the window at the bottom of the screen.
  gfx::Size modal_size(100, screen_size.height() - 70);
  gfx::Point modal_origin = gfx::Point(
      (container_bounds.right() - modal_size.width()) / 2,  // X centered
      container_bounds.bottom() - modal_size.height());     // at bottom
  gfx::Rect modal_bounds = gfx::Rect(modal_origin, modal_size);

  // Create a modal window.
  std::unique_ptr<aura::Window> parent(ShowToplevelTestWindow(false));
  std::unique_ptr<aura::Window> modal_window(
      ShowTestWindowWithParent(parent.get(), true));
  modal_window->SetBounds(modal_bounds);

  EXPECT_EQ(modal_bounds.ToString(), modal_window->bounds().ToString());

  // The keyboard gets shown and the dialog should get pushed up, but not get
  // cropped (and aligned to the top).
  ShowKeyboard(true);
  EXPECT_EQ(modal_size.ToString(), modal_window->bounds().size().ToString());
  EXPECT_EQ(modal_origin.x(), modal_window->bounds().x());
  EXPECT_EQ(0, modal_window->bounds().y());

  ShowKeyboard(false);
}

// Test that windows will not get cropped through the visible virtual keyboard -
// if not centered.
TEST_F(SystemModalContainerLayoutManagerTest,
       SystemModalDialogGetPushedButNotCroppedFromKeyboardIfNotCentered) {
  const gfx::Size screen_size = Shell::GetPrimaryRootWindow()->bounds().size();
  // Place the window at the bottom of the screen.
  gfx::Size modal_size(100, screen_size.height() - 70);
  gfx::Point modal_origin = gfx::Point(10, 20);
  gfx::Rect modal_bounds = gfx::Rect(modal_origin, modal_size);

  // Create a modal window.
  std::unique_ptr<aura::Window> parent(ShowToplevelTestWindow(false));
  std::unique_ptr<aura::Window> modal_window(
      ShowTestWindowWithParent(parent.get(), true));
  modal_window->SetBounds(modal_bounds);

  EXPECT_EQ(modal_bounds.ToString(), modal_window->bounds().ToString());

  // The keyboard gets shown and the dialog should get pushed up, but not get
  // cropped (and aligned to the top).
  ShowKeyboard(true);
  EXPECT_EQ(modal_size.ToString(), modal_window->bounds().size().ToString());
  EXPECT_EQ(modal_origin.x(), modal_window->bounds().x());
  EXPECT_EQ(0, modal_window->bounds().y());

  ShowKeyboard(false);
}

TEST_F(SystemModalContainerLayoutManagerTest, UpdateModalType) {
  aura::Window* modal_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_SystemModalContainer);
  aura::Window* window = ShowTestWindowWithParent(modal_container, false);
  EXPECT_FALSE(Shell::IsSystemModalWindowOpen());

  window->SetProperty(aura::client::kModalKey, ui::mojom::ModalType::kSystem);
  EXPECT_TRUE(Shell::IsSystemModalWindowOpen());

  // Setting twice should not cause error.
  window->SetProperty(aura::client::kModalKey, ui::mojom::ModalType::kSystem);
  EXPECT_TRUE(Shell::IsSystemModalWindowOpen());

  window->SetProperty(aura::client::kModalKey, ui::mojom::ModalType::kNone);
  EXPECT_FALSE(Shell::IsSystemModalWindowOpen());

  window->SetProperty(aura::client::kModalKey, ui::mojom::ModalType::kSystem);
  EXPECT_TRUE(Shell::IsSystemModalWindowOpen());

  window_util::CloseWidgetForWindow(window);
  EXPECT_FALSE(Shell::IsSystemModalWindowOpen());
}

TEST_F(SystemModalContainerLayoutManagerTest, VisibilityChange) {
  std::unique_ptr<aura::Window> window(ShowToplevelTestWindow(false));
  std::unique_ptr<aura::Window> modal_window(
      views::Widget::CreateWindowWithContext(new TestWindow(true), GetContext())
          ->GetNativeWindow());
  SystemModalContainerLayoutManager* layout_manager =
      Shell::GetPrimaryRootWindowController()->GetSystemModalLayoutManager(
          modal_window.get());

  EXPECT_FALSE(Shell::IsSystemModalWindowOpen());
  EXPECT_FALSE(layout_manager->has_window_dimmer());

  modal_window->Show();
  EXPECT_TRUE(Shell::IsSystemModalWindowOpen());
  EXPECT_TRUE(layout_manager->has_window_dimmer());

  // Make sure that a child visibility change should not cause
  // inconsistent state.
  std::unique_ptr<aura::Window> child = std::make_unique<aura::Window>(nullptr);
  child->SetType(aura::client::WINDOW_TYPE_CONTROL);
  child->Init(ui::LAYER_TEXTURED);
  modal_window->AddChild(child.get());
  child->Show();
  EXPECT_TRUE(Shell::IsSystemModalWindowOpen());
  EXPECT_TRUE(layout_manager->has_window_dimmer());

  modal_window->Hide();
  EXPECT_FALSE(Shell::IsSystemModalWindowOpen());
  EXPECT_FALSE(layout_manager->has_window_dimmer());

  modal_window->Show();
  EXPECT_TRUE(Shell::IsSystemModalWindowOpen());
  EXPECT_TRUE(layout_manager->has_window_dimmer());
}

namespace {

class InputTestDelegate : public aura::test::TestWindowDelegate {
 public:
  InputTestDelegate() = default;

  InputTestDelegate(const InputTestDelegate&) = delete;
  InputTestDelegate& operator=(const InputTestDelegate&) = delete;

  ~InputTestDelegate() override = default;

  void RunTest(AshTestBase* test_base) {
    std::unique_ptr<aura::Window> window(
        test_base->CreateTestWindowInShellWithDelegate(
            this, 0, gfx::Rect(0, 0, 100, 100)));
    window->Show();

    GenerateEvents(window.get());

    EXPECT_EQ(2, mouse_event_count_);
    EXPECT_EQ(3, scroll_event_count_);
    EXPECT_EQ(4, touch_event_count_);
    EXPECT_EQ(10, gesture_event_count_);
    Reset();

    views::Widget* widget = views::Widget::CreateWindowWithContext(
        new TestWindow(true), Shell::GetPrimaryRootWindow(),
        gfx::Rect(200, 200, 100, 100));
    widget->Show();
    EXPECT_TRUE(Shell::IsSystemModalWindowOpen());

    // Events should be blocked.
    GenerateEvents(window.get());

    EXPECT_EQ(0, mouse_event_count_);
    EXPECT_EQ(0, scroll_event_count_);
    EXPECT_EQ(0, touch_event_count_);
    EXPECT_EQ(0, gesture_event_count_);
    Reset();

    widget->Close();
    EXPECT_FALSE(Shell::IsSystemModalWindowOpen());

    GenerateEvents(window.get());

    EXPECT_EQ(2, mouse_event_count_);
    EXPECT_EQ(3, scroll_event_count_);
    EXPECT_EQ(4, touch_event_count_);
    EXPECT_EQ(10, gesture_event_count_);
  }

 private:
  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override { mouse_event_count_++; }
  void OnScrollEvent(ui::ScrollEvent* event) override { scroll_event_count_++; }
  void OnTouchEvent(ui::TouchEvent* event) override { touch_event_count_++; }
  void OnGestureEvent(ui::GestureEvent* event) override {
    gesture_event_count_++;
  }

  void GenerateEvents(aura::Window* window) {
    ui::test::EventGenerator event_generator(Shell::GetPrimaryRootWindow(),
                                             window);
    event_generator.ClickLeftButton();
    event_generator.ScrollSequence(window->bounds().CenterPoint(),
                                   base::TimeDelta(), 0, 10, 1, 2);
    event_generator.PressTouch();
    event_generator.ReleaseTouch();
    event_generator.GestureTapAt(window->bounds().CenterPoint());
  }

  void Reset() {
    mouse_event_count_ = 0;
    scroll_event_count_ = 0;
    touch_event_count_ = 0;
    gesture_event_count_ = 0;
  }

  int mouse_event_count_ = 0;
  int scroll_event_count_ = 0;
  int touch_event_count_ = 0;
  int gesture_event_count_ = 0;
};

}  // namespace

TEST_F(SystemModalContainerLayoutManagerTest, BlockAllEvents) {
  InputTestDelegate delegate;
  delegate.RunTest(this);
}

// Make sure that events are properly blocked in multi displays environment.
TEST_F(SystemModalContainerLayoutManagerTest, BlockEventsInMultiDisplays) {
  UpdateDisplay("600x500, 600x500");
  InputTestDelegate delegate;
  delegate.RunTest(this);
}

}  // namespace ash
