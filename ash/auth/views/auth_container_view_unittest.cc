// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/auth/views/auth_container_view.h"

#include <memory>
#include <optional>

#include "ash/auth/views/auth_input_row_view.h"
#include "ash/auth/views/fingerprint_view.h"
#include "ash/auth/views/pin_keyboard_view.h"
#include "ash/auth/views/test_support/mock_auth_container_view_observer.h"
#include "ash/public/cpp/login_types.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "base/containers/enum_set.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

class AuthContainerBaseUnitTest : public AshTestBase {
 public:
  explicit AuthContainerBaseUnitTest(AuthFactorSet auth_factors)
      : auth_factors_(auth_factors) {}
  AuthContainerBaseUnitTest(const AuthContainerBaseUnitTest&) = delete;
  AuthContainerBaseUnitTest& operator=(const AuthContainerBaseUnitTest&) =
      delete;
  ~AuthContainerBaseUnitTest() override = default;

 protected:
  void SetUp() override {
    AshTestBase::SetUp();

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    widget_->Show();

    container_view_ = widget_->SetContentsView(
        std::make_unique<AuthContainerView>(auth_factors_));
    test_api_ = std::make_unique<AuthContainerView::TestApi>(container_view_);
    test_api_pin_container_ = std::make_unique<PinContainerView::TestApi>(
        test_api_->GetPinContainerView());
    test_api_pin_keyboard_ = std::make_unique<PinKeyboardView::TestApi>(
        test_api_pin_container_->GetPinKeyboardView());
    test_api_pin_input_ = std::make_unique<AuthInputRowView::TestApi>(
        test_api_pin_container_->GetAuthInputRowView());

    test_api_password_ = std::make_unique<AuthInputRowView::TestApi>(
        test_api_->GetPasswordView());
    test_pin_status_ =
        std::make_unique<PinStatusView::TestApi>(test_api_->GetPinStatusView());

    mock_observer_ = std::make_unique<MockAuthContainerViewObserver>();
    container_view_->AddObserver(mock_observer_.get());

    CHECK(widget_->GetRootView());
  }

  void TearDown() override {
    test_api_pin_input_.reset();
    test_api_pin_keyboard_.reset();
    test_api_pin_container_.reset();
    test_api_password_.reset();
    test_pin_status_.reset();
    test_api_.reset();
    container_view_->RemoveObserver(mock_observer_.get());
    mock_observer_.reset();
    container_view_ = nullptr;
    widget_.reset();
    AshTestBase::TearDown();
  }

  const AuthFactorSet auth_factors_;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<MockAuthContainerViewObserver> mock_observer_;
  std::unique_ptr<AuthInputRowView::TestApi> test_api_pin_input_;
  std::unique_ptr<PinKeyboardView::TestApi> test_api_pin_keyboard_;
  std::unique_ptr<PinContainerView::TestApi> test_api_pin_container_;
  std::unique_ptr<AuthInputRowView::TestApi> test_api_password_;
  std::unique_ptr<PinStatusView::TestApi> test_pin_status_;
  std::unique_ptr<AuthContainerView::TestApi> test_api_;
  raw_ptr<AuthContainerView> container_view_ = nullptr;
};

class AuthContainerWithPasswordAndPinTest : public AuthContainerBaseUnitTest {
 public:
  AuthContainerWithPasswordAndPinTest()
      : AuthContainerBaseUnitTest(
            {AuthInputType::kPin, AuthInputType::kPassword}) {}
  AuthContainerWithPasswordAndPinTest(
      const AuthContainerWithPasswordAndPinTest&) = delete;
  AuthContainerWithPasswordAndPinTest& operator=(
      const AuthContainerWithPasswordAndPinTest&) = delete;
  ~AuthContainerWithPasswordAndPinTest() override = default;

 protected:
  void SetUp() override {
    AuthContainerBaseUnitTest::SetUp();

    // At start the password is visible and the pin is hidden.
    CHECK(test_api_password_->GetView()->GetVisible());
    CHECK(!test_api_pin_container_->GetView()->GetVisible());
    CHECK(test_api_->GetSwitchButton()->GetVisible());
  }
};

