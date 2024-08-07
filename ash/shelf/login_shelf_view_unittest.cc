// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/login_shelf_view.h"

#include <memory>
#include <optional>
#include <vector>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/focus_cycler.h"
#include "ash/lock_screen_action/lock_screen_action_background_controller.h"
#include "ash/lock_screen_action/test_lock_screen_action_background_controller.h"
#include "ash/login/login_screen_controller.h"
#include "ash/login/mock_login_screen_client.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/kiosk_app_menu.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shelf/login_shelf_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_shutdown_confirmation_bubble.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shutdown_controller_impl.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/tray_action/test_tray_action_client.h"
#include "ash/tray_action/tray_action.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/login/auth/auth_events_recorder.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/manager/test/action_logger.h"
#include "ui/display/manager/test/test_native_display_delegate.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/accessibility/view_accessibility.h"
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

std::vector<KioskAppMenuEntry> GetNFakeKioskApps(int n) {
  return std::vector<KioskAppMenuEntry>(
      n, KioskAppMenuEntry(KioskAppMenuEntry::AppType::kWebApp,
                           AccountId::FromUserEmail("fake@email.com"),
                           /*chrome_app_id=*/std::nullopt,
                           /*name=*/u"Fake App",
                           /*icon=*/gfx::ImageSkia()));
}

class LoginShelfViewTest : public LoginTestBase {
 public:
  LoginShelfViewTest() = default;
  LoginShelfViewTest(const LoginShelfViewTest&) = delete;
  LoginShelfViewTest& operator=(const LoginShelfViewTest&) = delete;
  ~LoginShelfViewTest() override = default;

  void SetUp() override {
    action_background_controller_factory_ = base::BindRepeating(
        &LoginShelfViewTest::CreateActionBackgroundController,
        base::Unretained(this));
    LockScreenActionBackgroundController::SetFactoryCallbackForTesting(
        &action_background_controller_factory_);

    // Guest Button is visible while session hasn't started.
    LoginTestBase::SetUp();
    login_shelf_view_ = GetPrimaryShelf()->shelf_widget()->GetLoginShelfView();
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
    GetSessionControllerClient()->FlushForTest();
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
    DCHECK(login_shelf_view_->GetViewByID(id)->GetVisible());

    ui::test::EventGenerator* event_generator = GetEventGenerator();
    event_generator->MoveMouseTo(
        login_shelf_view_->GetViewByID(id)->GetBoundsInScreen().CenterPoint());
    event_generator->ClickLeftButton();

    base::RunLoop().RunUntilIdle();
  }

  // Checks if the shelf is only showing the buttons in the list. The IDs in
  // the specified list must be unique.
  bool ShowsShelfButtons(const std::vector<LoginShelfView::ButtonId>& ids) {
    for (LoginShelfView::ButtonId id : ids) {
      if (!login_shelf_view_->GetViewByID(id)->GetVisible()) {
        return false;
      }
    }
    const size_t visible_buttons = base::ranges::count_if(
        login_shelf_view_->children(), &views::View::GetVisible);
    return visible_buttons == ids.size();
  }

  // Check if the former button is shown before the latter button
  bool AreButtonsInOrder(LoginShelfView::ButtonId former,
                         LoginShelfView::ButtonId latter) {
    auto* former_button_view = login_shelf_view_->GetViewByID(former);
    auto* latter_button_view = login_shelf_view_->GetViewByID(latter);
    EXPECT_TRUE(former_button_view->GetVisible() &&
                latter_button_view->GetVisible());
    auto* former_button_view_container =
        login_shelf_view_->GetButtonContainerByID(former);
    auto* latter_button_view_container =
        login_shelf_view_->GetButtonContainerByID(latter);
    EXPECT_TRUE(former_button_view_container->GetVisible() &&
                latter_button_view_container->GetVisible());
    return login_shelf_view_->GetIndexOf(former_button_view_container) <
           login_shelf_view_->GetIndexOf(latter_button_view_container);
  }

  // Check whether the button is enabled.
  bool IsButtonEnabled(LoginShelfView::ButtonId id) const {
    return login_shelf_view_->GetViewByID(id)->GetEnabled();
  }

  void FocusOnLoginShelfButton() {
    LoginShelfWidget* login_shelf_widget = GetLoginShelfWidget();
    login_shelf_widget->SetDefaultLastFocusableChild(/*reverse=*/false);

    Shell::Get()->focus_cycler()->FocusWidget(login_shelf_widget);
    ExpectFocused(login_shelf_widget->GetContentsView());
  }

  // Confirm shutdown confirmation bubble.
  void ShutdownAndConfirm() {
    Click(LoginShelfView::kShutdown);
    ui::test::EventGenerator* event_generator = GetEventGenerator();
    event_generator->MoveMouseTo(
        login_shelf_view_->GetShutdownConfirmationBubbleForTesting()
            ->GetViewByID(static_cast<int>(
                ShelfShutdownConfirmationBubble::ButtonId::kShutdown))
            ->GetBoundsInScreen()
            .CenterPoint());
    event_generator->ClickLeftButton();

    base::RunLoop().RunUntilIdle();
  }

  // Returns the widget where the login shelf view lives.
  LoginShelfWidget* GetLoginShelfWidget() {
    Shelf* shelf =
        Shelf::ForWindow(login_shelf_view_->GetWidget()->GetNativeWindow());
    return shelf->login_shelf_widget();
  }

  TestTrayActionClient tray_action_client_;

  raw_ptr<LoginShelfView, DanglingUntriaged> login_shelf_view_ =
      nullptr;  // Unowned.

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
  raw_ptr<TestLockScreenActionBackgroundController>
      action_background_controller_ = nullptr;
};

// Checks the login shelf updates UI after session state changes.
TEST_F(LoginShelfViewTest, ShouldUpdateUiAfterSessionStateChange) {
  EXPECT_TRUE(ShowsShelfButtons({}));

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

// Checks that the login shelf is not displayed in Shimless RMA.
TEST_F(LoginShelfViewTest, ShouldHideUiInShimlessRma) {
  EXPECT_TRUE(ShowsShelfButtons({}));
  NotifySessionStateChanged(SessionState::RMA);
  EXPECT_TRUE(ShowsShelfButtons({}));
}

// Checks the login shelf updates UI after shutdown policy change when the
// screen is locked.
TEST_F(LoginShelfViewTest,
       ShouldUpdateUiAfterShutdownPolicyChangeAtLockScreen) {
  EXPECT_TRUE(ShowsShelfButtons({}));

  CreateUserSessions(1);
  NotifySessionStateChanged(SessionState::LOCKED);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kSignOut}));

  NotifyShutdownPolicyChanged(true /*reboot_on_shutdown*/);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kRestart, LoginShelfView::kSignOut}));
  EXPECT_TRUE(
      AreButtonsInOrder(LoginShelfView::kRestart, LoginShelfView::kSignOut));

  NotifyShutdownPolicyChanged(false /*reboot_on_shutdown*/);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kSignOut}));
  EXPECT_TRUE(
      AreButtonsInOrder(LoginShelfView::kShutdown, LoginShelfView::kSignOut));
}

