// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"

#include <algorithm>
#include <memory>
#include <queue>
#include <vector>

#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/drag_drop/drag_drop_controller_test_api.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test_shell_delegate.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "ash/window_factory.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/account_id/account_id.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/display/scoped_display_for_new_windows.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/events/test/test_event_handler.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/dialog_delegate.h"

using aura::RootWindow;

namespace ash {

namespace {

aura::Window* GetActiveDeskContainer() {
  return Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                             desks_util::GetActiveDeskContainerId());
}

aura::Window* GetAlwaysOnTopContainer() {
  return Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                             kShellWindowId_AlwaysOnTopContainer);
}

// Expect ALL the containers!
void ExpectAllContainers() {
  aura::Window* root_window = Shell::GetPrimaryRootWindow();

  // Validate no duplicate container IDs.
  base::flat_set<int> container_ids;
  std::queue<aura::Window*> window_queue;
  window_queue.push(root_window);
  while (!window_queue.empty()) {
    aura::Window* current_window = window_queue.front();
    window_queue.pop();
    for (aura::Window* child : current_window->children())
      window_queue.push(child);

    const int id = current_window->id();

    // Skip windows with no IDs.
    if (id == aura::Window::kInitialId)
      continue;

    EXPECT_TRUE(container_ids.insert(id).second)
        << "Found duplicate ID: " << id
        << " at window: " << current_window->GetName();
  }

  EXPECT_TRUE(
      Shell::GetContainer(root_window, kShellWindowId_WallpaperContainer));

  for (int desk_id : desks_util::GetDesksContainersIds())
    EXPECT_TRUE(Shell::GetContainer(root_window, desk_id));

  EXPECT_TRUE(
      Shell::GetContainer(root_window, kShellWindowId_AlwaysOnTopContainer));
  EXPECT_TRUE(Shell::GetContainer(root_window, kShellWindowId_ShelfContainer));
  EXPECT_TRUE(
      Shell::GetContainer(root_window, kShellWindowId_SystemModalContainer));
  EXPECT_TRUE(Shell::GetContainer(root_window,
                                  kShellWindowId_LockScreenWallpaperContainer));
  EXPECT_TRUE(
      Shell::GetContainer(root_window, kShellWindowId_LockScreenContainer));
  EXPECT_TRUE(Shell::GetContainer(root_window,
                                  kShellWindowId_LockSystemModalContainer));
  EXPECT_TRUE(Shell::GetContainer(root_window, kShellWindowId_MenuContainer));
  EXPECT_TRUE(Shell::GetContainer(root_window,
                                  kShellWindowId_DragImageAndTooltipContainer));
  EXPECT_TRUE(
      Shell::GetContainer(root_window, kShellWindowId_SettingBubbleContainer));
  EXPECT_TRUE(
      Shell::GetContainer(root_window, kShellWindowId_OverlayContainer));
  EXPECT_TRUE(Shell::GetContainer(root_window,
                                  kShellWindowId_ImeWindowParentContainer));
  EXPECT_TRUE(Shell::GetContainer(root_window,
                                  kShellWindowId_VirtualKeyboardContainer));
  EXPECT_TRUE(
      Shell::GetContainer(root_window, kShellWindowId_MouseCursorContainer));

  // Phantom window is not a container.
  EXPECT_EQ(0u, container_ids.count(kShellWindowId_PhantomWindow));
  EXPECT_FALSE(Shell::GetContainer(root_window, kShellWindowId_PhantomWindow));
}

views::WidgetDelegateView* CreateModalWidgetDelegate() {
  auto delegate = std::make_unique<views::WidgetDelegateView>();
  delegate->SetCanResize(true);
  delegate->SetModalType(ui::MODAL_TYPE_SYSTEM);
  delegate->SetTitle(base::ASCIIToUTF16("Modal Window"));
  return delegate.release();
}

class SimpleMenuDelegate : public ui::SimpleMenuModel::Delegate {
 public:
  SimpleMenuDelegate() = default;
  ~SimpleMenuDelegate() override = default;

  bool IsCommandIdChecked(int command_id) const override { return false; }

  bool IsCommandIdEnabled(int command_id) const override { return true; }

  void ExecuteCommand(int command_id, int event_flags) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SimpleMenuDelegate);
};

}  // namespace

class ShellTest : public AshTestBase {
 public:
  // TODO(jamescook): Convert to AshTestBase::CreateTestWidget().
  views::Widget* CreateTestWindow(views::Widget::InitParams params) {
    views::Widget* widget = new views::Widget;
    params.context = GetContext();
    widget->Init(std::move(params));
    return widget;
  }