// Verify pin UI with key presses and submit.
TEST_F(AuthContainerWithPasswordAndPinTest, PinUITestWithPinPad) {
  EXPECT_THAT(test_api_->GetCurrentInputType(),
              testing::Optional(AuthInputType::kPassword));

  const std::u16string kPin(u"6893112");
  // The auth container content changes kPin times because of the input changes
  // + 1 times since at the beginning we switching from password to pin.
  EXPECT_CALL(*mock_observer_, OnContentsChanged()).Times(kPin.size() + 1);
  // Switch to the pin UI.
  LeftClickOn(test_api_->GetSwitchButton());

  views::test::RunScheduledLayout(widget_.get());

  EXPECT_TRUE(test_api_->GetCurrentInputType().has_value());
  EXPECT_EQ(test_api_->GetCurrentInputType(), AuthInputType::kPin);
  EXPECT_EQ(test_api_password_->GetView()->GetVisible(), false);
  EXPECT_TRUE(test_api_pin_container_->GetView()->GetVisible());
  EXPECT_TRUE(test_api_->GetSwitchButton()->GetVisible());
  EXPECT_TRUE(test_api_pin_keyboard_->GetEnabled());

  for (auto c : kPin) {
    LeftClickOn(test_api_pin_keyboard_->digit_button(c - u'0'));
  }

  EXPECT_EQ(test_api_pin_input_->GetTextfield()->GetText(), kPin);
  EXPECT_EQ(test_api_password_->GetTextfield()->GetText(), std::u16string());

  EXPECT_CALL(*mock_observer_, OnPinSubmit(kPin));
  // Click on Submit.
  LeftClickOn(test_api_pin_input_->GetSubmitButton());
}

// Verify pin UI and submit.
TEST_F(AuthContainerWithPasswordAndPinTest, PinUITestWithKeyPress) {
  EXPECT_TRUE(test_api_->GetCurrentInputType().has_value());
  EXPECT_EQ(test_api_->GetCurrentInputType(), AuthInputType::kPassword);

  const std::u16string kPin(u"6893112");
  // The auth container content changes kPin times because of the input changes
  // + 1 times since at the beginning we switching from password to pin.
  EXPECT_CALL(*mock_observer_, OnContentsChanged()).Times(kPin.size() + 1);
  // Switch to the pin UI.
  LeftClickOn(test_api_->GetSwitchButton());

  views::test::RunScheduledLayout(widget_.get());

  EXPECT_TRUE(test_api_->GetCurrentInputType().has_value());
  EXPECT_EQ(test_api_->GetCurrentInputType(), AuthInputType::kPin);
  EXPECT_EQ(test_api_password_->GetView()->GetVisible(), false);
  EXPECT_TRUE(test_api_pin_container_->GetView()->GetVisible());
  EXPECT_TRUE(test_api_->GetSwitchButton()->GetVisible());
  EXPECT_TRUE(test_api_pin_keyboard_->GetEnabled());

  container_view_->GetFocusManager()->SetFocusedView(
      test_api_pin_input_->GetTextfield());

  for (const char16_t c : kPin) {
    PressAndReleaseKey(ui::DomCodeToUsLayoutNonLocatedKeyboardCode(
        ui::UsLayoutDomKeyToDomCode(ui::DomKey::FromCharacter(c))));
  }
  EXPECT_EQ(test_api_pin_input_->GetTextfield()->GetText(), kPin);
  EXPECT_EQ(test_api_password_->GetTextfield()->GetText(), std::u16string());

  EXPECT_CALL(*mock_observer_, OnPinSubmit(kPin));
  // Click on Submit.
  LeftClickOn(test_api_pin_input_->GetSubmitButton());
}

// Verify switch button is not operate with disabled input.
TEST_F(AuthContainerWithPasswordAndPinTest, DisabledSwitchTest) {
  EXPECT_TRUE(test_api_->GetCurrentInputType().has_value());
  EXPECT_EQ(test_api_->GetCurrentInputType(), AuthInputType::kPassword);
  container_view_->SetInputEnabled(false);
  // The auth container content changes two times since at the two press the
  // toggle.
  EXPECT_CALL(*mock_observer_, OnContentsChanged()).Times(0);
  // First click on the switch button.
  LeftClickOn(test_api_->GetSwitchButton());

  views::test::RunScheduledLayout(widget_.get());

  EXPECT_TRUE(test_api_->GetCurrentInputType().has_value());
  EXPECT_EQ(test_api_->GetCurrentInputType(), AuthInputType::kPassword);
}

