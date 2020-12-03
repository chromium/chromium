// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_auth_user_view.h"
#include "ash/login/mock_login_screen_client.h"
#include "ash/login/ui/login_password_view.h"
#include "ash/login/ui/login_pin_input_view.h"
#include "ash/login/ui/login_pin_view.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/login_test_utils.h"
#include "ash/login/ui/login_user_view.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_test_api.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

using testing::_;

namespace ash {
namespace {

// Input field mode to element visibility mapping.
struct InputFieldVisibility {
  bool password;
  bool pin_input;
  bool toggle;
};
const std::map<LoginAuthUserView::InputFieldMode, InputFieldVisibility>
    expected_visibilities = {
        {LoginAuthUserView::InputFieldMode::NONE,
         {/*pwd*/ false, /*pin_input*/ false, /*toggle*/ false}},
        {LoginAuthUserView::InputFieldMode::PASSWORD_ONLY,
         {/*pwd*/ true, /*pin_input*/ false, /*toggle*/ false}},
        {LoginAuthUserView::InputFieldMode::PIN_AND_PASSWORD,
         {/*pwd*/ true, /*pin_input*/ false, /*toggle*/ false}},
        {LoginAuthUserView::InputFieldMode::PIN_WITH_TOGGLE,
         {/*pwd*/ false, /*pin_input*/ true, /*toggle*/ true}},
        {LoginAuthUserView::InputFieldMode::PWD_WITH_TOGGLE,
         {/*pwd*/ true, /*pin_input*/ false, /*toggle*/ true}},
};

}  // namespace

class LoginAuthUserViewUnittest
    : public LoginTestBase,
      /*<autosubmit_feature, display_password_feature>*/
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  static std::string ParamInfoToString(
      testing::TestParamInfo<LoginAuthUserViewUnittest::ParamType> info) {
    return base::StrCat(
        {std::get<0>(info.param) ? "AutosubmitEnabled" : "AutosubmitDisabled",
         std::get<1>(info.param) ? "DisplayPasswordEnabled"
                                 : "DisplayPasswordDisabled"});
  }

 protected:
  LoginAuthUserViewUnittest() = default;
  ~LoginAuthUserViewUnittest() override = default;

  // LoginTestBase:
  void SetUp() override {
    LoginTestBase::SetUp();
    SetUpFeatures();
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
    container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    container_->AddChildView(view_);
    SetWidget(CreateWidgetWithContent(container_));
  }

  void SetUpFeatures() {
    auto param = GetParam();
    autosubmit_feature_enabled_ = std::get<0>(param);
    display_password_feature_enabled_ = std::get<1>(param);
    std::vector<base::Feature> enabled;
    std::vector<base::Feature> disabled;
    autosubmit_feature_enabled_
        ? enabled.push_back(chromeos::features::kQuickUnlockPinAutosubmit)
        : disabled.push_back(chromeos::features::kQuickUnlockPinAutosubmit);
    display_password_feature_enabled_
        ? enabled.push_back(chromeos::features::kLoginDisplayPasswordButton)
        : disabled.push_back(chromeos::features::kLoginDisplayPasswordButton);
    feature_list_.InitWithFeatures(enabled, disabled);
  }

  void SetAuthMethods(uint32_t auth_methods,
                      bool show_pinpad_for_pw = false,
                      bool virtual_keyboard_visible = false,
                      size_t autosubmit_pin_length = 0) {
    LoginAuthUserView::AuthMethodsMetadata auth_metadata;
    auth_metadata.show_pinpad_for_pw = show_pinpad_for_pw;
    auth_metadata.virtual_keyboard_visible = virtual_keyboard_visible;
    auth_metadata.autosubmit_pin_length = autosubmit_pin_length;
    view_->CaptureStateForAnimationPreLayout();
    view_->SetAuthMethods(auth_methods, auth_metadata);
    view_->ApplyAnimationPostLayout(true);
  }

  // Enables password and pin with the given length.
  void SetAuthPin(int autosubmit_length) {
    auto auth = LoginAuthUserView::AUTH_PASSWORD | LoginAuthUserView::AUTH_PIN;
    SetAuthMethods(/*auth_methods*/ auth,
                   /*show_pinpad_for_pw*/ false,
                   /*virtual_keyboard_visible*/ false,
                   /*autosubmit_pin_length*/ autosubmit_length);
  }

