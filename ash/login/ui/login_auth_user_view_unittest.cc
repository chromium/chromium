// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_auth_user_view.h"

#include "ash/login/login_screen_controller.h"
#include "ash/login/mock_login_screen_client.h"
#include "ash/login/ui/auth_factor_model.h"
#include "ash/login/ui/fake_smart_lock_auth_factor_model.h"
#include "ash/login/ui/fingerprint_auth_factor_model.h"
#include "ash/login/ui/login_auth_factors_view.h"
#include "ash/login/ui/login_password_view.h"
#include "ash/login/ui/login_pin_input_view.h"
#include "ash/login/ui/login_pin_view.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/login_test_utils.h"
#include "ash/login/ui/login_user_view.h"
#include "ash/login/ui/smart_lock_auth_factor_model.h"
#include "ash/shell.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_test_api.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/views_test_utils.h"
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

class LoginAuthUserViewTestBase : public LoginTestBase {
 public:
  LoginAuthUserViewTestBase(const LoginAuthUserViewTestBase&) = delete;
  LoginAuthUserViewTestBase& operator=(const LoginAuthUserViewTestBase&) =
      delete;

 protected:
  LoginAuthUserViewTestBase() = default;
  ~LoginAuthUserViewTestBase() override = default;

  // LoginTestBase:
  void SetUp() override { LoginTestBase::SetUp(); }

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

  void InitializeViewForUser(LoginUserInfo user) {
    user_ = user;
    LoginAuthUserView::Callbacks auth_callbacks;
    auth_callbacks.on_auth = base::DoNothing();
    auth_callbacks.on_easy_unlock_icon_hovered = base::DoNothing();
    auth_callbacks.on_tap = base::DoNothing();
    auth_callbacks.on_remove_warning_shown = base::DoNothing();
    auth_callbacks.on_remove = base::DoNothing();
    auth_callbacks.on_auth_factor_is_hiding_password_changed =
        base::DoNothing();
    view_ = new LoginAuthUserView(user_, auth_callbacks);

    // We proxy |view_| inside of |container_| so we can control layout.
    container_ = new views::View();
    container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    container_->AddChildView(view_.get());
    SetWidget(CreateWidgetWithContent(container_));
  }

  base::test::ScopedFeatureList feature_list_;
  LoginUserInfo user_;
  raw_ptr<views::View, ExperimentalAsh> container_ =
      nullptr;  // Owned by test widget view hierarchy.
  raw_ptr<LoginAuthUserView, ExperimentalAsh> view_ =
      nullptr;  // Owned by test widget view hierarchy.
};

class LoginAuthUserViewUnittest : public LoginAuthUserViewTestBase,
                                  /*autosubmit_feature*/
                                  public ::testing::WithParamInterface<bool> {
 public:
  LoginAuthUserViewUnittest(const LoginAuthUserViewUnittest&) = delete;
  LoginAuthUserViewUnittest& operator=(const LoginAuthUserViewUnittest&) =
      delete;

  static std::string ParamInfoToString(
      testing::TestParamInfo<LoginAuthUserViewUnittest::ParamType> info) {
    return info.param ? "AutosubmitEnabled" : "AutosubmitDisabled";
  }

 protected:
  LoginAuthUserViewUnittest() = default;
  ~LoginAuthUserViewUnittest() override = default;

  // LoginTestBase:
  void SetUp() override {
    LoginAuthUserViewTestBase::SetUp();
    feature_list_.InitWithFeatureState(features::kQuickUnlockPinAutosubmit,
                                       GetParam());
    InitializeViewForUser(CreateUser("user@domain.com"));
  }
};

class LoginAuthUserViewAutosumbitUnittest : public LoginAuthUserViewTestBase {
 public:
  LoginAuthUserViewAutosumbitUnittest(
      const LoginAuthUserViewAutosumbitUnittest&) = delete;
  LoginAuthUserViewAutosumbitUnittest& operator=(
      const LoginAuthUserViewAutosumbitUnittest&) = delete;

