// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/login_shelf_view.h"

#include <memory>
#include <vector>

#include "ash/focus_cycler.h"
#include "ash/lock_screen_action/lock_screen_action_background_controller.h"
#include "ash/lock_screen_action/test_lock_screen_action_background_controller.h"
#include "ash/login/login_screen_controller.h"
#include "ash/login/mock_login_screen_client.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/kiosk_app_menu.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shutdown_controller_impl.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/tray_action/test_tray_action_client.h"
#include "ash/tray_action/tray_action.h"
#include "ash/wm/lock_state_controller.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/label_button.h"

using session_manager::SessionState;

namespace ash {
namespace {

void ExpectFocused(views::View* view) {
  EXPECT_TRUE(view->GetWidget()->IsActive());
  EXPECT_TRUE(login_views_utils::HasFocusInAnyChildView(view));
}

void ExpectNotFocused(views::View* view) {
  EXPECT_FALSE(view->GetWidget()->IsActive());
  EXPECT_FALSE(login_views_utils::HasFocusInAnyChildView(view));
}

class LoginShelfViewTest : public LoginTestBase {
 public:
  LoginShelfViewTest() = default;
  ~LoginShelfViewTest() override = default;

  void SetUp() override {
    action_background_controller_factory_ =
        base::Bind(&LoginShelfViewTest::CreateActionBackgroundController,
                   base::Unretained(this));
    LockScreenActionBackgroundController::SetFactoryCallbackForTesting(
        &action_background_controller_factory_);

    // Guest Button is visible while session hasn't started.
    set_start_session(false);
    LoginTestBase::SetUp();
    login_shelf_view_ = GetPrimaryShelf()->shelf_widget()->login_shelf_view();
    Shell::Get()->tray_action()->SetClient(
        tray_action_client_.CreateRemoteAndBind(),
        mojom::TrayActionState::kNotAvailable);
    // Set initial states.
    NotifySessionStateChanged(SessionState::OOBE);
    NotifyShutdownPolicyChanged(false);
  }

  void TearDown() override {
    LockScreenActionBackgroundController::SetFactoryCallbackForTesting(nullptr);
    action_background_controller_ = nullptr;
    LoginTestBase::TearDown();
  }

 protected:
  void NotifySessionStateChanged(SessionState state) {
    GetSessionControllerClient()->SetSessionState(state);
  }

  void NotifyShutdownPolicyChanged(bool reboot_on_shutdown) {
    Shell::Get()->shutdown_controller()->SetRebootOnShutdown(
        reboot_on_shutdown);
  }

  void NotifyLockScreenNoteStateChanged(mojom::TrayActionState state) {
    Shell::Get()->tray_action()->UpdateLockScreenNoteState(state);
  }

  // Simulates a click event on the button.
  void Click(LoginShelfView::ButtonId id) {
    const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                               ui::EventTimeForNow(), 0, 0);
    login_shelf_view_->ButtonPressed(
        static_cast<views::Button*>(login_shelf_view_->GetViewByID(id)), event);
    base::RunLoop().RunUntilIdle();
  }

  // Checks if the shelf is only showing the buttons in the list. The IDs in
  // the specified list must be unique.
  bool ShowsShelfButtons(std::vector<LoginShelfView::ButtonId> ids) {
    for (LoginShelfView::ButtonId id : ids) {
      if (!login_shelf_view_->GetViewByID(id)->GetVisible())
        return false;
    }
    const auto& children = login_shelf_view_->children();
    const size_t visible_buttons =
        std::count_if(children.cbegin(), login_shelf_view_->children().cend(),
                      [](const auto* v) { return v->GetVisible(); });
    return visible_buttons == ids.size();
  }

  // Check whether the button is enabled.
  bool IsButtonEnabled(LoginShelfView::ButtonId id) {
    return login_shelf_view_->GetViewByID(id)->GetEnabled();
  }

  TestTrayActionClient tray_action_client_;

  LoginShelfView* login_shelf_view_;  // Unowned.

  TestLockScreenActionBackgroundController* action_background_controller() {
    return action_background_controller_;
  }

 private:
  std::unique_ptr<LockScreenActionBackgroundController>
  CreateActionBackgroundController() {
    auto result = std::make_unique<TestLockScreenActionBackgroundController>();
    EXPECT_FALSE(action_background_controller_);
    action_background_controller_ = result.get();
    return result;
  }

  LockScreenActionBackgroundController::FactoryCallback
      action_background_controller_factory_;

  // LockScreenActionBackgroundController created by
  // |CreateActionBackgroundController|.
  TestLockScreenActionBackgroundController* action_background_controller_ =
      nullptr;

