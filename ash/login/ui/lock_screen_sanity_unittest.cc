// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/login/login_screen_controller.h"
#include "ash/login/mock_login_screen_client.h"
#include "ash/login/ui/fake_login_detachable_base_model.h"
#include "ash/login/ui/lock_contents_view.h"
#include "ash/login/ui/login_auth_user_view.h"
#include "ash/login/ui/login_big_user_view.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/login_test_utils.h"
#include "ash/root_window_controller.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shelf/login_shelf_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"

using ::testing::_;
using ::testing::Invoke;
using LockScreenSanityTest = ash::LoginTestBase;

namespace ash {
namespace {

// Returns the widget contents view that contains login shelf in the same root
// window as `native_window`.
views::View* GetLoginShelfContentsView(gfx::NativeWindow native_window) {
  Shelf* shelf = Shelf::ForWindow(native_window);
  return shelf->login_shelf_widget()->GetContentsView();
}

class LockScreenAppFocuser {
 public:
  explicit LockScreenAppFocuser(views::Widget* lock_screen_app_widget)
      : lock_screen_app_widget_(lock_screen_app_widget) {}

  LockScreenAppFocuser(const LockScreenAppFocuser&) = delete;
  LockScreenAppFocuser& operator=(const LockScreenAppFocuser&) = delete;

  ~LockScreenAppFocuser() = default;

  bool reversed_tab_order() const { return reversed_tab_order_; }

  void FocusLockScreenApp(bool reverse) {
    reversed_tab_order_ = reverse;
    lock_screen_app_widget_->Activate();
  }

 private:
  bool reversed_tab_order_ = false;
  raw_ptr<views::Widget> lock_screen_app_widget_;
};

testing::AssertionResult VerifyFocused(views::View* view) {
  if (!view->GetWidget()->IsActive()) {
    return testing::AssertionFailure() << "Widget not active.";
  }
  if (!HasFocusInAnyChildView(view)) {
    return testing::AssertionFailure() << "No focused descendant.";
  }
  return testing::AssertionSuccess();
}

testing::AssertionResult VerifyNotFocused(views::View* view) {
  if (view->GetWidget()->IsActive()) {
    return testing::AssertionFailure() << "Widget active";
  }
  if (HasFocusInAnyChildView(view)) {
    return testing::AssertionFailure() << "Has focused descendant.";
  }
  return testing::AssertionSuccess();
}

}  // namespace

// Verifies that the password input box has focus.
TEST_F(LockScreenSanityTest, PasswordIsInitiallyFocused) {
  // Build lock screen.
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));

  // The lock screen requires at least one user.
  SetUserCount(1);

  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);

  // Textfield should have focus.
  EXPECT_EQ(
      MakeLoginPasswordTestApi(contents, AuthTarget::kPrimary).textfield(),
      contents->GetFocusManager()->GetFocusedView());
}

// Verifies submitting the password invokes mojo lock screen client.
TEST_F(LockScreenSanityTest, PasswordSubmitCallsLoginScreenClient) {
  // Build lock screen.
  auto* contents = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));

  // The lock screen requires at least one user.
  SetUserCount(1);

  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);

  // Password submit runs mojo.
  auto client = std::make_unique<MockLoginScreenClient>();
  client->set_authenticate_user_callback_result(false);
  EXPECT_CALL(*client, AuthenticateUserWithPasswordOrPin_(
                           users()[0].basic_user_info.account_id, _, false, _));
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode::VKEY_A, 0);
  generator->PressKey(ui::KeyboardCode::VKEY_RETURN, 0);
  base::RunLoop().RunUntilIdle();
}

