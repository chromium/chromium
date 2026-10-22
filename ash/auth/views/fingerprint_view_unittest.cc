// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/auth/views/fingerprint_view.h"

#include <memory>
#include <string>

#include "ash/public/cpp/login_types.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/animation/animation.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

class FingerprintViewUnitTest : public AshTestBase {
 public:
  FingerprintViewUnitTest()
      : AshTestBase(std::make_unique<base::test::TaskEnvironment>(
            base::test::TaskEnvironment::MainThreadType::UI,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME)) {}

  FingerprintViewUnitTest(const FingerprintViewUnitTest&) = delete;
  FingerprintViewUnitTest& operator=(const FingerprintViewUnitTest&) = delete;
  ~FingerprintViewUnitTest() override = default;

 protected:
  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    UpdateDisplay("600x800");

    widget_ = CreateFramelessTestWidget();
    std::unique_ptr<views::View> container_view =
        std::make_unique<views::View>();

    auto* layout =
        container_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);

    container_view->SetPreferredSize(gfx::Size({400, 300}));

    container_view->SetBackground(views::CreateRoundedRectBackground(
        cros_tokens::kCrosSysSystemBaseElevated, 0));

    fingerprint_view_ =
        container_view->AddChildView(std::make_unique<FingerprintView>());
    widget_->SetBounds({400, 300});
    widget_->Show();

    container_view_ = widget_->SetContentsView(std::move(container_view));
  }

  void TearDown() override {
    gfx::Animation::SetPrefersReducedMotionForTesting(false);
    AshTestBase::TearDown();
    fingerprint_view_ = nullptr;
    container_view_ = nullptr;
    widget_.reset();
  }

  raw_ptr<FingerprintView> fingerprint_view_ = nullptr;
  raw_ptr<views::View> container_view_ = nullptr;
  std::unique_ptr<views::Widget> widget_;
};

// Verify the AVAILABLE_WITH_FAILED_ATTEMPT state transitions.
TEST_F(FingerprintViewUnitTest, AvailableWithFailedAttemptTest) {
  FingerprintView::TestApi test_api(fingerprint_view_);

  fingerprint_view_->SetState(FingerprintState::AVAILABLE_DEFAULT);
  EXPECT_EQ(test_api.GetState(), FingerprintState::AVAILABLE_DEFAULT);

  fingerprint_view_->NotifyAuthFailure();
  EXPECT_EQ(test_api.GetState(),
            FingerprintState::AVAILABLE_WITH_FAILED_ATTEMPT);

  //  The reset should happen after 1300 msec.
  task_environment()->FastForwardBy(base::Seconds(2));
  EXPECT_EQ(test_api.GetState(), FingerprintState::AVAILABLE_DEFAULT);
}

// Verify the AVAILABLE_WITH_TOUCH_SENSOR_WARNING state transitions.
TEST_F(FingerprintViewUnitTest, AvailableWithTouchSensorWarningTest) {
  FingerprintView::TestApi test_api(fingerprint_view_);

  fingerprint_view_->SetState(FingerprintState::AVAILABLE_DEFAULT);
  EXPECT_EQ(test_api.GetState(), FingerprintState::AVAILABLE_DEFAULT);
  views::test::RunScheduledLayout(widget_.get());

  // The reset should happen after 1300 msec.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->GestureTapAt(fingerprint_view_->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(test_api.GetState(),
            FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING);

  task_environment()->FastForwardBy(base::Seconds(4));
  EXPECT_EQ(test_api.GetState(), FingerprintState::AVAILABLE_DEFAULT);
}

// Verify NotifyAuthSuccess skips the animation when reduced motion is enabled.
TEST_F(FingerprintViewUnitTest, NotifyAuthSuccessReducedMotionTest) {
  gfx::Animation::SetPrefersReducedMotionForTesting(true);
  FingerprintView::TestApi test_api(fingerprint_view_);

  fingerprint_view_->SetState(FingerprintState::AVAILABLE_DEFAULT);
  base::test::TestFuture<void> animation_finished;
  fingerprint_view_->NotifyAuthSuccess(animation_finished.GetCallback());

  EXPECT_TRUE(test_api.GetIcon()->GetVisible());
  EXPECT_FALSE(test_api.GetLottieAnimationView()->GetVisible());
  EXPECT_TRUE(animation_finished.Wait());
}

// Verify NotifyAuthSuccess plays the animation and runs the callback.
TEST_F(FingerprintViewUnitTest, NotifyAuthSuccessAnimationTest) {
  FingerprintView::TestApi test_api(fingerprint_view_);

  fingerprint_view_->SetState(FingerprintState::AVAILABLE_DEFAULT);
  base::test::TestFuture<void> animation_finished;
  fingerprint_view_->NotifyAuthSuccess(animation_finished.GetCallback());

  EXPECT_FALSE(test_api.GetIcon()->GetVisible());
  EXPECT_TRUE(test_api.GetLottieAnimationView()->GetVisible());
  EXPECT_FALSE(animation_finished.IsReady());

  fingerprint_view_->AnimationCycleEnded(nullptr);
  EXPECT_TRUE(animation_finished.Wait());
}

// Verify NotifyAuthSuccess runs the callback when the animation is stopped.
TEST_F(FingerprintViewUnitTest, NotifyAuthSuccessAnimationStoppedTest) {
  FingerprintView::TestApi test_api(fingerprint_view_);

  fingerprint_view_->SetState(FingerprintState::AVAILABLE_DEFAULT);
  base::test::TestFuture<void> animation_finished;
  fingerprint_view_->NotifyAuthSuccess(animation_finished.GetCallback());
  EXPECT_FALSE(animation_finished.IsReady());

  test_api.GetLottieAnimationView()->Stop();
  EXPECT_TRUE(animation_finished.Wait());
}

}  // namespace ash