// Checks shutdown policy change during another session state (e.g. ACTIVE)
// will be reflected when the screen becomes locked.
TEST_F(LoginShelfViewTest, ShouldUpdateUiBasedOnShutdownPolicyInActiveSession) {
  // The initial state of |reboot_on_shutdown| is false.
  EXPECT_TRUE(ShowsShelfButtons({}));

  CreateUserSessions(1);
  NotifyShutdownPolicyChanged(true /*reboot_on_shutdown*/);

  NotifySessionStateChanged(SessionState::LOCKED);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kRestart, LoginShelfView::kSignOut}));
  EXPECT_TRUE(
      AreButtonsInOrder(LoginShelfView::kRestart, LoginShelfView::kSignOut));
}

// Checks that the Apps button is hidden if a session has started
TEST_F(LoginShelfViewTest, ShouldNotShowAppsButtonAfterSessionStarted) {
  NotifySessionStateChanged(SessionState::LOGIN_PRIMARY);

  login_shelf_view_->SetKioskApps(GetNFakeKioskApps(1));
  EXPECT_TRUE(
      login_shelf_view_->GetViewByID(LoginShelfView::kApps)->GetVisible());

  CreateUserSessions(1);
  EXPECT_FALSE(
      login_shelf_view_->GetViewByID(LoginShelfView::kApps)->GetVisible());
}

// Checks that the shutdown or restart buttons shown before the Apps button when
// kiosk mode is enabled
TEST_F(LoginShelfViewTest, ShouldShowShutdownOrRestartButtonsBeforeApps) {
  NotifySessionStateChanged(SessionState::LOGIN_PRIMARY);

  login_shelf_view_->SetKioskApps(GetNFakeKioskApps(1));

  // |reboot_on_shutdown| is initially off
  EXPECT_TRUE(ShowsShelfButtons(
      {LoginShelfView::kShutdown, LoginShelfView::kBrowseAsGuest,
       LoginShelfView::kAddUser, LoginShelfView::kApps}));
  EXPECT_TRUE(
      AreButtonsInOrder(LoginShelfView::kShutdown, LoginShelfView::kApps));

  NotifyShutdownPolicyChanged(true /*reboot_on_shutdown*/);
  EXPECT_TRUE(ShowsShelfButtons(
      {LoginShelfView::kRestart, LoginShelfView::kBrowseAsGuest,
       LoginShelfView::kAddUser, LoginShelfView::kApps}));
  EXPECT_TRUE(
      AreButtonsInOrder(LoginShelfView::kRestart, LoginShelfView::kApps));
}

// Checks the login shelf updates UI after lock screen note state changes.
TEST_F(LoginShelfViewTest, ShouldUpdateUiAfterLockScreenNoteState) {
  EXPECT_TRUE(ShowsShelfButtons({}));

  CreateUserSessions(1);
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

  login_shelf_view_->SetKioskApps(GetNFakeKioskApps(2));
  EXPECT_TRUE(ShowsShelfButtons(
      {LoginShelfView::kShutdown, LoginShelfView::kBrowseAsGuest,
       LoginShelfView::kAddUser, LoginShelfView::kApps}));
  EXPECT_TRUE(
      AreButtonsInOrder(LoginShelfView::kShutdown, LoginShelfView::kApps));

  login_shelf_view_->SetKioskApps({});
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
  EXPECT_TRUE(ShowsShelfButtons({}));
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
  // Shutdown button is only visible when it is first signin step.
  login_shelf_view_->SetIsFirstSigninStep(/*is_first=*/true);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown}));

  // Guest button is hidden if dialog state ==
  // OobeDialogState::WRONG_HWID_WARNING or SAML_PASSWORD_CONFIRM.
  login_shelf_view_->SetLoginDialogState(OobeDialogState::HIDDEN);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown,
                                 LoginShelfView::kBrowseAsGuest,
                                 LoginShelfView::kAddUser}));
  login_shelf_view_->SetLoginDialogState(OobeDialogState::WRONG_HWID_WARNING);
  EXPECT_TRUE(ShowsShelfButtons({}));

  login_shelf_view_->SetLoginDialogState(OobeDialogState::HIDDEN);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown,
                                 LoginShelfView::kBrowseAsGuest,
                                 LoginShelfView::kAddUser}));
  login_shelf_view_->SetLoginDialogState(
      OobeDialogState::SAML_PASSWORD_CONFIRM);
  EXPECT_TRUE(ShowsShelfButtons({}));

  // By default guest login during gaia is not allowed.
  login_shelf_view_->SetLoginDialogState(OobeDialogState::GAIA_SIGNIN);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown}));

  // Guest button is hidden if SetAllowLoginAsGuest(false).
  login_shelf_view_->SetAllowLoginAsGuest(false /*allow_guest*/);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown}));

  // By default apps button is hidden during gaia sign in
  login_shelf_view_->SetLoginDialogState(OobeDialogState::GAIA_SIGNIN);
  login_shelf_view_->SetKioskApps(GetNFakeKioskApps(1));
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown}));

  // Apps button is hidden during SAML_PASSWORD_CONFIRM STATE
  login_shelf_view_->SetLoginDialogState(
      OobeDialogState::SAML_PASSWORD_CONFIRM);
  EXPECT_TRUE(ShowsShelfButtons({}));

  // Kiosk apps button is visible when dialog state == OobeDialogState::HIDDEN
  login_shelf_view_->SetLoginDialogState(OobeDialogState::HIDDEN);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kAddUser,
                         LoginShelfView::kApps}));
  EXPECT_TRUE(
      AreButtonsInOrder(LoginShelfView::kShutdown, LoginShelfView::kApps));

  // Kiosk app button is hidden when no app exists.
  login_shelf_view_->SetKioskApps({});
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kAddUser}));

  login_shelf_view_->SetAllowLoginAsGuest(true /*allow_guest*/);

  // Only shutdown button is visible when state ==
  // OobeDialogState::EXTENSION_LOGIN.
  login_shelf_view_->SetLoginDialogState(OobeDialogState::EXTENSION_LOGIN);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown}));

  // Show shutdown, browse as guest and add user buttons when state ==
  // OobeDialogState::EXTENSION_LOGIN_CLOSED.
  login_shelf_view_->SetLoginDialogState(
      OobeDialogState::EXTENSION_LOGIN_CLOSED);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown,
                                 LoginShelfView::kBrowseAsGuest,
                                 LoginShelfView::kAddUser}));

  // Hide shutdown button during enrollment.
  login_shelf_view_->SetLoginDialogState(
      OobeDialogState::ENROLLMENT_CANCEL_DISABLED);
  EXPECT_TRUE(ShowsShelfButtons({}));

  // Shutdown button is hidden during user onboarding, as well as during
  // any data migration steps.
  login_shelf_view_->SetLoginDialogState(OobeDialogState::ONBOARDING);
  EXPECT_TRUE(ShowsShelfButtons({}));
  login_shelf_view_->SetLoginDialogState(OobeDialogState::MIGRATION);
  EXPECT_TRUE(ShowsShelfButtons({}));

  // Only Shutdown button should be available if some device blocking
  // screen is shown (e.g. Device Disabled, or Update Required).
  login_shelf_view_->SetKioskApps(GetNFakeKioskApps(1));
  login_shelf_view_->SetLoginDialogState(OobeDialogState::BLOCKING);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown}));
}