  DISALLOW_COPY_AND_ASSIGN(LoginShelfViewTest);
};

// Checks the login shelf updates UI after session state changes.
TEST_F(LoginShelfViewTest, ShouldUpdateUiAfterSessionStateChange) {
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown}));

  login_shelf_view_->SetAllowLoginAsGuest(true /*allow_guest*/);
  NotifySessionStateChanged(SessionState::LOGIN_PRIMARY);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown,
                                 LoginShelfView::kBrowseAsGuest,
                                 LoginShelfView::kAddUser}));

  NotifySessionStateChanged(SessionState::LOGGED_IN_NOT_ACTIVE);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown}));

  NotifySessionStateChanged(SessionState::ACTIVE);
  EXPECT_TRUE(ShowsShelfButtons({}));

  NotifySessionStateChanged(SessionState::LOGIN_SECONDARY);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kCancel}));

  NotifySessionStateChanged(SessionState::ACTIVE);
  EXPECT_TRUE(ShowsShelfButtons({}));

  NotifySessionStateChanged(SessionState::LOCKED);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kSignOut}));
}

// Checks the login shelf updates UI after shutdown policy change when the
// screen is locked.
TEST_F(LoginShelfViewTest,
       ShouldUpdateUiAfterShutdownPolicyChangeAtLockScreen) {
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown}));

  NotifySessionStateChanged(SessionState::LOCKED);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kSignOut}));

  NotifyShutdownPolicyChanged(true /*reboot_on_shutdown*/);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kRestart, LoginShelfView::kSignOut}));

  NotifyShutdownPolicyChanged(false /*reboot_on_shutdown*/);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kSignOut}));
}

// Checks shutdown policy change during another session state (e.g. ACTIVE)
// will be reflected when the screen becomes locked.
TEST_F(LoginShelfViewTest, ShouldUpdateUiBasedOnShutdownPolicyInActiveSession) {
  // The initial state of |reboot_on_shutdown| is false.
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown}));

  NotifySessionStateChanged(SessionState::ACTIVE);
  NotifyShutdownPolicyChanged(true /*reboot_on_shutdown*/);

  NotifySessionStateChanged(SessionState::LOCKED);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kRestart, LoginShelfView::kSignOut}));
}

// Checks the login shelf updates UI after lock screen note state changes.
TEST_F(LoginShelfViewTest, ShouldUpdateUiAfterLockScreenNoteState) {
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown}));

  NotifySessionStateChanged(SessionState::LOCKED);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kSignOut}));

  NotifyLockScreenNoteStateChanged(mojom::TrayActionState::kAvailable);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kSignOut}));

  NotifyLockScreenNoteStateChanged(mojom::TrayActionState::kLaunching);
  // Shelf buttons should not be changed until the lock screen action background
  // show animation completes.
  ASSERT_EQ(LockScreenActionBackgroundState::kShowing,
            action_background_controller()->state());
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kSignOut}));

  // Complete lock screen action background animation - this should change the
  // visible buttons.
  ASSERT_TRUE(action_background_controller()->FinishShow());
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kCloseNote}));

  NotifyLockScreenNoteStateChanged(mojom::TrayActionState::kActive);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kCloseNote}));

  NotifyLockScreenNoteStateChanged(mojom::TrayActionState::kAvailable);
  // When lock screen action background is animating to hidden state, the close
  // button should immediately be replaced by kShutdown and kSignout buttons.
  ASSERT_EQ(LockScreenActionBackgroundState::kHiding,
            action_background_controller()->state());
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kSignOut}));

  ASSERT_TRUE(action_background_controller()->FinishHide());
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kSignOut}));

  NotifyLockScreenNoteStateChanged(mojom::TrayActionState::kNotAvailable);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kSignOut}));
}

TEST_F(LoginShelfViewTest, ShouldUpdateUiAfterKioskAppsLoaded) {
  login_shelf_view_->SetAllowLoginAsGuest(true /*allow_guest*/);
  NotifySessionStateChanged(SessionState::LOGIN_PRIMARY);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown,
                                 LoginShelfView::kBrowseAsGuest,
                                 LoginShelfView::kAddUser}));

  std::vector<KioskAppMenuEntry> kiosk_apps(2);
  login_shelf_view_->SetKioskApps(kiosk_apps, {});
  EXPECT_TRUE(ShowsShelfButtons(
      {LoginShelfView::kShutdown, LoginShelfView::kBrowseAsGuest,
       LoginShelfView::kAddUser, LoginShelfView::kApps}));

  login_shelf_view_->SetKioskApps({}, {});
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown,
                                 LoginShelfView::kBrowseAsGuest,
                                 LoginShelfView::kAddUser}));
}

