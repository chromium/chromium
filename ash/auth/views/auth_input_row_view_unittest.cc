// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/auth/views/auth_input_row_view.h"

#include <memory>
#include <string>

#include "ash/auth/views/auth_textfield.h"
#include "ash/auth/views/test_support/mock_auth_input_row_view_observer.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr std::u16string kPassword = u"password";
constexpr std::u16string kPIN = u"123456";

}  // namespace

class InputRowWithPasswordUnitTest : public AshTestBase {
 public:
  InputRowWithPasswordUnitTest() = default;
  InputRowWithPasswordUnitTest(const InputRowWithPasswordUnitTest&) = delete;
  InputRowWithPasswordUnitTest& operator=(const InputRowWithPasswordUnitTest&) =
      delete;
  ~InputRowWithPasswordUnitTest() override = default;

  void SetInputRowToFocus() { auth_input_->RequestFocus(); }

 protected:
  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    widget_->Show();

    auth_input_ = widget_->SetContentsView(std::make_unique<AuthInputRowView>(
        AuthInputRowView::AuthType::kPassword));
    test_api_ = std::make_unique<AuthInputRowView::TestApi>(auth_input_);
    // Initialize the textfield with some text.
    auth_input_->RequestFocus();
    for (const char16_t c : kPassword) {
      PressAndReleaseKey(ui::DomCodeToUsLayoutNonLocatedKeyboardCode(
          ui::UsLayoutDomKeyToDomCode(ui::DomKey::FromCharacter(c))));
    }
    CHECK(test_api_->GetTextfield()->HasFocus());
    CHECK(test_api_->GetSubmitButton()->GetEnabled());
    CHECK(test_api_->GetDisplayTextButton()->GetEnabled());
    CHECK_EQ(test_api_->GetDisplayTextButton()->GetToggled(), false);

    // Add observer.
    mock_observer_ = std::make_unique<MockAuthInputRowViewObserver>();
    auth_input_->AddObserver(mock_observer_.get());
  }

  void TearDown() override {
    test_api_.reset();
    auth_input_->RemoveObserver(mock_observer_.get());
    mock_observer_.reset();
    auth_input_ = nullptr;
    widget_.reset();
    AshTestBase::TearDown();
  }

  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<MockAuthInputRowViewObserver> mock_observer_;
  std::unique_ptr<AuthInputRowView::TestApi> test_api_;
  raw_ptr<AuthInputRowView> auth_input_ = nullptr;
};

// Testing tab on the textfield the textfield lost the focus and the display
// text button gets it.
TEST_F(InputRowWithPasswordUnitTest, OnBlurObserverTest) {
  EXPECT_CALL(*mock_observer_, OnTextfieldBlur()).Times(1);
  PressAndReleaseKey(ui::VKEY_TAB);
  CHECK(test_api_->GetDisplayTextButton()->HasFocus());
}

// Testing textfield OnFocus observer.
TEST_F(InputRowWithPasswordUnitTest, OnFocusObserverWithClickTest) {
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_CALL(*mock_observer_, OnTextfieldFocus()).Times(1);
  LeftClickOn(test_api_->GetTextfield());
}

// Testing textfield OnContentsChanged observer.
TEST_F(InputRowWithPasswordUnitTest, OnContentsChangedTest) {
  const std::u16string modifiedString = kPassword + u"s";
  EXPECT_CALL(*mock_observer_, OnContentsChanged(modifiedString)).Times(1);
  PressAndReleaseKey(ui::VKEY_S);
}

// Testing OnTextVisibleChanged Observer with keyboard interactions.
TEST_F(InputRowWithPasswordUnitTest, OnTextVisibleChangedTestWithKeyPresses) {
  PressAndReleaseKey(ui::VKEY_TAB);
  CHECK(test_api_->GetDisplayTextButton()->HasFocus());
  EXPECT_CALL(*mock_observer_, OnTextVisibleChanged(true)).Times(1);
  PressAndReleaseKey(ui::VKEY_RETURN);
}

// Testing OnTextVisibleChanged observer with mouse click.
TEST_F(InputRowWithPasswordUnitTest, OnTextVisibleChangedTestWithMouseClick) {
  EXPECT_CALL(*mock_observer_, OnTextVisibleChanged(true)).Times(1);
  LeftClickOn(test_api_->GetDisplayTextButton());
}

// Testing press enter in the textfield calls the submit with the password.
TEST_F(InputRowWithPasswordUnitTest, OnSubmitTestWithEnter) {
  EXPECT_CALL(*mock_observer_, OnSubmit(kPassword)).Times(1);
  PressAndReleaseKey(ui::VKEY_RETURN);
}

