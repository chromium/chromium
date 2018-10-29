// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_auth_user_view.h"
#include "ash/login/mock_login_screen_client.h"
#include "ash/login/ui/login_password_view.h"
#include "ash/login/ui/login_pin_view.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/login_test_utils.h"
#include "ash/login/ui/login_user_view.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

class LoginAuthUserViewUnittest : public LoginTestBase {
 protected:
  LoginAuthUserViewUnittest() = default;
  ~LoginAuthUserViewUnittest() override = default;

  // LoginTestBase:
  void SetUp() override {
    LoginTestBase::SetUp();

    user_ = CreateUser("user@domain.com");

    LoginAuthUserView::Callbacks auth_callbacks;
    auth_callbacks.on_auth = base::DoNothing();
    auth_callbacks.on_easy_unlock_icon_hovered = base::DoNothing();
    auth_callbacks.on_easy_unlock_icon_tapped = base::DoNothing();
    auth_callbacks.on_tap = base::DoNothing();
    auth_callbacks.on_remove_warning_shown = base::DoNothing();
    auth_callbacks.on_remove = base::DoNothing();
    view_ = new LoginAuthUserView(user_, auth_callbacks);

    // We proxy |view_| inside of |container_| so we can control layout.
    container_ = new views::View();
    container_->SetLayoutManager(
        std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
    container_->AddChildView(view_);
    SetWidget(CreateWidgetWithContent(container_));
  }

  void SetAuthMethods(uint32_t auth_methods) {
    bool can_use_pin = (auth_methods & LoginAuthUserView::AUTH_PIN) != 0;
    view_->SetAuthMethods(auth_methods, can_use_pin);
  }

  mojom::LoginUserInfoPtr user_;
  views::View* container_ = nullptr;   // Owned by test widget view hierarchy.
  LoginAuthUserView* view_ = nullptr;  // Owned by test widget view hierarchy.

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginAuthUserViewUnittest);
};

}  // namespace

// Verifies showing the PIN keyboard makes the user view grow.
TEST_F(LoginAuthUserViewUnittest, ShowingPinExpandsView) {
  gfx::Size start_size = view_->size();
  SetAuthMethods(LoginAuthUserView::AUTH_PIN);
  container_->Layout();
  gfx::Size expanded_size = view_->size();
  EXPECT_GT(expanded_size.height(), start_size.height());
}

// Verifies that an auth user that shows a password is opaque.
TEST_F(LoginAuthUserViewUnittest, ShowingPasswordForcesOpaque) {
  LoginAuthUserView::TestApi auth_test(view_);
  LoginUserView::TestApi user_test(auth_test.user_view());

  // Add another view that will hold focus. The user view cannot have focus
  // since focus will keep it opaque.
  auto* focus = new views::View();
  focus->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  container_->AddChildView(focus);
  focus->RequestFocus();
  EXPECT_FALSE(auth_test.user_view()->HasFocus());

  // If the user view is showing a password it must be opaque.
  SetAuthMethods(LoginAuthUserView::AUTH_PASSWORD);
  EXPECT_TRUE(user_test.is_opaque());
  SetAuthMethods(LoginAuthUserView::AUTH_NONE);
  EXPECT_FALSE(user_test.is_opaque());
  SetAuthMethods(LoginAuthUserView::AUTH_PASSWORD);
  EXPECT_TRUE(user_test.is_opaque());
}

// Verifies that pressing return with an empty password field when tap-to-unlock
// is enabled attempts unlock.
TEST_F(LoginAuthUserViewUnittest, PressReturnWithTapToUnlockEnabled) {
  std::unique_ptr<MockLoginScreenClient> client = BindMockLoginScreenClient();

  ui::test::EventGenerator* generator = GetEventGenerator();

  LoginAuthUserView::TestApi test_auth_user_view(view_);
  LoginPasswordView* password_view(test_auth_user_view.password_view());
  LoginUserView* user_view(test_auth_user_view.user_view());

  SetUserCount(1);

  EXPECT_CALL(*client,
              AuthenticateUserWithEasyUnlock(
                  user_view->current_user()->basic_user_info->account_id));
  SetAuthMethods(LoginAuthUserView::AUTH_PASSWORD |
                 LoginAuthUserView::AUTH_TAP);
  password_view->Clear();

  generator->PressKey(ui::KeyboardCode::VKEY_RETURN, 0);
  base::RunLoop().RunUntilIdle();
}

TEST_F(LoginAuthUserViewUnittest, OnlineSignInMessage) {
  std::unique_ptr<MockLoginScreenClient> client = BindMockLoginScreenClient();
  LoginAuthUserView::TestApi test_auth_user_view(view_);
  views::Button* online_sign_in_message(
      test_auth_user_view.online_sign_in_message());
  LoginPasswordView* password_view(test_auth_user_view.password_view());
  LoginPinView* pin_view(test_auth_user_view.pin_view());
  LoginUserView* user_view(test_auth_user_view.user_view());

  // When auth method is |AUTH_ONLINE_SIGN_IN|, the online sign-in message is
  // visible. The password field and PIN keyboard are invisible.
  SetAuthMethods(LoginAuthUserView::AUTH_ONLINE_SIGN_IN);
  EXPECT_TRUE(online_sign_in_message->visible());
  EXPECT_FALSE(password_view->visible());
  EXPECT_FALSE(pin_view->visible());

  // Clicking the message triggers |ShowGaiaSignin|.
  EXPECT_CALL(*client,
              ShowGaiaSignin(
                  true /*can_close*/,
                  base::Optional<AccountId>(
                      user_view->current_user()->basic_user_info->account_id)));
  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), 0, 0);
  view_->ButtonPressed(online_sign_in_message, event);
  base::RunLoop().RunUntilIdle();

  // The online sign-in message is invisible for all other auth methods.
  SetAuthMethods(LoginAuthUserView::AUTH_NONE);
  EXPECT_FALSE(online_sign_in_message->visible());
  SetAuthMethods(LoginAuthUserView::AUTH_PASSWORD);
  EXPECT_FALSE(online_sign_in_message->visible());
  SetAuthMethods(LoginAuthUserView::AUTH_PIN);
  EXPECT_FALSE(online_sign_in_message->visible());
  SetAuthMethods(LoginAuthUserView::AUTH_TAP);
  EXPECT_FALSE(online_sign_in_message->visible());
}

// Verifies that password is cleared after AUTH_PASSWORD is disabled.
TEST_F(LoginAuthUserViewUnittest,
       PasswordClearedAfterAnimationIfPasswordDisabled) {
  LoginPasswordView::TestApi password_test(view_->password_view());
  auto has_password = [&]() {
    return !password_test.textfield()->text().empty();
  };

  // Set a password.
  SetAuthMethods(LoginAuthUserView::AUTH_PASSWORD);
  password_test.textfield()->SetText(base::ASCIIToUTF16("Hello"));

  // Enable some other auth method (PIN), password is not cleared.
  view_->CaptureStateForAnimationPreLayout();
  SetAuthMethods(LoginAuthUserView::AUTH_PASSWORD |
                 LoginAuthUserView::AUTH_PIN);
  EXPECT_TRUE(has_password());
  view_->ApplyAnimationPostLayout();
  EXPECT_TRUE(has_password());

  // Disable password, password is cleared.
  view_->CaptureStateForAnimationPreLayout();
  SetAuthMethods(LoginAuthUserView::AUTH_NONE);
  EXPECT_TRUE(has_password());
  view_->ApplyAnimationPostLayout();
  EXPECT_FALSE(has_password());
}

}  // namespace ash