TEST_F(LoginShelfViewTest, ShouldShowGuestButtonWhenNoUserPods) {
  login_shelf_view_->SetAllowLoginAsGuest(/*allow_guest=*/true);
  login_shelf_view_->SetIsFirstSigninStep(/*is_first=*/true);
  SetUserCount(0);

  NotifySessionStateChanged(SessionState::LOGIN_PRIMARY);
  // When no user pods are visible, the Gaia dialog would normally pop up. We
  // need to simulate that behavior in this test.
  login_shelf_view_->SetLoginDialogState(OobeDialogState::GAIA_SIGNIN);
  EXPECT_TRUE(ShowsShelfButtons(
      {LoginShelfView::kShutdown, LoginShelfView::kBrowseAsGuest}));
}

TEST_F(LoginShelfViewTest, ClickShutdownButtonOnLoginScreen) {
  NotifySessionStateChanged(SessionState::LOGIN_PRIMARY);
  ShutdownAndConfirm();
  EXPECT_TRUE(Shell::Get()->lock_state_controller()->ShutdownRequested());
}

TEST_F(LoginShelfViewTest, ClickShutdownButton) {
  NotifySessionStateChanged(session_manager::SessionState::OOBE);
  login_shelf_view_->SetLoginDialogState(OobeDialogState::GAIA_SIGNIN);
  login_shelf_view_->SetIsFirstSigninStep(/*is_first=*/true);
  ShutdownAndConfirm();
  EXPECT_TRUE(Shell::Get()->lock_state_controller()->ShutdownRequested());
}

TEST_F(LoginShelfViewTest, ClickShutdownButtonOnLockScreen) {
  CreateUserSessions(1);
  NotifySessionStateChanged(SessionState::LOCKED);
  ShutdownAndConfirm();
  EXPECT_TRUE(Shell::Get()->lock_state_controller()->ShutdownRequested());
}

// Tests that shutdown button can be clicked on the lock screen for active
// session that starts with side shelf. See https://crbug.com/1050192.
TEST_F(LoginShelfViewTest,
       ClickShutdownButtonOnLockScreenWithVerticalInSessionShelf) {
  CreateUserSessions(1);
  SetShelfAlignmentPref(
      Shell::Get()->session_controller()->GetPrimaryUserPrefService(),
      GetPrimaryDisplay().id(), ShelfAlignment::kLeft);
  ClearLogin();

  CreateUserSessions(1);
  NotifySessionStateChanged(SessionState::LOCKED);

  ShutdownAndConfirm();
  EXPECT_TRUE(Shell::Get()->lock_state_controller()->ShutdownRequested());
}

TEST_F(LoginShelfViewTest, ClickRestartButton) {
  // The Restart button is not available in OOBE session state.
  CreateUserSessions(1);
  NotifySessionStateChanged(SessionState::LOCKED);

  NotifyShutdownPolicyChanged(true /*reboot_on_shutdown*/);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kRestart, LoginShelfView::kSignOut}));

  Click(LoginShelfView::kRestart);
  EXPECT_TRUE(Shell::Get()->lock_state_controller()->ShutdownRequested());
}

TEST_F(LoginShelfViewTest, ClickSignOutButton) {
  CreateUserSessions(1);
  EXPECT_EQ(session_manager::SessionState::ACTIVE,
            Shell::Get()->session_controller()->GetSessionState());

  NotifySessionStateChanged(SessionState::LOCKED);
  Click(LoginShelfView::kSignOut);
  EXPECT_EQ(session_manager::SessionState::LOGIN_PRIMARY,
            Shell::Get()->session_controller()->GetSessionState());
}

TEST_F(LoginShelfViewTest, ClickUnlockButton) {
  // The unlock button is visible only when session state is LOCKED and note
  // state is kActive or kLaunching.
  CreateUserSessions(1);
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
  CreateUserSessions(1);
  NotifySessionStateChanged(SessionState::LOGIN_SECONDARY);
  Click(LoginShelfView::kCancel);
}

TEST_F(LoginShelfViewTest, ClickBrowseAsGuestButton) {
  auto client = std::make_unique<MockLoginScreenClient>();

  EXPECT_CALL(*client, ShowGuestTosScreen());
  login_shelf_view_->SetAllowLoginAsGuest(true /*allow_guest*/);
  NotifySessionStateChanged(SessionState::LOGIN_PRIMARY);
  Click(LoginShelfView::kBrowseAsGuest);
}

TEST_F(LoginShelfViewTest, ClickEnterpriseEnrollmentButton) {
  auto client = std::make_unique<MockLoginScreenClient>();
  EXPECT_CALL(*client,
              HandleAccelerator(ash::LoginAcceleratorAction::kStartEnrollment));

  login_shelf_view_->SetLoginDialogState(OobeDialogState::USER_CREATION);
  Click(LoginShelfView::kEnterpriseEnrollment);
}

TEST_F(LoginShelfViewTest, TabGoesFromShelfToStatusAreaAndBackToShelf) {
  CreateUserSessions(1);
  NotifySessionStateChanged(SessionState::LOCKED);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kSignOut}));

  gfx::NativeWindow window = login_shelf_view_->GetWidget()->GetNativeWindow();
  views::View* status_area = RootWindowController::ForWindow(window)
                                 ->GetStatusAreaWidget()
                                 ->GetContentsView();

  // Give focus to the shelf. The tabbing between lock screen and shelf is
  // verified by |LockScreenSanityTest::TabGoesFromLockToShelfAndBackToLock|.
  FocusOnLoginShelfButton();
  ExpectNotFocused(status_area);
  EXPECT_TRUE(
      login_shelf_view_->GetViewByID(LoginShelfView::kShutdown)->HasFocus());

  // Focus from the first button to the second button.
  views::View* login_shelf_contents_view =
      GetLoginShelfWidget()->GetContentsView();
  PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB);
  ExpectFocused(login_shelf_contents_view);
  ExpectNotFocused(status_area);
  EXPECT_TRUE(
      login_shelf_view_->GetViewByID(LoginShelfView::kSignOut)->HasFocus());

  // Focus from the second button to the status area.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB);
  ExpectNotFocused(login_shelf_contents_view);
  ExpectFocused(status_area);

  // A single shift+tab brings focus back to the second shelf button.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB, ui::EF_SHIFT_DOWN);
  ExpectFocused(login_shelf_contents_view);
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

