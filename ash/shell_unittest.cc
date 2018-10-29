// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/drag_drop/drag_drop_controller_test_api.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/scoped_root_window_for_new_windows.h"
#include "ash/session/session_controller.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell_test_api.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test_shell_delegate.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "ash/window_factory.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/account_id/account_id.h"
#include "components/prefs/testing_pref_service.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/test/mus/test_window_tree_client_delegate.h"
#include "ui/aura/test/mus/test_window_tree_client_setup.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/events/test/test_event_handler.h"
#include "ui/gfx/geometry/size.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/dialog_delegate.h"

using aura::RootWindow;

namespace ash {

namespace {

aura::Window* GetDefaultContainer() {
  return Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                             kShellWindowId_DefaultContainer);
}

aura::Window* GetAlwaysOnTopContainer() {
  return Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                             kShellWindowId_AlwaysOnTopContainer);
}

// Expect ALL the containers!
void ExpectAllContainers() {
  // Validate no duplicate container IDs.
  const size_t all_shell_container_ids_size = arraysize(kAllShellContainerIds);
  std::set<int32_t> container_ids;
  for (size_t i = 0; i < all_shell_container_ids_size; ++i)
    EXPECT_TRUE(container_ids.insert(kAllShellContainerIds[i]).second);

  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  EXPECT_TRUE(
      Shell::GetContainer(root_window, kShellWindowId_WallpaperContainer));
  EXPECT_TRUE(
      Shell::GetContainer(root_window, kShellWindowId_DefaultContainer));
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
  EXPECT_TRUE(Shell::GetContainer(root_window, kShellWindowId_StatusContainer));
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

class ModalWindow : public views::WidgetDelegateView {
 public:
  ModalWindow() = default;
  ~ModalWindow() override = default;

  // Overridden from views::WidgetDelegate:
  bool CanResize() const override { return true; }
  base::string16 GetWindowTitle() const override {
    return base::ASCIIToUTF16("Modal Window");
  }
  ui::ModalType GetModalType() const override { return ui::MODAL_TYPE_SYSTEM; }

 private:
  DISALLOW_COPY_AND_ASSIGN(ModalWindow);
};

class WindowWithPreferredSize : public views::WidgetDelegateView {
 public:
  WindowWithPreferredSize() = default;
  ~WindowWithPreferredSize() override = default;

  // views::WidgetDelegate:
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(400, 300);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowWithPreferredSize);
};

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

class TestShellObserver : public ShellObserver {
 public:
  TestShellObserver() = default;
  ~TestShellObserver() override = default;

  // ShellObserver:
  void OnLocalStatePrefServiceInitialized(PrefService* pref_service) override {
    last_local_state_ = pref_service;
  }

  PrefService* last_local_state_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestShellObserver);
};

}  // namespace

class ShellTest : public AshTestBase {
 public:
  // TODO(jamescook): Convert to AshTestBase::CreateTestWidget().
  views::Widget* CreateTestWindow(views::Widget::InitParams params) {
    views::Widget* widget = new views::Widget;
    params.context = CurrentContext();
    widget->Init(params);
    return widget;
  }

  void TestCreateWindow(views::Widget::InitParams::Type type,
                        bool always_on_top,
                        aura::Window* expected_container) {
    views::Widget::InitParams widget_params(type);
    widget_params.keep_on_top = always_on_top;

    views::Widget* widget = CreateTestWindow(widget_params);
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
    EXPECT_EQ(views::MenuController::EXIT_NONE, menu_controller->exit_type());

    // Create a LockScreen window.
    views::Widget::InitParams widget_params(
        views::Widget::InitParams::TYPE_WINDOW);
    views::Widget* lock_widget = CreateTestWindow(widget_params);
    Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                        kShellWindowId_LockScreenContainer)
        ->AddChild(lock_widget->GetNativeView());
    lock_widget->Show();

    // Simulate real screen locker to change session state to LOCKED
    // when it is shown.
    SessionController* controller = Shell::Get()->session_controller();
    controller->LockScreenAndFlushForTest();

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
                   GetDefaultContainer());
  TestCreateWindow(views::Widget::InitParams::TYPE_POPUP,
                   false,  // always_on_top
                   GetDefaultContainer());

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
  ScopedRootWindowForNewWindows scoped_root(secondary_root);

  views::Widget::InitParams params;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  // Don't specify bounds, parent or context.
  params.delegate = new WindowWithPreferredSize;
  views::Widget widget;
  params.context = CurrentContext();
  widget.Init(params);

  // Widget is centered on secondary display.
  EXPECT_EQ(secondary_root, widget.GetNativeWindow()->GetRootWindow());
  EXPECT_EQ(GetSecondaryDisplay().work_area().CenterPoint(),
            widget.GetRestoredBounds().CenterPoint());
}