// Verify double switch button press shows password UI.
TEST_F(AuthContainerWithPasswordAndPinTest, DoubleSwitchTest) {
  EXPECT_TRUE(test_api_->GetCurrentInputType().has_value());
  EXPECT_EQ(test_api_->GetCurrentInputType(), AuthInputType::kPassword);
  // The auth container content changes two times since at the two press the
  // toggle.
  EXPECT_CALL(*mock_observer_, OnContentsChanged()).Times(2);
  // First click on the switch button.
  LeftClickOn(test_api_->GetSwitchButton());

  views::test::RunScheduledLayout(widget_.get());

  EXPECT_TRUE(test_api_->GetCurrentInputType().has_value());
  EXPECT_EQ(test_api_->GetCurrentInputType(), AuthInputType::kPin);
  EXPECT_EQ(test_api_password_->GetView()->GetVisible(), false);
  EXPECT_TRUE(test_api_pin_container_->GetView()->GetVisible());
  EXPECT_TRUE(test_api_->GetSwitchButton()->GetVisible());
  EXPECT_TRUE(test_api_pin_keyboard_->GetEnabled());

  // Second click on the switch button.
  LeftClickOn(test_api_->GetSwitchButton());

  views::test::RunScheduledLayout(widget_.get());

  EXPECT_TRUE(test_api_->GetCurrentInputType().has_value());
  EXPECT_EQ(test_api_->GetCurrentInputType(), AuthInputType::kPassword);
  EXPECT_TRUE(test_api_password_->GetView()->GetVisible());
  EXPECT_EQ(test_api_pin_container_->GetView()->GetVisible(), false);
  EXPECT_TRUE(test_api_->GetSwitchButton()->GetVisible());
}

TEST_F(AuthContainerWithPasswordAndPinTest, PasswordSubmitTest) {
  const std::u16string kPassword(u"password");
  container_view_->GetFocusManager()->SetFocusedView(
      test_api_password_->GetTextfield());
  for (const char16_t c : kPassword) {
    PressAndReleaseKey(ui::DomCodeToUsLayoutNonLocatedKeyboardCode(
        ui::UsLayoutDomKeyToDomCode(ui::DomKey::FromCharacter(c))));
  }
  EXPECT_EQ(test_api_pin_input_->GetTextfield()->GetText(), std::u16string());
  EXPECT_EQ(test_api_password_->GetTextfield()->GetText(), kPassword);

  EXPECT_CALL(*mock_observer_, OnPasswordSubmit(kPassword));
  // Click on Submit.
  LeftClickOn(test_api_password_->GetSubmitButton());
}

// Verify password is not functioning with disabled input area.
TEST_F(AuthContainerWithPasswordAndPinTest, DisabledPasswordSubmitTest) {
  container_view_->SetInputEnabled(false);
  const std::u16string kPassword(u"password");
  container_view_->GetFocusManager()->SetFocusedView(
      test_api_password_->GetTextfield());
  for (const char16_t c : kPassword) {
    PressAndReleaseKey(ui::DomCodeToUsLayoutNonLocatedKeyboardCode(
        ui::UsLayoutDomKeyToDomCode(ui::DomKey::FromCharacter(c))));
  }
  EXPECT_EQ(test_api_pin_input_->GetTextfield()->GetText(), std::u16string());
  EXPECT_EQ(test_api_password_->GetTextfield()->GetText(), std::u16string());

  EXPECT_CALL(*mock_observer_, OnPasswordSubmit(std::u16string())).Times(0);
  // Click on Submit.
  LeftClickOn(test_api_password_->GetSubmitButton());
}

// Verify the UI after turning off the password factor.
TEST_F(AuthContainerWithPasswordAndPinTest, PinOnlyTest) {
  // Turn off the password factor availability.
  EXPECT_TRUE(test_api_->GetView()->HasPassword());
  EXPECT_TRUE(test_api_->GetView()->HasPin());
  test_api_->GetView()->SetHasPassword(false);
  EXPECT_EQ(test_api_->GetView()->HasPassword(), false);

  views::test::RunScheduledLayout(widget_.get());

  EXPECT_EQ(test_api_password_->GetView()->GetVisible(), false);
  EXPECT_TRUE(test_api_pin_container_->GetView()->GetVisible());
  EXPECT_TRUE(test_api_pin_keyboard_->GetEnabled());
  EXPECT_EQ(test_api_->GetSwitchButton()->GetVisible(), false);
}