 protected:
  LoginAuthUserViewAutosumbitUnittest() = default;
  ~LoginAuthUserViewAutosumbitUnittest() override = default;

  // LoginTestBase:
  void SetUp() override {
    LoginAuthUserViewTestBase::SetUp();
    feature_list_.InitAndEnableFeature(features::kQuickUnlockPinAutosubmit);
    InitializeViewForUser(CreateUser("user@domain.com"));
  }
};

// Verifies showing the PIN keyboard makes the user view grow.
TEST_P(LoginAuthUserViewUnittest, ShowingPinExpandsView) {
  gfx::Size start_size = view_->size();
  SetAuthMethods(LoginAuthUserView::AUTH_PASSWORD |
                 LoginAuthUserView::AUTH_PIN);
  views::test::RunScheduledLayout(container_);
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
  views::test::ButtonTestApi(online_sign_in_message).NotifyClick(event);
  base::RunLoop().RunUntilIdle();

  // The online sign-in message is invisible for all other auth methods.
  SetAuthMethods(LoginAuthUserView::AUTH_NONE);
  EXPECT_FALSE(online_sign_in_message->GetVisible());
  SetAuthMethods(LoginAuthUserView::AUTH_PASSWORD);
  EXPECT_FALSE(online_sign_in_message->GetVisible());
  SetAuthMethods(LoginAuthUserView::AUTH_PIN);
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
  password_test.textfield()->SetText(u"Hello");

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

  const std::u16string password = u"abc1";
  password_test.textfield()->SetText(password);
  view_->UpdateForUser(user_);
  EXPECT_EQ(password_test.textfield()->GetText(), password);

  auto another_user = CreateUser("user2@domain.com");
  view_->UpdateForUser(another_user);
  EXPECT_TRUE(password_test.textfield()->GetText().empty());

  password_test.textfield()->SetTextInputType(ui::TEXT_INPUT_TYPE_NULL);
  EXPECT_EQ(password_test.textfield()->GetTextInputType(),
            ui::TEXT_INPUT_TYPE_NULL);

  // Updating user should make the textfield as a password again.
  view_->UpdateForUser(user_);
  EXPECT_EQ(password_test.textfield()->GetTextInputType(),
            ui::TEXT_INPUT_TYPE_PASSWORD);
}

TEST_F(LoginAuthUserViewAutosumbitUnittest, ResetPinFieldOnUpdateUser) {
  LoginAuthUserView::TestApi auth_test(view_);
  LoginPinInputView* pin_input(auth_test.pin_input_view());
  LoginPinInputView::TestApi pin_input_test_api(auth_test.pin_input_view());

  // Set up PIN with auto submit.
  SetUserCount(1);
  SetAuthPin(/*autosubmit_length*/ 6);
  ExpectModeVisibility(LoginAuthUserView::InputFieldMode::PIN_WITH_TOGGLE);

  // Insert some random digits.
  pin_input->InsertDigit(1);
  pin_input->InsertDigit(2);
  pin_input->InsertDigit(3);
  EXPECT_FALSE(pin_input_test_api.IsEmpty());

  // Verify PIN field gets reset when user is updated.
  auto another_user = CreateUser("user2@domain.com");
  view_->UpdateForUser(another_user);
  EXPECT_TRUE(pin_input_test_api.IsEmpty());
}

