// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/login_screen_test_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/auth_error_bubble.h"
#include "ash/login/ui/kiosk_app_default_message.h"
#include "ash/login/ui/local_authentication_request_view.h"
#include "ash/login/ui/local_authentication_request_widget.h"
#include "ash/login/ui/lock_contents_view_test_api.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_auth_user_view.h"
#include "ash/login/ui/login_big_user_view.h"
#include "ash/login/ui/login_expanded_public_account_view.h"
#include "ash/login/ui/login_password_view.h"
#include "ash/login/ui/login_pin_view.h"
#include "ash/login/ui/login_remove_account_dialog.h"
#include "ash/login/ui/login_user_view.h"
#include "ash/login/ui/pin_request_view.h"
#include "ash/login/ui/pin_request_widget.h"
#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/login_screen_client.h"
#include "ash/shelf/login_shelf_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "components/account_id/account_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/ui_controls.h"
#include "ui/compositor/layer.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Creates an event generator for simulating interactions with the Ash window.
std::unique_ptr<ui::test::EventGenerator> MakeAshEventGenerator() {
  return std::make_unique<ui::test::EventGenerator>(
      Shell::GetPrimaryRootWindow());
}

LoginShelfView* GetLoginShelfView() {
  if (!Shell::HasInstance()) {
    return nullptr;
  }

  return Shelf::ForWindow(Shell::GetPrimaryRootWindow())
      ->shelf_widget()
      ->GetLoginShelfView();
}

bool IsLoginShelfViewButtonShown(int button_view_id) {
  LoginShelfView* shelf_view = GetLoginShelfView();
  if (!shelf_view) {
    return false;
  }

  views::View* button_view = shelf_view->GetViewByID(button_view_id);

  return button_view && button_view->GetVisible();
}

views::View* GetShutDownButton() {
  LoginShelfView* shelf_view = GetLoginShelfView();
  if (!shelf_view) {
    return nullptr;
  }

  return shelf_view->GetViewByID(LoginShelfView::kShutdown);
}

views::View* GetShutDownButtonContainer() {
  LoginShelfView* shelf_view = GetLoginShelfView();
  if (!shelf_view) {
    return nullptr;
  }

  return shelf_view->GetButtonContainerByID(LoginShelfView::kShutdown);
}

views::View* GetAppsButton() {
  LoginShelfView* shelf_view = GetLoginShelfView();
  if (!shelf_view) {
    return nullptr;
  }

  return shelf_view->GetViewByID(LoginShelfView::kApps);
}

LoginBigUserView* GetBigUserView(const AccountId& account_id) {
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsViewTestApi lock_contents_test(lock_screen_test.contents_view());
  return lock_contents_test.FindBigUser(account_id);
}

bool SimulateButtonPressedForTesting(LoginShelfView::ButtonId button_id) {
  LoginShelfView* shelf_view = GetLoginShelfView();
  if (!shelf_view) {
    return false;
  }

  views::View* button = shelf_view->GetViewByID(button_id);
  if (!button->GetEnabled()) {
    return false;
  }

  views::test::ButtonTestApi(views::Button::AsButton(button))
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::PointF(),
                                  gfx::PointF(), base::TimeTicks(), 0, 0));
  return true;
}

}  // anonymous namespace

class ShelfTestUiUpdateDelegate : public LoginShelfView::TestUiUpdateDelegate {
 public:
  // Returns instance owned by LoginShelfView. Installs instance of
  // ShelfTestUiUpdateDelegate when needed.
  static ShelfTestUiUpdateDelegate* Get(LoginShelfView* shelf) {
    if (!shelf->test_ui_update_delegate()) {
      shelf->InstallTestUiUpdateDelegate(
          std::make_unique<ShelfTestUiUpdateDelegate>());
    }
    return static_cast<ShelfTestUiUpdateDelegate*>(
        shelf->test_ui_update_delegate());
  }

  ShelfTestUiUpdateDelegate() = default;

  ShelfTestUiUpdateDelegate(const ShelfTestUiUpdateDelegate&) = delete;
  ShelfTestUiUpdateDelegate& operator=(const ShelfTestUiUpdateDelegate&) =
      delete;

  ~ShelfTestUiUpdateDelegate() override {
    for (PendingCallback& entry : heap_) {
      std::move(entry.callback).Run();
    }
  }

  // Returns UI update count.
  int64_t ui_update_count() const { return ui_update_count_; }

