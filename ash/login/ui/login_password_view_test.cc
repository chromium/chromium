// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_password_view.h"

#include "ash/login/ui/login_palette.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/public/cpp/login_types.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/mock_timer.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/textfield/textfield.h"
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

    view_ = new LoginPasswordView(CreateDefaultLoginPalette());
    view_->Init(
        base::BindRepeating(&LoginPasswordViewTest::OnPasswordSubmit,
                            base::Unretained(this)),
        base::BindRepeating(&LoginPasswordViewTest::OnPasswordTextChanged,
                            base::Unretained(this)),
        base::BindRepeating(&LoginPasswordViewTest::OnEasyUnlockIconHovered,
                            base::Unretained(this)),
        base::BindRepeating(&LoginPasswordViewTest::OnEasyUnlockIconTapped,
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

// LoginPasswordViewTest with display password button feature enabled.
class LoginPasswordViewTestFeatureEnabled : public LoginPasswordViewTest {
 protected:
  LoginPasswordViewTestFeatureEnabled() {
    feature_list_.InitWithFeatures(
        {chromeos::features::kLoginDisplayPasswordButton}, {});
  }
  LoginPasswordViewTestFeatureEnabled(
      const LoginPasswordViewTestFeatureEnabled&) = delete;
  LoginPasswordViewTestFeatureEnabled& operator=(
      const LoginPasswordViewTestFeatureEnabled&) = delete;
  ~LoginPasswordViewTestFeatureEnabled() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
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

// Verifies that the display password button updates its UI state.
TEST_F(LoginPasswordViewTestFeatureEnabled,
       DisplayPasswordButtonUpdatesUiState) {
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
  EXPECT_EQ(base::ASCIIToUTF16("abc1"), *password_);

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
  EXPECT_EQ(base::ASCIIToUTF16("abc1"), *password_);

  // Expect the password field to be read only after submitting.
  EXPECT_EQ(test_api.textfield()->GetReadOnly(), true);
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
  view_->SetReadOnly(false);
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

// Checks that the user can't hit Ctrl+Z to revert the password when it has been
// cleared.
TEST_F(LoginPasswordViewTest, CtrlZDisabled) {
  LoginPasswordView::TestApi test_api(view_);
  ui::test::EventGenerator* generator = GetEventGenerator();

  generator->PressKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  EXPECT_FALSE(is_password_field_empty_);
  view_->Clear();
  EXPECT_TRUE(is_password_field_empty_);
  generator->PressKey(ui::KeyboardCode::VKEY_Z, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(is_password_field_empty_);
}

// Verifies that the password textfield clears after a delay when the display
// password button is shown.
TEST_F(LoginPasswordViewTestFeatureEnabled, PasswordAutoClearsAndHides) {
  LoginPasswordView::TestApi test_api(view_);
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Install mock timers into the password view.
  auto clear_timer0 = std::make_unique<base::MockRetainingOneShotTimer>();
  auto hide_timer0 = std::make_unique<base::MockRetainingOneShotTimer>();
  base::MockRetainingOneShotTimer* clear_timer = clear_timer0.get();
  base::MockRetainingOneShotTimer* hide_timer = hide_timer0.get();
  test_api.SetTimers(std::move(clear_timer0), std::move(hide_timer0));

  // Verify clearing timer works.
  generator->PressKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  EXPECT_FALSE(is_password_field_empty_);

  clear_timer->Fire();
  EXPECT_TRUE(is_password_field_empty_);

  // Check a second time.
  generator->PressKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  EXPECT_FALSE(is_password_field_empty_);
  clear_timer->Fire();
  EXPECT_TRUE(is_password_field_empty_);

  // Verify hiding timer works; set the password visible first then fire the
  // hiding timer and check it is hidden.
  generator->PressKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(test_api.textfield()->GetTextInputType(),
            ui::TEXT_INPUT_TYPE_PASSWORD);
  generator->MoveMouseTo(
      test_api.display_password_button()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();
  EXPECT_EQ(test_api.textfield()->GetTextInputType(), ui::TEXT_INPUT_TYPE_NULL);
  hide_timer->Fire();
  EXPECT_EQ(test_api.textfield()->GetTextInputType(),
            ui::TEXT_INPUT_TYPE_PASSWORD);
  // Hide an empty password already hidden and make sure a second fire works.
  hide_timer->Fire();
  EXPECT_EQ(test_api.textfield()->GetTextInputType(),
            ui::TEXT_INPUT_TYPE_PASSWORD);
}

// Verifies that the password textfield remains in the same visibility state
// when the content changes.
TEST_F(LoginPasswordViewTestFeatureEnabled,
       ContentChangesDoNotImpactPasswordVisibility) {
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
  test_api.textfield()->InsertText(base::ASCIIToUTF16("test"));
  EXPECT_EQ(test_api.textfield()->GetTextInputType(), ui::TEXT_INPUT_TYPE_NULL);

  // Click again on the display password button to hide the password.
  generator->MoveMouseTo(
      test_api.display_password_button()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();
  EXPECT_EQ(test_api.textfield()->GetTextInputType(),
            ui::TEXT_INPUT_TYPE_PASSWORD);

  // Type manually and programmatically, and check if the password textfield
  // remains invisible.
  test_api.textfield()->InsertText(base::ASCIIToUTF16("test"));
  EXPECT_EQ(test_api.textfield()->GetTextInputType(),
            ui::TEXT_INPUT_TYPE_PASSWORD);
}

// Checks that the display password button is disabled when the textfield is
// empty and enabled when it is not.
TEST_F(LoginPasswordViewTestFeatureEnabled,
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

  test_api.textfield()->InsertText(base::ASCIIToUTF16("test"));
  EXPECT_FALSE(is_password_field_empty_);
  EXPECT_TRUE(test_api.display_password_button()->GetEnabled());
  view_->Clear();
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
TEST_F(LoginPasswordViewTestFeatureEnabled, FocusReturn) {
  LoginPasswordView::TestApi test_api(view_);
  ui::test::EventGenerator* generator = GetEventGenerator();
  // Verify that focus is returned to view after the number insertion.
  view_->InsertNumber(0);
  EXPECT_TRUE(test_api.textfield()->HasFocus());
  // Focus on the next element to check that following focus return will not
  // delete what was already inserted into textfield.
  generator->PressKey(ui::KeyboardCode::VKEY_TAB, ui::EventFlags::EF_NONE);
  EXPECT_FALSE(test_api.textfield()->HasFocus());
  view_->InsertNumber(1);
  EXPECT_TRUE(test_api.textfield()->HasFocus());
  EXPECT_EQ(test_api.textfield()->GetText().length(), 2u);
}

}  // namespace ash