TEST_F(LoginShelfViewTest, ShelfWidgetStackedAtBottomInActiveSession) {
  gfx::NativeWindow window = login_shelf_view_->GetWidget()->GetNativeWindow();
  ShelfWidget* shelf_widget = Shelf::ForWindow(window)->shelf_widget();

  // Focus on the login shelf button (which could happen if user tabs to move
  // the focus).
  NotifySessionStateChanged(SessionState::LOGIN_PRIMARY);
  FocusOnLoginShelfButton();

  // Verify that shelf widget is no longer focused, and is stacked at the bottom
  // of shelf container when the session is activated.
  NotifySessionStateChanged(SessionState::ACTIVE);

  ExpectNotFocused(shelf_widget->GetContentsView());
  EXPECT_EQ(shelf_widget->GetNativeWindow(),
            shelf_widget->GetNativeWindow()->parent()->children()[0]);

  // Lock screen and focus the shelf again.
  NotifySessionStateChanged(SessionState::LOCKED);
  Shell::Get()->focus_cycler()->FocusWidget(shelf_widget);

  // Move focus away from the shelf, to verify the shelf widget stacking is
  // updated even if the widget is not active when the session state changes.
  PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB);
  PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB);

  ExpectNotFocused(shelf_widget->GetContentsView());

  // Verify that shelf widget is no longer focused, and is stacked at the bottom
  // of shelf container when the session is activated.
  NotifySessionStateChanged(SessionState::ACTIVE);
  ExpectNotFocused(shelf_widget->GetContentsView());
  EXPECT_EQ(shelf_widget->GetNativeWindow(),
            shelf_widget->GetNativeWindow()->parent()->children()[0]);
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
  CreateUserSessions(1);
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

TEST_F(LoginShelfViewTest, EnterpriseEnrollmentButtonVisibility) {
  // Enterprise enrollment button should only be available when user creation
  // screen is shown in OOBE.
  login_shelf_view_->SetLoginDialogState(OobeDialogState::USER_CREATION);

  NotifySessionStateChanged(SessionState::OOBE);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kEnterpriseEnrollment}));

  NotifySessionStateChanged(SessionState::LOGIN_PRIMARY);
  EXPECT_TRUE(ShowsShelfButtons({}));

  NotifySessionStateChanged(SessionState::LOGGED_IN_NOT_ACTIVE);
  EXPECT_TRUE(ShowsShelfButtons({}));

  NotifySessionStateChanged(SessionState::ACTIVE);
  EXPECT_TRUE(ShowsShelfButtons({}));

  NotifySessionStateChanged(SessionState::LOCKED);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kSignOut}));

  NotifySessionStateChanged(SessionState::LOGIN_SECONDARY);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kCancel}));
}

TEST_F(LoginShelfViewTest, OsInstallButtonHidden) {
  // OS Install Button should be hidden if the kAllowOsInstall switch is
  // not set.
  NotifySessionStateChanged(SessionState::LOGIN_PRIMARY);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown,
                                 LoginShelfView::kBrowseAsGuest,
                                 LoginShelfView::kAddUser}));

  login_shelf_view_->SetIsFirstSigninStep(/*is_first=*/true);
  SetUserCount(0);
  // When no user pods are visible, the Gaia dialog would normally pop up. We
  // need to simulate that behavior in this test.
  login_shelf_view_->SetLoginDialogState(OobeDialogState::GAIA_SIGNIN);
  EXPECT_TRUE(ShowsShelfButtons(
      {LoginShelfView::kShutdown, LoginShelfView::kBrowseAsGuest}));
}

TEST_F(LoginShelfViewTest, TapShutdownInTabletLoginPrimary) {
  NotifySessionStateChanged(session_manager::SessionState::LOGIN_PRIMARY);
  TabletModeControllerTestApi().EnterTabletMode();

  ShutdownAndConfirm();
  EXPECT_TRUE(Shell::Get()->lock_state_controller()->ShutdownRequested());
}

TEST_F(LoginShelfViewTest, TapShutdownInTabletOobe) {
  TabletModeControllerTestApi().EnterTabletMode();
  NotifySessionStateChanged(session_manager::SessionState::OOBE);
  login_shelf_view_->SetLoginDialogState(OobeDialogState::GAIA_SIGNIN);
  login_shelf_view_->SetIsFirstSigninStep(/*is_first=*/true);
  ShutdownAndConfirm();
  EXPECT_TRUE(Shell::Get()->lock_state_controller()->ShutdownRequested());
}

TEST_F(LoginShelfViewTest, MouseWheelOnLoginShelf) {
  gfx::NativeWindow window = login_shelf_view_->GetWidget()->GetNativeWindow();
  ShelfWidget* const shelf_widget = Shelf::ForWindow(window)->shelf_widget();
  const gfx::Rect shelf_bounds = shelf_widget->GetWindowBoundsInScreen();

  gfx::Point kLocations[] = {
      shelf_bounds.left_center() + gfx::Vector2d(10, 0),
      shelf_bounds.right_center() + gfx::Vector2d(-10, 0),
      shelf_bounds.CenterPoint()};

  ui::test::EventGenerator* event_generator = GetEventGenerator();
  auto test_mouse_wheel_noop = [&event_generator, &shelf_widget,
                                &shelf_bounds](const gfx::Point& location) {
    event_generator->MoveMouseTo(location);

    event_generator->MoveMouseWheel(/*delta_x=*/0, 100);
    EXPECT_EQ(shelf_bounds, shelf_widget->GetWindowBoundsInScreen());
    EXPECT_FALSE(Shell::Get()->app_list_controller()->IsVisible());

    event_generator->MoveMouseWheel(/*delta_x=*/0, -100);
    EXPECT_EQ(shelf_bounds, shelf_widget->GetWindowBoundsInScreen());
    EXPECT_FALSE(Shell::Get()->app_list_controller()->IsVisible());
  };

  for (const auto& location : kLocations) {
    SCOPED_TRACE(testing::Message()
                 << "Mouse wheel in OOBE at " << location.ToString());

    test_mouse_wheel_noop(location);
  }

  NotifySessionStateChanged(SessionState::LOGIN_PRIMARY);

  for (const auto& location : kLocations) {
    SCOPED_TRACE(testing::Message()
                 << "Mouse wheel on login at " << location.ToString());
    test_mouse_wheel_noop(location);
  }

  CreateUserSessions(1);
  NotifySessionStateChanged(SessionState::LOCKED);

  for (const auto& location : kLocations) {
    SCOPED_TRACE(testing::Message()
                 << "Mouse wheel on lock screen at " << location.ToString());
    test_mouse_wheel_noop(location);
  }
}