  void TestCreateWindow(views::Widget::InitParams::Type type,
                        bool always_on_top,
                        aura::Window* expected_container) {
    views::Widget::InitParams widget_params(type);
    if (always_on_top)
      widget_params.z_order = ui::ZOrderLevel::kFloatingWindow;

    views::Widget* widget = CreateTestWindow(std::move(widget_params));
    widget->Show();

    EXPECT_TRUE(
        expected_container->Contains(widget->GetNativeWindow()->parent()))
        << "TestCreateWindow: type=" << type
        << ", always_on_top=" << always_on_top;

    widget->Close();
  }

  void LockScreenAndVerifyMenuClosed() {
    // Verify a menu is open before locking.
    views::MenuController* menu_controller =
        views::MenuController::GetActiveInstance();
    DCHECK(menu_controller);
    EXPECT_EQ(views::MenuController::ExitType::kNone,
              menu_controller->exit_type());

    // Create a LockScreen window.
    views::Widget::InitParams widget_params(
        views::Widget::InitParams::TYPE_WINDOW);
    views::Widget* lock_widget = CreateTestWindow(std::move(widget_params));
    Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                        kShellWindowId_LockScreenContainer)
        ->AddChild(lock_widget->GetNativeView());
    lock_widget->Show();

    // Simulate real screen locker to change session state to LOCKED
    // when it is shown.
    GetSessionControllerClient()->LockScreen();

    SessionControllerImpl* controller = Shell::Get()->session_controller();
    EXPECT_TRUE(controller->IsScreenLocked());
    EXPECT_TRUE(lock_widget->GetNativeView()->HasFocus());

    // Verify menu is closed.
    EXPECT_EQ(nullptr, views::MenuController::GetActiveInstance());
    lock_widget->Close();
    GetSessionControllerClient()->UnlockScreen();
  }
};

TEST_F(ShellTest, CreateWindow) {
  // Normal window should be created in default container.
  TestCreateWindow(views::Widget::InitParams::TYPE_WINDOW,
                   false,  // always_on_top
                   GetActiveDeskContainer());
  TestCreateWindow(views::Widget::InitParams::TYPE_POPUP,
                   false,  // always_on_top
                   GetActiveDeskContainer());

  // Always-on-top window and popup are created in always-on-top container.
  TestCreateWindow(views::Widget::InitParams::TYPE_WINDOW,
                   true,  // always_on_top
                   GetAlwaysOnTopContainer());
  TestCreateWindow(views::Widget::InitParams::TYPE_POPUP,
                   true,  // always_on_top
                   GetAlwaysOnTopContainer());
}

// Verifies that a window with a preferred size is created centered on the
// default display for new windows. Mojo apps like shortcut_viewer rely on this
// behavior.
TEST_F(ShellTest, CreateWindowWithPreferredSize) {
  UpdateDisplay("1024x768,800x600");

  aura::Window* secondary_root = Shell::GetAllRootWindows()[1];
  display::ScopedDisplayForNewWindows scoped_display(secondary_root);

  views::Widget::InitParams params;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  // Don't specify bounds, parent or context.
  {
    auto delegate = std::make_unique<views::WidgetDelegateView>();
    delegate->SetPreferredSize(gfx::Size(400, 300));
    params.delegate = delegate.release();
  }
  views::Widget widget;
  params.context = GetContext();
  widget.Init(std::move(params));

  // Widget is centered on secondary display.
  EXPECT_EQ(secondary_root, widget.GetNativeWindow()->GetRootWindow());
  EXPECT_EQ(GetSecondaryDisplay().work_area().CenterPoint(),
            widget.GetRestoredBounds().CenterPoint());
}

TEST_F(ShellTest, ChangeZOrderLevel) {
  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);

  // Creates a normal window.
  views::Widget* widget = CreateTestWindow(std::move(widget_params));
  widget->Show();

  // It should be in the active desk container.
  EXPECT_TRUE(
      GetActiveDeskContainer()->Contains(widget->GetNativeWindow()->parent()));

  // Set the z-order to float.
  widget->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);
  // And it should in always on top container now.
  EXPECT_EQ(GetAlwaysOnTopContainer(), widget->GetNativeWindow()->parent());

  // Put the z-order back to normal.
  widget->SetZOrderLevel(ui::ZOrderLevel::kNormal);
  // It should go back to the active desk container.
  EXPECT_TRUE(
      GetActiveDeskContainer()->Contains(widget->GetNativeWindow()->parent()));

  // Set the z-order again to the normal value.
  widget->SetZOrderLevel(ui::ZOrderLevel::kNormal);
  // Should have no effect and we are still in the the active desk container.
  EXPECT_TRUE(
      GetActiveDeskContainer()->Contains(widget->GetNativeWindow()->parent()));

  widget->Close();
}