  // Add a callback to be invoked when ui update count is greater than
  // |previous_update_count|. Note |callback| could be invoked synchronously
  // when the current ui update count is already greater than
  // |previous_update_count|.
  void AddCallback(int64_t previous_update_count, base::OnceClosure callback) {
    if (previous_update_count < ui_update_count_) {
      std::move(callback).Run();
    } else {
      heap_.emplace_back(previous_update_count, std::move(callback));
      std::push_heap(heap_.begin(), heap_.end());
    }
  }

  // LoginShelfView::TestUiUpdateDelegate
  void OnUiUpdate() override {
    ++ui_update_count_;
    while (!heap_.empty() && heap_.front().old_count < ui_update_count_) {
      std::move(heap_.front().callback).Run();
      std::pop_heap(heap_.begin(), heap_.end());
      heap_.pop_back();
    }
  }

 private:
  struct PendingCallback {
    PendingCallback(int64_t old_count, base::OnceClosure callback)
        : old_count(old_count), callback(std::move(callback)) {}

    bool operator<(const PendingCallback& right) const {
      // We need min_heap, therefore this returns true when another element on
      // the right is less than this count. (regular heap is max_heap).
      return old_count > right.old_count;
    }

    int64_t old_count = 0;
    base::OnceClosure callback;
  };

  std::vector<PendingCallback> heap_;

  int64_t ui_update_count_ = 0;
};

// static
bool LoginScreenTestApi::IsLockShown() {
  return LockScreen::HasInstance() && LockScreen::Get()->is_shown() &&
         LockScreen::Get()->screen_type() == LockScreen::ScreenType::kLock;
}

// static
void LoginScreenTestApi::AddOnLockScreenShownCallback(
    base::OnceClosure on_lock_screen_shown) {
  if (!LockScreen::HasInstance()) {
    FAIL() << "No lock screen";
  }
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  lock_screen_test.AddOnShownCallback(std::move(on_lock_screen_shown));
}

// static
bool LoginScreenTestApi::IsLoginShelfShown() {
  LoginShelfView* view = GetLoginShelfView();
  return view && view->GetVisible();
}

// static
bool LoginScreenTestApi::IsRestartButtonShown() {
  return IsLoginShelfViewButtonShown(LoginShelfView::kRestart);
}

// static
bool LoginScreenTestApi::IsShutdownButtonShown() {
  return IsLoginShelfViewButtonShown(LoginShelfView::kShutdown);
}

// static
bool LoginScreenTestApi::IsAppsButtonShown() {
  return IsLoginShelfViewButtonShown(LoginShelfView::kApps);
}

// static
bool LoginScreenTestApi::IsGuestButtonShown() {
  return IsLoginShelfViewButtonShown(LoginShelfView::kBrowseAsGuest);
}

// static
bool LoginScreenTestApi::IsAddUserButtonShown() {
  return IsLoginShelfViewButtonShown(LoginShelfView::kAddUser);
}

// static
bool LoginScreenTestApi::IsCancelButtonShown() {
  return IsLoginShelfViewButtonShown(LoginShelfView::kCancel);
}

// static
bool LoginScreenTestApi::IsParentAccessButtonShown() {
  return IsLoginShelfViewButtonShown(LoginShelfView::kParentAccess);
}

// static
bool LoginScreenTestApi::IsEnterpriseEnrollmentButtonShown() {
  return IsLoginShelfViewButtonShown(LoginShelfView::kEnterpriseEnrollment);
}

// static
bool LoginScreenTestApi::IsOsInstallButtonShown() {
  return IsLoginShelfViewButtonShown(LoginShelfView::kOsInstall);
}

// static
bool LoginScreenTestApi::IsUserAddingScreenIndicatorShown() {
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsViewTestApi lock_contents_test(lock_screen_test.contents_view());
  views::View* indicator = lock_contents_test.user_adding_screen_indicator();
  return indicator && indicator->GetVisible();
}

// static
bool LoginScreenTestApi::IsWarningBubbleShown() {
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsViewTestApi lock_contents_test(lock_screen_test.contents_view());
  return lock_contents_test.warning_banner_bubble()->GetVisible();
}

// static
bool LoginScreenTestApi::IsSystemInfoShown() {
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsViewTestApi lock_contents_test(lock_screen_test.contents_view());
  // Check if all views in the hierarchy are visible.
  for (views::View* view = lock_contents_test.system_info(); view != nullptr;
       view = view->parent()) {
    if (!view->GetVisible()) {
      return false;
    }
  }
  return true;
}