// When display is on Shutdown button clicks should not be blocked.
TEST_F(LoginShelfViewTest, DisplayOn) {
  display::DisplayConfigurator* configurator =
      ash::Shell::Get()->display_configurator();
  ASSERT_TRUE(configurator->IsDisplayOn());

  // Set a State where LoginShelfView::kShutdown is visible
  NotifySessionStateChanged(SessionState::LOGIN_PRIMARY);
  ShutdownAndConfirm();
  EXPECT_TRUE(Shell::Get()->lock_state_controller()->ShutdownRequested());
}

// When display is off Shutdown button clicks should be blocked
// `kMaxDroppedCallsWhenDisplaysOff` times.
TEST_F(LoginShelfViewTest, DisplayOff) {
  display::DisplayConfigurator* configurator =
      ash::Shell::Get()->display_configurator();
  display::test::ActionLogger action_logger;
  configurator->SetDelegateForTesting(
      std::make_unique<display::test::TestNativeDisplayDelegate>(
          &action_logger));

  base::RunLoop run_loop;
  configurator->SuspendDisplays(base::BindOnce(
      [](base::OnceClosure quit_closure, bool success) {
        EXPECT_TRUE(success);
        std::move(quit_closure).Run();
      },
      run_loop.QuitClosure()));

  run_loop.Run();
  ASSERT_FALSE(configurator->IsDisplayOn());

  // Set a State where LoginShelfView::kShutdown is visible
  NotifySessionStateChanged(session_manager::SessionState::OOBE);
  login_shelf_view_->SetLoginDialogState(OobeDialogState::GAIA_SIGNIN);
  login_shelf_view_->SetIsFirstSigninStep(/*is_first=*/true);

  // The first calls are blocked.
  constexpr int kMaxDropped =
      3;  // correspond to `kMaxDroppedCallsWhenDisplaysOff`
  for (int i = 0; i < kMaxDropped; ++i) {
    Click(LoginShelfView::kShutdown);
    EXPECT_FALSE(Shell::Get()->lock_state_controller()->ShutdownRequested());
  }

  // This should go through.
  ShutdownAndConfirm();
  EXPECT_TRUE(Shell::Get()->lock_state_controller()->ShutdownRequested());
}

// Checks that the add button click appears in the auth events.
TEST_F(LoginShelfViewTest, AddUserAuthEventRecord) {
  EXPECT_TRUE(ShowsShelfButtons({}));
  NotifySessionStateChanged(SessionState::LOGIN_PRIMARY);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown,
                                 LoginShelfView::kBrowseAsGuest,
                                 LoginShelfView::kAddUser}));
  Click(LoginShelfView::kAddUser);
  AuthEventsRecorder* auth_recorder = AuthEventsRecorder::Get();
  std::string auth_events = auth_recorder->GetAuthEventsLog();
  EXPECT_EQ(auth_events, "auth_surface_change_Login,add_user,");
}

TEST_F(LoginShelfViewTest, AccessibleProperties) {
  ui::AXNodeData data;

  login_shelf_view_->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kToolbar);
  EXPECT_EQ(data.GetStringAttribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringUTF8(IDS_ASH_SHELF_ACCESSIBLE_NAME));
}

class OsInstallButtonTest : public LoginShelfViewTest {
 public:
  OsInstallButtonTest() = default;
  ~OsInstallButtonTest() override = default;
  OsInstallButtonTest(const OsInstallButtonTest&) = delete;
  void operator=(const OsInstallButtonTest&) = delete;

  void SetUp() override {
    LoginShelfViewTest::SetUp();
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAllowOsInstall);
  }
};

TEST_F(OsInstallButtonTest, ClickOsInstallButton) {
  auto client = std::make_unique<MockLoginScreenClient>();
  EXPECT_CALL(*client, ShowOsInstallScreen);
  NotifySessionStateChanged(SessionState::LOGIN_PRIMARY);

  Click(LoginShelfView::kOsInstall);
}

TEST_F(OsInstallButtonTest, OsInstallButtonVisibility) {
  NotifySessionStateChanged(SessionState::LOGIN_PRIMARY);
  EXPECT_TRUE(ShowsShelfButtons(
      {LoginShelfView::kShutdown, LoginShelfView::kBrowseAsGuest,
       LoginShelfView::kAddUser, LoginShelfView::kOsInstall}));

  NotifySessionStateChanged(SessionState::LOGGED_IN_NOT_ACTIVE);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown}));

  NotifySessionStateChanged(SessionState::ACTIVE);
  EXPECT_TRUE(ShowsShelfButtons({}));

  NotifySessionStateChanged(SessionState::LOCKED);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kSignOut}));

  NotifySessionStateChanged(SessionState::LOGIN_SECONDARY);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kCancel}));

  // OS Install button should be shown if the user_creation dialog was
  // shown during OOBE.
  SetUserCount(0);
  login_shelf_view_->SetIsFirstSigninStep(/*is_first=*/true);
  login_shelf_view_->SetLoginDialogState(OobeDialogState::USER_CREATION);
  NotifySessionStateChanged(SessionState::OOBE);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kEnterpriseEnrollment,
                                 LoginShelfView::kBrowseAsGuest,
                                 LoginShelfView::kOsInstall}));

  // When no user pods are visible, the Gaia dialog would normally pop up. We
  // need to simulate that behavior in this test.
  login_shelf_view_->SetLoginDialogState(OobeDialogState::GAIA_SIGNIN);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown,
                                 LoginShelfView::kBrowseAsGuest,
                                 LoginShelfView::kOsInstall}));

  // OS Install button should be hidden if the user_creation dialog was
  // opened from the primary login screen.
  SetUserCount(1);
  login_shelf_view_->SetIsFirstSigninStep(/*is_first=*/false);
  login_shelf_view_->SetLoginDialogState(OobeDialogState::USER_CREATION);
  NotifySessionStateChanged(SessionState::LOGIN_PRIMARY);
  EXPECT_TRUE(ShowsShelfButtons({}));
}

