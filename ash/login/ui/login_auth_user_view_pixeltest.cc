// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "ash/login/login_screen_controller.h"
#include "ash/login/mock_login_screen_client.h"
#include "ash/login/ui/auth_factor_model.h"
#include "ash/login/ui/login_auth_factors_view.h"
#include "ash/login/ui/login_auth_user_view.h"
#include "ash/login/ui/login_password_view.h"
#include "ash/login/ui/login_pin_input_view.h"
#include "ash/login/ui/login_pin_view.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/login/ui/login_test_utils.h"
#include "ash/login/ui/login_user_view.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
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
        {LoginAuthUserView::InputFieldMode::kNone,
         {/*pwd*/ false, /*pin_input*/ false, /*toggle*/ false}},
        {LoginAuthUserView::InputFieldMode::kPasswordOnly,
         {/*pwd*/ true, /*pin_input*/ false, /*toggle*/ false}},
        {LoginAuthUserView::InputFieldMode::kPinOnlyAutosubmitOn,
         {/*pwd*/ false, /*pin_input*/ true, /*toggle*/ false}},
        {LoginAuthUserView::InputFieldMode::kPinOnlyAutosubmitOff,
         {/*pwd*/ true, /*pin_input*/ false, /*toggle*/ false}},
        {LoginAuthUserView::InputFieldMode::kPasswordAndPin,
         {/*pwd*/ true, /*pin_input*/ false, /*toggle*/ false}},
        {LoginAuthUserView::InputFieldMode::kPinWithToggle,
         {/*pwd*/ false, /*pin_input*/ true, /*toggle*/ true}},
        {LoginAuthUserView::InputFieldMode::kPwdWithToggle,
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

  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  // Enables password and pin with the given length.
  void SetAuthPasswordAndPin(int autosubmit_length) {
    auto auth = LoginAuthUserView::AUTH_PASSWORD | LoginAuthUserView::AUTH_PIN;
    SetAuthMethods(/*auth_methods*/ auth,
                   /*show_pinpad_for_pw*/ false,
                   /*virtual_keyboard_visible*/ false,
                   /*autosubmit_pin_length*/ autosubmit_length);
  }

  void SetAuthPin(int autosubmit_length) {
    auto auth = LoginAuthUserView::AUTH_PIN;
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
  raw_ptr<views::View, DanglingUntriaged> container_ =
      nullptr;  // Owned by test widget view hierarchy.
  raw_ptr<LoginAuthUserView, DanglingUntriaged> view_ =
      nullptr;  // Owned by test widget view hierarchy.
};

class LoginAuthUserViewPixeltest : public LoginAuthUserViewTestBase {
 public:
  LoginAuthUserViewPixeltest(const LoginAuthUserViewPixeltest&) = delete;
  LoginAuthUserViewPixeltest& operator=(const LoginAuthUserViewPixeltest&) =
      delete;

 protected:
  LoginAuthUserViewPixeltest() = default;
  ~LoginAuthUserViewPixeltest() override = default;

  // LoginTestBase:
  void SetUp() override {
    LoginAuthUserViewTestBase::SetUp();
    InitializeViewForUser(CreateUser("user@domain.com"));
    // DarkLightModeControllerImpl::Get()->SetDarkModeEnabledForTest(false);
  }
};

// Verifies the PIN and password look a like option.
TEST_F(LoginAuthUserViewPixeltest, PinAndPassword) {
  SetAuthMethods(LoginAuthUserView::AUTH_PASSWORD |
                 LoginAuthUserView::AUTH_PIN);
  views::test::RunScheduledLayout(container_);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "PinAndPassword", /*revision_number=*/0, view_));
}

class LoginAuthUserViewPinOnlyPixeltest : public LoginAuthUserViewPixeltest {
 public:
  LoginAuthUserViewPinOnlyPixeltest() {
    feature_list_.Reset();
    feature_list_.InitAndEnableFeature(features::kAllowPasswordlessSetup);
  }
};

// Verifies the PIN only with auto submit case. Take two pictures:
// - before entering the pin
// - after all six pin character filled
TEST_F(LoginAuthUserViewPinOnlyPixeltest, PinOnlyModeWithAutosubmitEnabled) {
  LoginAuthUserView::TestApi auth_test(view_);
  auto client = std::make_unique<MockLoginScreenClient>();
  LoginPinInputView::TestApi pin_input_test{auth_test.pin_input_view()};
  LoginPinView::TestApi pin_pad_api{auth_test.pin_view()};

  // Set up PIN with auto submit.
  SetUserCount(1);
  SetAuthPin(/*autosubmit_length*/ 6);
  ExpectModeVisibility(LoginAuthUserView::InputFieldMode::kPinOnlyAutosubmitOn);

  views::test::RunScheduledLayout(container_);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "PinOnlyEmpty", /*revision_number=*/0, view_));

  const auto pin = std::string("123456");

  for (auto c : pin) {
    pin_pad_api.ClickOnDigit(c - '0');
  }

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "PinOnlyFilled", /*revision_number=*/0, view_));
}

// Verifies the PIN only with auto submit off case. Take two pictures:
// - before entering the pin
// - after six pin character entered
TEST_F(LoginAuthUserViewPinOnlyPixeltest, PinOnlyModeWithAutosubmitDisabled) {
  LoginAuthUserView::TestApi auth_test(view_);
  auto client = std::make_unique<MockLoginScreenClient>();
  LoginPinInputView::TestApi pin_input_test{auth_test.pin_input_view()};
  LoginPinView::TestApi pin_pad_api{auth_test.pin_view()};

  // Set up PIN with auto submit.
  SetUserCount(1);
  SetAuthPin(/*autosubmit_length*/ 0);
  ExpectModeVisibility(
      LoginAuthUserView::InputFieldMode::kPinOnlyAutosubmitOff);

  views::test::RunScheduledLayout(container_);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "PinOnlyEmpty", /*revision_number=*/0, view_));

  const auto pin = std::string("123456");

  for (auto c : pin) {
    pin_pad_api.ClickOnDigit(c - '0');
  }

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "PinOnlyFilled", /*revision_number=*/0, view_));
}

}  // namespace ash