// static
bool LoginScreenTestApi::IsKioskDefaultMessageShown() {
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsViewTestApi test_api(lock_screen_test.contents_view());
  return test_api.kiosk_default_message() &&
         test_api.kiosk_default_message()->GetVisible();
}

// static
bool LoginScreenTestApi::IsKioskInstructionBubbleShown() {
  LoginShelfView* view = GetLoginShelfView();
  return view->GetKioskInstructionBubbleForTesting() &&
         view->GetKioskInstructionBubbleForTesting()->GetWidget() &&
         view->GetKioskInstructionBubbleForTesting()->GetWidget()->IsVisible();
}

// static
bool LoginScreenTestApi::IsPasswordFieldShown(const AccountId& account_id) {
  if (GetFocusedUser() != account_id) {
    ADD_FAILURE() << "The user " << account_id.Serialize() << " is not focused";
    return false;
  }
  LoginBigUserView* big_user_view = GetBigUserView(account_id);
  if (!big_user_view) {
    ADD_FAILURE() << "Could not find user " << account_id.Serialize();
    return false;
  }
  LoginAuthUserView::TestApi auth_test(big_user_view->auth_user());
  return auth_test.password_view()->IsDrawn();
}

// static
bool LoginScreenTestApi::IsDisplayPasswordButtonShown(
    const AccountId& account_id) {
  if (!FocusUser(account_id)) {
    ADD_FAILURE() << "Could not focus on user " << account_id.Serialize();
    return false;
  }
  LoginBigUserView* big_user_view = GetBigUserView(account_id);
  if (!big_user_view) {
    ADD_FAILURE() << "Could not find user " << account_id.Serialize();
    return false;
  }
  if (!big_user_view->IsAuthEnabled()) {
    ADD_FAILURE() << "Auth is not enabled for user " << account_id.Serialize();
    return false;
  }
  LoginAuthUserView::TestApi auth_test(big_user_view->auth_user());
  if (!auth_test.HasAuthMethod(LoginAuthUserView::AUTH_PASSWORD)) {
    ADD_FAILURE() << "Password auth is not enabled for user "
                  << account_id.Serialize();
    return false;
  }
  LoginPasswordView::TestApi password_test(auth_test.password_view());
  bool display_password_button_visible =
      auth_test.user_view()->current_user().show_display_password_button;
  EXPECT_EQ(display_password_button_visible,
            password_test.display_password_button()->GetVisible());
  return display_password_button_visible;
}

// static
bool LoginScreenTestApi::IsManagedIconShown(const AccountId& account_id) {
  LoginBigUserView* big_user_view = GetBigUserView(account_id);
  if (!big_user_view) {
    ADD_FAILURE() << "Could not find user " << account_id.Serialize();
    return false;
  }
  LoginUserView::TestApi user_test(big_user_view->GetUserView());
  auto* enterprise_icon_container = user_test.enterprise_icon_container();
  return enterprise_icon_container->GetVisible();
}

// static
bool LoginScreenTestApi::ShowRemoveAccountDialog(const AccountId& account_id) {
  LoginBigUserView* big_user_view = GetBigUserView(account_id);
  if (!big_user_view) {
    ADD_FAILURE() << "Could not find user " << account_id.Serialize();
    return false;
  }
  LoginAuthUserView::TestApi auth_test(big_user_view->auth_user());
  if (auth_test.remove_account_dialog()) {
    ADD_FAILURE() << "Dialog already shown for user " << account_id.Serialize();
    return false;
  }
  auth_test.ShowDialog();
  return true;
}

// static
bool LoginScreenTestApi::IsManagedMessageInDialogShown(
    const AccountId& account_id) {
  LoginBigUserView* big_user_view = GetBigUserView(account_id);
  if (!big_user_view) {
    ADD_FAILURE() << "Could not find user " << account_id.Serialize();
    return false;
  }
  LoginAuthUserView::TestApi auth_test(big_user_view->auth_user());
  if (!auth_test.remove_account_dialog()) {
    ADD_FAILURE() << "Could not find dialog for user "
                  << account_id.Serialize();
    return false;
  }
  LoginRemoveAccountDialog::TestApi user_dialog_test(
      auth_test.remove_account_dialog());
  auto* management_disclosure_label =
      user_dialog_test.management_disclosure_label();
  return management_disclosure_label &&
         management_disclosure_label->GetVisible();
}