// Verifies that password text is cleared only after the browser-process
// authentication request is complete and the auth fails.
TEST_F(LockScreenSanityTest,
       PasswordSubmitClearsPasswordAfterFailedAuthentication) {
  auto client = std::make_unique<MockLoginScreenClient>();

  auto* contents = new LockContentsView(
      mojom::TrayActionState::kAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);
  LoginPasswordView::TestApi password_test_api =
      MakeLoginPasswordTestApi(contents, AuthTarget::kPrimary);

  base::OnceCallback<void(bool)> callback;
  auto submit_password = [&]() {
    // Capture the authentication callback.
    client->set_authenticate_user_with_password_or_pin_callback_storage(
        &callback);
    EXPECT_CALL(*client, AuthenticateUserWithPasswordOrPin_(
                             testing::_, testing::_, testing::_, testing::_));

    // Submit password with content 'a'. This creates a browser-process
    // authentication request stored in |callback|.
    DCHECK(callback.is_null());
    ui::test::EventGenerator* generator = GetEventGenerator();
    generator->PressKey(ui::KeyboardCode::VKEY_A, 0);
    generator->PressKey(ui::KeyboardCode::VKEY_RETURN, 0);
    base::RunLoop().RunUntilIdle();
    DCHECK(!callback.is_null());
  };

  // Run the browser-process authentication request. Verify that the password is
  // cleared after the ash callback handler has completed and auth has failed.
  submit_password();
  EXPECT_FALSE(password_test_api.textfield()->GetText().empty());
  EXPECT_TRUE(password_test_api.textfield()->GetReadOnly());
  std::move(callback).Run(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(password_test_api.textfield()->GetText().empty());
  EXPECT_FALSE(password_test_api.textfield()->GetReadOnly());

  // Repeat the above process. Verify that the password is not cleared if auth
  // succeeds.
  submit_password();
  EXPECT_FALSE(password_test_api.textfield()->GetText().empty());
  EXPECT_TRUE(password_test_api.textfield()->GetReadOnly());
  std::move(callback).Run(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(password_test_api.textfield()->GetText().empty());
  EXPECT_TRUE(password_test_api.textfield()->GetReadOnly());
}

// Verifies that tabbing from the lock screen will eventually focus the shelf.
// Then, a shift+tab will bring focus back to the lock screen.
TEST_F(LockScreenSanityTest, TabGoesFromLockToShelfAndBackToLock) {
  auto client = std::make_unique<MockLoginScreenClient>();
  // Make lock screen shelf visible.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);

  // Create lock screen.
  auto* lock = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(lock);
  views::View* login_shelf_contents_view =
      GetLoginShelfContentsView(lock->GetWidget()->GetNativeWindow());

  // Lock has focus.
  EXPECT_TRUE(VerifyFocused(lock));
  EXPECT_TRUE(VerifyNotFocused(login_shelf_contents_view));

  // Tab (eventually) goes to the shelf.
  ASSERT_TRUE(TabThroughView(GetEventGenerator(), lock, false /*reverse*/));
  EXPECT_TRUE(VerifyNotFocused(lock));
  EXPECT_TRUE(VerifyFocused(login_shelf_contents_view));

  // A single shift+tab brings focus back to the lock screen.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(VerifyFocused(lock));
  EXPECT_TRUE(VerifyNotFocused(login_shelf_contents_view));
}

// Verifies that shift-tabbing from the lock screen will eventually focus the
// status area. Then, a tab will bring focus back to the lock screen.
TEST_F(LockScreenSanityTest, ShiftTabGoesFromLockToStatusAreaAndBackToLock) {
  // The status area will not focus out unless we are on the lock screen. See
  // StatusAreaWidgetDelegate::ShouldFocusOut.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);

  auto* lock = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(lock);
  views::View* status_area =
      RootWindowController::ForWindow(lock->GetWidget()->GetNativeWindow())
          ->GetStatusAreaWidget()
          ->GetContentsView();

  // Lock screen has focus.
  EXPECT_TRUE(VerifyFocused(lock));
  EXPECT_TRUE(VerifyNotFocused(status_area));

  // Focus from user view to the status area.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_SHIFT_DOWN);

  EXPECT_TRUE(VerifyNotFocused(lock));
  EXPECT_TRUE(VerifyFocused(status_area));

  // A single tab brings focus back to the lock screen.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_TAB, 0);
  EXPECT_TRUE(VerifyFocused(lock));
  EXPECT_TRUE(VerifyNotFocused(status_area));
}

TEST_F(LockScreenSanityTest, TabWithLockScreenAppActive) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);

  auto* lock = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(lock);

  views::View* login_shelf_contents_view =
      GetLoginShelfContentsView((lock->GetWidget()->GetNativeWindow()));

  views::View* status_area =
      RootWindowController::ForWindow(lock->GetWidget()->GetNativeWindow())
          ->GetStatusAreaWidget()
          ->GetContentsView();

  // Initialize lock screen action state.
  DataDispatcher()->SetLockScreenNoteState(mojom::TrayActionState::kActive);

  // Create and focus a lock screen app window.
  auto* lock_screen_app = new views::View();
  lock_screen_app->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  std::unique_ptr<views::Widget> app_widget =
      CreateWidgetWithContent(lock_screen_app);
  app_widget->Show();

  // Lock screen app focus is requested using lock screen mojo client - set up
  // the mock client.
  LockScreenAppFocuser app_widget_focuser(app_widget.get());
  auto client = std::make_unique<MockLoginScreenClient>();
  EXPECT_CALL(*client, FocusLockScreenApps(_))
      .WillRepeatedly(Invoke(&app_widget_focuser,
                             &LockScreenAppFocuser::FocusLockScreenApp));

  // Initially, focus should be with the lock screen app - when the app loses
  // focus (notified via mojo interface), shelf should get the focus next.
  EXPECT_TRUE(VerifyFocused(lock_screen_app));
  DataDispatcher()->HandleFocusLeavingLockScreenApps(false /*reverse*/);
  EXPECT_TRUE(VerifyFocused(login_shelf_contents_view));

  // Reversing focus should bring focus back to the lock screen app.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(VerifyFocused(lock_screen_app));
  EXPECT_TRUE(app_widget_focuser.reversed_tab_order());

  // Have the app tab out in reverse tab order - in this case, the status area
  // should get the focus.
  DataDispatcher()->HandleFocusLeavingLockScreenApps(true /*reverse*/);
  EXPECT_TRUE(VerifyFocused(status_area));

  // Tabbing out of the status area (in default order) should focus the lock
  // screen app again.
  GetEventGenerator()->PressKey(ui::KeyboardCode::VKEY_TAB, 0);
  EXPECT_TRUE(VerifyFocused(lock_screen_app));
  EXPECT_FALSE(app_widget_focuser.reversed_tab_order());

  // Tab out of the lock screen app once more - the shelf should get the focus
  // again.
  DataDispatcher()->HandleFocusLeavingLockScreenApps(false /*reverse*/);
  EXPECT_TRUE(VerifyFocused(login_shelf_contents_view));
}

