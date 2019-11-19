// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_password_view.h"

#include "ash/login/ui/login_test_base.h"
#include "ash/public/cpp/login_types.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

class LoginPasswordViewTest : public LoginTestBase {
 protected:
  LoginPasswordViewTest() = default;
  ~LoginPasswordViewTest() override = default;

  // LoginScreenTest:
  void SetUp() override {
    LoginTestBase::SetUp();

    view_ = new LoginPasswordView();
    view_->Init(base::Bind(&LoginPasswordViewTest::OnPasswordSubmit,
                           base::Unretained(this)),
                base::Bind(&LoginPasswordViewTest::OnPasswordTextChanged,
                           base::Unretained(this)),
                base::Bind(&LoginPasswordViewTest::OnEasyUnlockIconHovered,
                           base::Unretained(this)),
                base::Bind(&LoginPasswordViewTest::OnEasyUnlockIconTapped,
                           base::Unretained(this)));

    SetWidget(CreateWidgetWithContent(view_));
  }

  void OnPasswordSubmit(const base::string16& password) {
    password_ = password;
  }
  void OnPasswordTextChanged(bool is_empty) {
    is_password_field_empty_ = is_empty;
  }
  void OnEasyUnlockIconHovered() { easy_unlock_icon_hovered_called_ = true; }
  void OnEasyUnlockIconTapped() { easy_unlock_icon_tapped_called_ = true; }

  LoginPasswordView* view_ = nullptr;
  base::Optional<base::string16> password_;
  bool is_password_field_empty_ = true;
  bool easy_unlock_icon_hovered_called_ = false;
  bool easy_unlock_icon_tapped_called_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginPasswordViewTest);
};

}  // namespace

// Verifies that the submit button updates its UI state.
TEST_F(LoginPasswordViewTest, SubmitButtonUpdatesUiState) {
  LoginPasswordView::TestApi test_api(view_);
  ui::test::EventGenerator* generator = GetEventGenerator();

  // The submit button starts with the disabled state.
  EXPECT_TRUE(is_password_field_empty_);
  EXPECT_FALSE(test_api.submit_button()->GetEnabled());
  // Enter 'a'. The submit button is enabled.
  generator->PressKey(ui::KeyboardCode::VKEY_A, 0);
  EXPECT_FALSE(is_password_field_empty_);
  EXPECT_TRUE(test_api.submit_button()->GetEnabled());
  // Enter 'b'. The submit button stays enabled.
  generator->PressKey(ui::KeyboardCode::VKEY_B, 0);
  EXPECT_FALSE(is_password_field_empty_);
  EXPECT_TRUE(test_api.submit_button()->GetEnabled());

  // Clear password. The submit button is disabled.
  view_->Clear();
  EXPECT_TRUE(is_password_field_empty_);
  EXPECT_FALSE(test_api.submit_button()->GetEnabled());

  // Enter 'a'. The submit button is enabled.
  generator->PressKey(ui::KeyboardCode::VKEY_A, 0);
  EXPECT_FALSE(is_password_field_empty_);
  EXPECT_TRUE(test_api.submit_button()->GetEnabled());
  // Set the text field to be read-only. The submit button is disabled.
  view_->SetReadOnly(true);
  EXPECT_FALSE(is_password_field_empty_);
  EXPECT_FALSE(test_api.submit_button()->GetEnabled());
  // Set the text field to be not read-only. The submit button is enabled.
  view_->SetReadOnly(false);
  EXPECT_FALSE(is_password_field_empty_);
  EXPECT_TRUE(test_api.submit_button()->GetEnabled());
}

// Verifies that password submit works with 'Enter'.
TEST_F(LoginPasswordViewTest, PasswordSubmitIncludesPasswordText) {
  LoginPasswordView::TestApi test_api(view_);

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode::VKEY_A, 0);
  generator->PressKey(ui::KeyboardCode::VKEY_B, 0);
  generator->PressKey(ui::KeyboardCode::VKEY_C, 0);
  generator->PressKey(ui::KeyboardCode::VKEY_1, 0);
  generator->PressKey(ui::KeyboardCode::VKEY_RETURN, 0);

  ASSERT_TRUE(password_.has_value());
  EXPECT_EQ(base::ASCIIToUTF16("abc1"), *password_);
}