// Tests that the appropriate InputFieldMode is used based on the exposed
// length of the user's PIN. An exposed PIN length of zero (0) means that
// auto submit is not being used.
TEST_P(LoginAuthUserViewUnittest, CorrectFieldModeForExposedPinLength) {
  SetUserCount(1);

  for (int pin_length = 0; pin_length <= 64; pin_length++) {
    SetAuthPin(/*autosubmit_length*/ pin_length);

    if (LoginPinInputView::IsAutosubmitSupported(pin_length)) {
      ExpectModeVisibility(LoginAuthUserView::InputFieldMode::PIN_WITH_TOGGLE);
    } else {
      ExpectModeVisibility(LoginAuthUserView::InputFieldMode::PIN_AND_PASSWORD);
    }
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

// Tests the correctness of InputFieldMode::PASSWORD_ONLY. With only the
// password field present and no PIN, the authentication call must
// have 'authenticated_by_pin' as false.
TEST_P(LoginAuthUserViewUnittest, PasswordOnlyFieldMode) {
  LoginAuthUserView::TestApi auth_test(view_);
  ui::test::EventGenerator* generator = GetEventGenerator();
  auto client = std::make_unique<MockLoginScreenClient>();
  LoginUserView* user_view(auth_test.user_view());
  LoginPasswordView::TestApi password_test(view_->password_view());

  // Only password, no PIN.
  SetUserCount(1);
  SetAuthMethods(LoginAuthUserView::AUTH_PASSWORD);
  ExpectModeVisibility(LoginAuthUserView::InputFieldMode::PASSWORD_ONLY);

  password_test.textfield()->SetText(u"test_password");

  EXPECT_CALL(*client, AuthenticateUserWithPasswordOrPin_(
                           user_view->current_user().basic_user_info.account_id,
                           /*password=*/"test_password",
                           /*authenticated_by_pin=*/false,
                           /*callback=*/_));

  generator->PressKey(ui::KeyboardCode::VKEY_RETURN, 0);
  base::RunLoop().RunUntilIdle();
}

/**
 * Tests the correctness of InputFieldMode::PIN_AND_PASSWORD by ensuring the
 * following:
 * - Clicking on the pin pad inserts digits into the field
 * - Digits are correctly ignored when the field is set to read-only
 * - Submitting the credentials results in the correct auth call
 */
TEST_P(LoginAuthUserViewUnittest, PinAndPasswordFieldModeCorrectness) {
  LoginAuthUserView::TestApi auth_test(view_);
  ui::test::EventGenerator* generator = GetEventGenerator();
  auto client = std::make_unique<MockLoginScreenClient>();
  LoginUserView* user_view(auth_test.user_view());
  LoginPinView::TestApi pin_pad_api{auth_test.pin_view()};

  // PIN length not exposed and thus no auto submit.
  SetUserCount(1);
  SetAuthPin(/*autosubmit_length*/ 0);
  ExpectModeVisibility(LoginAuthUserView::InputFieldMode::PIN_AND_PASSWORD);

  // Typing '123456789'.
  pin_pad_api.ClickOnDigit(1);
  pin_pad_api.ClickOnDigit(2);
  pin_pad_api.ClickOnDigit(3);
  // '456' must be ignored.
  auth_test.password_view()->SetReadOnly(true);
  pin_pad_api.ClickOnDigit(4);
  pin_pad_api.ClickOnDigit(5);
  pin_pad_api.ClickOnDigit(6);
  auth_test.password_view()->SetReadOnly(false);
  pin_pad_api.ClickOnDigit(7);
  pin_pad_api.ClickOnDigit(8);
  pin_pad_api.ClickOnDigit(9);

  EXPECT_CALL(*client, AuthenticateUserWithPasswordOrPin_(
                           user_view->current_user().basic_user_info.account_id,
                           /*password=*/"123789",
                           /*authenticated_by_pin=*/true,
                           /*callback=*/_));

  generator->PressKey(ui::KeyboardCode::VKEY_RETURN, 0);
  base::RunLoop().RunUntilIdle();
}

/**
 * Tests the correctness of InputFieldMode::PIN_WITH_TOGGLE - the default
 * input mode when using a PIN with auto submit enabled, by ensuring the
 * following:
 * - Clicking on the pin pad inserts digits into the field
 * - Digits are correctly ignored when the field is set to read-only
 * - Submitting the last digit triggers the correct auth call
 */
TEST_F(LoginAuthUserViewAutosumbitUnittest, PinWithToggleFieldModeCorrectness) {
  LoginAuthUserView::TestApi auth_test(view_);
  auto client = std::make_unique<MockLoginScreenClient>();
  LoginUserView* user_view(auth_test.user_view());
  LoginPinInputView::TestApi pin_input_test{auth_test.pin_input_view()};
  LoginPinView::TestApi pin_pad_api{auth_test.pin_view()};

  // Set up PIN with auto submit.
  SetUserCount(1);
  SetAuthPin(/*autosubmit_length*/ 6);
  ExpectModeVisibility(LoginAuthUserView::InputFieldMode::PIN_WITH_TOGGLE);

  const auto pin = std::string("123456");
  EXPECT_CALL(*client, AuthenticateUserWithPasswordOrPin_(
                           user_view->current_user().basic_user_info.account_id,
                           /*password=*/pin,
                           /*authenticated_by_pin=*/true,
                           /*callback=*/_));

  // Inserting `1230456`.
  pin_pad_api.ClickOnDigit(1);
  pin_pad_api.ClickOnDigit(2);
  pin_pad_api.ClickOnDigit(3);
  // `0` must be ignored when the field is read only.
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

/**
 * Tests the correctness of InputFieldMode::PWD_WITH_TOGGLE. This is the
 * mode that shows just the password field with the option to switch to PIN.
 * It is only available when the user has auto submit enabled.
 */
TEST_F(LoginAuthUserViewAutosumbitUnittest, PwdWithToggleFieldModeCorrectness) {
  LoginAuthUserView::TestApi auth_test(view_);
  ui::test::EventGenerator* generator = GetEventGenerator();
  auto client = std::make_unique<MockLoginScreenClient>();
  LoginUserView* user_view(auth_test.user_view());
  LoginPasswordView::TestApi password_test(view_->password_view());

  // Set up PIN with auto submit and click on the toggle to switch to password.
  SetUserCount(1);
  SetAuthPin(/*autosubmit_length*/ 6);
  ExpectModeVisibility(LoginAuthUserView::InputFieldMode::PIN_WITH_TOGGLE);
  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi(auth_test.pin_password_toggle())
      .NotifyClick(event);
  base::RunLoop().RunUntilIdle();
  ExpectModeVisibility(LoginAuthUserView::InputFieldMode::PWD_WITH_TOGGLE);

  // Insert a password consisting of numbers only and expect it to be treated
  // as a password, not a PIN. This means 'authenticated_by_pin' must be false.
  password_test.textfield()->SetText(u"12345678");

  EXPECT_CALL(*client, AuthenticateUserWithPasswordOrPin_(
                           user_view->current_user().basic_user_info.account_id,
                           /*password=*/"12345678",
                           /*authenticated_by_pin=*/false,
                           /*callback=*/_));

  generator->PressKey(ui::KeyboardCode::VKEY_RETURN, 0);
  base::RunLoop().RunUntilIdle();
}

INSTANTIATE_TEST_SUITE_P(LoginAuthUserViewTests,
                         LoginAuthUserViewUnittest,
                         testing::Bool(),  // PIN autosubmit feature
                         LoginAuthUserViewUnittest::ParamInfoToString);

/**
 * This subclass is a test fixture for tests validating logic with auth factors.
 * The test requires passing a custom user object per test to initialize the
 * view to test.
 */
class LoginAuthUserViewAuthFactorsUnittest : public LoginAuthUserViewUnittest {
 public:
  LoginAuthUserViewAuthFactorsUnittest(
      const LoginAuthUserViewAuthFactorsUnittest&) = delete;
  LoginAuthUserViewAuthFactorsUnittest& operator=(
      const LoginAuthUserViewAuthFactorsUnittest&) = delete;

 protected:
  LoginAuthUserViewAuthFactorsUnittest() = default;
  ~LoginAuthUserViewAuthFactorsUnittest() override = default;

  void SetUp() override {
    LoginTestBase::SetUp();
    fake_smart_lock_auth_factor_model_factory_ =
        std::make_unique<FakeSmartLockAuthFactorModelFactory>();
    SmartLockAuthFactorModel::Factory::SetFactoryForTesting(
        fake_smart_lock_auth_factor_model_factory_.get());
  }

  void TearDown() override {
    SmartLockAuthFactorModel::Factory::SetFactoryForTesting(nullptr);
    LoginAuthUserViewUnittest::TearDown();
  }

  std::unique_ptr<FakeSmartLockAuthFactorModelFactory>
      fake_smart_lock_auth_factor_model_factory_;
};

TEST_F(LoginAuthUserViewAuthFactorsUnittest, ShowFingerprintIfAvailable) {
  auto user = CreateUser("user@domain.com");
  user.fingerprint_state = FingerprintState::AVAILABLE_DEFAULT;
  InitializeViewForUser(user);
  SetAuthMethods(LoginAuthUserView::AuthMethods::AUTH_FINGERPRINT);
  LoginAuthUserView::TestApi auth_test(view_);
  AuthFactorModel* fingerprint_auth_factor =
      auth_test.fingerprint_auth_factor_model();
  EXPECT_EQ(fingerprint_auth_factor->GetAuthFactorState(),
            AuthFactorModel::AuthFactorState::kReady);
}

TEST_F(LoginAuthUserViewAuthFactorsUnittest, NotShowFingerprintIfUnavaialble) {
  auto user = CreateUser("user@domain.com");
  user.fingerprint_state = FingerprintState::UNAVAILABLE;
  InitializeViewForUser(user);
  SetAuthMethods(LoginAuthUserView::AuthMethods::AUTH_FINGERPRINT);
  LoginAuthUserView::TestApi auth_test(view_);
  AuthFactorModel* fingerprint_auth_factor =
      auth_test.fingerprint_auth_factor_model();
  EXPECT_EQ(fingerprint_auth_factor->GetAuthFactorState(),
            AuthFactorModel::AuthFactorState::kUnavailable);
}

TEST_F(LoginAuthUserViewAuthFactorsUnittest, SmartLockInitialState) {
  auto user = CreateUser("user@domain.com");
  user.smart_lock_state = SmartLockState::kConnectingToPhone;
  InitializeViewForUser(user);

  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  Shell::Get()->login_screen_controller()->ShowLockScreen();
  LoginAuthUserView::TestApi auth_test(view_);

  EXPECT_EQ(SmartLockState::kConnectingToPhone,
            fake_smart_lock_auth_factor_model_factory_->GetLastCreatedModel()
                ->GetSmartLockState());
}

TEST_F(LoginAuthUserViewAuthFactorsUnittest, VerifySmartLockArrowTapCallback) {
  auto user = CreateUser("user@domain.com");
  InitializeViewForUser(user);
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  Shell::Get()->login_screen_controller()->ShowLockScreen();
  LoginAuthUserView::TestApi auth_test(view_);
  auth_test.SetSmartLockState(SmartLockState::kPhoneAuthenticated);
  auto client = std::make_unique<MockLoginScreenClient>();
  EXPECT_CALL(*client,
              AuthenticateUserWithEasyUnlock(user.basic_user_info.account_id));
  auth_test.smart_lock_auth_factor_model()->OnArrowButtonTapOrClickEvent();
}

// Regression test for b/215630674. The transform applied to
// LoginAuthFactorsView was getting applied multiple times on suspend/wake,
// causing the auth factors to slide up farther than they should. This checks
// that multiple calls to SetAuthMethods() has no further effect on the
// y-offset.
TEST_F(LoginAuthUserViewAuthFactorsUnittest,
       SmartLockPasswordCollapseAnimationAppliedOnce) {
  auto user = CreateUser("user@domain.com");
  InitializeViewForUser(user);
  LoginAuthUserView::TestApi auth_test(view_);
  auto* auth_factors_view = auth_test.auth_factors_view();

  SetAuthMethods(LoginAuthUserView::AUTH_PASSWORD |
                 LoginAuthUserView::AUTH_AUTH_FACTOR_IS_HIDING_PASSWORD);
  views::test::RunScheduledLayout(container_);

  int auth_factors_y_offset_1 =
      auth_factors_view
          ->ConvertRectToWidget(auth_factors_view->GetLocalBounds())
          .y();

  SetAuthMethods(LoginAuthUserView::AUTH_PASSWORD |
                 LoginAuthUserView::AUTH_AUTH_FACTOR_IS_HIDING_PASSWORD);
  views::test::RunScheduledLayout(container_);

  int auth_factors_y_offset_2 =
      auth_factors_view
          ->ConvertRectToWidget(auth_factors_view->GetLocalBounds())
          .y();

  EXPECT_EQ(auth_factors_y_offset_1, auth_factors_y_offset_2);
}

// Check that LoginAuthUserView hides/shows elements appropriately when the
// AUTH_AUTH_FACTOR_IS_HIDING_PASSWORD bit is set/unset.
TEST_F(LoginAuthUserViewAuthFactorsUnittest,
       SmartLockPasswordPinHiddenWhenAuthFactorIsHidingPasswordBitSet) {
  auto user = CreateUser("user@domain.com");
  InitializeViewForUser(user);

  SetAuthMethods(LoginAuthUserView::AUTH_PASSWORD);
  ExpectModeVisibility(LoginAuthUserView::InputFieldMode::PASSWORD_ONLY);

  SetAuthMethods(LoginAuthUserView::AUTH_PASSWORD |
                 LoginAuthUserView::AUTH_AUTH_FACTOR_IS_HIDING_PASSWORD);
  ExpectModeVisibility(LoginAuthUserView::InputFieldMode::NONE);

  SetAuthMethods(LoginAuthUserView::AUTH_PASSWORD |
                 LoginAuthUserView::AUTH_PIN);
  ExpectModeVisibility(LoginAuthUserView::InputFieldMode::PIN_AND_PASSWORD);

  SetAuthMethods(LoginAuthUserView::AUTH_PASSWORD |
                 LoginAuthUserView::AUTH_PIN |
                 LoginAuthUserView::AUTH_AUTH_FACTOR_IS_HIDING_PASSWORD);
  ExpectModeVisibility(LoginAuthUserView::InputFieldMode::NONE);
}

// Regression test for b/217385675.
// LoginAuthFactorsView should only be shown when auth methods are available.
TEST_F(LoginAuthUserViewAuthFactorsUnittest,
       AuthFactorsViewNotShownWhenNoAuthFactors) {
  auto user = CreateUser("user@domain.com");
  InitializeViewForUser(user);
  LoginAuthUserView::TestApi auth_test(view_);
  auto* auth_factors_view = auth_test.auth_factors_view();

  EXPECT_TRUE(auth_factors_view);
  EXPECT_FALSE(auth_factors_view->GetVisible());

  SetAuthMethods(LoginAuthUserView::AuthMethods::AUTH_SMART_LOCK);
  EXPECT_TRUE(auth_factors_view->GetVisible());

  SetAuthMethods(LoginAuthUserView::AuthMethods::AUTH_FINGERPRINT);
  EXPECT_TRUE(auth_factors_view->GetVisible());

  SetAuthMethods(LoginAuthUserView::AuthMethods::AUTH_SMART_LOCK |
                 LoginAuthUserView::AuthMethods::AUTH_FINGERPRINT);
  EXPECT_TRUE(auth_factors_view->GetVisible());

  SetAuthMethods(LoginAuthUserView::AUTH_NONE);
  EXPECT_FALSE(auth_factors_view->GetVisible());
}

}  // namespace ash