// Verify the UI after turning off the pin factor.
TEST_F(AuthContainerWithPasswordAndPinTest, PasswordOnlyTest) {
  // Turn off the password factor availability.
  EXPECT_TRUE(test_api_->GetView()->HasPassword());
  EXPECT_TRUE(test_api_->GetView()->HasPin());
  test_api_->GetView()->SetHasPin(false);
  EXPECT_EQ(test_api_->GetView()->HasPin(), false);

  views::test::RunScheduledLayout(widget_.get());

  EXPECT_TRUE(test_api_password_->GetView()->GetVisible());
  EXPECT_EQ(test_api_pin_container_->GetView()->GetVisible(), false);
  EXPECT_EQ(test_api_->GetSwitchButton()->GetVisible(), false);
}

// Verify the ResetInputfields functionality.
TEST_F(AuthContainerWithPasswordAndPinTest, ResetInputfieldsTest) {
  test_api_password_->GetTextfield()->SetText(u"password");
  test_api_pin_input_->GetTextfield()->SetText(u"pin");
  test_api_->GetView()->ResetInputfields();

  EXPECT_EQ(test_api_password_->GetTextfield()->GetText(), std::u16string());
  EXPECT_EQ(test_api_pin_input_->GetTextfield()->GetText(), std::u16string());
}

// Verify the ResetInputfields functionality.
TEST_F(AuthContainerWithPasswordAndPinTest, ResetInputfieldsWithSwitchTest) {
  test_api_password_->GetTextfield()->SetText(u"password");
  test_api_pin_input_->GetTextfield()->SetText(u"pin");
  LeftClickOn(test_api_->GetSwitchButton());

  EXPECT_EQ(test_api_password_->GetTextfield()->GetText(), std::u16string());
  EXPECT_EQ(test_api_pin_input_->GetTextfield()->GetText(), std::u16string());
}

TEST_F(AuthContainerWithPasswordAndPinTest, SetPinStatusTest) {
  const std::u16string status_message = u"Too many PIN attempts";

  cryptohome::PinStatus pin_status(base::TimeDelta::Max());

  test_api_->GetView()->SetPinStatus(
      std::make_unique<cryptohome::PinStatus>(pin_status));

  EXPECT_EQ(test_pin_status_->GetCurrentText(), status_message);
  EXPECT_TRUE(test_pin_status_->GetView()->GetVisible());

  // Now set the status back to an empty string.
  test_api_->GetView()->SetPinStatus(nullptr);
  EXPECT_FALSE(test_pin_status_->GetView()->GetVisible());
}

// Verify the fingerprint view visibility.
TEST_F(AuthContainerWithPasswordAndPinTest, FingerprintTest) {
  FingerprintView* fp_view = test_api_->GetFingerprintView();
  FingerprintView::TestApi test_fp_view(fp_view);

  EXPECT_FALSE(fp_view->GetVisible());
  EXPECT_EQ(test_fp_view.GetState(), FingerprintState::UNAVAILABLE);

  // Turn on the fingerprint factor availability.
  container_view_->SetFingerprintState(FingerprintState::AVAILABLE_DEFAULT);
  EXPECT_TRUE(fp_view->GetVisible());
  EXPECT_EQ(test_fp_view.GetState(), FingerprintState::AVAILABLE_DEFAULT);

  // Turn off the fingerprint factor availability.
  container_view_->SetFingerprintState(FingerprintState::UNAVAILABLE);
  EXPECT_FALSE(fp_view->GetVisible());
  EXPECT_EQ(test_fp_view.GetState(), FingerprintState::UNAVAILABLE);
}

TEST_F(AuthContainerWithPasswordAndPinTest, AccessibleProperties) {
  ui::AXNodeData node_data;
  container_view_->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kInvisible));
}