TEST_F(ShellTest, ChangeAlwaysOnTop) {
  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);

  // Creates a normal window
  views::Widget* widget = CreateTestWindow(widget_params);
  widget->Show();

  // It should be in default container.
  EXPECT_TRUE(
      GetDefaultContainer()->Contains(widget->GetNativeWindow()->parent()));

  // Flip always-on-top flag.
  widget->SetAlwaysOnTop(true);
  // And it should in always on top container now.
  EXPECT_EQ(GetAlwaysOnTopContainer(), widget->GetNativeWindow()->parent());

  // Flip always-on-top flag.
  widget->SetAlwaysOnTop(false);
  // It should go back to default container.
  EXPECT_TRUE(
      GetDefaultContainer()->Contains(widget->GetNativeWindow()->parent()));

  // Set the same always-on-top flag again.
  widget->SetAlwaysOnTop(false);
  // Should have no effect and we are still in the default container.
  EXPECT_TRUE(
      GetDefaultContainer()->Contains(widget->GetNativeWindow()->parent()));

  widget->Close();
}

TEST_F(ShellTest, CreateModalWindow) {
  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);

  // Create a normal window.
  views::Widget* widget = CreateTestWindow(widget_params);
  widget->Show();

  // It should be in default container.
  EXPECT_TRUE(
      GetDefaultContainer()->Contains(widget->GetNativeWindow()->parent()));

  // Create a modal window.
  views::Widget* modal_widget = views::Widget::CreateWindowWithParent(
      new ModalWindow(), widget->GetNativeView());
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
  views::Widget* widget = CreateTestWindow(widget_params);
  widget->Show();
  EXPECT_TRUE(widget->GetNativeView()->HasFocus());

  // It should be in default container.
  EXPECT_TRUE(
      GetDefaultContainer()->Contains(widget->GetNativeWindow()->parent()));

  Shell::Get()->session_controller()->LockScreenAndFlushForTest();
  // Create a LockScreen window.
  views::Widget* lock_widget = CreateTestWindow(widget_params);
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
      new ModalWindow(), lock_widget->GetNativeView());
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
      new ModalWindow(), widget->GetNativeView());
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
      new TestModalDialogDelegate(), CurrentContext(), NULL);

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
  SessionController* controller = Shell::Get()->session_controller();
  controller->LockScreenAndFlushForTest();
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

  menu_runner->RunMenuAt(widget, NULL, gfx::Rect(), views::MENU_ANCHOR_TOPLEFT,
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
  views::Widget* widget = CreateTestWindow(widget_params);
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
  views::Widget* widget = CreateTestWindow(widget_params);
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
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());

  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());
}

// Tests that the cursor-filter is ahead of the drag-drop controller in the
// pre-target list.
TEST_F(ShellTest, TestPreTargetHandlerOrder) {
  Shell* shell = Shell::Get();
  ui::EventTargetTestApi test_api(shell);
  ShellTestApi shell_test_api(shell);

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
  Shell::Get()->aura_env()->AddPreTargetHandler(&event_handler);
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.MoveMouseBy(1, 1);
  EXPECT_NE(0, event_handler.num_mouse_events());
  Shell::Get()->aura_env()->RemovePreTargetHandler(&event_handler);
}