// Verifies that password submit works when clicking the submit button.
TEST_F(LoginPasswordViewTest, PasswordSubmitViaButton) {
  LoginPasswordView::TestApi test_api(view_);

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode::VKEY_A, 0);
  generator->PressKey(ui::KeyboardCode::VKEY_B, 0);
  generator->PressKey(ui::KeyboardCode::VKEY_C, 0);
  generator->PressKey(ui::KeyboardCode::VKEY_1, 0);
  generator->MoveMouseTo(
      test_api.submit_button()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();

  ASSERT_TRUE(password_.has_value());
  EXPECT_EQ(base::ASCIIToUTF16("abc1"), *password_);
}

// Verifies that text is not cleared after submitting a password.
TEST_F(LoginPasswordViewTest, PasswordSubmitClearsPassword) {
  LoginPasswordView::TestApi test_api(view_);
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Submit 'a' password.
  EXPECT_TRUE(is_password_field_empty_);
  generator->PressKey(ui::KeyboardCode::VKEY_A, 0);
  EXPECT_FALSE(is_password_field_empty_);
  generator->PressKey(ui::KeyboardCode::VKEY_RETURN, 0);
  EXPECT_FALSE(is_password_field_empty_);
  ASSERT_TRUE(password_.has_value());
  EXPECT_EQ(base::ASCIIToUTF16("a"), *password_);

  // Clear password.
  password_.reset();
  view_->Clear();
  EXPECT_TRUE(is_password_field_empty_);

  // Submit 'b' password.
  generator->PressKey(ui::KeyboardCode::VKEY_B, 0);
  EXPECT_FALSE(is_password_field_empty_);
  generator->PressKey(ui::KeyboardCode::VKEY_RETURN, 0);
  EXPECT_FALSE(is_password_field_empty_);
  ASSERT_TRUE(password_.has_value());
  // The submitted password is 'b' instead of "ab".
  EXPECT_EQ(base::ASCIIToUTF16("b"), *password_);
}

// Verifies that clicking the easy unlock icon fires the click event.
TEST_F(LoginPasswordViewTest, EasyUnlockClickFiresEvent) {
  LoginPasswordView::TestApi test_api(view_);
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Enable icon.
  view_->SetEasyUnlockIcon(EasyUnlockIconId::SPINNER,
                           base::string16() /*accessibility_label*/);
  ASSERT_TRUE(test_api.easy_unlock_icon()->GetVisible());

  // Click to the right of the icon, call is not generated.
  EXPECT_FALSE(easy_unlock_icon_tapped_called_);
  generator->MoveMouseTo(
      test_api.easy_unlock_icon()->GetBoundsInScreen().bottom_right() +
      gfx::Vector2d(2, 0));
  generator->ClickLeftButton();
  EXPECT_FALSE(easy_unlock_icon_tapped_called_);

  // Click the icon.
  EXPECT_FALSE(easy_unlock_icon_tapped_called_);
  generator->MoveMouseTo(
      test_api.easy_unlock_icon()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();
  EXPECT_TRUE(easy_unlock_icon_tapped_called_);

  // Icon was not hovered (since we did not enable immediate hover).
  EXPECT_FALSE(easy_unlock_icon_hovered_called_);
}

// Verifies that hovering the icon fires the hover event.
TEST_F(LoginPasswordViewTest, EasyUnlockMouseHover) {
  LoginPasswordView::TestApi test_api(view_);
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Enable icon, enable immediate hovering.
  view_->SetEasyUnlockIcon(EasyUnlockIconId::SPINNER,
                           base::string16() /*accessibility_label*/);
  test_api.set_immediately_hover_easy_unlock_icon();
  ASSERT_TRUE(test_api.easy_unlock_icon()->GetVisible());

  // Hover over the icon.
  EXPECT_FALSE(easy_unlock_icon_hovered_called_);
  generator->MoveMouseTo(
      test_api.easy_unlock_icon()->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(easy_unlock_icon_hovered_called_);

  // Icon was not tapped.
  EXPECT_FALSE(easy_unlock_icon_tapped_called_);
}

}  // namespace ash
