// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/auth/views/active_session_auth_view.h"

#include <memory>
#include <optional>

#include "ash/auth/views/auth_container_view.h"
#include "ash/auth/views/auth_header_view.h"
#include "ash/auth/views/auth_input_row_view.h"
#include "ash/auth/views/pin_keyboard_view.h"
#include "ash/auth/views/test_support/mock_active_session_auth_view_observer.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "base/containers/enum_set.h"
#include "base/time/time.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

const char kTestAccount[] = "user@test.com";
const std::u16string title = u"title";
const std::u16string description = u"description";

class ActiveSessionAuthViewUnitTest : public AshTestBase {
 public:
  ActiveSessionAuthViewUnitTest() = default;
  ActiveSessionAuthViewUnitTest(const ActiveSessionAuthViewUnitTest&) = delete;
  ActiveSessionAuthViewUnitTest& operator=(
      const ActiveSessionAuthViewUnitTest&) = delete;
  ~ActiveSessionAuthViewUnitTest() override = default;

 protected:
  void SetUp() override {
    AshTestBase::SetUp();

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    widget_->Show();

    AccountId account_id = AccountId::FromUserEmail(kTestAccount);
    auto fake_user_manager = std::make_unique<user_manager::FakeUserManager>();
    fake_user_manager->AddUser(account_id);
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));

    container_view_ =
        widget_->SetContentsView(std::make_unique<ActiveSessionAuthView>(
            account_id, title, description,
            AuthFactorSet{AuthInputType::kPassword, AuthInputType::kPin}));
    test_api_ =
        std::make_unique<ActiveSessionAuthView::TestApi>(container_view_);

    // Going through the components of the container view and initialized their
    // test api's variables.
    test_api_header_ = std::make_unique<AuthHeaderView::TestApi>(
        test_api_->GetAuthHeaderView());
    test_api_auth_container_ = std::make_unique<AuthContainerView::TestApi>(
        test_api_->GetAuthContainerView());
    close_button_ = test_api_->GetCloseButton();

    test_api_pin_container_ = std::make_unique<PinContainerView::TestApi>(
        test_api_auth_container_->GetPinContainerView());
    test_api_pin_keyboard_ = std::make_unique<PinKeyboardView::TestApi>(
        test_api_pin_container_->GetPinKeyboardView());
    test_api_pin_input_ = std::make_unique<AuthInputRowView::TestApi>(
        test_api_pin_container_->GetAuthInputRowView());

    test_api_password_ = std::make_unique<AuthInputRowView::TestApi>(
        test_api_auth_container_->GetPasswordView());
    test_pin_status_ = std::make_unique<PinStatusView::TestApi>(
        test_api_auth_container_->GetPinStatusView());

    mock_observer_ = std::make_unique<MockActiveSessionAuthViewObserver>();
    container_view_->AddObserver(mock_observer_.get());

    CHECK(close_button_->GetVisible());
    CHECK(test_api_header_->GetView()->GetVisible());
    CHECK(test_api_auth_container_->GetView()->GetVisible());
  }

  void TearDown() override {
    test_api_pin_input_.reset();
    test_api_pin_keyboard_.reset();
    test_api_pin_container_.reset();
    test_api_password_.reset();
    test_api_auth_container_.reset();
    test_api_header_.reset();
    test_pin_status_.reset();
    close_button_ = nullptr;
    test_api_.reset();
    container_view_->RemoveObserver(mock_observer_.get());
    mock_observer_.reset();
    container_view_ = nullptr;
    widget_.reset();
    scoped_user_manager_.reset();
    AshTestBase::TearDown();
  }

  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<MockActiveSessionAuthViewObserver> mock_observer_;
  std::unique_ptr<AuthInputRowView::TestApi> test_api_pin_input_;
  std::unique_ptr<PinKeyboardView::TestApi> test_api_pin_keyboard_;
  std::unique_ptr<PinContainerView::TestApi> test_api_pin_container_;
  std::unique_ptr<AuthInputRowView::TestApi> test_api_password_;
  std::unique_ptr<AuthHeaderView::TestApi> test_api_header_;
  std::unique_ptr<AuthContainerView::TestApi> test_api_auth_container_;
  std::unique_ptr<PinStatusView::TestApi> test_pin_status_;
  std::unique_ptr<ActiveSessionAuthView::TestApi> test_api_;
  raw_ptr<views::Button> close_button_;
  raw_ptr<ActiveSessionAuthView> container_view_ = nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
};

// Verify close observer.
TEST_F(ActiveSessionAuthViewUnitTest, CloseButtonTest) {
  EXPECT_CALL(*mock_observer_, OnClose()).Times(1);

  // Click on close button.
  LeftClickOn(close_button_);
}

// Verify close observer with disabled input area.
TEST_F(ActiveSessionAuthViewUnitTest, CloseButtonWithDisabledInputTest) {
  EXPECT_CALL(*mock_observer_, OnClose()).Times(1);
  container_view_->SetInputEnabled(false);
  // Click on close button.
  LeftClickOn(close_button_);
}