// Testing press enter with disabled state.
TEST_F(InputRowWithPasswordUnitTest, TestDisabledEnter) {
  auth_input_->SetInputEnabled(false);
  EXPECT_CALL(*mock_observer_, OnSubmit(kPassword)).Times(0);
  PressAndReleaseKey(ui::VKEY_RETURN);
}

// Testing clicking on the submit button calls the submit with the password.
TEST_F(InputRowWithPasswordUnitTest, OnSubmitTestWithClick) {
  EXPECT_CALL(*mock_observer_, OnSubmit(kPassword)).Times(1);
  LeftClickOn(test_api_->GetSubmitButton());
}

// Testing clicking on the submit button with disabled state.
TEST_F(InputRowWithPasswordUnitTest, DisabledSubmitClickTest) {
  auth_input_->SetInputEnabled(false);
  EXPECT_CALL(*mock_observer_, OnSubmit(kPassword)).Times(0);
  LeftClickOn(test_api_->GetSubmitButton());
}

// Testing ESC press in the textfield calls OnEscape observer.
TEST_F(InputRowWithPasswordUnitTest, TextfieldOnEscapeTest) {
  EXPECT_CALL(*mock_observer_, OnEscape()).Times(1);
  PressAndReleaseKey(ui::VKEY_ESCAPE);
}

// Testing ESC press on the display text calls OnEscape observer.
TEST_F(InputRowWithPasswordUnitTest, DisplayButtonOnEscapeTest) {
  PressAndReleaseKey(ui::VKEY_TAB);
  CHECK(test_api_->GetDisplayTextButton()->HasFocus());
  EXPECT_CALL(*mock_observer_, OnEscape()).Times(1);
  PressAndReleaseKey(ui::VKEY_ESCAPE);
}

// Testing ESC press on the Submit button calls OnEscape observer.
TEST_F(InputRowWithPasswordUnitTest, SubmitButtonOnEscapeTest) {
  auth_input_->GetFocusManager()->SetFocusedView(test_api_->GetSubmitButton());
  CHECK(test_api_->GetSubmitButton()->HasFocus());
  EXPECT_CALL(*mock_observer_, OnEscape()).Times(1);
  PressAndReleaseKey(ui::VKEY_ESCAPE);
}

// Testing When password is visible and select all on the password and remove
// the text should call: 1: OnContentsChanged 2: OnTextVisibleChanged(/*
// visible=*/false)
TEST_F(InputRowWithPasswordUnitTest, RemoveTextTest) {
  LeftClickOn(test_api_->GetDisplayTextButton());
  // Select all and delete.
  EXPECT_CALL(*mock_observer_, OnContentsChanged(std::u16string())).Times(1);
  EXPECT_CALL(*mock_observer_, OnTextVisibleChanged(false)).Times(1);
  PressAndReleaseKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  PressAndReleaseKey(ui::VKEY_BACK);
  // Submit button should be disabled with empty text.
  CHECK_EQ(test_api_->GetSubmitButton()->GetEnabled(), false);
  // Display text button should be disabled with empty text.
  CHECK_EQ(test_api_->GetDisplayTextButton()->GetEnabled(), false);
}

// Testing ResetState functionality.
TEST_F(InputRowWithPasswordUnitTest, ResetStateTest) {
  EXPECT_CALL(*mock_observer_, OnContentsChanged(std::u16string())).Times(1);
  auth_input_->ResetState();

  // The textfield should be empty.
  EXPECT_EQ(test_api_->GetTextfield()->GetText(), std::u16string());
  // Submit button should be disabled with empty text.
  CHECK_EQ(test_api_->GetSubmitButton()->GetEnabled(), false);
  // Display text button should be disabled with empty text.
  CHECK_EQ(test_api_->GetDisplayTextButton()->GetEnabled(), false);
}

class InputRowWithPinUnitTest : public AshTestBase {
 public:
  InputRowWithPinUnitTest() = default;
  InputRowWithPinUnitTest(const InputRowWithPinUnitTest&) = delete;
  InputRowWithPinUnitTest& operator=(const InputRowWithPinUnitTest&) = delete;
  ~InputRowWithPinUnitTest() override = default;

  void SetInputRowToFocus() { auth_input_->RequestFocus(); }

 protected:
  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    widget_->Show();

    auth_input_ = widget_->SetContentsView(
        std::make_unique<AuthInputRowView>(AuthInputRowView::AuthType::kPin));
    test_api_ = std::make_unique<AuthInputRowView::TestApi>(auth_input_);
    // Initialize the textfield with some text.
    auth_input_->RequestFocus();

