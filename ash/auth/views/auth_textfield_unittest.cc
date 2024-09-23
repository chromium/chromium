// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/auth/views/auth_textfield.h"

#include <memory>
#include <string>

#include "ash/auth/views/test_support/mock_auth_textfield_observer.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
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

class PasswordTextfieldUnitTest : public AshTestBase {
 public:
  PasswordTextfieldUnitTest() = default;
  PasswordTextfieldUnitTest(const PasswordTextfieldUnitTest&) = delete;
  PasswordTextfieldUnitTest& operator=(const PasswordTextfieldUnitTest&) =
      delete;
  ~PasswordTextfieldUnitTest() override = default;

  void SetTextfieldToFocus() {
    auth_textfield_->GetFocusManager()->SetFocusedView(auth_textfield_);
  }

 protected:
  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    widget_->Show();

    auth_textfield_ = widget_->SetContentsView(
        std::make_unique<AuthTextfield>(AuthTextfield::AuthType::kPassword));
    mock_observer_ = std::make_unique<MockAuthTextfieldObserver>();
    auth_textfield_->AddObserver(mock_observer_.get());
  }

  void TearDown() override {
    AshTestBase::TearDown();
    auth_textfield_->RemoveObserver(mock_observer_.get());
    mock_observer_.reset();
    auth_textfield_ = nullptr;
    widget_.reset();
  }

  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<MockAuthTextfieldObserver> mock_observer_;
  raw_ptr<AuthTextfield> auth_textfield_;
};

// Testing textfield OnBlur Observer.
TEST_F(PasswordTextfieldUnitTest, OnBlurObserverTest) {
  EXPECT_CALL(*mock_observer_, OnTextfieldBlur()).Times(1);
  auth_textfield_->OnBlur();
}

// Testing textfield OnFocus Observer.
TEST_F(PasswordTextfieldUnitTest, OnFocusObserverTest) {
  EXPECT_CALL(*mock_observer_, OnTextfieldFocus()).Times(1);
  SetTextfieldToFocus();
}

// Testing textfield OnContentsChanged Observer.
TEST_F(PasswordTextfieldUnitTest, OnContentsChangedTest) {
  auth_textfield_->SetText(kPassword);
  SetTextfieldToFocus();
  const std::u16string modifiedString = kPassword + u"s";
  EXPECT_CALL(*mock_observer_, OnContentsChanged(modifiedString)).Times(1);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressAndReleaseKey(ui::VKEY_S);
}

// Testing textfield OnTextVisibleChanged Observer.
TEST_F(PasswordTextfieldUnitTest, OnTextVisibleChangedTest) {
  auth_textfield_->SetText(kPassword);
  SetTextfieldToFocus();
  // by default the password should be hidden.
  CHECK(!auth_textfield_->IsTextVisible());
  EXPECT_CALL(*mock_observer_, OnTextVisibleChanged(true)).Times(1);
  auth_textfield_->SetTextVisible(true);
  CHECK(auth_textfield_->IsTextVisible());
}

// Testing password textfield OnSubmit Observer.
TEST_F(PasswordTextfieldUnitTest, OnSubmitTest) {
  auth_textfield_->SetText(kPassword);
  SetTextfieldToFocus();
  EXPECT_CALL(*mock_observer_, OnSubmit()).Times(1);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressAndReleaseKey(ui::VKEY_RETURN);
}

// Testing password textfield OnEscape Observer.
TEST_F(PasswordTextfieldUnitTest, OnEscapeTest) {
  SetTextfieldToFocus();
  EXPECT_CALL(*mock_observer_, OnEscape()).Times(1);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressAndReleaseKey(ui::VKEY_ESCAPE);
}

class PinTextfieldUnitTest : public AshTestBase {
 public:
  PinTextfieldUnitTest() = default;
  PinTextfieldUnitTest(const PinTextfieldUnitTest&) = delete;
  PinTextfieldUnitTest& operator=(const PinTextfieldUnitTest&) = delete;
  ~PinTextfieldUnitTest() override = default;

  void SetTextfieldToFocus() {
    auth_textfield_->GetFocusManager()->SetFocusedView(auth_textfield_);
  }

 protected:
  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);

    mock_observer_ = std::make_unique<MockAuthTextfieldObserver>();
    auth_textfield_ = widget_->SetContentsView(
        std::make_unique<AuthTextfield>(AuthTextfield::AuthType::kPin));
    auth_textfield_->SetText(kPIN);
    auth_textfield_->AddObserver(mock_observer_.get());
  }

  void TearDown() override {
    AshTestBase::TearDown();
    auth_textfield_->RemoveObserver(mock_observer_.get());
    mock_observer_.reset();
    auth_textfield_ = nullptr;
    widget_.reset();
  }

  std::unique_ptr<views::Widget> widget_;
  raw_ptr<AuthTextfield> auth_textfield_;
  std::unique_ptr<MockAuthTextfieldObserver> mock_observer_;
};

// Testing PIN textfield OnContentsChanged Observer.
TEST_F(PinTextfieldUnitTest, OnContentsChangedTest) {
  SetTextfieldToFocus();
  const std::u16string modifiedPIN = kPIN + u"5";
  EXPECT_CALL(*mock_observer_, OnContentsChanged(modifiedPIN)).Times(1);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressAndReleaseKey(ui::VKEY_5);
}

// Testing PIN textfield OnContentsChanged Observer with letter.
TEST_F(PinTextfieldUnitTest, OnContentsChangedWithLetterTest) {
  SetTextfieldToFocus();
  EXPECT_CALL(*mock_observer_, OnContentsChanged(kPIN)).Times(0);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressAndReleaseKey(ui::VKEY_E);
  CHECK_EQ(auth_textfield_->GetText(), kPIN);
}

// Testing PIN textfield InsertDigit function.
TEST_F(PinTextfieldUnitTest, InsertDigitTest) {
  const std::u16string modifiedPIN = kPIN + u"5";
  EXPECT_CALL(*mock_observer_, OnContentsChanged(modifiedPIN)).Times(1);
  auth_textfield_->InsertDigit(5);
}

// Testing PIN textfield backspace press.
TEST_F(PinTextfieldUnitTest, BackspacePressTest) {
  std::u16string modifiedPIN = kPIN;
  modifiedPIN.pop_back();
  SetTextfieldToFocus();
  EXPECT_CALL(*mock_observer_, OnContentsChanged(modifiedPIN)).Times(1);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressAndReleaseKey(ui::VKEY_BACK);
}

// Testing PIN textfield Backspace function.
TEST_F(PinTextfieldUnitTest, BackspaceTest) {
  std::u16string modifiedPIN = kPIN;
  modifiedPIN.pop_back();
  EXPECT_CALL(*mock_observer_, OnContentsChanged(modifiedPIN)).Times(1);
  auth_textfield_->Backspace();
}

// Testing PIN textfield OnSubmit Observer.
TEST_F(PinTextfieldUnitTest, OnSubmitTest) {
  SetTextfieldToFocus();
  EXPECT_CALL(*mock_observer_, OnSubmit()).Times(1);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressAndReleaseKey(ui::VKEY_RETURN);
}

// Testing PIN textfield OnEscape Observer.
TEST_F(PinTextfieldUnitTest, OnEscapeTest) {
  EXPECT_CALL(*mock_observer_, OnEscape()).Times(1);
  SetTextfieldToFocus();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressAndReleaseKey(ui::VKEY_ESCAPE);
}

}  // namespace ash
