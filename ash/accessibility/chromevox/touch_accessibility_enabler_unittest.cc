// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/chromevox/touch_accessibility_enabler.h"

#include <memory>

#include "ash/accessibility/chromevox/mock_touch_exploration_controller_delegate.h"
#include "ash/accessibility/chromevox/touch_exploration_controller.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/gestures/gesture_provider_aura.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/gfx/geometry/point.h"

namespace ash {

namespace {

class MockTouchAccessibilityEnablerDelegate
    : public TouchAccessibilityEnablerDelegate {
 public:
  MockTouchAccessibilityEnablerDelegate() {}

  MockTouchAccessibilityEnablerDelegate(
      const MockTouchAccessibilityEnablerDelegate&) = delete;
  MockTouchAccessibilityEnablerDelegate& operator=(
      const MockTouchAccessibilityEnablerDelegate&) = delete;

  ~MockTouchAccessibilityEnablerDelegate() override {}

  void OnTwoFingerTouchStart() override { started_ = true; }

  void OnTwoFingerTouchStop() override { stopped_ = true; }

  void PlaySpokenFeedbackToggleCountdown(int tick_count) override {
    ++feedback_progress_sound_count_;
  }
  void ToggleSpokenFeedback() override { toggle_spoken_feedback_ = true; }

  bool started() { return started_; }
  bool stopped() { return stopped_; }
  size_t feedback_progress_sound_count() const {
    return feedback_progress_sound_count_;
  }
  bool toggle_spoken_feedback() const { return toggle_spoken_feedback_; }

 private:
  bool started_ = false;
  bool stopped_ = false;
  size_t feedback_progress_sound_count_ = 0;
  bool toggle_spoken_feedback_ = false;
};

class TouchAccessibilityEnablerTest : public aura::test::AuraTestBase {
 public:
  TouchAccessibilityEnablerTest() {}

  TouchAccessibilityEnablerTest(const TouchAccessibilityEnablerTest&) = delete;
  TouchAccessibilityEnablerTest& operator=(
      const TouchAccessibilityEnablerTest&) = delete;

  ~TouchAccessibilityEnablerTest() override {}

  void SetUp() override {
    aura::test::AuraTestBase::SetUp();

    generator_ = std::make_unique<ui::test::EventGenerator>(root_window());

    // Tests fail if time is ever 0.
    simulated_clock_.Advance(base::Milliseconds(10));
    ui::SetEventTickClockForTesting(&simulated_clock_);

    enabler_ =
        std::make_unique<TouchAccessibilityEnabler>(root_window(), &delegate_);
  }

  void TearDown() override {
    enabler_.reset(nullptr);
    ui::SetEventTickClockForTesting(nullptr);
    aura::test::AuraTestBase::TearDown();
  }

 protected:
  base::TimeTicks Now() {
    // This is the same as what EventTimeForNow() does, but here we do it
    // with our simulated clock.
    return simulated_clock_.NowTicks();
  }