TEST_F(ShellTest, CreateModalWindow) {
  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);

  // Create a normal window.
  views::Widget* widget = CreateTestWindow(std::move(widget_params));
  widget->Show();

  // It should be in the active desk container.
  EXPECT_TRUE(
      GetActiveDeskContainer()->Contains(widget->GetNativeWindow()->parent()));

  // Create a modal window.
  views::Widget* modal_widget = views::Widget::CreateWindowWithParent(
      CreateModalWidgetDelegate(), widget->GetNativeView());
  modal_widget->Show();

  // It should be in modal container.
  aura::Window* modal_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_SystemModalContainer);
  EXPECT_EQ(modal_container, modal_widget->GetNativeWindow()->parent());

  modal_widget->Close();
  widget->Close();
}

class TestModalDialogDelegate : public views::DialogDelegateView {
 public:
  TestModalDialogDelegate() = default;

  // Overridden from views::WidgetDelegate:
  ui::ModalType GetModalType() const override { return ui::MODAL_TYPE_SYSTEM; }
};

TEST_F(ShellTest, CreateLockScreenModalWindow) {
  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);

  // Create a normal window.
  views::Widget* widget = CreateTestWindow(std::move(widget_params));
  widget->Show();
  EXPECT_TRUE(widget->GetNativeView()->HasFocus());

  // It should be in the active desk container.
  EXPECT_TRUE(
      GetActiveDeskContainer()->Contains(widget->GetNativeWindow()->parent()));

  GetSessionControllerClient()->LockScreen();
  // Create a LockScreen window.
  views::Widget* lock_widget = CreateTestWindow(std::move(widget_params));
  Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                      kShellWindowId_LockScreenContainer)
      ->AddChild(lock_widget->GetNativeView());
  lock_widget->Show();
  EXPECT_TRUE(lock_widget->GetNativeView()->HasFocus());

  // It should be in LockScreen container.
  aura::Window* lock_screen = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_LockScreenContainer);
  EXPECT_EQ(lock_screen, lock_widget->GetNativeWindow()->parent());

  // Create a modal window with a lock window as parent.
  views::Widget* lock_modal_widget = views::Widget::CreateWindowWithParent(
      CreateModalWidgetDelegate(), lock_widget->GetNativeView());
  lock_modal_widget->Show();
  EXPECT_TRUE(lock_modal_widget->GetNativeView()->HasFocus());

  // It should be in LockScreen modal container.
  aura::Window* lock_modal_container =
      Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                          kShellWindowId_LockSystemModalContainer);
  EXPECT_EQ(lock_modal_container,
            lock_modal_widget->GetNativeWindow()->parent());

  // Create a modal window with a normal window as parent.
  views::Widget* modal_widget = views::Widget::CreateWindowWithParent(
      CreateModalWidgetDelegate(), widget->GetNativeView());
  modal_widget->Show();
  // Window on lock screen shouldn't lost focus.
  EXPECT_FALSE(modal_widget->GetNativeView()->HasFocus());
  EXPECT_TRUE(lock_modal_widget->GetNativeView()->HasFocus());

  // It should be in non-LockScreen modal container.
  aura::Window* modal_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_SystemModalContainer);
  EXPECT_EQ(modal_container, modal_widget->GetNativeWindow()->parent());

  // Modal dialog without parent, caused crash see crbug.com/226141
  views::Widget* modal_dialog = views::DialogDelegate::CreateDialogWidget(
      new TestModalDialogDelegate(), GetContext(), nullptr);

  modal_dialog->Show();
  EXPECT_FALSE(modal_dialog->GetNativeView()->HasFocus());
  EXPECT_TRUE(lock_modal_widget->GetNativeView()->HasFocus());

  modal_dialog->Close();
  modal_widget->Close();
  modal_widget->Close();
  lock_modal_widget->Close();
  lock_widget->Close();
  widget->Close();
}

TEST_F(ShellTest, IsScreenLocked) {
  SessionControllerImpl* controller = Shell::Get()->session_controller();
  GetSessionControllerClient()->LockScreen();
  EXPECT_TRUE(controller->IsScreenLocked());
  GetSessionControllerClient()->UnlockScreen();
  EXPECT_FALSE(controller->IsScreenLocked());
}

