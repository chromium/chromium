// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_password_view.h"

#include <memory>

#include "ash/login/ui/login_arrow_navigation_delegate.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/public/cpp/login_types.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr base::TimeDelta kClearPasswordAfterDelay = base::Seconds(30);

constexpr base::TimeDelta kHidePasswordAfterDelay = base::Seconds(5);

class LoginPasswordViewTest : public LoginTestBase {
 public:
  LoginPasswordViewTest(const LoginPasswordViewTest&) = delete;
  LoginPasswordViewTest& operator=(const LoginPasswordViewTest&) = delete;

 protected:
  LoginPasswordViewTest() = default;
  ~LoginPasswordViewTest() override = default;

  // LoginScreenTest:
  void SetUp() override {
    LoginTestBase::SetUp();

    view_ = new LoginPasswordView();
    arrow_navigation_delegate_ =
        std::make_unique<LoginScreenArrowNavigationDelegate>();
    view_->SetLoginArrowNavigationDelegate(arrow_navigation_delegate_.get());
    // Focusable views are expected to have accessible names in order to pass
    // the accessibility paint checks.
    view_->SetAccessibleNameOnTextfield(u"Password");
    view_->Init(
        base::BindRepeating(&LoginPasswordViewTest::OnPasswordSubmit,
                            base::Unretained(this)),
        base::BindRepeating(&LoginPasswordViewTest::OnPasswordTextChanged,
                            base::Unretained(this)));

    SetWidget(CreateWidgetWithContent(view_));
  }

  void OnPasswordSubmit(const std::u16string& password) {
    password_ = password;
  }
  void OnPasswordTextChanged(bool is_empty) {
    is_password_field_empty_ = is_empty;
  }