TEST_F(LoginShelfViewTest, SetAllowLoginByGuest) {
  login_shelf_view_->SetAllowLoginAsGuest(true /*allow_guest*/);
  NotifySessionStateChanged(SessionState::LOGIN_PRIMARY);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown,
                                 LoginShelfView::kBrowseAsGuest,
                                 LoginShelfView::kAddUser}));

  // SetAllowLoginAsGuest(false) always hides the guest button.
  login_shelf_view_->SetAllowLoginAsGuest(false /*allow_guest*/);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kAddUser}));

  // SetAllowLoginAsGuest(true) brings the guest button back.
  login_shelf_view_->SetAllowLoginAsGuest(true /*allow_guest*/);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown,
                                 LoginShelfView::kBrowseAsGuest,
                                 LoginShelfView::kAddUser}));

  // However, SetAllowLoginAsGuest(true) does not mean that the guest button is
  // always visible.
  login_shelf_view_->SetLoginDialogState(
      OobeDialogState::SAML_PASSWORD_CONFIRM);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown}));
}

TEST_F(LoginShelfViewTest, ShouldUpdateUiAfterDialogStateChange) {
  login_shelf_view_->SetAllowLoginAsGuest(true /*allow_guest*/);
  // The conditions in this test should only hold while there are user pods on
  // the signin screen.
  AddUsers(1);

  NotifySessionStateChanged(SessionState::LOGIN_PRIMARY);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown,
                                 LoginShelfView::kBrowseAsGuest,
                                 LoginShelfView::kAddUser}));

  // Add user button is always hidden if dialog state !=
  // OobeDialogState::HIDDEN.
  login_shelf_view_->SetLoginDialogState(OobeDialogState::GAIA_SIGNIN);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown}));

  // Guest button is hidden if dialog state ==
  // OobeDialogState::WRONG_HWID_WARNING or SAML_PASSWORD_CONFIRM.
  login_shelf_view_->SetLoginDialogState(OobeDialogState::HIDDEN);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown,
                                 LoginShelfView::kBrowseAsGuest,
                                 LoginShelfView::kAddUser}));
  login_shelf_view_->SetLoginDialogState(OobeDialogState::WRONG_HWID_WARNING);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown}));

  login_shelf_view_->SetLoginDialogState(OobeDialogState::HIDDEN);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown,
                                 LoginShelfView::kBrowseAsGuest,
                                 LoginShelfView::kAddUser}));
  login_shelf_view_->SetLoginDialogState(
      OobeDialogState::SAML_PASSWORD_CONFIRM);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown}));

  // By default guest login during gaia is not allowed.
  login_shelf_view_->SetLoginDialogState(OobeDialogState::GAIA_SIGNIN);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown}));

  // Guest button is hidden if SetAllowLoginAsGuest(false).
  login_shelf_view_->SetAllowLoginAsGuest(false /*allow_guest*/);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown}));

  // Kiosk app button is visible when dialog state == OobeDialogState::HIDDEN
  // or GAIA_SIGNIN.
  login_shelf_view_->SetLoginDialogState(OobeDialogState::GAIA_SIGNIN);
  std::vector<KioskAppMenuEntry> kiosk_apps(1);
  login_shelf_view_->SetKioskApps(kiosk_apps, {});
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kApps}));

  login_shelf_view_->SetLoginDialogState(
      OobeDialogState::SAML_PASSWORD_CONFIRM);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown}));

  login_shelf_view_->SetLoginDialogState(OobeDialogState::HIDDEN);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kAddUser,
                         LoginShelfView::kApps}));

  // Kiosk app button is hidden when no app exists.
  login_shelf_view_->SetKioskApps({}, {});
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kAddUser}));

  // Only shutdown button is visible when state ==
  // OobeDialogState::EXTENSION_LOGIN.
  login_shelf_view_->SetLoginDialogState(OobeDialogState::EXTENSION_LOGIN);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown}));
}

TEST_F(LoginShelfViewTest, ShouldShowGuestButtonWhenNoUserPods) {
  login_shelf_view_->SetAllowLoginAsGuest(/*allow_guest=*/true);
  login_shelf_view_->ShowGuestButtonInOobe(/*show=*/true);
  SetUserCount(0);

  NotifySessionStateChanged(SessionState::LOGIN_PRIMARY);
  // When no user pods are visible, the Gaia dialog would normally pop up. We
  // need to simulate that behavior in this test.
  login_shelf_view_->SetLoginDialogState(OobeDialogState::GAIA_SIGNIN);
  EXPECT_TRUE(ShowsShelfButtons(
      {LoginShelfView::kShutdown, LoginShelfView::kBrowseAsGuest}));
}