class AuthContainerWithPinTest : public AuthContainerBaseUnitTest {
 public:
  AuthContainerWithPinTest()
      : AuthContainerBaseUnitTest({AuthInputType::kPin}) {}
  AuthContainerWithPinTest(const AuthContainerWithPinTest&) = delete;
  AuthContainerWithPinTest& operator=(const AuthContainerWithPinTest&) = delete;
  ~AuthContainerWithPinTest() override = default;

 protected:
  void SetUp() override {
    AuthContainerBaseUnitTest::SetUp();

    // At start the password is hidden and the pin is visible.
    CHECK(!test_api_password_->GetView()->GetVisible());
    CHECK(test_api_pin_container_->GetView()->GetVisible());
    CHECK(!test_api_->GetSwitchButton()->GetVisible());
  }
};

TEST_F(AuthContainerWithPinTest, PinSubmitTest) {
  const std::u16string kPin(u"6893112");
  // The auth container content changes kPin times because of the input changes
  EXPECT_CALL(*mock_observer_, OnContentsChanged()).Times(kPin.size());

  EXPECT_TRUE(test_api_->GetCurrentInputType().has_value());
  EXPECT_EQ(test_api_->GetCurrentInputType(), AuthInputType::kPin);
  EXPECT_TRUE(test_api_pin_keyboard_->GetEnabled());

  for (auto c : kPin) {
    LeftClickOn(test_api_pin_keyboard_->digit_button(c - u'0'));
  }

  EXPECT_EQ(test_api_pin_input_->GetTextfield()->GetText(), kPin);
  EXPECT_EQ(test_api_password_->GetTextfield()->GetText(), std::u16string());

  EXPECT_CALL(*mock_observer_, OnPinSubmit(kPin));
  // Click on Submit.
  LeftClickOn(test_api_pin_input_->GetSubmitButton());
}

TEST_F(AuthContainerWithPinTest, LockoutTest) {
  const std::u16string status_message = u"Too many PIN attempts";

  cryptohome::PinStatus pin_status(base::TimeDelta::Max());

  test_api_->GetView()->SetPinStatus(
      std::make_unique<cryptohome::PinStatus>(pin_status));

  EXPECT_EQ(test_pin_status_->GetCurrentText(), status_message);
  EXPECT_TRUE(test_pin_status_->GetView()->GetVisible());

  CHECK(!test_api_password_->GetView()->GetVisible());
  CHECK(!test_api_pin_container_->GetView()->GetVisible());

  // Now set the status back to an empty string.
  test_api_->GetView()->SetPinStatus(nullptr);
  EXPECT_FALSE(test_pin_status_->GetView()->GetVisible());
}

class AuthContainerWithLockedOutPinTest : public AuthContainerBaseUnitTest {
 public:
  AuthContainerWithLockedOutPinTest() : AuthContainerBaseUnitTest({}) {}
  AuthContainerWithLockedOutPinTest(const AuthContainerWithLockedOutPinTest&) =
      delete;
  AuthContainerWithLockedOutPinTest& operator=(
      const AuthContainerWithLockedOutPinTest&) = delete;
  ~AuthContainerWithLockedOutPinTest() override = default;

 protected:
  void SetUp() override {
    AuthContainerBaseUnitTest::SetUp();

    // At start both the password and the pin are hidden.
    CHECK(!test_api_password_->GetView()->GetVisible());
    CHECK(!test_api_pin_container_->GetView()->GetVisible());
    CHECK(!test_api_->GetSwitchButton()->GetVisible());
  }
};

TEST_F(AuthContainerWithLockedOutPinTest, LockoutReopenTest) {
  const std::u16string status_message = u"Too many PIN attempts";

  cryptohome::PinStatus pin_status(base::TimeDelta::Max());

  test_api_->GetView()->SetPinStatus(
      std::make_unique<cryptohome::PinStatus>(pin_status));

  EXPECT_EQ(test_pin_status_->GetCurrentText(), status_message);
  EXPECT_TRUE(test_pin_status_->GetView()->GetVisible());

  CHECK(!test_api_password_->GetView()->GetVisible());
  CHECK(!test_api_pin_container_->GetView()->GetVisible());

  // Now set the status back to an empty string.
  test_api_->GetView()->SetPinStatus(nullptr);
  EXPECT_FALSE(test_pin_status_->GetView()->GetVisible());
}

}  // namespace

}  // namespace ash