TEST_F(ShellTest, LockScreenClosesActiveMenu) {
  SimpleMenuDelegate menu_delegate;
  std::unique_ptr<ui::SimpleMenuModel> menu_model(
      new ui::SimpleMenuModel(&menu_delegate));
  menu_model->AddItem(0, base::ASCIIToUTF16("Menu item"));
  views::Widget* widget = Shell::GetPrimaryRootWindowController()
                              ->wallpaper_widget_controller()
                              ->GetWidget();
  std::unique_ptr<views::MenuRunner> menu_runner(
      new views::MenuRunner(menu_model.get(), views::MenuRunner::CONTEXT_MENU));

  menu_runner->RunMenuAt(widget, nullptr, gfx::Rect(),
                         views::MenuAnchorPosition::kTopLeft,
                         ui::MENU_SOURCE_MOUSE);
  LockScreenAndVerifyMenuClosed();
}

TEST_F(ShellTest, ManagedWindowModeBasics) {
  // We start with the usual window containers.
  ExpectAllContainers();
  // Shelf is visible.
  ShelfWidget* shelf_widget = GetPrimaryShelf()->shelf_widget();
  EXPECT_TRUE(shelf_widget->IsVisible());
  // Shelf is at bottom-left of screen.
  EXPECT_EQ(0, shelf_widget->GetWindowBoundsInScreen().x());
  EXPECT_EQ(
      Shell::GetPrimaryRootWindow()->GetHost()->GetBoundsInPixels().height(),
      shelf_widget->GetWindowBoundsInScreen().bottom());
  // We have a wallpaper but not a bare layer.
  // TODO (antrim): enable once we find out why it fails component build.
  //  WallpaperWidgetController* wallpaper =
  //      Shell::GetPrimaryRootWindow()->
  //          GetProperty(kWindowDesktopComponent);
  //  EXPECT_TRUE(wallpaper);
  //  EXPECT_TRUE(wallpaper->widget());
  //  EXPECT_FALSE(wallpaper->layer());

  // Create a normal window.  It is not maximized.
  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);
  widget_params.bounds.SetRect(11, 22, 300, 400);
  views::Widget* widget = CreateTestWindow(std::move(widget_params));
  widget->Show();
  EXPECT_FALSE(widget->IsMaximized());

  // Clean up.
  widget->Close();
}

TEST_F(ShellTest, FullscreenWindowHidesShelf) {
  ExpectAllContainers();

  // Create a normal window.  It is not maximized.
  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);
  widget_params.bounds.SetRect(11, 22, 300, 400);
  views::Widget* widget = CreateTestWindow(std::move(widget_params));
  widget->Show();
  EXPECT_FALSE(widget->IsMaximized());

  // Shelf defaults to visible.
  EXPECT_EQ(SHELF_VISIBLE, Shell::GetPrimaryRootWindowController()
                               ->GetShelfLayoutManager()
                               ->visibility_state());

  // Fullscreen window hides it.
  widget->SetFullscreen(true);
  EXPECT_EQ(SHELF_HIDDEN, Shell::GetPrimaryRootWindowController()
                              ->GetShelfLayoutManager()
                              ->visibility_state());

  // Restoring the window restores it.
  widget->Restore();
  EXPECT_EQ(SHELF_VISIBLE, Shell::GetPrimaryRootWindowController()
                               ->GetShelfLayoutManager()
                               ->visibility_state());

  // Clean up.
  widget->Close();
}

// Various assertions around auto-hide behavior.
// TODO(jamescook): Move this to ShelfTest.
TEST_F(ShellTest, ToggleAutoHide) {
  std::unique_ptr<aura::Window> window = window_factory::NewWindow();
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  window->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_TEXTURED);
  ParentWindowInPrimaryRootWindow(window.get());
  window->Show();
  wm::ActivateWindow(window.get());

  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());

  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);
  EXPECT_EQ(ShelfAutoHideBehavior::kNever, shelf->auto_hide_behavior());

  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ(ShelfAutoHideBehavior::kNever, shelf->auto_hide_behavior());

  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());

  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);
  EXPECT_EQ(ShelfAutoHideBehavior::kNever, shelf->auto_hide_behavior());
}

// Tests that the cursor-filter is ahead of the drag-drop controller in the
// pre-target list.
TEST_F(ShellTest, TestPreTargetHandlerOrder) {
  Shell* shell = Shell::Get();
  ui::EventTargetTestApi test_api(shell);
  ShellTestApi shell_test_api;

  ui::EventHandlerList handlers = test_api.GetPreTargetHandlers();
  ui::EventHandlerList::const_iterator cursor_filter =
      std::find(handlers.begin(), handlers.end(), shell->mouse_cursor_filter());
  ui::EventHandlerList::const_iterator drag_drop = std::find(
      handlers.begin(), handlers.end(), shell_test_api.drag_drop_controller());
  EXPECT_NE(handlers.end(), cursor_filter);
  EXPECT_NE(handlers.end(), drag_drop);
  EXPECT_GT(drag_drop, cursor_filter);
}