// static
bool LoginScreenTestApi::IsForcedOnlineSignin(const AccountId& account_id) {
  LoginBigUserView* big_user_view = GetBigUserView(account_id);
  if (!big_user_view) {
    ADD_FAILURE() << "Could not find user " << account_id.Serialize();
    return false;
  }
  LoginAuthUserView::TestApi auth_test(big_user_view->auth_user());
  return auth_test.HasAuthMethod(LoginAuthUserView::AUTH_ONLINE_SIGN_IN);
}

// static
void LoginScreenTestApi::SubmitPassword(const AccountId& account_id,
                                        const std::string& password,
                                        bool check_if_submittable) {
  // It'd be better to generate keyevents dynamically and dispatch them instead
  // of reaching into the views structure, but at the time of writing I could
  // not find a good way to do this. If you know of a way feel free to change
  // this code.
  ASSERT_TRUE(FocusUser(account_id));
  LoginBigUserView* big_user_view = GetBigUserView(account_id);
  ASSERT_TRUE(big_user_view);
  ASSERT_TRUE(big_user_view->IsAuthEnabled());
  LoginAuthUserView::TestApi auth_test(big_user_view->auth_user());
  if (check_if_submittable) {
    ASSERT_TRUE(auth_test.HasAuthMethod(LoginAuthUserView::AUTH_PASSWORD));
  }
  LoginPasswordView::TestApi password_test(auth_test.password_view());
  ASSERT_EQ(account_id,
            auth_test.user_view()->current_user().basic_user_info.account_id);
  password_test.SubmitPassword(password);
}

// static
std::u16string LoginScreenTestApi::GetChallengeResponseLabel(
    const AccountId& account_id) {
  if (GetFocusedUser() != account_id) {
    ADD_FAILURE() << "The user " << account_id.Serialize() << " is not focused";
    return std::u16string();
  }
  LoginBigUserView* big_user_view = GetBigUserView(account_id);
  if (!big_user_view) {
    ADD_FAILURE() << "Could not find user " << account_id.Serialize();
    return std::u16string();
  }
  LoginAuthUserView::TestApi auth_test(big_user_view->auth_user());
  if (!auth_test.challenge_response_label()->IsDrawn()) {
    ADD_FAILURE() << "Challenge-response label is not drawn for user "
                  << account_id.Serialize();
    return std::u16string();
  }
  return auth_test.challenge_response_label()->GetText();
}

// static
bool LoginScreenTestApi::IsChallengeResponseButtonClickable(
    const AccountId& account_id) {
  if (GetFocusedUser() != account_id) {
    ADD_FAILURE() << "The user " << account_id.Serialize() << " is not focused";
    return false;
  }
  LoginBigUserView* big_user_view = GetBigUserView(account_id);
  if (!big_user_view) {
    ADD_FAILURE() << "Could not find user " << account_id.Serialize();
    return false;
  }
  LoginAuthUserView::TestApi auth_test(big_user_view->auth_user());
  if (!auth_test.challenge_response_button()->IsDrawn()) {
    ADD_FAILURE() << "Challenge-response button is not drawn for user "
                  << account_id.Serialize();
    return false;
  }
  return auth_test.challenge_response_button()->GetEnabled();
}

