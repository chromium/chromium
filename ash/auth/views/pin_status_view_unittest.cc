// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/auth/views/pin_status_view.h"

#include <memory>
#include <string>

#include "ash/test/ash_test_base.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/widget/widget.h"

namespace ash {

class PinStatusUnitTest : public AshTestBase {
 public:
  PinStatusUnitTest()
      : AshTestBase(std::make_unique<base::test::TaskEnvironment>(
            base::test::TaskEnvironment::MainThreadType::UI,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME)) {}
  PinStatusUnitTest(const PinStatusUnitTest&) = delete;
  PinStatusUnitTest& operator=(const PinStatusUnitTest&) = delete;
  ~PinStatusUnitTest() override = default;

 protected:
  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    widget_->Show();

    pin_status_view_ =
        widget_->SetContentsView(std::make_unique<PinStatusView>());
    test_api_ = std::make_unique<PinStatusView::TestApi>(pin_status_view_);
  }

  void TearDown() override {
    test_api_.reset();
    pin_status_view_ = nullptr;
    widget_.reset();
    AshTestBase::TearDown();
  }

  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<PinStatusView::TestApi> test_api_;
  raw_ptr<PinStatusView> pin_status_view_;
};

// Testing PinStatus with nullptr.
TEST_F(PinStatusUnitTest, NullPinStatus) {
  pin_status_view_->SetPinStatus(nullptr);
  EXPECT_EQ(pin_status_view_->GetCurrentText(),
            test_api_->GetTextLabel()->GetViewAccessibility().GetCachedName());
  EXPECT_FALSE(pin_status_view_->GetVisible());
}

// Testing PinStatus with max time.
TEST_F(PinStatusUnitTest, MaxPinStatus) {
  const std::u16string status_message = u"Too many PIN attempts";

  cryptohome::PinStatus pin_status(base::TimeDelta::Max());

  pin_status_view_->SetPinStatus(
      std::make_unique<cryptohome::PinStatus>(pin_status));

  EXPECT_EQ(pin_status_view_->GetCurrentText(), status_message);
  EXPECT_EQ(pin_status_view_->GetCurrentText(),
            test_api_->GetTextLabel()->GetViewAccessibility().GetCachedName());
  EXPECT_TRUE(pin_status_view_->GetVisible());
}

// Testing PinStatus with 30sec.
TEST_F(PinStatusUnitTest, ShortDelayPinStatus) {
  const std::u16string status_message =
      u"Too many PIN attempts. Wait 30 seconds and try again";

  cryptohome::PinStatus pin_status(base::Seconds(30));

  pin_status_view_->SetPinStatus(
      std::make_unique<cryptohome::PinStatus>(pin_status));

  EXPECT_EQ(pin_status_view_->GetCurrentText(), status_message);
  EXPECT_EQ(pin_status_view_->GetCurrentText(),
            test_api_->GetTextLabel()->GetViewAccessibility().GetCachedName());
  EXPECT_TRUE(pin_status_view_->GetVisible());

  task_environment()->FastForwardBy(base::Seconds(5));
  const std::u16string updated_status_message =
      u"Too many PIN attempts. Wait 25 seconds and try again";
  EXPECT_EQ(pin_status_view_->GetCurrentText(), updated_status_message);
  EXPECT_TRUE(pin_status_view_->GetVisible());

  task_environment()->FastForwardBy(base::Seconds(25));
  EXPECT_THAT(pin_status_view_->GetCurrentText(), testing::IsEmpty());
  EXPECT_EQ(pin_status_view_->GetVisible(), false);
}

}  // namespace ash