// Verifies an EventHandler added to Env gets notified from EventGenerator.
TEST_F(ShellTest, EnvPreTargetHandler) {
  ui::test::TestEventHandler event_handler;
  aura::Env::GetInstance()->AddPreTargetHandler(&event_handler);
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.MoveMouseBy(1, 1);
  EXPECT_NE(0, event_handler.num_mouse_events());
  aura::Env::GetInstance()->RemovePreTargetHandler(&event_handler);
}

// Verifies that pressing tab on an empty shell (one with no windows visible)
// will put focus on the shelf. This enables keyboard only users to get to the
// shelf without knowing the more obscure accelerators. Tab should move focus to
// the home button, shift + tab to the status widget. From there, normal shelf
// tab behaviour takes over, and the shell no longer catches that event.
TEST_F(ShellTest, NoWindowTabFocus) {
  ExpectAllContainers();

  auto* generator = GetEventGenerator();

  StatusAreaWidget* status_area_widget =
      GetPrimaryShelf()->status_area_widget();
  ShelfNavigationWidget* home_button = GetPrimaryShelf()->navigation_widget();

  // Create a normal window.  It is not maximized.
  auto widget = CreateTestWidget();

  // Hit tab with window open, and expect that focus is not on the navigation
  // widget or status widget.
  generator->PressKey(ui::VKEY_TAB, ui::EF_NONE);
  generator->ReleaseKey(ui::VKEY_TAB, ui::EF_NONE);
  EXPECT_FALSE(home_button->GetNativeView()->HasFocus());
  EXPECT_FALSE(status_area_widget->GetNativeView()->HasFocus());

  // Minimize the window, hit tab and expect that focus is on the launcher.
  widget->Minimize();
  generator->PressKey(ui::VKEY_TAB, ui::EF_NONE);
  generator->ReleaseKey(ui::VKEY_TAB, ui::EF_NONE);
  EXPECT_TRUE(home_button->GetNativeView()->HasFocus());

  // Show (to steal focus back before continuing testing) and close the window.
  widget->Show();
  widget->Close();
  EXPECT_FALSE(home_button->GetNativeView()->HasFocus());

  // Confirm that pressing tab when overview mode is open does not go to home
  // button. Tab should be handled by overview mode and not hit the shell event
  // handler.
  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();
  generator->PressKey(ui::VKEY_TAB, ui::EF_NONE);
  generator->ReleaseKey(ui::VKEY_TAB, ui::EF_NONE);
  EXPECT_FALSE(home_button->GetNativeView()->HasFocus());
  overview_controller->EndOverview();

  // Hit shift tab and expect that focus is on status widget.
  generator->PressKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  generator->ReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(status_area_widget->GetNativeView()->HasFocus());
}

// This verifies WindowObservers are removed when a window is destroyed after
// the Shell is destroyed. This scenario (aura::Windows being deleted after the
// Shell) occurs if someone is holding a reference to an unparented Window, as
// is the case with a RenderWidgetHostViewAura that isn't on screen. As long as
// everything is ok, we won't crash. If there is a bug, window's destructor will
// notify some deleted object (say VideoDetector or ActivationController) and
// this will crash.
class ShellTest2 : public AshTestBase {
 public:
  ShellTest2() = default;
  ~ShellTest2() override = default;

 protected:
  std::unique_ptr<aura::Window> window_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellTest2);
};

TEST_F(ShellTest2, DontCrashWhenWindowDeleted) {
  window_ = std::make_unique<aura::Window>(nullptr,
                                           aura::client::WINDOW_TYPE_UNKNOWN);
  window_->Init(ui::LAYER_NOT_DRAWN);
}

using ShellLoginTest = NoSessionAshTestBase;

TEST_F(ShellLoginTest, DragAndDropDisabledBeforeLogin) {
  DragDropController* drag_drop_controller =
      ShellTestApi().drag_drop_controller();
  DragDropControllerTestApi drag_drop_controller_test_api(drag_drop_controller);
  EXPECT_FALSE(drag_drop_controller_test_api.enabled());

  SimulateUserLogin("user1@test.com");
  EXPECT_TRUE(drag_drop_controller_test_api.enabled());
}

using NoDuplicateShellContainerIdsTest = AshTestBase;

TEST_F(NoDuplicateShellContainerIdsTest, ValidateContainersIds) {
  ExpectAllContainers();
}

}  // namespace ash