  std::unique_ptr<ui::test::EventGenerator> generator_;
  base::SimpleTestTickClock simulated_clock_;
  MockTouchAccessibilityEnablerDelegate delegate_;
  std::unique_ptr<TouchAccessibilityEnabler> enabler_;
  ui::GestureDetector::Config gesture_detector_config_;
};

}  // namespace

TEST_F(TouchAccessibilityEnablerTest, InteractsWithTouchExplorationController) {
  // This test ensures that if TouchExplorationController starts and stops,
  // TouchAccessibilityEnabler continues to work correctly. Because
  // TouchExplorationController rewrites most touch events, it can screw up
  // TouchAccessibilityEnabler if they don't explicitly coordinate.

  MockTouchExplorationControllerDelegate delegate;
  std::unique_ptr<TouchExplorationController> controller(
      new TouchExplorationController(root_window(), &delegate,
                                     enabler_->GetWeakPtr()));

  EXPECT_TRUE(enabler_->IsInNoFingersDownForTesting());
  generator_->set_current_screen_location(gfx::Point(11, 12));
  generator_->PressTouchId(1);

  simulated_clock_.Advance(base::Milliseconds(500));

  generator_->set_current_screen_location(gfx::Point(22, 34));
  generator_->PressTouchId(2);

  EXPECT_TRUE(enabler_->IsInTwoFingersDownForTesting());

  controller.reset();

  generator_->ReleaseTouchId(1);
  generator_->ReleaseTouchId(2);
  EXPECT_TRUE(enabler_->IsInNoFingersDownForTesting());
}

TEST_F(TouchAccessibilityEnablerTest, EntersOneFingerDownMode) {
  EXPECT_TRUE(enabler_->IsInNoFingersDownForTesting());
  EXPECT_FALSE(enabler_->IsInOneFingerDownForTesting());
  generator_->set_current_screen_location(gfx::Point(11, 12));
  generator_->PressTouch();

  EXPECT_FALSE(enabler_->IsInNoFingersDownForTesting());
  EXPECT_TRUE(enabler_->IsInOneFingerDownForTesting());
}

TEST_F(TouchAccessibilityEnablerTest, EntersTwoFingersDownMode) {
  EXPECT_TRUE(enabler_->IsInNoFingersDownForTesting());
  generator_->set_current_screen_location(gfx::Point(11, 12));
  generator_->PressTouchId(1);

  generator_->set_current_screen_location(gfx::Point(22, 34));
  generator_->PressTouchId(2);

  EXPECT_TRUE(enabler_->IsInTwoFingersDownForTesting());
}

TEST_F(TouchAccessibilityEnablerTest, PlaysProgressSound) {
  EXPECT_TRUE(enabler_->IsInNoFingersDownForTesting());
  generator_->set_current_screen_location(gfx::Point(11, 12));
  generator_->PressTouchId(1);

  generator_->set_current_screen_location(gfx::Point(22, 34));
  generator_->PressTouchId(2);

  EXPECT_TRUE(enabler_->IsInTwoFingersDownForTesting());
  EXPECT_EQ(0U, delegate_.feedback_progress_sound_count());

  enabler_->TriggerOnTimerForTesting();
  EXPECT_EQ(0U, delegate_.feedback_progress_sound_count());

  simulated_clock_.Advance(base::Milliseconds(3000));
  enabler_->TriggerOnTimerForTesting();
  EXPECT_EQ(1U, delegate_.feedback_progress_sound_count());
}

TEST_F(TouchAccessibilityEnablerTest, TogglesSpokenFeedback) {
  EXPECT_TRUE(enabler_->IsInNoFingersDownForTesting());
  generator_->set_current_screen_location(gfx::Point(11, 12));
  generator_->PressTouchId(1);

  generator_->set_current_screen_location(gfx::Point(22, 34));
  generator_->PressTouchId(2);

  EXPECT_TRUE(enabler_->IsInTwoFingersDownForTesting());
  EXPECT_FALSE(delegate_.toggle_spoken_feedback());
  EXPECT_TRUE(delegate_.started());
  EXPECT_FALSE(delegate_.stopped());

  enabler_->TriggerOnTimerForTesting();
  EXPECT_FALSE(delegate_.toggle_spoken_feedback());

  simulated_clock_.Advance(base::Milliseconds(5000));
  enabler_->TriggerOnTimerForTesting();
  EXPECT_TRUE(delegate_.toggle_spoken_feedback());
  EXPECT_TRUE(delegate_.started());
  EXPECT_FALSE(delegate_.stopped());
}

TEST_F(TouchAccessibilityEnablerTest, ThreeFingersCancelsDetection) {
  EXPECT_TRUE(enabler_->IsInNoFingersDownForTesting());
  generator_->set_current_screen_location(gfx::Point(11, 12));
  generator_->PressTouchId(1);

  generator_->set_current_screen_location(gfx::Point(22, 34));
  generator_->PressTouchId(2);

  EXPECT_TRUE(enabler_->IsInTwoFingersDownForTesting());
  EXPECT_TRUE(delegate_.started());
  EXPECT_FALSE(delegate_.stopped());

  generator_->set_current_screen_location(gfx::Point(33, 56));
  generator_->PressTouchId(3);

  EXPECT_TRUE(enabler_->IsInWaitForNoFingersForTesting());
  EXPECT_TRUE(delegate_.started());
  EXPECT_TRUE(delegate_.stopped());
}

TEST_F(TouchAccessibilityEnablerTest, MovingFingerPastSlopCancelsDetection) {
  EXPECT_TRUE(enabler_->IsInNoFingersDownForTesting());
  generator_->set_current_screen_location(gfx::Point(11, 12));
  generator_->PressTouch();

  int slop = gesture_detector_config_.double_tap_slop;
  int half_slop = slop / 2;

  generator_->MoveTouch(gfx::Point(11 + half_slop, 12));
  EXPECT_TRUE(enabler_->IsInOneFingerDownForTesting());

  generator_->MoveTouch(gfx::Point(11 + slop + 1, 12));
  EXPECT_TRUE(enabler_->IsInWaitForNoFingersForTesting());
}

}  // namespace ash