  // Expects the given input field mode and the corresponding visibility.
  void ExpectModeVisibility(LoginAuthUserView::InputFieldMode mode) {
    EXPECT_EQ(view_->input_field_mode(), mode);
    InputFieldVisibility visibility = expected_visibilities.at(mode);
    LoginAuthUserView::TestApi test(view_);
    EXPECT_EQ(test.password_view()->GetVisible(), visibility.password);
    EXPECT_EQ(test.pin_input_view()->GetVisible(), visibility.pin_input);
    EXPECT_EQ(test.pin_password_toggle()->GetVisible(), visibility.toggle);
  }

  // Initialized by test parameters in `SetUpFeatures`
  bool autosubmit_feature_enabled_ = false;
  bool display_password_feature_enabled_ = false;

  base::test::ScopedFeatureList feature_list_;
  LoginUserInfo user_;
  views::View* container_ = nullptr;   // Owned by test widget view hierarchy.
  LoginAuthUserView* view_ = nullptr;  // Owned by test widget view hierarchy.

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginAuthUserViewUnittest);
};

// Verifies showing the PIN keyboard makes the user view grow.
TEST_P(LoginAuthUserViewUnittest, ShowingPinExpandsView) {
  gfx::Size start_size = view_->size();
  SetAuthMethods(LoginAuthUserView::AUTH_PASSWORD |
                 LoginAuthUserView::AUTH_PIN);
  container_->Layout();
  gfx::Size expanded_size = view_->size();
  EXPECT_GT(expanded_size.height(), start_size.height());
}

// Verifies that an auth user that shows a password is opaque.
TEST_P(LoginAuthUserViewUnittest, ShowingPasswordForcesOpaque) {
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
TEST_P(LoginAuthUserViewUnittest, PressReturnWithTapToUnlockEnabled) {
  auto client = std::make_unique<MockLoginScreenClient>();

  ui::test::EventGenerator* generator = GetEventGenerator();

  LoginAuthUserView::TestApi auth_test(view_);
  LoginPasswordView* password_view(auth_test.password_view());
  LoginUserView* user_view(auth_test.user_view());

  SetUserCount(1);

  EXPECT_CALL(*client,
              AuthenticateUserWithEasyUnlock(
                  user_view->current_user().basic_user_info.account_id));
  SetAuthMethods(LoginAuthUserView::AUTH_PASSWORD |
                 LoginAuthUserView::AUTH_TAP);
  password_view->Clear();

  generator->PressKey(ui::KeyboardCode::VKEY_RETURN, 0);
  base::RunLoop().RunUntilIdle();
}

TEST_P(LoginAuthUserViewUnittest, OnlineSignInMessage) {
  auto client = std::make_unique<MockLoginScreenClient>();
  LoginAuthUserView::TestApi auth_test(view_);
  views::Button* online_sign_in_message(auth_test.online_sign_in_message());
  LoginUserView* user_view(auth_test.user_view());

  // When auth method is |AUTH_ONLINE_SIGN_IN|, the online sign-in message is
  // visible. The password field and PIN keyboard are invisible.
  SetAuthMethods(LoginAuthUserView::AUTH_ONLINE_SIGN_IN);
  EXPECT_TRUE(online_sign_in_message->GetVisible());
  ExpectModeVisibility(LoginAuthUserView::InputFieldMode::NONE);

  // Clicking the message triggers |ShowGaiaSignin|.
  EXPECT_CALL(
      *client,
      ShowGaiaSignin(user_view->current_user().basic_user_info.account_id));
  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), 0, 0);
  view_->ButtonPressed(online_sign_in_message, event);
  base::RunLoop().RunUntilIdle();

  // The online sign-in message is invisible for all other auth methods.
  SetAuthMethods(LoginAuthUserView::AUTH_NONE);
  EXPECT_FALSE(online_sign_in_message->GetVisible());
  SetAuthMethods(LoginAuthUserView::AUTH_PASSWORD);
  EXPECT_FALSE(online_sign_in_message->GetVisible());
  SetAuthMethods(LoginAuthUserView::AUTH_PIN);
  EXPECT_FALSE(online_sign_in_message->GetVisible());
  SetAuthMethods(LoginAuthUserView::AUTH_TAP);
  EXPECT_FALSE(online_sign_in_message->GetVisible());
}

