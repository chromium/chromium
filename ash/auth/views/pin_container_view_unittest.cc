// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/auth/views/pin_container_view.h"

#include <memory>
#include <optional>

#include "ash/auth/views/auth_input_row_view.h"
#include "ash/auth/views/pin_keyboard_view.h"
#include "ash/auth/views/test_support/mock_auth_input_row_view_observer.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

typedef MockAuthInputRowViewObserver MockPinContainerViewObserver;

class PinContainerUnitTest : public AshTestBase {
 public:
  PinContainerUnitTest() = default;
  PinContainerUnitTest(const PinContainerUnitTest&) = delete;
  PinContainerUnitTest& operator=(const PinContainerUnitTest&) = delete;
  ~PinContainerUnitTest() override = default;

 protected:
  void SetUp() override {
    AshTestBase::SetUp();

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    widget_->Show();

    container_view_ =
        widget_->SetContentsView(std::make_unique<PinContainerView>());
    test_api_ = std::make_unique<PinContainerView::TestApi>(container_view_);
    test_api_pin_keyboard_ = std::make_unique<PinKeyboardView::TestApi>(
        test_api_->GetPinKeyboardView());
    test_api_auth_input_ = std::make_unique<AuthInputRowView::TestApi>(
        test_api_->GetAuthInputRowView());

    mock_observer_ = std::make_unique<MockPinContainerViewObserver>();
    container_view_->AddObserver(mock_observer_.get());
  }

  void TearDown() override {
    test_api_pin_keyboard_.reset();
    test_api_auth_input_.reset();
    test_api_.reset();
    container_view_->RemoveObserver(mock_observer_.get());
    mock_observer_.reset();
    container_view_ = nullptr;
    widget_.reset();
    AshTestBase::TearDown();
  }

  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<MockPinContainerViewObserver> mock_observer_;
  std::unique_ptr<PinKeyboardView::TestApi> test_api_pin_keyboard_;
  std::unique_ptr<AuthInputRowView::TestApi> test_api_auth_input_;
  std::unique_ptr<PinContainerView::TestApi> test_api_;
  raw_ptr<PinContainerView> container_view_ = nullptr;
};

// Verify pin keyboard connected with auth input row.
TEST_F(PinContainerUnitTest, TypePin) {
  // Press 1,2,3,4,5,6 buttons on the pin pad.
  const std::u16string kPin(u"123456");
  for (size_t i = 1; i <= kPin.size(); ++i) {
    EXPECT_CALL(*mock_observer_, OnContentsChanged(kPin.substr(0, i))).Times(1);
    LeftClickOn(test_api_pin_keyboard_->digit_button(kPin[i - 1] - u'0'));
  }
  EXPECT_EQ(test_api_auth_input_->GetTextfield()->GetText(), kPin);
}

// Verify type is not operating with disabled input.
TEST_F(PinContainerUnitTest, DisabledTypePin) {
  container_view_->SetInputEnabled(false);
  // Press 1,2,3,4,5,6 buttons on the pin pad.
  const std::u16string kPin(u"123456");
  for (size_t i = 1; i <= kPin.size(); ++i) {
    LeftClickOn(test_api_pin_keyboard_->digit_button(kPin[i - 1] - u'0'));
  }
  EXPECT_EQ(test_api_auth_input_->GetTextfield()->GetText(), std::u16string());
}

// Verify pin keyboard connected with auth input row.
TEST_F(PinContainerUnitTest, BackspaceTest) {
  const std::u16string kPin(u"6893112");
  for (size_t i = 1; i <= kPin.size(); ++i) {
    LeftClickOn(test_api_pin_keyboard_->digit_button(kPin[i - 1] - u'0'));
  }
  const std::u16string modified_pin = kPin.substr(0, kPin.size() - 1);
  EXPECT_CALL(*mock_observer_, OnContentsChanged(modified_pin)).Times(1);
  LeftClickOn(test_api_pin_keyboard_->backspace_button());
  EXPECT_EQ(test_api_auth_input_->GetTextfield()->GetText(), modified_pin);
}

// Verify enter press on the textfield submits the pin.
TEST_F(PinContainerUnitTest, SubmitTest) {
  const std::u16string kPin(u"60012345");
  // Set textfield to be focused.
  container_view_->GetFocusManager()->SetFocusedView(
      test_api_auth_input_->GetTextfield());
  for (size_t i = 1; i <= kPin.size(); ++i) {
    LeftClickOn(test_api_pin_keyboard_->digit_button(kPin[i - 1] - u'0'));
  }
  EXPECT_CALL(*mock_observer_, OnSubmit(kPin)).Times(1);
  PressAndReleaseKey(ui::VKEY_RETURN);
}

// Verify enter press is not operating with disabled input.
TEST_F(PinContainerUnitTest, SubmitDisabledTest) {
  const std::u16string kPin(u"60012345");
  // Set textfield to be focused.
  container_view_->GetFocusManager()->SetFocusedView(
      test_api_auth_input_->GetTextfield());
  for (size_t i = 1; i <= kPin.size(); ++i) {
    LeftClickOn(test_api_pin_keyboard_->digit_button(kPin[i - 1] - u'0'));
  }
  container_view_->SetInputEnabled(false);
  EXPECT_CALL(*mock_observer_, OnSubmit(kPin)).Times(0);
  PressAndReleaseKey(ui::VKEY_RETURN);
}

// Verify enter press on the digit presses the key.
TEST_F(PinContainerUnitTest, EnterOnPinkeyboardTest) {
  const std::u16string kPin(u"0894329");
  for (size_t i = 1; i <= kPin.size(); ++i) {
    // Set digit key to be focused and press enter on the button.
    container_view_->GetFocusManager()->SetFocusedView(
        test_api_pin_keyboard_->digit_button(kPin[i - 1] - u'0'));
    EXPECT_CALL(*mock_observer_, OnContentsChanged(kPin.substr(0, i))).Times(1);
    PressAndReleaseKey(ui::VKEY_RETURN);
  }
}

// Verify the ResetState functionality.
TEST_F(PinContainerUnitTest, ResetStateTest) {
  const std::u16string kPin(u"0894329");
  test_api_auth_input_->GetTextfield()->SetText(kPin);
  EXPECT_CALL(*mock_observer_, OnContentsChanged(std::u16string()));
  test_api_->GetView()->ResetState();
}

}  // namespace
}  // namespace ash