TEST_F(LockScreenSanityTest, FocusLockScreenWhenLockScreenAppExit) {
  // Set up lock screen.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  auto* lock = new LockContentsView(
      mojom::TrayActionState::kNotAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));
  SetUserCount(1);
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(lock);

  views::View* login_shelf_contents_view =
      GetLoginShelfContentsView(lock->GetWidget()->GetNativeWindow());

  // Setup and focus a lock screen app.
  DataDispatcher()->SetLockScreenNoteState(mojom::TrayActionState::kActive);
  auto* lock_screen_app = new views::View();
  lock_screen_app->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  std::unique_ptr<views::Widget> app_widget =
      CreateWidgetWithContent(lock_screen_app);
  app_widget->Show();
  EXPECT_TRUE(VerifyFocused(lock_screen_app));

  // Tab out of the lock screen app - shelf should get the focus.
  DataDispatcher()->HandleFocusLeavingLockScreenApps(false /*reverse*/);
  EXPECT_TRUE(VerifyFocused(login_shelf_contents_view));

  // Move the lock screen note taking to available state (which happens when the
  // app session ends) - this should focus the lock screen.
  DataDispatcher()->SetLockScreenNoteState(mojom::TrayActionState::kAvailable);
  EXPECT_TRUE(VerifyFocused(lock));

  // Tab through the lock screen - the focus should eventually get to the shelf.
  ASSERT_TRUE(TabThroughView(GetEventGenerator(), lock, false /*reverse*/));
  EXPECT_TRUE(VerifyFocused(login_shelf_contents_view));
}

