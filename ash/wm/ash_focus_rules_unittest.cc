// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_pref_service_provider.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/window_factory.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// Defines a |SessionControllerClient| that is used to create and destroy the
// test lock screen widget.
class LockScreenSessionControllerClient : public TestSessionControllerClient {
 public:
  LockScreenSessionControllerClient(SessionControllerImpl* controller,
                                    TestPrefServiceProvider* prefs_provider)
      : TestSessionControllerClient(controller, prefs_provider) {
    InitializeAndSetClient();
    CreatePredefinedUserSessions(1);
  }
  ~LockScreenSessionControllerClient() override = default;

  // TestSessionControllerClient:
  void RequestLockScreen() override {
    TestSessionControllerClient::RequestLockScreen();
    CreateLockScreen();
    Shell::Get()->UpdateShelfVisibility();
  }

  void UnlockScreen() override {
    TestSessionControllerClient::UnlockScreen();
    if (lock_screen_widget_.get()) {
      lock_screen_widget_->Close();
      lock_screen_widget_.reset(nullptr);
    }

    Shell::Get()->UpdateShelfVisibility();
  }

 private:
  void CreateLockScreen() {
    views::View* lock_view = new views::View;
    lock_screen_widget_.reset(new views::Widget);
    views::Widget::InitParams params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    gfx::Size ps = lock_view->GetPreferredSize();

    gfx::Size root_window_size = Shell::GetPrimaryRootWindow()->bounds().size();
    params.bounds = gfx::Rect((root_window_size.width() - ps.width()) / 2,
                              (root_window_size.height() - ps.height()) / 2,
                              ps.width(), ps.height());
    params.parent = Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                                        kShellWindowId_LockScreenContainer);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    lock_screen_widget_->Init(std::move(params));
    lock_screen_widget_->SetContentsView(lock_view);
    lock_screen_widget_->Show();
    lock_screen_widget_->GetNativeView()->SetName("LockView");
    lock_screen_widget_->GetNativeView()->Focus();
  }

  std::unique_ptr<views::Widget> lock_screen_widget_;

  DISALLOW_COPY_AND_ASSIGN(LockScreenSessionControllerClient);
};

////////////////////////////////////////////////////////////////////////////////

// Defines a class that will be used to test the correct behavior of
// |AshFocusRules| when locking and unlocking the screen.
class LockScreenAshFocusRulesTest : public AshTestBase {
 public:
  LockScreenAshFocusRulesTest() = default;
  ~LockScreenAshFocusRulesTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    ash_test_helper()->set_test_session_controller_client(
        std::make_unique<LockScreenSessionControllerClient>(
            Shell::Get()->session_controller(),
            ash_test_helper()->prefs_provider()));
  }

  aura::Window* CreateWindowInActiveDesk() {
    return CreateWindowInContainer(desks_util::GetActiveDeskContainerId());
  }

  aura::Window* CreateWindowInAppListContainer() {
    return CreateWindowInContainer(kShellWindowId_AppListContainer);
  }

  aura::Window* CreateWindowInAlwaysOnTopContainer() {
    aura::Window* window =
        CreateWindowInContainer(kShellWindowId_AlwaysOnTopContainer);
    window->SetProperty(aura::client::kZOrderingKey,
                        ui::ZOrderLevel::kFloatingWindow);
    return window;
  }

  aura::Window* CreateWindowInLockContainer() {
    return CreateWindowInContainer(kShellWindowId_LockScreenContainer);
  }

  aura::Window* CreateWindowInShelfContainer() {
    return CreateWindowInContainer(kShellWindowId_ShelfContainer);
  }

  aura::Window* CreateWindowInLockSystemModalContainer() {
    aura::Window* window =
        CreateWindowInContainer(kShellWindowId_LockSystemModalContainer);
    window->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_SYSTEM);
    return window;
  }

  aura::Window* CreateWindowInSystemModalContainer() {
    aura::Window* window =
        CreateWindowInContainer(kShellWindowId_SystemModalContainer);
    window->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_SYSTEM);
    return window;
  }

 private:
  aura::Window* CreateWindowInContainer(int container_id) {
    aura::Window* root_window = Shell::GetPrimaryRootWindow();
    aura::Window* container = Shell::GetContainer(root_window, container_id);
    aura::Window* window = window_factory::NewWindow().release();
    window->set_id(0);
    window->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window->Init(ui::LAYER_TEXTURED);
    window->Show();
    window->SetProperty(aura::client::kResizeBehaviorKey,
                        aura::client::kResizeBehaviorCanMaximize |
                            aura::client::kResizeBehaviorCanMinimize |
                            aura::client::kResizeBehaviorCanResize);
    container->AddChild(window);
    return window;
  }

  std::unique_ptr<LockScreenSessionControllerClient> session_controller_client_;

  DISALLOW_COPY_AND_ASSIGN(LockScreenAshFocusRulesTest);
};

}  // namespace

