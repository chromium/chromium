// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/auth/views/auth_textfield_timer.h"

#include <memory>
#include <string>

#include "ash/auth/views/auth_textfield.h"
#include "ash/auth/views/test_support/mock_auth_textfield_observer.h"
#include "ash/test/ash_test_base.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr std::u16string kPassword = u"password";

}  // namespace

class AuthTextfieldWithTimerUnitTest : public AshTestBase {
 public:
  AuthTextfieldWithTimerUnitTest()
      : AshTestBase(std::make_unique<base::test::TaskEnvironment>(
            base::test::TaskEnvironment::MainThreadType::UI,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME)) {}
  AuthTextfieldWithTimerUnitTest(const AuthTextfieldWithTimerUnitTest&) =
      delete;
  AuthTextfieldWithTimerUnitTest& operator=(
      const AuthTextfieldWithTimerUnitTest&) = delete;
  ~AuthTextfieldWithTimerUnitTest() override = default;

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
    auth_textfield_->SetText(kPassword);
    auth_textfield_->AddObserver(mock_observer_.get());
    auth_textfield_timer_ =
        std::make_unique<AuthTextfieldTimer>(auth_textfield_);
  }

  void TearDown() override {
    AshTestBase::TearDown();
    auth_textfield_timer_.reset();
    auth_textfield_->RemoveObserver(mock_observer_.get());
    mock_observer_.reset();
    auth_textfield_ = nullptr;
    widget_.reset();
  }

  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<MockAuthTextfieldObserver> mock_observer_;
  std::unique_ptr<AuthTextfieldTimer> auth_textfield_timer_;
  raw_ptr<AuthTextfield> auth_textfield_;
};

// Testing password become hidden after 5 sec elapsed without user interaction.
TEST_F(AuthTextfieldWithTimerUnitTest, HidePasswordTest) {
  SetTextfieldToFocus();
  CHECK(!auth_textfield_->IsTextVisible());
  auth_textfield_->SetTextVisible(true);
  EXPECT_CALL(*mock_observer_, OnTextVisibleChanged(false)).Times(1);
  task_environment()->FastForwardBy(base::Seconds(5));
  CHECK(!auth_textfield_->IsTextVisible());
}

// Testing password visibility not changing if the password is hidden after 10
// sec.
TEST_F(AuthTextfieldWithTimerUnitTest, HidePasswordTest2) {
  SetTextfieldToFocus();
  CHECK(!auth_textfield_->IsTextVisible());
  // Make the password visible.
  auth_textfield_->SetTextVisible(true);
  // Hide the password.
  auth_textfield_->SetTextVisible(false);
  EXPECT_CALL(*mock_observer_, OnTextVisibleChanged(false)).Times(0);
  EXPECT_CALL(*mock_observer_, OnTextVisibleChanged(true)).Times(0);
  task_environment()->FastForwardBy(base::Seconds(10));
  CHECK(!auth_textfield_->IsTextVisible());
}

// Testing password become hidden after 30 sec elapsed without user interaction.
TEST_F(AuthTextfieldWithTimerUnitTest, ClearPasswordTest) {
  SetTextfieldToFocus();
  CHECK(!auth_textfield_->IsTextVisible());
  // Make a user interaction.
  const std::u16string modifiedString = kPassword + u"s";
  EXPECT_CALL(*mock_observer_, OnContentsChanged(modifiedString)).Times(1);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressAndReleaseKey(ui::VKEY_S);

  // After 30 sec without user interaction the textfield should be cleared.
  const std::u16string emptyStr;
  EXPECT_CALL(*mock_observer_, OnContentsChanged(emptyStr)).Times(1);
  task_environment()->FastForwardBy(base::Seconds(30));
  CHECK_EQ(auth_textfield_->GetText(), emptyStr);
}

}  // namespace ash