// static
void LoginScreenTestApi::ClickChallengeResponseButton(
    const AccountId& account_id) {
  if (!FocusUser(account_id)) {
    FAIL() << "Could not focus on user " << account_id.Serialize();
  }
  LoginBigUserView* big_user_view = GetBigUserView(account_id);
  if (!big_user_view) {
    FAIL() << "Could not find user " << account_id.Serialize();
  }
  if (!big_user_view->IsAuthEnabled()) {
    FAIL() << "Auth is not enabled for user " << account_id.Serialize();
  }
  LoginAuthUserView::TestApi auth_test(big_user_view->auth_user());
  if (!auth_test.HasAuthMethod(LoginAuthUserView::AUTH_CHALLENGE_RESPONSE)) {
    FAIL() << "Challenge-response auth is not enabled for user "
           << account_id.Serialize();
  }
  if (!auth_test.challenge_response_button()->IsDrawn()) {
    FAIL() << "Challenge-response button is not drawn for user "
           << account_id.Serialize();
  }
  auto event_generator = MakeAshEventGenerator();
  event_generator->MoveMouseTo(
      auth_test.challenge_response_button()->GetBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();
}

// static
int64_t LoginScreenTestApi::GetUiUpdateCount() {
  LoginShelfView* view = GetLoginShelfView();
  return view ? ShelfTestUiUpdateDelegate::Get(view)->ui_update_count() : 0;
}

// static
bool LoginScreenTestApi::LaunchApp(const std::string& app_id) {
  LoginShelfView* view = GetLoginShelfView();
  return view && view->LaunchAppForTesting(app_id);
}

// static
bool LoginScreenTestApi::LaunchApp(const AccountId& account_id) {
  LoginShelfView* view = GetLoginShelfView();
  return view && view->LaunchAppForTesting(account_id);
}

// static
bool LoginScreenTestApi::ClickAppsButton() {
  return SimulateButtonPressedForTesting(LoginShelfView::kApps);
}

// static
bool LoginScreenTestApi::ClickAddUserButton() {
  return SimulateButtonPressedForTesting(LoginShelfView::kAddUser);
}

// static
bool LoginScreenTestApi::ClickCancelButton() {
  return SimulateButtonPressedForTesting(LoginShelfView::kCancel);
}

// static
bool LoginScreenTestApi::ClickGuestButton() {
  return SimulateButtonPressedForTesting(LoginShelfView::kBrowseAsGuest);
}

// static
bool LoginScreenTestApi::ClickEnterpriseEnrollmentButton() {
  return SimulateButtonPressedForTesting(LoginShelfView::kEnterpriseEnrollment);
}

// static
bool LoginScreenTestApi::ClickOsInstallButton() {
  return SimulateButtonPressedForTesting(LoginShelfView::kOsInstall);
}

// static
bool LoginScreenTestApi::PressAccelerator(const ui::Accelerator& accelerator) {
  // TODO(https://crbug.com/1321609): Migrate to SendAcceleratorNatively.
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  return lock_screen_test.contents_view()->AcceleratorPressed(accelerator);
}

// static
bool LoginScreenTestApi::SendAcceleratorNatively(
    const ui::Accelerator& accelerator) {
  gfx::NativeWindow login_window = gfx::NativeWindow();
  if (LockScreen::HasInstance()) {
    login_window = LockScreen::Get()->widget()->GetNativeWindow();
  } else {
    login_window =
        LoginScreen::Get()->GetLoginWindowWidget()->GetNativeWindow();
  }
  if (!login_window) {
    return false;
  }
  return ui_controls::SendKeyPress(
      login_window, accelerator.key_code(), accelerator.IsCtrlDown(),
      accelerator.IsShiftDown(), accelerator.IsAltDown(),
      accelerator.IsCmdDown());
}

// static
bool LoginScreenTestApi::WaitForUiUpdate(int64_t previous_update_count) {
  LoginShelfView* view = GetLoginShelfView();
  if (view) {
    base::RunLoop run_loop;
    ShelfTestUiUpdateDelegate::Get(view)->AddCallback(previous_update_count,
                                                      run_loop.QuitClosure());
    run_loop.Run();
    return true;
  }

  return false;
}

int LoginScreenTestApi::GetUsersCount() {
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsViewTestApi lock_contents_test(lock_screen_test.contents_view());
  return lock_contents_test.users().size();
}

// static
bool LoginScreenTestApi::FocusKioskDefaultMessage() {
  if (!IsKioskDefaultMessageShown()) {
    ADD_FAILURE() << "Kiosk default message is not visible.";
    return false;
  }
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsViewTestApi test_api(lock_screen_test.contents_view());
  auto event_generator = MakeAshEventGenerator();
  event_generator->MoveMouseTo(
      test_api.kiosk_default_message()->GetBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();
  return true;
}

// static
bool LoginScreenTestApi::FocusUser(const AccountId& account_id) {
  LoginBigUserView* big_user_view = GetBigUserView(account_id);
  if (!big_user_view) {
    ADD_FAILURE() << "User not found " << account_id;
    return false;
  }
  LoginUserView::TestApi user_test(big_user_view->GetUserView());
  user_test.OnTap();
  return GetFocusedUser() == account_id;
}

// static
bool LoginScreenTestApi::ExpandPublicSessionPod(const AccountId& account_id) {
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsViewTestApi lock_contents_test(lock_screen_test.contents_view());
  LoginBigUserView* big_user_view = GetBigUserView(account_id);
  if (!big_user_view) {
    return false;
  }
  LoginPublicAccountUserView::TestApi public_account_test(
      big_user_view->public_account());
  if (!public_account_test.arrow_button()->GetVisible()) {
    ADD_FAILURE() << "Arrow button not visible";
    return false;
  }
  views::test::ButtonTestApi(
      views::Button::AsButton(public_account_test.arrow_button()))
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::PointF(),
                                  gfx::PointF(), base::TimeTicks(), 0, 0));
  return lock_contents_test.expanded_view();
}