namespace {

const char kShelfShutdownConfirmationActionHistogramName[] =
    "Ash.Shelf.ShutdownConfirmationBubble.Action";

}  // namespace

class LoginShelfViewWithShutdownConfirmationTest : public LoginShelfViewTest {
 public:
  LoginShelfViewWithShutdownConfirmationTest() = default;

  LoginShelfViewWithShutdownConfirmationTest(
      const LoginShelfViewWithShutdownConfirmationTest&) = delete;
  LoginShelfViewWithShutdownConfirmationTest& operator=(
      const LoginShelfViewWithShutdownConfirmationTest&) = delete;

  ~LoginShelfViewWithShutdownConfirmationTest() override = default;

  base::HistogramTester& histograms() { return histograms_; }

 protected:
  // Check whether the shutdown confirmation is visible.
  bool IsShutdownConfirmationVisible() {
    return login_shelf_view_->GetShutdownConfirmationBubbleForTesting() &&
           login_shelf_view_->GetShutdownConfirmationBubbleForTesting()
               ->GetWidget()
               ->IsVisible();
  }

  // Cancel shutdown confirmation bubble.
  void CancelShutdown() {
    ui::test::EventGenerator* event_generator = GetEventGenerator();
    event_generator->MoveMouseTo(
        login_shelf_view_->GetShutdownConfirmationBubbleForTesting()
            ->GetViewByID(static_cast<int>(
                ShelfShutdownConfirmationBubble::ButtonId::kCancel))
            ->GetBoundsInScreen()
            .CenterPoint());
    event_generator->ClickLeftButton();

    base::RunLoop().RunUntilIdle();
  }

  // Confirm shutdown confirmation bubble.
  void ConfirmShutdown() {
    ui::test::EventGenerator* event_generator = GetEventGenerator();
    event_generator->MoveMouseTo(
        login_shelf_view_->GetShutdownConfirmationBubbleForTesting()
            ->GetViewByID(static_cast<int>(
                ShelfShutdownConfirmationBubble::ButtonId::kShutdown))
            ->GetBoundsInScreen()
            .CenterPoint());
    event_generator->ClickLeftButton();

    base::RunLoop().RunUntilIdle();
  }

  // Dismiss shutdown confirmation bubble.
  void DismissShutdown() {
    // Focus on the login shelf button (which could happen if user tabs to move
    // the focus).
    FocusOnLoginShelfButton();

    base::RunLoop().RunUntilIdle();
  }

 private:
  // Histogram value verifier.
  base::HistogramTester histograms_;
};

// Checks that shutdown confirmation bubble appears after pressing the
// shutdown button on the lockscreen
TEST_F(LoginShelfViewWithShutdownConfirmationTest,
       ShouldShowAfterShutdownButtonLockSession) {
  CreateUserSessions(1);
  NotifySessionStateChanged(SessionState::LOCKED);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kSignOut}));
  EXPECT_FALSE(IsShutdownConfirmationVisible());

  Click(LoginShelfView::kShutdown);
  EXPECT_TRUE(IsShutdownConfirmationVisible());
  EXPECT_FALSE(Shell::Get()->lock_state_controller()->ShutdownRequested());

  histograms().ExpectUniqueSample(
      kShelfShutdownConfirmationActionHistogramName,
      ShelfShutdownConfirmationBubble::BubbleAction::kOpened, 1);
  histograms().ExpectTotalCount(kShelfShutdownConfirmationActionHistogramName,
                                1);
}

// Checks that shutdown confirmation bubble appears after pressing the
// shutdown button on the lockscreen
TEST_F(LoginShelfViewWithShutdownConfirmationTest,
       ShouldShowAfterShutdownButtonLoginPrimarySession) {
  login_shelf_view_->SetAllowLoginAsGuest(true /*allow_guest*/);
  NotifySessionStateChanged(SessionState::LOGIN_PRIMARY);
  EXPECT_TRUE(ShowsShelfButtons({LoginShelfView::kShutdown,
                                 LoginShelfView::kBrowseAsGuest,
                                 LoginShelfView::kAddUser}));
  EXPECT_FALSE(IsShutdownConfirmationVisible());

  Click(LoginShelfView::kShutdown);
  EXPECT_TRUE(IsShutdownConfirmationVisible());
  EXPECT_FALSE(Shell::Get()->lock_state_controller()->ShutdownRequested());

  histograms().ExpectUniqueSample(
      kShelfShutdownConfirmationActionHistogramName,
      ShelfShutdownConfirmationBubble::BubbleAction::kOpened, 1);
  histograms().ExpectTotalCount(kShelfShutdownConfirmationActionHistogramName,
                                1);
}

// Checks that shutdown confirmation bubble disappears after pressing the
// cancel button on the shutdown confirmation bubble and could be shown again.
TEST_F(LoginShelfViewWithShutdownConfirmationTest,
       ShouldCloseAfterCancelButton) {
  CreateUserSessions(1);
  NotifySessionStateChanged(SessionState::LOCKED);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kSignOut}));
  EXPECT_FALSE(IsShutdownConfirmationVisible());

  Click(LoginShelfView::kShutdown);
  EXPECT_TRUE(IsShutdownConfirmationVisible());
  EXPECT_FALSE(Shell::Get()->lock_state_controller()->ShutdownRequested());
  histograms().ExpectBucketCount(
      kShelfShutdownConfirmationActionHistogramName,
      ShelfShutdownConfirmationBubble::BubbleAction::kOpened, 1);
  histograms().ExpectTotalCount(kShelfShutdownConfirmationActionHistogramName,
                                1);

  // Shutdown confirmation is cancelled and disappeared.
  CancelShutdown();
  EXPECT_FALSE(IsShutdownConfirmationVisible());
  EXPECT_FALSE(Shell::Get()->lock_state_controller()->ShutdownRequested());
  histograms().ExpectBucketCount(
      kShelfShutdownConfirmationActionHistogramName,
      ShelfShutdownConfirmationBubble::BubbleAction::kCancelled, 1);
  histograms().ExpectTotalCount(kShelfShutdownConfirmationActionHistogramName,
                                2);

  // Shutdown confirmation could be shown again.
  Click(LoginShelfView::kShutdown);
  EXPECT_TRUE(IsShutdownConfirmationVisible());
  EXPECT_FALSE(Shell::Get()->lock_state_controller()->ShutdownRequested());
  histograms().ExpectBucketCount(
      kShelfShutdownConfirmationActionHistogramName,
      ShelfShutdownConfirmationBubble::BubbleAction::kOpened, 2);
  histograms().ExpectTotalCount(kShelfShutdownConfirmationActionHistogramName,
                                3);
}