    for (const char16_t c : kPIN) {
      PressAndReleaseKey(ui::DomCodeToUsLayoutNonLocatedKeyboardCode(
          ui::UsLayoutDomKeyToDomCode(ui::DomKey::FromCharacter(c))));
    }
    CHECK(test_api_->GetTextfield()->HasFocus());
    CHECK_EQ(test_api_->GetTextfield()->GetText(), kPIN);
    CHECK(test_api_->GetSubmitButton()->GetEnabled());
    CHECK(test_api_->GetDisplayTextButton()->GetEnabled());
    CHECK_EQ(test_api_->GetDisplayTextButton()->GetToggled(), false);

    // Add observer.
    mock_observer_ = std::make_unique<MockAuthInputRowViewObserver>();
    auth_input_->AddObserver(mock_observer_.get());
  }

  void TearDown() override {
    test_api_.reset();
    auth_input_->RemoveObserver(mock_observer_.get());
    mock_observer_.reset();
    auth_input_ = nullptr;
    widget_.reset();
    AshTestBase::TearDown();
  }

  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<MockAuthInputRowViewObserver> mock_observer_;
  std::unique_ptr<AuthInputRowView::TestApi> test_api_;
  raw_ptr<AuthInputRowView> auth_input_ = nullptr;
};

// Testing PIN OnContentsChanged observer.
TEST_F(InputRowWithPinUnitTest, OnContentsChangedTest) {
  const std::u16string modifiedPIN = kPIN + u"5";
  EXPECT_CALL(*mock_observer_, OnContentsChanged(modifiedPIN)).Times(1);
  PressAndReleaseKey(ui::VKEY_5);
}

// Testing PIN OnContentsChanged observer with disabled input area.
TEST_F(InputRowWithPinUnitTest, DisabledDigitPressTest) {
  auth_input_->SetInputEnabled(false);
  const std::u16string modifiedPIN = kPIN + u"5";
  EXPECT_CALL(*mock_observer_, OnContentsChanged(modifiedPIN)).Times(0);
  PressAndReleaseKey(ui::VKEY_5);
}

// Testing PIN OnContentsChanged observer with letter.
TEST_F(InputRowWithPinUnitTest, OnContentsChangedWithLetterTest) {
  EXPECT_CALL(*mock_observer_, OnContentsChanged(kPIN)).Times(0);
  PressAndReleaseKey(ui::VKEY_E);
  CHECK_EQ(test_api_->GetTextfield()->GetText(), kPIN);
}

// Testing PIN backspace press.
TEST_F(InputRowWithPinUnitTest, BackspacePressTest) {
  std::u16string modifiedPIN = kPIN;
  modifiedPIN.pop_back();
  EXPECT_CALL(*mock_observer_, OnContentsChanged(modifiedPIN)).Times(1);
  PressAndReleaseKey(ui::VKEY_BACK);
}

// Testing PIN backspace press with disabled input area.
TEST_F(InputRowWithPinUnitTest, DisabledBackspacePressTest) {
  auth_input_->SetInputEnabled(false);
  std::u16string modifiedPIN = kPIN;
  modifiedPIN.pop_back();
  EXPECT_CALL(*mock_observer_, OnContentsChanged(modifiedPIN)).Times(0);
  PressAndReleaseKey(ui::VKEY_BACK);
}

// Testing PIN backspace press with disabled and after that re-enabled input
// area.
TEST_F(InputRowWithPinUnitTest, ReenabledBackspacePressTest) {
  auth_input_->SetInputEnabled(false);
  auth_input_->SetInputEnabled(true);
  auth_input_->RequestFocus();

  std::u16string modifiedPIN = kPIN;
  modifiedPIN.pop_back();
  EXPECT_CALL(*mock_observer_, OnContentsChanged(modifiedPIN)).Times(1);
  PressAndReleaseKey(ui::VKEY_BACK);
}

// Testing PIN OnSubmit observer.
TEST_F(InputRowWithPinUnitTest, OnSubmitTest) {
  EXPECT_CALL(*mock_observer_, OnSubmit(kPIN)).Times(1);
  PressAndReleaseKey(ui::VKEY_RETURN);
}

// Testing PIN OnEscape observer.
TEST_F(InputRowWithPinUnitTest, OnEscapeTest) {
  EXPECT_CALL(*mock_observer_, OnEscape()).Times(1);
  PressAndReleaseKey(ui::VKEY_ESCAPE);
}

}  // namespace ash