// static
bool LoginScreenTestApi::HidePublicSessionExpandedPod() {
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsViewTestApi lock_contents_test(lock_screen_test.contents_view());
  LoginExpandedPublicAccountView* expanded_view =
      lock_contents_test.expanded_view();
  if (!expanded_view || !expanded_view->GetVisible()) {
    return false;
  }
  expanded_view->Hide();
  return true;
}

// static
bool LoginScreenTestApi::IsPublicSessionExpanded() {
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsViewTestApi lock_contents_test(lock_screen_test.contents_view());
  LoginExpandedPublicAccountView* expanded_view =
      lock_contents_test.expanded_view();
  return expanded_view && expanded_view->GetVisible();
}

// static
bool LoginScreenTestApi::IsExpandedPublicSessionAdvanced() {
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsViewTestApi lock_contents_test(lock_screen_test.contents_view());
  LoginExpandedPublicAccountView::TestApi expanded_test(
      lock_contents_test.expanded_view());
  return expanded_test.advanced_view()->GetVisible();
}

bool LoginScreenTestApi::IsPublicSessionWarningShown() {
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsViewTestApi lock_contents_test(lock_screen_test.contents_view());
  LoginExpandedPublicAccountView::TestApi expanded_test(
      lock_contents_test.expanded_view());
  return expanded_test.monitoring_warning_icon() &&
         expanded_test.monitoring_warning_label();
}

// static
void LoginScreenTestApi::ClickPublicExpandedAdvancedViewButton() {
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsViewTestApi lock_contents_test(lock_screen_test.contents_view());
  LoginExpandedPublicAccountView::TestApi expanded_test(
      lock_contents_test.expanded_view());
  views::test::ButtonTestApi(
      views::Button::AsButton(expanded_test.advanced_view_button()))
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::PointF(),
                                  gfx::PointF(), base::TimeTicks(), 0, 0));
}

// static
void LoginScreenTestApi::ClickPublicExpandedSubmitButton() {
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsViewTestApi lock_contents_test(lock_screen_test.contents_view());
  LoginExpandedPublicAccountView::TestApi expanded_test(
      lock_contents_test.expanded_view());
  views::test::ButtonTestApi(
      views::Button::AsButton(expanded_test.submit_button()))
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::PointF(),
                                  gfx::PointF(), base::TimeTicks(), 0, 0));
}

// static
void LoginScreenTestApi::SetPublicSessionLocale(const std::string& locale) {
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsViewTestApi lock_contents_test(lock_screen_test.contents_view());
  LoginExpandedPublicAccountView::TestApi expanded_test(
      lock_contents_test.expanded_view());
  ASSERT_TRUE(expanded_test.SelectLanguage(locale));
}

// static
void LoginScreenTestApi::SetPublicSessionKeyboard(const std::string& ime_id) {
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsViewTestApi lock_contents_test(lock_screen_test.contents_view());
  LoginExpandedPublicAccountView::TestApi expanded_test(
      lock_contents_test.expanded_view());
  ASSERT_TRUE(expanded_test.SelectKeyboard(ime_id))
      << "Failed to select " << ime_id;
}

// static
std::vector<ash::LocaleItem> LoginScreenTestApi::GetPublicSessionLocales(
    const AccountId& account_id) {
  LoginBigUserView* big_user_view = GetBigUserView(account_id);
  if (!big_user_view) {
    ADD_FAILURE() << "Could not find user " << account_id.Serialize();
    return std::vector<ash::LocaleItem>();
  }
  return big_user_view->public_account()
      ->user_view()
      ->current_user()
      .public_account_info->available_locales;
}

// static
std::vector<ash::LocaleItem>
LoginScreenTestApi::GetExpandedPublicSessionLocales() {
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsViewTestApi lock_contents_test(lock_screen_test.contents_view());
  LoginExpandedPublicAccountView::TestApi expanded_test(
      lock_contents_test.expanded_view());
  return expanded_test.GetLocales();
}