TEST_F(LoginShelfViewTest, ClickShutdownButton) {
  Click(LoginShelfView::kShutdown);
  EXPECT_TRUE(Shell::Get()->lock_state_controller()->ShutdownRequested());
}

TEST_F(LoginShelfViewTest, ClickRestartButton) {
  Click(LoginShelfView::kRestart);
  EXPECT_TRUE(Shell::Get()->lock_state_controller()->ShutdownRequested());
}

TEST_F(LoginShelfViewTest, ClickSignOutButton) {
  NotifySessionStateChanged(SessionState::ACTIVE);
  EXPECT_EQ(session_manager::SessionState::ACTIVE,
            Shell::Get()->session_controller()->GetSessionState());
  Click(LoginShelfView::kSignOut);
  EXPECT_EQ(session_manager::SessionState::LOGIN_PRIMARY,
            Shell::Get()->session_controller()->GetSessionState());
}

TEST_F(LoginShelfViewTest, ClickUnlockButton) {
  // The unlock button is visible only when session state is LOCKED and note
  // state is kActive or kLaunching.
  NotifySessionStateChanged(SessionState::LOCKED);

  NotifyLockScreenNoteStateChanged(mojom::TrayActionState::kActive);
  ASSERT_TRUE(action_background_controller()->FinishShow());
  EXPECT_TRUE(tray_action_client_.close_note_reasons().empty());
  Click(LoginShelfView::kCloseNote);
  EXPECT_EQ(std::vector<mojom::CloseLockScreenNoteReason>(
                {mojom::CloseLockScreenNoteReason::kUnlockButtonPressed}),
            tray_action_client_.close_note_reasons());

  tray_action_client_.ClearRecordedRequests();
  NotifyLockScreenNoteStateChanged(mojom::TrayActionState::kLaunching);
  EXPECT_TRUE(tray_action_client_.close_note_reasons().empty());
  Click(LoginShelfView::kCloseNote);
  EXPECT_EQ(std::vector<mojom::CloseLockScreenNoteReason>(
                {mojom::CloseLockScreenNoteReason::kUnlockButtonPressed}),
            tray_action_client_.close_note_reasons());
}

TEST_F(LoginShelfViewTest, ClickCancelButton) {
  auto client = std::make_unique<MockLoginScreenClient>();
  EXPECT_CALL(*client, CancelAddUser());
  Click(LoginShelfView::kCancel);
}

TEST_F(LoginShelfViewTest, ClickBrowseAsGuestButton) {
  auto client = std::make_unique<MockLoginScreenClient>();
  EXPECT_CALL(*client, LoginAsGuest());
  Click(LoginShelfView::kBrowseAsGuest);
}

TEST_F(LoginShelfViewTest, TabGoesFromShelfToStatusAreaAndBackToShelf) {
  NotifySessionStateChanged(SessionState::LOCKED);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kSignOut}));

  gfx::NativeWindow window = login_shelf_view_->GetWidget()->GetNativeWindow();
  views::View* shelf =
      Shelf::ForWindow(window)->shelf_widget()->GetContentsView();
  views::View* status_area = RootWindowController::ForWindow(window)
                                 ->GetStatusAreaWidget()
                                 ->GetContentsView();

  // Give focus to the shelf. The tabbing between lock screen and shelf is
  // verified by |LockScreenSanityTest::TabGoesFromLockToShelfAndBackToLock|.
  Shelf::ForWindow(window)->shelf_widget()->set_default_last_focusable_child(
      false /*reverse*/);
  Shell::Get()->focus_cycler()->FocusWidget(
      Shelf::ForWindow(window)->shelf_widget());
  // The first shelf button has focus.
  ExpectFocused(shelf);
  ExpectNotFocused(status_area);
  EXPECT_TRUE(
      login_shelf_view_->GetViewByID(LoginShelfView::kShutdown)->HasFocus());

  // Focus from the first button to the second button.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_TAB, 0);
  ExpectFocused(shelf);
  ExpectNotFocused(status_area);
  EXPECT_TRUE(
      login_shelf_view_->GetViewByID(LoginShelfView::kSignOut)->HasFocus());

  // Focus from the second button to the status area.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_TAB, 0);
  ExpectNotFocused(shelf);
  ExpectFocused(status_area);

  // A single shift+tab brings focus back to the second shelf button.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_SHIFT_DOWN);
  ExpectFocused(shelf);
  ExpectNotFocused(status_area);
  EXPECT_TRUE(
      login_shelf_view_->GetViewByID(LoginShelfView::kSignOut)->HasFocus());
}