// Verifies that password is cleared after AUTH_PASSWORD is disabled.
TEST_P(LoginAuthUserViewUnittest,
       PasswordClearedAfterAnimationIfPasswordDisabled) {
  LoginPasswordView::TestApi password_test(view_->password_view());
  auto has_password = [&]() {
    return !password_test.textfield()->GetText().empty();
  };

  // Set a password.
  SetAuthMethods(LoginAuthUserView::AUTH_PASSWORD);
  password_test.textfield()->SetText(base::ASCIIToUTF16("Hello"));

  // Enable some other auth method (PIN), password is not cleared.
  EXPECT_TRUE(has_password());
  SetAuthMethods(LoginAuthUserView::AUTH_PASSWORD |
                 LoginAuthUserView::AUTH_PIN);
  EXPECT_TRUE(has_password());

  // Disable password, password is cleared.
  EXPECT_TRUE(has_password());
  SetAuthMethods(LoginAuthUserView::AUTH_NONE);
  EXPECT_FALSE(has_password());
}

TEST_P(LoginAuthUserViewUnittest, PasswordFieldChangeOnUpdateUser) {
  LoginAuthUserView::TestApi auth_test(view_);
  LoginPasswordView::TestApi password_test(auth_test.password_view());

  const auto password = base::ASCIIToUTF16("abc1");
  password_test.textfield()->SetText(password);
  view_->UpdateForUser(user_);
  EXPECT_EQ(password_test.textfield()->GetText(), password);

  auto another_user = CreateUser("user2@domain.com");
  view_->UpdateForUser(another_user);
  EXPECT_TRUE(password_test.textfield()->GetText().empty());

  // TODO(tellier) - Check that this test is doing what it is intended to
  if (display_password_feature_enabled_) {
    password_test.textfield()->SetTextInputType(ui::TEXT_INPUT_TYPE_NULL);
    EXPECT_EQ(password_test.textfield()->GetTextInputType(),
              ui::TEXT_INPUT_TYPE_NULL);

    // Updating user should make the textfield as a password again.
    view_->UpdateForUser(user_);
    EXPECT_EQ(password_test.textfield()->GetTextInputType(),
              ui::TEXT_INPUT_TYPE_PASSWORD);
  }
}

// Tests the correctness of InputFieldMode::NONE
TEST_P(LoginAuthUserViewUnittest, ModesWithoutInputFields) {
  LoginAuthUserView::TestApi auth_test(view_);
  LoginAuthUserView::AuthMethods methods_without_input[] = {
      LoginAuthUserView::AUTH_CHALLENGE_RESPONSE,
      LoginAuthUserView::AUTH_DISABLED, LoginAuthUserView::AUTH_NONE,
      LoginAuthUserView::AUTH_ONLINE_SIGN_IN};
  for (auto method : methods_without_input) {
    SetAuthMethods(method);
    ExpectModeVisibility(LoginAuthUserView::InputFieldMode::NONE);
  }
}

// Tests the correctness of InputFieldMode::PASSWORD_ONLY
TEST_P(LoginAuthUserViewUnittest, PasswordOnlyFieldMode) {
  LoginAuthUserView::TestApi auth_test(view_);
  // Only password, no PIN.
  SetAuthMethods(LoginAuthUserView::AUTH_PASSWORD);
  ExpectModeVisibility(LoginAuthUserView::InputFieldMode::PASSWORD_ONLY);
}

// Tests the correctness of InputFieldMode::PIN_AND_PASSWORD
TEST_P(LoginAuthUserViewUnittest, PinAndPasswordFieldMode) {
  LoginAuthUserView::TestApi auth_test(view_);
  // Invalid pin lengths always result in a combined PIN and PWD input.
  const int invalid_lengths[] = {0, 1, 2, 3, 4, 5, 13, 14};
  for (auto length : invalid_lengths) {
    SetAuthPin(/*autosubmit_length*/ length);
    ExpectModeVisibility(LoginAuthUserView::InputFieldMode::PIN_AND_PASSWORD);
  }
}