// Verifies keyboard is re-enabled on proper timing.
TEST_F(ShellTest, KeyboardCreation) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      keyboard::switches::kEnableVirtualKeyboard);

  ASSERT_TRUE(keyboard::IsKeyboardEnabled());

  EXPECT_FALSE(keyboard::KeyboardController::Get()->IsEnabled());

  mojom::SessionInfoPtr info = mojom::SessionInfo::New();
  info->state = session_manager::SessionState::LOGGED_IN_NOT_ACTIVE;
  ash::Shell::Get()->session_controller()->SetSessionInfo(std::move(info));

  EXPECT_TRUE(keyboard::KeyboardController::Get()->IsEnabled());
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
  // DontCrashWhenWindowDeletedSingleProcess covers the SingleProcessMash case.
  if (::features::IsSingleProcessMash())
    return;

  // This test explicitly uses aura::Env::GetInstance() rather than
  // Shell->aura_env() as the Window outlives the Shell. In order for a Window
  // to outlive Shell the Window must be created outside of Ash, which uses
  // aura::Env::GetInstance() as the Env.
  window_ = std::make_unique<aura::Window>(
      nullptr, aura::client::WINDOW_TYPE_UNKNOWN, aura::Env::GetInstance());
  window_->Init(ui::LAYER_NOT_DRAWN);
}

// This verifies WindowObservers are removed when a window is destroyed after
// the Shell is destroyed. This scenario (aura::Windows being deleted after the
// Shell) occurs if someone is holding a reference to an unparented Window, as
// is the case with a RenderWidgetHostViewAura that isn't on screen. As long as
// everything is ok, we won't crash. If there is a bug, window's destructor will
// notify some deleted object (say VideoDetector or ActivationController) and
// this will crash.
class ShellTest3 : public AshTestBase {
 public:
  ShellTest3() = default;
  ~ShellTest3() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    if (!::features::IsSingleProcessMash())
      return;
    window_service_setup_ = std::make_unique<aura::TestWindowTreeClientSetup>();
    window_service_setup_->InitWithoutEmbed(&test_window_tree_client_delegate_);
    aura::Env::GetInstance()->SetWindowTreeClient(
        window_service_setup_->window_tree_client());
  }

  void TearDown() override {
    AshTestBase::TearDown();
    if (!::features::IsSingleProcessMash())
      return;
    window_.reset();
    window_service_setup_.reset();
    aura::Env::GetInstance()->SetWindowTreeClient(nullptr);
  }

 protected:
  std::unique_ptr<aura::Window> window_;

 private:
  aura::TestWindowTreeClientDelegate test_window_tree_client_delegate_;
  std::unique_ptr<aura::TestWindowTreeClientSetup> window_service_setup_;

  DISALLOW_COPY_AND_ASSIGN(ShellTest3);
};

TEST_F(ShellTest3, DontCrashWhenWindowDeletedSingleProcess) {
  if (!::features::IsSingleProcessMash())
    return;
  // This test explicitly uses aura::Env::GetInstance() rather than
  // Shell->aura_env() as the Window outlives the Shell. In order for a Window
  // to outlive Shell the Window must be created outside of Ash, which uses
  // aura::Env::GetInstance() as the Env.
  window_ = std::make_unique<aura::Window>(
      nullptr, aura::client::WINDOW_TYPE_UNKNOWN, aura::Env::GetInstance());
  window_->Init(ui::LAYER_NOT_DRAWN);
}

// Tests the local state code path.
class ShellLocalStateTest : public AshTestBase {
 public:
  ShellLocalStateTest() { disable_provide_local_state(); }
};

TEST_F(ShellLocalStateTest, LocalState) {
  TestShellObserver observer;
  Shell::Get()->AddShellObserver(&observer);

  // Prefs service wrapper code creates a PrefService.
  std::unique_ptr<TestingPrefServiceSimple> local_state =
      std::make_unique<TestingPrefServiceSimple>();
  Shell::RegisterLocalStatePrefs(local_state->registry(), true);
  TestingPrefServiceSimple* local_state_ptr = local_state.get();
  ShellTestApi().OnLocalStatePrefServiceInitialized(std::move(local_state));
  EXPECT_EQ(local_state_ptr, observer.last_local_state_);
  EXPECT_EQ(local_state_ptr, ash_test_helper()->GetLocalStatePrefService());

  Shell::Get()->RemoveShellObserver(&observer);
}

using ShellLoginTest = NoSessionAshTestBase;

TEST_F(ShellLoginTest, DragAndDropDisabledBeforeLogin) {
  DragDropController* drag_drop_controller =
      ShellTestApi(Shell::Get()).drag_drop_controller();
  DragDropControllerTestApi drag_drop_controller_test_api(drag_drop_controller);
  EXPECT_FALSE(drag_drop_controller_test_api.enabled());

  SimulateUserLogin("user1@test.com");
  EXPECT_TRUE(drag_drop_controller_test_api.enabled());
}

}  // namespace ash