// Verifies focus is returned (after unlocking the screen) to the most recent
// window that had it before locking the screen.
TEST_F(LockScreenAshFocusRulesTest, RegainFocusAfterUnlock) {
  std::unique_ptr<aura::Window> normal_window(CreateWindowInActiveDesk());
  std::unique_ptr<aura::Window> always_on_top_window(
      CreateWindowInAlwaysOnTopContainer());

  wm::ActivateWindow(always_on_top_window.get());
  wm::ActivateWindow(normal_window.get());

  EXPECT_TRUE(wm::IsActiveWindow(normal_window.get()));
  EXPECT_TRUE(normal_window->IsVisible());
  EXPECT_TRUE(always_on_top_window->IsVisible());
  EXPECT_TRUE(normal_window->HasFocus());
  EXPECT_FALSE(always_on_top_window->HasFocus());

  WindowState* normal_window_state = WindowState::Get(normal_window.get());
  WindowState* always_on_top_window_state =
      WindowState::Get(always_on_top_window.get());

  EXPECT_TRUE(normal_window_state->CanActivate());
  EXPECT_TRUE(always_on_top_window_state->CanActivate());

  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);

  EXPECT_TRUE(Shell::Get()->session_controller()->IsScreenLocked());
  EXPECT_FALSE(normal_window->HasFocus());
  EXPECT_FALSE(always_on_top_window->HasFocus());
  EXPECT_FALSE(normal_window_state->IsMinimized());
  EXPECT_FALSE(always_on_top_window_state->IsMinimized());
  EXPECT_FALSE(normal_window_state->CanActivate());
  EXPECT_FALSE(always_on_top_window_state->CanActivate());

  UnblockUserSession();

  EXPECT_FALSE(Shell::Get()->session_controller()->IsScreenLocked());
  EXPECT_FALSE(normal_window_state->IsMinimized());
  EXPECT_FALSE(always_on_top_window_state->IsMinimized());
  EXPECT_TRUE(normal_window_state->CanActivate());
  EXPECT_TRUE(always_on_top_window_state->CanActivate());
  EXPECT_FALSE(always_on_top_window->HasFocus());
  EXPECT_TRUE(normal_window->HasFocus());
}

// Tests that if a widget has a view which should be initially focused, this
// view doesn't get focused if the widget shows behind the lock screen.
TEST_F(LockScreenAshFocusRulesTest, PreventFocusChangeWithLockScreenPresent) {
  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  EXPECT_TRUE(Shell::Get()->session_controller()->IsScreenLocked());

  views::test::TestInitialFocusWidgetDelegate delegate(CurrentContext());
  EXPECT_FALSE(delegate.view()->HasFocus());
  delegate.GetWidget()->Show();
  EXPECT_FALSE(delegate.GetWidget()->IsActive());
  EXPECT_FALSE(delegate.view()->HasFocus());

  UnblockUserSession();
  EXPECT_FALSE(Shell::Get()->session_controller()->IsScreenLocked());
  EXPECT_TRUE(delegate.GetWidget()->IsActive());
  EXPECT_TRUE(delegate.view()->HasFocus());
}

// Verifies that a window in lock container cannot be activated if a lock
// system modal window is shown.
TEST_F(LockScreenAshFocusRulesTest,
       PreventLockScreenActivationUnderLockSystemModalWindow) {
  // System modal window - given that it's not lock system modal, it should
  // have no impact on activation of windows while user session is locked.
  std::unique_ptr<aura::Window> system_modal_window(
      CreateWindowInSystemModalContainer());
  EXPECT_TRUE(wm::IsActiveWindow(system_modal_window.get()));

  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  EXPECT_TRUE(Shell::Get()->session_controller()->IsScreenLocked());

  std::unique_ptr<aura::Window> lock_window(CreateWindowInLockContainer());
  std::unique_ptr<aura::Window> lock_system_modal_window(
      CreateWindowInLockSystemModalContainer());

  EXPECT_TRUE(wm::IsActiveWindow(lock_system_modal_window.get()));

  // Try to activate a lock container window - it should not succeed if a lock
  // system modal dialog is present.
  wm::ActivateWindow(lock_window.get());
  EXPECT_TRUE(wm::IsActiveWindow(lock_system_modal_window.get()));

  lock_system_modal_window.reset();

  // Activating lock window should succeed after system modal widnow is closed.
  wm::ActivateWindow(lock_window.get());
  EXPECT_TRUE(wm::IsActiveWindow(lock_window.get()));

  lock_window.reset();
  EXPECT_FALSE(wm::IsActiveWindow(system_modal_window.get()));

  UnblockUserSession();

  // Upon unlocking the session, the system modal window should be reactivated.
  EXPECT_TRUE(wm::IsActiveWindow(system_modal_window.get()));
}

// Verifies that the shelf can be activated in login/lock screen even if there
// is a lock system modal present.
TEST_F(LockScreenAshFocusRulesTest,
       AllowShelfActivationWithLockSystemModalWindow) {
  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  EXPECT_TRUE(Shell::Get()->session_controller()->IsScreenLocked());

  std::unique_ptr<aura::Window> lock_window(CreateWindowInLockContainer());
  std::unique_ptr<aura::Window> lock_shelf_window(
      CreateWindowInShelfContainer());
  std::unique_ptr<aura::Window> lock_system_modal_window(
      CreateWindowInLockSystemModalContainer());
  EXPECT_TRUE(wm::IsActiveWindow(lock_system_modal_window.get()));

  wm::ActivateWindow(lock_shelf_window.get());
  EXPECT_TRUE(wm::IsActiveWindow(lock_shelf_window.get()));
}

// Simulates a transient child dialog of the applist when it gets closed and
// loses focus, the focus should be returned to the applist transient parent,
// not to another window in the MRU list. https://crbug.com/950469.
TEST_F(LockScreenAshFocusRulesTest, TransientChildLosingFocus) {
  std::unique_ptr<aura::Window> normal_window(CreateWindowInActiveDesk());
  std::unique_ptr<aura::Window> transient_parent(
      CreateWindowInAppListContainer());
  std::unique_ptr<aura::Window> transient_child(
      CreateWindowInAppListContainer());
  ::wm::AddTransientChild(transient_parent.get(), transient_child.get());
  wm::ActivateWindow(transient_child.get());
  EXPECT_TRUE(wm::IsActiveWindow(transient_child.get()));

  transient_child->Hide();
  EXPECT_TRUE(wm::IsActiveWindow(transient_parent.get()));
}

}  // namespace ash