  raw_ptr<LoginPasswordView, DanglingUntriaged> view_ = nullptr;
  std::optional<std::u16string> password_;
  bool is_password_field_empty_ = true;
  std::unique_ptr<LoginScreenArrowNavigationDelegate>
      arrow_navigation_delegate_;
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
  view_->Reset();
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

// Verifies that the display password button updates its UI state.
TEST_F(LoginPasswordViewTest, DisplayPasswordButtonUpdatesUiState) {
  LoginPasswordView::TestApi test_api(view_);
  ui::test::EventGenerator* generator = GetEventGenerator();

  // The display password button is not toggled by default and the password is
  // not visible.
  EXPECT_FALSE(test_api.display_password_button()->GetToggled());
  EXPECT_EQ(test_api.textfield()->GetTextInputType(),
            ui::TEXT_INPUT_TYPE_PASSWORD);

  // Click the display password button. This should not work as long as the
  // password textfield is empty.
  generator->MoveMouseTo(
      test_api.display_password_button()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();
  EXPECT_FALSE(test_api.display_password_button()->GetToggled());
  EXPECT_EQ(test_api.textfield()->GetTextInputType(),
            ui::TEXT_INPUT_TYPE_PASSWORD);

  generator->PressKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  // Click the display password button. The password should be visible.
  generator->MoveMouseTo(
      test_api.display_password_button()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();
  EXPECT_TRUE(test_api.display_password_button()->GetToggled());
  EXPECT_EQ(test_api.textfield()->GetTextInputType(), ui::TEXT_INPUT_TYPE_NULL);

  // Click the display password button again. The password should be hidden.
  generator->MoveMouseTo(
      test_api.display_password_button()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();
  EXPECT_FALSE(test_api.display_password_button()->GetToggled());
  EXPECT_EQ(test_api.textfield()->GetTextInputType(),
            ui::TEXT_INPUT_TYPE_PASSWORD);
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
  EXPECT_EQ(u"abc1", *password_);

  // Expect the password field to be read only after submitting.
  EXPECT_EQ(test_api.textfield()->GetReadOnly(), true);
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
  EXPECT_EQ(u"abc1", *password_);

  // Expect the password field to be read only after submitting.
  EXPECT_EQ(test_api.textfield()->GetReadOnly(), true);
}

// Verifies that pressing 'Return' on an empty password has no effect.
TEST_F(LoginPasswordViewTest, PressingReturnHasNoEffect) {
  LoginPasswordView::TestApi test_api(view_);

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);
  ASSERT_FALSE(password_.has_value());
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
  EXPECT_EQ(u"a", *password_);

  // Clear password.
  password_.reset();
  view_->Reset();
  view_->SetReadOnly(false);
  EXPECT_TRUE(is_password_field_empty_);

  // Submit 'b' password.
  generator->PressKey(ui::KeyboardCode::VKEY_B, 0);
  EXPECT_FALSE(is_password_field_empty_);
  generator->PressKey(ui::KeyboardCode::VKEY_RETURN, 0);
  EXPECT_FALSE(is_password_field_empty_);
  ASSERT_TRUE(password_.has_value());
  // The submitted password is 'b' instead of "ab".
  EXPECT_EQ(u"b", *password_);
}

// Checks that the user can't hit Ctrl+Z to revert the password when it has been
// cleared.
TEST_F(LoginPasswordViewTest, CtrlZDisabled) {
  LoginPasswordView::TestApi test_api(view_);
  ui::test::EventGenerator* generator = GetEventGenerator();

  generator->PressKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  EXPECT_FALSE(is_password_field_empty_);
  view_->Reset();
  EXPECT_TRUE(is_password_field_empty_);
  generator->PressKey(ui::KeyboardCode::VKEY_Z, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(is_password_field_empty_);
}

// Verifies that the password textfield clears after a delay when the display
// password button is shown.
TEST_F(LoginPasswordViewTest, PasswordAutoClearsAndHides) {
  LoginPasswordView::TestApi test_api(view_);
  ui::test::EventGenerator* generator = GetEventGenerator();

  view_->SetDisplayPasswordButtonVisible(true);

  // Verify clearing timer works.
  generator->PressKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  EXPECT_FALSE(is_password_field_empty_);

  task_environment()->FastForwardBy(kClearPasswordAfterDelay);
  EXPECT_TRUE(is_password_field_empty_);

  // Check a second time.
  generator->PressKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  EXPECT_FALSE(is_password_field_empty_);
  task_environment()->FastForwardBy(kClearPasswordAfterDelay);
  EXPECT_TRUE(is_password_field_empty_);

  // Verify hiding timer works; set the password visible first then wait for
  // the hiding timer to trigger and check it is hidden.
  generator->PressKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(test_api.textfield()->GetTextInputType(),
            ui::TEXT_INPUT_TYPE_PASSWORD);
  generator->MoveMouseTo(
      test_api.display_password_button()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();
  EXPECT_EQ(test_api.textfield()->GetTextInputType(), ui::TEXT_INPUT_TYPE_NULL);
  task_environment()->FastForwardBy(kHidePasswordAfterDelay);
  EXPECT_EQ(test_api.textfield()->GetTextInputType(),
            ui::TEXT_INPUT_TYPE_PASSWORD);
  // Hide an empty password already hidden and make sure a second trigger works.
  task_environment()->FastForwardBy(kHidePasswordAfterDelay);
  EXPECT_EQ(test_api.textfield()->GetTextInputType(),
            ui::TEXT_INPUT_TYPE_PASSWORD);
}

// Verifies that the password textfield remains in the same visibility state
// when the content changes.
TEST_F(LoginPasswordViewTest, ContentChangesDoNotImpactPasswordVisibility) {
  LoginPasswordView::TestApi test_api(view_);
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Type to enable the display password button and click on it to display the
  // password.
  EXPECT_EQ(test_api.textfield()->GetTextInputType(),
            ui::TEXT_INPUT_TYPE_PASSWORD);
  generator->PressKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  generator->MoveMouseTo(
      test_api.display_password_button()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();
  EXPECT_EQ(test_api.textfield()->GetTextInputType(), ui::TEXT_INPUT_TYPE_NULL);

  // Type manually and programmatically, and check if the password textfield
  // remains visible.
  generator->PressKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(test_api.textfield()->GetTextInputType(), ui::TEXT_INPUT_TYPE_NULL);
  test_api.textfield()->InsertText(
      u"test",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_EQ(test_api.textfield()->GetTextInputType(), ui::TEXT_INPUT_TYPE_NULL);

  // Click again on the display password button to hide the password.
  generator->MoveMouseTo(
      test_api.display_password_button()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();
  EXPECT_EQ(test_api.textfield()->GetTextInputType(),
            ui::TEXT_INPUT_TYPE_PASSWORD);

  // Type manually and programmatically, and check if the password textfield
  // remains invisible.
  test_api.textfield()->InsertText(
      u"test",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_EQ(test_api.textfield()->GetTextInputType(),
            ui::TEXT_INPUT_TYPE_PASSWORD);
}

// Checks that the display password button is disabled when the textfield is
// empty and enabled when it is not.
TEST_F(LoginPasswordViewTest,
       DisplayPasswordButonIsEnabledIFFTextfieldIsNotEmpty) {
  LoginPasswordView::TestApi test_api(view_);
  ui::test::EventGenerator* generator = GetEventGenerator();

  EXPECT_TRUE(is_password_field_empty_);
  EXPECT_FALSE(test_api.display_password_button()->GetEnabled());

  generator->PressKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  EXPECT_FALSE(is_password_field_empty_);
  EXPECT_TRUE(test_api.display_password_button()->GetEnabled());
  generator->PressKey(ui::KeyboardCode::VKEY_BACK, ui::EF_NONE);
  EXPECT_TRUE(is_password_field_empty_);
  EXPECT_FALSE(test_api.display_password_button()->GetEnabled());

  test_api.textfield()->InsertText(
      u"test",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  EXPECT_FALSE(is_password_field_empty_);
  EXPECT_TRUE(test_api.display_password_button()->GetEnabled());
  view_->Reset();
  EXPECT_TRUE(is_password_field_empty_);
  EXPECT_FALSE(test_api.display_password_button()->GetEnabled());

  generator->PressKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  EXPECT_FALSE(is_password_field_empty_);
  EXPECT_TRUE(test_api.display_password_button()->GetEnabled());
  test_api.textfield()->SelectAll(false /* reversed */);
  generator->PressKey(ui::KeyboardCode::VKEY_DELETE, ui::EF_NONE);
  EXPECT_TRUE(is_password_field_empty_);
  EXPECT_FALSE(test_api.display_password_button()->GetEnabled());
}

// Verifies that focus returned to the textfield after InsertNumber is called.
TEST_F(LoginPasswordViewTest, FocusReturn) {
  LoginPasswordView::TestApi test_api(view_);
  ui::test::EventGenerator* generator = GetEventGenerator();
  // Verify that focus is returned to view after the number insertion.
  view_->InsertNumber(0);
  EXPECT_TRUE(test_api.textfield()->HasFocus());
  // Focus on the next element to check that following focus return will not
  // delete what was already inserted into textfield.
  generator->PressKey(ui::KeyboardCode::VKEY_TAB, ui::EF_NONE);
  EXPECT_FALSE(test_api.textfield()->HasFocus());
  view_->InsertNumber(1);
  EXPECT_TRUE(test_api.textfield()->HasFocus());
  EXPECT_EQ(test_api.textfield()->GetText().length(), 2u);
}

// Verifies that the display password button state does not interact badly with
// the clearing timer.
TEST_F(LoginPasswordViewTest, MakePasswordVisibleJustBeforeClearing) {
  LoginPasswordView::TestApi test_api(view_);
  ui::test::EventGenerator* generator = GetEventGenerator();

  view_->SetDisplayPasswordButtonVisible(true);

  // Enable the clearing timer by typing something.
  generator->PressKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  EXPECT_FALSE(is_password_field_empty_);

  // Click on the display password button just before the password field gets
  // cleared.
  auto small_delay = kHidePasswordAfterDelay / 3;
  task_environment()->FastForwardBy(kClearPasswordAfterDelay - small_delay);
  generator->MoveMouseTo(
      test_api.display_password_button()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();
  EXPECT_EQ(test_api.textfield()->GetTextInputType(), ui::TEXT_INPUT_TYPE_NULL);

  // Once the clearing delay has passed, the password should get cleared. In
  // such a case, the input type should be password and the display password
  // button should be disabled.
  task_environment()->FastForwardBy(small_delay);
  EXPECT_TRUE(is_password_field_empty_);
  EXPECT_EQ(test_api.textfield()->GetTextInputType(),
            ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_FALSE(test_api.display_password_button()->GetEnabled());

  // The situation should remain the same once the hide password delay has
  // passed. Especially since a password reset should reset the hiding timer.
  task_environment()->FastForwardBy(small_delay);
  EXPECT_TRUE(is_password_field_empty_);
  EXPECT_EQ(test_api.textfield()->GetTextInputType(),
            ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_FALSE(test_api.display_password_button()->GetEnabled());
}

}  // namespace ash