// Tests that clicking on the digits on the pin pad inserts digits on the
// password field, but it does not when the field is read only.
TEST_P(LoginAuthUserViewUnittest, PinPadWorksForPasswordField) {
  LoginAuthUserView::TestApi auth_test(view_);
  LoginPasswordView::TestApi password_test(auth_test.password_view());

  // Set up PIN without auto submit.
  SetAuthPin(/*autosubmit_length*/ 0);

  LoginPinView::TestApi pin_pad_api{auth_test.pin_view()};
  const auto password = base::ASCIIToUTF16("123");

  // Click on "1" "2" and "3"
  pin_pad_api.ClickOnDigit(1);
  pin_pad_api.ClickOnDigit(2);
  pin_pad_api.ClickOnDigit(3);

  EXPECT_EQ(password_test.textfield()->GetText(), password);

  // Set the field to read only. Clicking on the digits has no effect.
  auth_test.password_view()->SetReadOnly(true);
  pin_pad_api.ClickOnDigit(4);
  pin_pad_api.ClickOnDigit(5);
  pin_pad_api.ClickOnDigit(6);

  EXPECT_EQ(password_test.textfield()->GetText(), password);
}

// Tests that clicking on the digits on the pin pad inserts digits on the
// PIN input field, but it does not when the field is read only.
TEST_P(LoginAuthUserViewUnittest, PinPadWorksForPinInputField) {
  if (!autosubmit_feature_enabled_)
    return;

  auto client = std::make_unique<MockLoginScreenClient>();
  LoginAuthUserView::TestApi auth_test(view_);
  LoginUserView* user_view(auth_test.user_view());
  LoginPinInputView::TestApi pin_input_test{auth_test.pin_input_view()};
  SetUserCount(1);
  EXPECT_CALL(*client, AuthenticateUserWithPasswordOrPin_(
                           user_view->current_user().basic_user_info.account_id,
                           "123456", /*password*/
                           true,     /*authenticated_by_pin*/
                           _ /*callback*/));

  // Set up PIN with auto submit.
  SetAuthPin(/*autosubmit_length*/ 6);

  LoginPinView::TestApi pin_pad_api{auth_test.pin_view()};
  const auto pin = std::string("123456");

  // Click on "1" "2" and "3"
  pin_pad_api.ClickOnDigit(1);
  pin_pad_api.ClickOnDigit(2);
  pin_pad_api.ClickOnDigit(3);

  // Ignore input when read only.
  auth_test.pin_input_view()->SetReadOnly(true);
  pin_pad_api.ClickOnDigit(0);
  auth_test.pin_input_view()->SetReadOnly(false);

  pin_pad_api.ClickOnDigit(4);
  pin_pad_api.ClickOnDigit(5);
  pin_pad_api.ClickOnDigit(6);

  ASSERT_TRUE(pin_input_test.GetCode().has_value());
  EXPECT_EQ(pin_input_test.GetCode().value(), pin);
  base::RunLoop().RunUntilIdle();
}

// Tests the correctness of InputFieldModes used for PIN autosubmit.
TEST_P(LoginAuthUserViewUnittest, PinAutosubmitFieldModes) {
  LoginAuthUserView::TestApi auth_test(view_);
  // Valid autosubmit lenths default to showing the pin input field,
  // if autosubmit is available.
  const int valid_lengths[] = {6, 7, 8, 9, 10, 11, 12};
  for (auto length : valid_lengths) {
    SetAuthPin(/*autosubmit_length*/ length);

    // No autosubmit when the feature is disabled
    if (!autosubmit_feature_enabled_) {
      ExpectModeVisibility(LoginAuthUserView::InputFieldMode::PIN_AND_PASSWORD);
      continue;
    }

    // Autosubmit feature present
    ExpectModeVisibility(LoginAuthUserView::InputFieldMode::PIN_WITH_TOGGLE);
    // Clicking on the switch button changes to the password field
    const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                               ui::EventTimeForNow(), 0, 0);
    view_->ButtonPressed(auth_test.pin_password_toggle(), event);
    base::RunLoop().RunUntilIdle();
    ExpectModeVisibility(LoginAuthUserView::InputFieldMode::PWD_WITH_TOGGLE);
    SetAuthMethods(LoginAuthUserView::AUTH_NONE);  // Clear state for next run.
  }
}

INSTANTIATE_TEST_SUITE_P(
    LoginAuthUserViewTests,
    LoginAuthUserViewUnittest,
    testing::Combine(testing::Bool(),   // Display password feature
                     testing::Bool()),  // PIN autosubmit feature
    LoginAuthUserViewUnittest::ParamInfoToString);

}  // namespace ash