// static
std::string LoginScreenTestApi::GetExpandedPublicSessionSelectedLocale() {
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsViewTestApi lock_contents_test(lock_screen_test.contents_view());
  LoginExpandedPublicAccountView::TestApi expanded_test(
      lock_contents_test.expanded_view());
  return expanded_test.selected_language_item_value();
}

// static
std::string LoginScreenTestApi::GetExpandedPublicSessionSelectedKeyboard() {
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsViewTestApi lock_contents_test(lock_screen_test.contents_view());
  LoginExpandedPublicAccountView::TestApi expanded_test(
      lock_contents_test.expanded_view());
  return expanded_test.selected_keyboard_item_value();
}

// static
AccountId LoginScreenTestApi::GetFocusedUser() {
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsViewTestApi lock_contents_test(lock_screen_test.contents_view());
  return lock_contents_test.focused_user();
}

// static
bool LoginScreenTestApi::RemoveUser(const AccountId& account_id) {
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsViewTestApi lock_contents_test(lock_screen_test.contents_view());
  return lock_contents_test.RemoveUser(account_id);
}

// static
std::string LoginScreenTestApi::GetDisplayedName(const AccountId& account_id) {
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsViewTestApi lock_contents_test(lock_screen_test.contents_view());
  LoginUserView* user_view = lock_contents_test.FindUserView(account_id);
  if (!user_view) {
    ADD_FAILURE() << "Could not find user " << account_id.Serialize();
    return std::string();
  }
  LoginUserView::TestApi user_view_test(user_view);
  return base::UTF16ToUTF8(user_view_test.displayed_name());
}

// static
std::u16string LoginScreenTestApi::GetDisabledAuthMessage(
    const AccountId& account_id) {
  LoginBigUserView* big_user_view = GetBigUserView(account_id);
  if (!big_user_view) {
    ADD_FAILURE() << "Could not find user " << account_id.Serialize();
    return std::u16string();
  }
  LoginAuthUserView::TestApi auth_test(big_user_view->auth_user());

  return auth_test.GetDisabledAuthMessageContent();
}

// static
std::u16string LoginScreenTestApi::GetManagementDisclosureText(
    const AccountId& account_id) {
  LoginBigUserView* big_user_view = GetBigUserView(account_id);
  if (!big_user_view) {
    ADD_FAILURE() << "Could not find user " << account_id.Serialize();
    return std::u16string();
  }
  LoginAuthUserView::TestApi auth_test(big_user_view->auth_user());
  if (!auth_test.remove_account_dialog()) {
    ADD_FAILURE() << "Could not find dialog for user "
                  << account_id.Serialize();
    return std::u16string();
  }
  LoginRemoveAccountDialog::TestApi dialog(auth_test.remove_account_dialog());
  return dialog.management_disclosure_label()->GetText();
}

// static
bool LoginScreenTestApi::IsOobeDialogVisible() {
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsViewTestApi lock_contents_test(lock_screen_test.contents_view());
  return lock_contents_test.IsOobeDialogVisible();
}

// static
std::u16string LoginScreenTestApi::GetShutDownButtonLabel() {
  views::View* button = GetShutDownButton();
  if (!button) {
    return std::u16string();
  }

  return static_cast<views::LabelButton*>(button)->GetText();
}

// static
gfx::Rect LoginScreenTestApi::GetShutDownButtonTargetBounds() {
  views::View* button = GetShutDownButton();
  if (!button) {
    return gfx::Rect();
  }

  return button->layer()->GetTargetBounds();
}

// static
gfx::Rect LoginScreenTestApi::GetShutDownButtonMirroredBounds() {
  views::View* button_container = GetShutDownButtonContainer();
  views::View* button = GetShutDownButton();
  if (!button) {
    return gfx::Rect();
  }
  gfx::Point button_container_origin =
      button_container->GetMirroredBounds().origin();
  gfx::Rect button_mirrored_bounds = button->GetMirroredBounds();
  button_mirrored_bounds.set_origin(button_container_origin +
                                    button_mirrored_bounds.OffsetFromOrigin());
  return button_mirrored_bounds;
}

// static
std::string LoginScreenTestApi::GetAppsButtonClassName() {
  views::View* button = GetAppsButton();
  if (!button) {
    return "";
  }

  return button->GetClassName();
}

// static
void LoginScreenTestApi::SetPinRequestWidgetShownCallback(
    base::RepeatingClosure on_pin_request_widget_shown) {
  PinRequestWidget::SetShownCallbackForTesting(on_pin_request_widget_shown);
}