TEST_F(LoginShelfViewTest, ShouldUpdateUiAfterAddButtonStatusChange) {
  login_shelf_view_->SetAllowLoginAsGuest(true /*allow_guest*/);
  NotifySessionStateChanged(SessionState::LOGIN_PRIMARY);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown,
                                 LoginShelfView::kBrowseAsGuest,
                                 LoginShelfView::kAddUser}));
  EXPECT_TRUE(IsButtonEnabled(LoginShelfView::kAddUser));

  login_shelf_view_->SetAddUserButtonEnabled(false /*enable_add_user*/);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown,
                                 LoginShelfView::kBrowseAsGuest,
                                 LoginShelfView::kAddUser}));
  EXPECT_FALSE(IsButtonEnabled(LoginShelfView::kAddUser));

  login_shelf_view_->SetAddUserButtonEnabled(true /*enable_add_user*/);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown,
                                 LoginShelfView::kBrowseAsGuest,
                                 LoginShelfView::kAddUser}));
  EXPECT_TRUE(IsButtonEnabled(LoginShelfView::kAddUser));
}

TEST_F(LoginShelfViewTest, ShouldUpdateUiAfterShutdownButtonStatusChange) {
  login_shelf_view_->SetAllowLoginAsGuest(true /*allow_guest*/);
  NotifySessionStateChanged(SessionState::LOGIN_PRIMARY);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown,
                                 LoginShelfView::kBrowseAsGuest,
                                 LoginShelfView::kAddUser}));
  EXPECT_TRUE(IsButtonEnabled(LoginShelfView::kShutdown));

  login_shelf_view_->SetShutdownButtonEnabled(false /*enable_shutdown_button*/);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown,
                                 LoginShelfView::kBrowseAsGuest,
                                 LoginShelfView::kAddUser}));
  EXPECT_FALSE(IsButtonEnabled(LoginShelfView::kShutdown));

  login_shelf_view_->SetShutdownButtonEnabled(true /*enable_shutdown_button*/);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown,
                                 LoginShelfView::kBrowseAsGuest,
                                 LoginShelfView::kAddUser}));
  EXPECT_TRUE(IsButtonEnabled(LoginShelfView::kShutdown));
}

TEST_F(LoginShelfViewTest, ShouldNotShowNavigationAndHotseat) {
  gfx::NativeWindow window = login_shelf_view_->GetWidget()->GetNativeWindow();
  ShelfWidget* shelf_widget = Shelf::ForWindow(window)->shelf_widget();
  EXPECT_FALSE(shelf_widget->navigation_widget()->IsVisible())
      << "The navigation widget should not appear in the login shelf.";
  EXPECT_FALSE(shelf_widget->hotseat_widget()->IsVisible())
      << "The hotseat widget should not appear in the login shelf.";
}

TEST_F(LoginShelfViewTest, ParentAccessButtonVisibility) {
  // Parent access button should only be visible on lock screen.
  Shell::Get()->login_screen_controller()->ShowParentAccessButton(true);

  NotifySessionStateChanged(SessionState::LOGIN_PRIMARY);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown,
                                 LoginShelfView::kBrowseAsGuest,
                                 LoginShelfView::kAddUser}));

  NotifySessionStateChanged(SessionState::LOGGED_IN_NOT_ACTIVE);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown}));

  NotifySessionStateChanged(SessionState::ACTIVE);
  EXPECT_TRUE(ShowsShelfButtons({}));

  NotifySessionStateChanged(SessionState::LOGIN_SECONDARY);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kCancel}));

  NotifySessionStateChanged(SessionState::ACTIVE);
  EXPECT_TRUE(ShowsShelfButtons({}));

  NotifySessionStateChanged(SessionState::LOCKED);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kSignOut,
                         LoginShelfView::kParentAccess}));
}

TEST_F(LoginShelfViewTest, ParentAccessButtonVisibilityChangeOnLockScreen) {
  NotifySessionStateChanged(SessionState::LOCKED);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kSignOut}));

  Shell::Get()->login_screen_controller()->ShowParentAccessButton(true);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kSignOut,
                         LoginShelfView::kParentAccess}));

  Shell::Get()->login_screen_controller()->ShowParentAccessButton(false);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kSignOut}));
}

}  // namespace
}  // namespace ash