// Verify password submit observer.
TEST_F(ActiveSessionAuthViewUnitTest, PasswordSubmitTest) {
  const std::u16string kPassword(u"password");
  EXPECT_CALL(*mock_observer_, OnPasswordSubmit(kPassword)).Times(1);

  container_view_->GetFocusManager()->SetFocusedView(
      test_api_password_->GetTextfield());

  for (const char16_t c : kPassword) {
    PressAndReleaseKey(ui::DomCodeToUsLayoutNonLocatedKeyboardCode(
        ui::UsLayoutDomKeyToDomCode(ui::DomKey::FromCharacter(c))));
  }
  EXPECT_EQ(test_api_pin_input_->GetTextfield()->GetText(), std::u16string());
  EXPECT_EQ(test_api_password_->GetTextfield()->GetText(), kPassword);

  // Click on Submit.
  LeftClickOn(test_api_password_->GetSubmitButton());
}

// Verify the password input is no op with disabled input area.
TEST_F(ActiveSessionAuthViewUnitTest, PasswordSubmitWithDisabledInputTest) {
  const std::u16string kPassword(u"password");
  EXPECT_CALL(*mock_observer_, OnPasswordSubmit(kPassword)).Times(0);

  container_view_->GetFocusManager()->SetFocusedView(
      test_api_password_->GetTextfield());

  container_view_->SetInputEnabled(false);
  for (const char16_t c : kPassword) {
    PressAndReleaseKey(ui::DomCodeToUsLayoutNonLocatedKeyboardCode(
        ui::UsLayoutDomKeyToDomCode(ui::DomKey::FromCharacter(c))));
  }
  EXPECT_EQ(test_api_pin_input_->GetTextfield()->GetText(), std::u16string());
  EXPECT_EQ(test_api_password_->GetTextfield()->GetText(), std::u16string());

  // Click on Submit.
  LeftClickOn(test_api_password_->GetSubmitButton());
}

// Verify PIN submit observer.
TEST_F(ActiveSessionAuthViewUnitTest, PinSubmitTest) {
  // Switch to pin.
  LeftClickOn(test_api_auth_container_->GetSwitchButton());
  views::test::RunScheduledLayout(widget_.get());

  container_view_->GetFocusManager()->SetFocusedView(
      test_api_pin_input_->GetTextfield());

  const std::u16string kPin(u"6893112");
  EXPECT_CALL(*mock_observer_, OnPinSubmit(kPin)).Times(1);

  for (const char16_t c : kPin) {
    PressAndReleaseKey(ui::DomCodeToUsLayoutNonLocatedKeyboardCode(
        ui::UsLayoutDomKeyToDomCode(ui::DomKey::FromCharacter(c))));
  }
  EXPECT_EQ(test_api_pin_input_->GetTextfield()->GetText(), kPin);
  EXPECT_EQ(test_api_password_->GetTextfield()->GetText(), std::u16string());

  // Click on Submit.
  LeftClickOn(test_api_pin_input_->GetSubmitButton());
}

// Verify restore on input text change.
TEST_F(ActiveSessionAuthViewUnitTest, RestoreTitleTest1) {
  const std::u16string errorTitle = u"error";

  test_api_header_->GetView()->SetErrorTitle(errorTitle);
  EXPECT_EQ(test_api_header_->GetCurrentTitle(), errorTitle);

  container_view_->GetFocusManager()->SetFocusedView(
      test_api_password_->GetTextfield());

  PressAndReleaseKey(ui::VKEY_I);
  // Test the title is restore properly.
  EXPECT_EQ(test_api_header_->GetCurrentTitle(), u"title");
}

// Verify restore on switch button click.
TEST_F(ActiveSessionAuthViewUnitTest, RestoreTitleTest2) {
  const std::u16string errorTitle = u"error";

  test_api_header_->GetView()->SetErrorTitle(errorTitle);
  EXPECT_EQ(test_api_header_->GetCurrentTitle(), errorTitle);

  container_view_->GetFocusManager()->SetFocusedView(
      test_api_password_->GetTextfield());

  // Switch to pin.
  LeftClickOn(test_api_auth_container_->GetSwitchButton());

  // Test the title is restore properly.
  EXPECT_EQ(test_api_header_->GetCurrentTitle(), u"title");
}

// Verify the ResetInputfields functionality.
TEST_F(ActiveSessionAuthViewUnitTest, ResetInputfieldsTest) {
  test_api_password_->GetTextfield()->SetText(u"password");
  test_api_pin_input_->GetTextfield()->SetText(u"pin");
  test_api_->GetView()->ResetInputfields();

  EXPECT_EQ(test_api_password_->GetTextfield()->GetText(), std::u16string());
  EXPECT_EQ(test_api_pin_input_->GetTextfield()->GetText(), std::u16string());
}

TEST_F(ActiveSessionAuthViewUnitTest, SetPinStatusTest) {
  const std::u16string status_message = u"Too many PIN attempts";

  cryptohome::PinStatus pin_status(base::TimeDelta::Max());

  test_api_->GetView()->SetPinStatus(
      std::make_unique<cryptohome::PinStatus>(base::TimeDelta::Max()));

  EXPECT_EQ(test_pin_status_->GetCurrentText(), status_message);
}

}  // namespace

}  // namespace ash