// Checks that shutdown confirmation bubble disappears after pressing the
// confirmation button on the shutdown confirmation bubble and the device shuts
// down.
TEST_F(LoginShelfViewWithShutdownConfirmationTest,
       ShouldCloseAndShutdownAfterConfirmButton) {
  CreateUserSessions(1);
  NotifySessionStateChanged(SessionState::LOCKED);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kSignOut}));
  EXPECT_FALSE(IsShutdownConfirmationVisible());

  Click(LoginShelfView::kShutdown);
  EXPECT_TRUE(IsShutdownConfirmationVisible());
  EXPECT_FALSE(Shell::Get()->lock_state_controller()->ShutdownRequested());
  histograms().ExpectBucketCount(
      kShelfShutdownConfirmationActionHistogramName,
      ShelfShutdownConfirmationBubble::BubbleAction::kOpened, 1);
  histograms().ExpectTotalCount(kShelfShutdownConfirmationActionHistogramName,
                                1);

  // Shutdown confirmation is confirmed and disappeared.
  ConfirmShutdown();
  EXPECT_FALSE(IsShutdownConfirmationVisible());
  EXPECT_TRUE(Shell::Get()->lock_state_controller()->ShutdownRequested());
  histograms().ExpectBucketCount(
      kShelfShutdownConfirmationActionHistogramName,
      ShelfShutdownConfirmationBubble::BubbleAction::kConfirmed, 1);
  histograms().ExpectTotalCount(kShelfShutdownConfirmationActionHistogramName,
                                2);
}

// Checks that shutdown confirmation bubble disappears after inactive.
TEST_F(LoginShelfViewWithShutdownConfirmationTest, ShouldCloseAfterInactive) {
  CreateUserSessions(1);
  NotifySessionStateChanged(SessionState::LOCKED);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kSignOut}));
  EXPECT_FALSE(IsShutdownConfirmationVisible());

  Click(LoginShelfView::kShutdown);
  EXPECT_TRUE(IsShutdownConfirmationVisible());
  EXPECT_FALSE(Shell::Get()->lock_state_controller()->ShutdownRequested());
  histograms().ExpectBucketCount(
      kShelfShutdownConfirmationActionHistogramName,
      ShelfShutdownConfirmationBubble::BubbleAction::kOpened, 1);
  histograms().ExpectTotalCount(kShelfShutdownConfirmationActionHistogramName,
                                1);

  DismissShutdown();

  // Shutdown confirmation is inactive and disappeared.
  EXPECT_FALSE(IsShutdownConfirmationVisible());
  EXPECT_FALSE(Shell::Get()->lock_state_controller()->ShutdownRequested());
  histograms().ExpectBucketCount(
      kShelfShutdownConfirmationActionHistogramName,
      ShelfShutdownConfirmationBubble::BubbleAction::kDismissed, 1);
  histograms().ExpectTotalCount(kShelfShutdownConfirmationActionHistogramName,
                                2);
}

// Checks that shutdown confirmation was first cancelled, then confirmed
TEST_F(LoginShelfViewWithShutdownConfirmationTest,
       ShouldCloseAndShutdownAfterCancelAndConfirmButton) {
  CreateUserSessions(1);
  NotifySessionStateChanged(SessionState::LOCKED);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kSignOut}));
  EXPECT_FALSE(IsShutdownConfirmationVisible());

  Click(LoginShelfView::kShutdown);
  EXPECT_TRUE(IsShutdownConfirmationVisible());
  EXPECT_FALSE(Shell::Get()->lock_state_controller()->ShutdownRequested());
  histograms().ExpectBucketCount(
      kShelfShutdownConfirmationActionHistogramName,
      ShelfShutdownConfirmationBubble::BubbleAction::kOpened, 1);
  histograms().ExpectTotalCount(kShelfShutdownConfirmationActionHistogramName,
                                1);

  // Shutdown confirmation is cancelled and disappeared.
  CancelShutdown();
  EXPECT_FALSE(IsShutdownConfirmationVisible());
  EXPECT_FALSE(Shell::Get()->lock_state_controller()->ShutdownRequested());
  histograms().ExpectBucketCount(
      kShelfShutdownConfirmationActionHistogramName,
      ShelfShutdownConfirmationBubble::BubbleAction::kCancelled, 1);
  histograms().ExpectTotalCount(kShelfShutdownConfirmationActionHistogramName,
                                2);

  // Shutdown confirmation could be shown again.
  Click(LoginShelfView::kShutdown);
  EXPECT_TRUE(IsShutdownConfirmationVisible());
  EXPECT_FALSE(Shell::Get()->lock_state_controller()->ShutdownRequested());
  histograms().ExpectBucketCount(
      kShelfShutdownConfirmationActionHistogramName,
      ShelfShutdownConfirmationBubble::BubbleAction::kOpened, 2);
  histograms().ExpectTotalCount(kShelfShutdownConfirmationActionHistogramName,
                                3);

  // Shutdown confirmation is confirmed and disappeared.
  ConfirmShutdown();
  EXPECT_FALSE(IsShutdownConfirmationVisible());
  EXPECT_TRUE(Shell::Get()->lock_state_controller()->ShutdownRequested());
  histograms().ExpectBucketCount(
      kShelfShutdownConfirmationActionHistogramName,
      ShelfShutdownConfirmationBubble::BubbleAction::kConfirmed, 1);
  histograms().ExpectTotalCount(kShelfShutdownConfirmationActionHistogramName,
                                4);
}

// When display is on Shutdown button clicks should not be blocked.
TEST_F(LoginShelfViewWithShutdownConfirmationTest, DisplayOn) {
  display::DisplayConfigurator* configurator =
      ash::Shell::Get()->display_configurator();
  ASSERT_TRUE(configurator->IsDisplayOn());

  NotifySessionStateChanged(session_manager::SessionState::OOBE);
  login_shelf_view_->SetLoginDialogState(OobeDialogState::GAIA_SIGNIN);
  login_shelf_view_->SetIsFirstSigninStep(/*is_first=*/true);
  Click(LoginShelfView::kShutdown);
  EXPECT_TRUE(IsShutdownConfirmationVisible());
  EXPECT_FALSE(Shell::Get()->lock_state_controller()->ShutdownRequested());
}