// static
std::u16string LoginScreenTestApi::GetPinRequestWidgetTitle() {
  if (!PinRequestWidget::Get()) {
    ADD_FAILURE() << "No PIN request widget is shown";
    return std::u16string();
  }
  PinRequestWidget::TestApi pin_widget_test(PinRequestWidget::Get());
  PinRequestView::TestApi pin_view_test(pin_widget_test.pin_request_view());
  return pin_view_test.title_label()->GetText();
}

// static
void LoginScreenTestApi::SubmitPinRequestWidget(const std::string& pin) {
  if (!PinRequestWidget::Get()) {
    FAIL() << "No PIN request widget is shown";
  }
  auto event_generator = MakeAshEventGenerator();
  PinRequestWidget::TestApi pin_widget_test(PinRequestWidget::Get());
  PinRequestView::TestApi pin_test(pin_widget_test.pin_request_view());
  LoginPinView::TestApi pin_keyboard_test(pin_test.pin_keyboard_view());
  for (char c : pin) {
    DCHECK_GE(c, '0');
    DCHECK_LE(c, '9');
    event_generator->MoveMouseTo(pin_keyboard_test.GetButton(c - '0')
                                     ->GetBoundsInScreen()
                                     .CenterPoint());
    event_generator->ClickLeftButton();
  }
  event_generator->MoveMouseTo(
      pin_test.submit_button()->GetBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();
}

// static
void LoginScreenTestApi::CancelPinRequestWidget() {
  if (!PinRequestWidget::Get()) {
    FAIL() << "No PIN request widget is shown";
  }
  auto event_generator = MakeAshEventGenerator();
  PinRequestWidget::TestApi pin_widget_test(PinRequestWidget::Get());
  PinRequestView::TestApi pin_view_test(pin_widget_test.pin_request_view());
  event_generator->MoveMouseTo(
      pin_view_test.back_button()->GetBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();
}

// static
bool LoginScreenTestApi::IsLocalAuthenticationDialogVisible() {
  return LocalAuthenticationRequestWidget::TestApi::IsVisible();
}

// static
void LoginScreenTestApi::CancelLocalAuthenticationDialog() {
  bool dialog_exists =
      LocalAuthenticationRequestWidget::TestApi::CancelDialog();
  if (!dialog_exists) {
    FAIL() << "Local Authentication dialog is not shown";
  }
}

// static
void LoginScreenTestApi::SubmitPasswordLocalAuthenticationDialog(
    const std::string& password) {
  bool dialog_exists =
      LocalAuthenticationRequestWidget::TestApi::SubmitPassword(password);
  if (!dialog_exists) {
    FAIL() << "Local Authentication dialog is not shown";
  }
}

// static
bool LoginScreenTestApi::IsAuthErrorBubbleShown() {
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsViewTestApi lock_contents_test(lock_screen_test.contents_view());
  return lock_contents_test.IsAuthErrorBubbleVisible();
}

// static
void LoginScreenTestApi::ShowAuthError(int unlock_attempt) {
  if (IsAuthErrorBubbleShown()) {
    ADD_FAILURE() << "Auth error bubble is already shown.";
  }
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsViewTestApi lock_contents_test(lock_screen_test.contents_view());
  lock_contents_test.ShowAuthErrorBubble(unlock_attempt);
}

// static
void LoginScreenTestApi::HideAuthError() {
  if (!IsAuthErrorBubbleShown()) {
    ADD_FAILURE() << "Auth error bubble is not shown.";
  }
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsViewTestApi lock_contents_test(lock_screen_test.contents_view());
  lock_contents_test.HideAuthErrorBubble();
}

// static
void LoginScreenTestApi::PressAuthErrorRecoveryButton() {
  if (!IsAuthErrorBubbleShown()) {
    ADD_FAILURE() << "Auth error bubble is not shown.";
  }
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsViewTestApi lock_contents_test(lock_screen_test.contents_view());
  lock_contents_test.PressAuthErrorRecoveryButton();
}

// static
void LoginScreenTestApi::PressAuthErrorLearnMoreButton() {
  if (!IsAuthErrorBubbleShown()) {
    ADD_FAILURE() << "Auth error bubble is not shown.";
  }
  LockScreen::TestApi lock_screen_test(LockScreen::Get());
  LockContentsViewTestApi lock_contents_test(lock_screen_test.contents_view());
  lock_contents_test.PressAuthErrorLearnMoreButton();
}

}  // namespace ash