TEST_F(LockScreenSanityTest, RemoveUser) {
  auto client = std::make_unique<MockLoginScreenClient>();

  auto* contents = new LockContentsView(
      mojom::TrayActionState::kAvailable, LockScreen::ScreenType::kLock,
      DataDispatcher(),
      std::make_unique<FakeLoginDetachableBaseModel>(DataDispatcher()));

  // Add two users, the first of which can be removed.
  users().push_back(CreateUser("test1@test"));
  users()[0].can_remove = true;
  users().push_back(CreateUser("test2@test"));
  DataDispatcher()->SetUserList(users());

  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(contents);

  auto primary = MakeLoginAuthTestApi(contents, AuthTarget::kPrimary);
  auto primary_user_view = LoginUserView::TestApi(primary.user_view());
  auto secondary = MakeLoginAuthTestApi(contents, AuthTarget::kSecondary);
  auto secondary_user_view = LoginUserView::TestApi(secondary.user_view());

  // Fires a return and validates that mock expectations have been satisfied.
  auto submit = [&]() {
    GetEventGenerator()->PressKey(ui::VKEY_RETURN, 0);
    testing::Mock::VerifyAndClearExpectations(client.get());
  };
  auto focus_and_submit = [&](views::View* view) {
    view->RequestFocus();
    DCHECK(view->HasFocus());
    submit();
  };

  // The secondary user is not removable (as configured above) so showing the
  // dropdown does not result in an interactive/focusable view.
  focus_and_submit(secondary_user_view.dropdown());
  EXPECT_TRUE(secondary.remove_account_dialog());
  EXPECT_TRUE(secondary.remove_account_dialog()->GetVisible());
  EXPECT_FALSE(HasFocusInAnyChildView(secondary.remove_account_dialog()));

  // Verify that double-enter closes the bubble.
  submit();
  EXPECT_FALSE(secondary.remove_account_dialog());

  // The primary user is removable, so the remove account dialog is interactive.
  // Submitting the first time shows the remove user warning, submitting the
  // second time actually removes the user. Removing the user triggers a mojo
  // API call as well as removes the user from the UI.
  focus_and_submit(primary_user_view.dropdown());
  EXPECT_TRUE(primary.remove_account_dialog());
  EXPECT_TRUE(primary.remove_account_dialog()->GetVisible());
  EXPECT_TRUE(HasFocusInAnyChildView(primary.remove_account_dialog()));
  EXPECT_CALL(*client, OnRemoveUserWarningShown()).Times(1);
  submit();
  EXPECT_CALL(*client, RemoveUser(users()[0].basic_user_info.account_id))
      .Times(1)
      .WillOnce(Invoke(this, &LoginTestBase::RemoveUser));
  submit();

  // Secondary auth should be gone because it is now the primary auth.
  EXPECT_FALSE(MakeLockContentsViewTestApi(contents).opt_secondary_big_view());
  EXPECT_TRUE(MakeLockContentsViewTestApi(contents)
                  .primary_big_view()
                  ->GetCurrentUser()
                  .basic_user_info.account_id ==
              users()[0].basic_user_info.account_id);
}

TEST_F(LockScreenSanityTest, LockScreenKillsPreventsClipboardPaste) {
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"password");
  }

  ShowLockScreen();

  auto* text_input = new views::Textfield;
  std::unique_ptr<views::Widget> widget = CreateWidgetWithContent(text_input);

  text_input->RequestFocus();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode::VKEY_V, ui::EF_CONTROL_DOWN);

  EXPECT_TRUE(text_input->GetText().empty());

  LockScreen::Get()->Destroy();
  text_input->RequestFocus();
  generator->PressKey(ui::KeyboardCode::VKEY_V, ui::EF_CONTROL_DOWN);

  EXPECT_EQ(u"password", text_input->GetText());
}

}  // namespace ash