// When display is off Shutdown button clicks should be blocked
// `kMaxDroppedCallsWhenDisplaysOff` times.
TEST_F(LoginShelfViewWithShutdownConfirmationTest, DisplayOff) {
  display::DisplayConfigurator* configurator =
      ash::Shell::Get()->display_configurator();
  display::test::ActionLogger action_logger;
  configurator->SetDelegateForTesting(
      std::make_unique<display::test::TestNativeDisplayDelegate>(
          &action_logger));

  base::RunLoop run_loop;
  configurator->SuspendDisplays(base::BindOnce(
      [](base::OnceClosure quit_closure, bool success) {
        EXPECT_TRUE(success);
        std::move(quit_closure).Run();
      },
      run_loop.QuitClosure()));

  run_loop.Run();
  ASSERT_FALSE(configurator->IsDisplayOn());

  // Set a State where LoginShelfView::kShutdown is visible
  NotifySessionStateChanged(session_manager::SessionState::OOBE);
  login_shelf_view_->SetLoginDialogState(OobeDialogState::GAIA_SIGNIN);
  login_shelf_view_->SetIsFirstSigninStep(/*is_first=*/true);

  // The first calls are blocked.
  constexpr int kMaxDropped =
      3;  // correspond to `kMaxDroppedCallsWhenDisplaysOff`
  for (int i = 0; i < kMaxDropped; ++i) {
    Click(LoginShelfView::kShutdown);
    EXPECT_FALSE(IsShutdownConfirmationVisible());
    EXPECT_FALSE(Shell::Get()->lock_state_controller()->ShutdownRequested());
  }

  Click(LoginShelfView::kShutdown);
  EXPECT_TRUE(IsShutdownConfirmationVisible());
  ConfirmShutdown();
  EXPECT_FALSE(IsShutdownConfirmationVisible());
  EXPECT_TRUE(Shell::Get()->lock_state_controller()->ShutdownRequested());
}

TEST_F(LoginShelfViewWithShutdownConfirmationTest, ClickRestartButton) {
  CreateUserSessions(1);
  NotifySessionStateChanged(SessionState::LOCKED);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kShutdown, LoginShelfView::kSignOut}));

  NotifyShutdownPolicyChanged(true /*reboot_on_shutdown*/);
  EXPECT_TRUE(
      ShowsShelfButtons({LoginShelfView::kRestart, LoginShelfView::kSignOut}));

  Click(LoginShelfView::kRestart);
  EXPECT_FALSE(IsShutdownConfirmationVisible());
  EXPECT_TRUE(Shell::Get()->lock_state_controller()->ShutdownRequested());
}

TEST_F(LoginShelfViewWithShutdownConfirmationTest,
       ShelfShutdownConfirmationBubbleAccessibleProperties) {
  CreateUserSessions(1);
  NotifySessionStateChanged(SessionState::LOCKED);
  Click(LoginShelfView::kShutdown);
  auto* confirmation_bubble =
      login_shelf_view_->GetShutdownConfirmationBubbleForTesting();
  ui::AXNodeData data;

  ASSERT_TRUE(confirmation_bubble);
  confirmation_bubble->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kDialog);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringUTF16(IDS_ASH_SHUTDOWN_CONFIRMATION_TITLE));
}

class LoginShelfViewWithKioskLicenseTest : public LoginShelfViewTest {
 public:
  LoginShelfViewWithKioskLicenseTest() {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kEnableKioskLoginScreen);
  }
  LoginShelfViewWithKioskLicenseTest(
      const LoginShelfViewWithKioskLicenseTest&) = delete;
  LoginShelfViewWithKioskLicenseTest& operator=(
      const LoginShelfViewWithKioskLicenseTest&) = delete;

  ~LoginShelfViewWithKioskLicenseTest() override = default;

 protected:
  // Check whether the kiosk instruction bubble is visible.
  bool IsKioskInstructionBubbleVisible() {
    return login_shelf_view_->GetKioskInstructionBubbleForTesting() &&
           login_shelf_view_->GetKioskInstructionBubbleForTesting()
               ->GetWidget()
               ->IsVisible();
  }

  void SetKioskLicenseModeForTesting(bool is_kiosk_license_mode) {
    login_shelf_view_->SetKioskLicenseModeForTesting(is_kiosk_license_mode);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Checks that kiosk app button and kiosk instruction appears if device is with
// kiosk license.
TEST_F(LoginShelfViewWithKioskLicenseTest, ShouldShowKioskInstructionBubble) {
  SetKioskLicenseModeForTesting(true);
  NotifySessionStateChanged(SessionState::LOGIN_PRIMARY);

  login_shelf_view_->SetKioskApps(GetNFakeKioskApps(1));

  EXPECT_TRUE(
      login_shelf_view_->GetViewByID(LoginShelfView::kApps)->GetVisible());
  EXPECT_TRUE(IsKioskInstructionBubbleVisible());
}

// Checks that kiosk app button appears and kiosk instruction hidden if device
// is not with kiosk license.
TEST_F(LoginShelfViewWithKioskLicenseTest, ShouldHideKioskInstructionBubble) {
  SetKioskLicenseModeForTesting(false);
  NotifySessionStateChanged(SessionState::LOGIN_PRIMARY);

  login_shelf_view_->SetKioskApps(GetNFakeKioskApps(1));

  EXPECT_TRUE(
      login_shelf_view_->GetViewByID(LoginShelfView::kApps)->GetVisible());
  EXPECT_FALSE(IsKioskInstructionBubbleVisible());
}

// Checks that kiosk app button appears and kiosk instruction hidden if device
// is with kiosk license and no kiosk app is set up.
TEST_F(LoginShelfViewWithKioskLicenseTest,
       ShouldNotShowKioskInstructionBubble) {
  SetKioskLicenseModeForTesting(true);
  NotifySessionStateChanged(SessionState::LOGIN_PRIMARY);

  login_shelf_view_->SetKioskApps({});

  EXPECT_FALSE(
      login_shelf_view_->GetViewByID(LoginShelfView::kApps)->GetVisible());
  EXPECT_FALSE(IsKioskInstructionBubbleVisible());
}

// Checks that the button of guest mode is shown if allow_guest_ is set to
// true for devices with Kiosk SKU.
TEST_F(LoginShelfViewWithKioskLicenseTest, ShowGuestModeButton) {
  SetKioskLicenseModeForTesting(true);
  NotifySessionStateChanged(SessionState::LOGIN_PRIMARY);

  login_shelf_view_->SetAllowLoginAsGuest(true);

  EXPECT_TRUE(login_shelf_view_->GetViewByID(LoginShelfView::kBrowseAsGuest)
                  ->GetVisible());
}

// Checks that the button of guest mode is hidden if allow_guest_ is set to
// false for devices with Kiosk SKU.
TEST_F(LoginShelfViewWithKioskLicenseTest, HideGuestModeButton) {
  SetKioskLicenseModeForTesting(true);
  NotifySessionStateChanged(SessionState::LOGIN_PRIMARY);

  login_shelf_view_->SetAllowLoginAsGuest(false);

  EXPECT_FALSE(login_shelf_view_->GetViewByID(LoginShelfView::kBrowseAsGuest)
                   ->GetVisible());
}

}  // namespace
}  // namespace ash
