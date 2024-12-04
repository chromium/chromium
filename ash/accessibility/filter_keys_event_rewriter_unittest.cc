// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/filter_keys_event_rewriter.h"

#include <memory>

#include "ash/accessibility/test_event_recorder.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"

namespace ash {

class FilterKeysEventRewriterTest : public AshTestBase {
 public:
  FilterKeysEventRewriterTest() = default;
  FilterKeysEventRewriterTest(const FilterKeysEventRewriterTest&) = delete;
  FilterKeysEventRewriterTest& operator=(const FilterKeysEventRewriterTest&) =
      delete;
  ~FilterKeysEventRewriterTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    event_rewriter_ = std::make_unique<FilterKeysEventRewriter>();
    generator_ = AshTestBase::GetEventGenerator();
    GetContext()->GetHost()->GetEventSource()->AddEventRewriter(
        event_rewriter());
    GetContext()->GetHost()->GetEventSource()->AddEventRewriter(
        &event_recorder_);
  }

  void TearDown() override {
    GetContext()->GetHost()->GetEventSource()->RemoveEventRewriter(
        &event_recorder_);
    GetContext()->GetHost()->GetEventSource()->RemoveEventRewriter(
        event_rewriter());
    event_rewriter_.reset();
    generator_ = nullptr;
    AshTestBase::TearDown();
  }

  ui::test::EventGenerator* generator() { return generator_; }
  TestEventRecorder* event_recorder() { return &event_recorder_; }
  FilterKeysEventRewriter* event_rewriter() { return event_rewriter_.get(); }

  void PressA(int flags = ui::EF_NONE) {
    generator()->PressKey(ui::VKEY_A, flags);
  }
  void ReleaseA(int flags = ui::EF_NONE) {
    generator()->ReleaseKey(ui::VKEY_A, flags);
  }
  void PressB(int flags = ui::EF_NONE) {
    generator()->PressKey(ui::VKEY_B, flags);
  }
  void ReleaseB(int flags = ui::EF_NONE) {
    generator()->ReleaseKey(ui::VKEY_B, flags);
  }

  size_t NumRecordedEvents() {
    return event_recorder()->recorded_event_count();
  }
  ui::EventType LastEventType() {
    return event_recorder()->last_recorded_event_type();
  }
  ui::KeyboardCode LastEventKeyCode() { return LastKeyEvent()->key_code(); }
  const ui::KeyEvent* LastKeyEvent() {
    return event_recorder()->events().back()->AsKeyEvent();
  }

 private:
  // Generates ui::Events to simulate user input.
  raw_ptr<ui::test::EventGenerator> generator_ = nullptr;
  // Records events delivered to the next event rewriter after
  // FilterKeysEventRewriter.
  TestEventRecorder event_recorder_;
  // The FilterKeysEventRewriter instance.
  std::unique_ptr<FilterKeysEventRewriter> event_rewriter_;
};

class FilterKeysEventRewriterBounceKeysTest
    : public FilterKeysEventRewriterTest {
 public:
  static constexpr base::TimeDelta kTestBounceKeysDelay =
      base::Milliseconds(1000);

  FilterKeysEventRewriterBounceKeysTest() = default;
  FilterKeysEventRewriterBounceKeysTest(
      const FilterKeysEventRewriterBounceKeysTest&) = delete;
  FilterKeysEventRewriterBounceKeysTest& operator=(
      const FilterKeysEventRewriterBounceKeysTest&) = delete;
  ~FilterKeysEventRewriterBounceKeysTest() override = default;

  void SetUp() override {
    FilterKeysEventRewriterTest::SetUp();
    event_rewriter()->SetBounceKeysEnabled(true);
    event_rewriter()->SetBounceKeysDelay(kTestBounceKeysDelay);
  }

  void AdvanceClockRelativeToBounceDelay(float multiplier) {
    generator()->AdvanceClock(event_rewriter()->GetBounceKeysDelay() *
                              multiplier);
  }
  void AdvanceClockWithinBounceDelay() {
    AdvanceClockRelativeToBounceDelay(0.05);
  }
  void AdvanceClockPastBounceDelay() { AdvanceClockRelativeToBounceDelay(1.5); }
};

TEST_F(FilterKeysEventRewriterBounceKeysTest,
       KeyboardEventsNotCanceledIfDisabled) {
  event_rewriter()->SetBounceKeysEnabled(false);
  PressA();
  ASSERT_EQ(NumRecordedEvents(), 1u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyPressed);
  ReleaseA();
  ASSERT_EQ(NumRecordedEvents(), 2u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyReleased);
}

TEST_F(FilterKeysEventRewriterBounceKeysTest,
       MouseButtonsNotCanceledIfDisabled) {
  event_rewriter()->SetBounceKeysEnabled(false);
  generator()->PressLeftButton();
  ASSERT_EQ(NumRecordedEvents(), 1u);
  EXPECT_EQ(LastEventType(), ui::EventType::kMousePressed);
  generator()->ReleaseLeftButton();
  ASSERT_EQ(NumRecordedEvents(), 2u);
  EXPECT_EQ(LastEventType(), ui::EventType::kMouseReleased);
}

TEST_F(FilterKeysEventRewriterBounceKeysTest, KeyboardEventsNotCanceled) {
  PressA();
  ASSERT_EQ(NumRecordedEvents(), 1u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyPressed);
  ReleaseA();
  ASSERT_EQ(NumRecordedEvents(), 2u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyReleased);
}

TEST_F(FilterKeysEventRewriterBounceKeysTest, MouseButtonsNotCanceled) {
  generator()->PressLeftButton();
  ASSERT_EQ(NumRecordedEvents(), 1u);
  EXPECT_EQ(LastEventType(), ui::EventType::kMousePressed);
  generator()->ReleaseLeftButton();
  ASSERT_EQ(NumRecordedEvents(), 2u);
  EXPECT_EQ(LastEventType(), ui::EventType::kMouseReleased);
}

TEST_F(FilterKeysEventRewriterBounceKeysTest, SameKeysWithinDelayDiscarded) {
  PressA();
  ASSERT_EQ(NumRecordedEvents(), 1u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyPressed);

  AdvanceClockWithinBounceDelay();
  ReleaseA();
  ASSERT_EQ(NumRecordedEvents(), 2u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyReleased);

  AdvanceClockWithinBounceDelay();
  PressA();
  ASSERT_EQ(NumRecordedEvents(), 2u);

  AdvanceClockWithinBounceDelay();
  ReleaseA();
  ASSERT_EQ(NumRecordedEvents(), 2u);
}

TEST_F(FilterKeysEventRewriterBounceKeysTest, SameKeysAfterDelayAccepted) {
  PressA();
  ASSERT_EQ(NumRecordedEvents(), 1u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyPressed);

  AdvanceClockWithinBounceDelay();
  ReleaseA();
  ASSERT_EQ(NumRecordedEvents(), 2u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyReleased);

  AdvanceClockPastBounceDelay();
  PressA();
  ASSERT_EQ(NumRecordedEvents(), 3u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyPressed);

  AdvanceClockWithinBounceDelay();
  ReleaseA();
  ASSERT_EQ(NumRecordedEvents(), 4u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyReleased);
}

TEST_F(FilterKeysEventRewriterBounceKeysTest,
       KeyPressAndReleaseAcrossDelayBoundaryDiscarded) {
  // t = 0.0
  PressA();
  ASSERT_EQ(NumRecordedEvents(), 1u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyPressed);

  // t = 0.1 (delay until 1.1)
  AdvanceClockRelativeToBounceDelay(0.1);
  ReleaseA();
  ASSERT_EQ(NumRecordedEvents(), 2u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyReleased);

  // t = 0.8 (within delay)
  AdvanceClockRelativeToBounceDelay(0.7);
  PressA();
  ASSERT_EQ(NumRecordedEvents(), 2u);

  // t = 1.2 (after delay and extends delay to 2.2)
  AdvanceClockRelativeToBounceDelay(0.4);
  ReleaseA();
  ASSERT_EQ(NumRecordedEvents(), 2u);

  // t = 1.4
  AdvanceClockRelativeToBounceDelay(0.2);
  PressA();
  ASSERT_EQ(NumRecordedEvents(), 2u);

  // t = 1.5 (extends delay to 2.5)
  AdvanceClockRelativeToBounceDelay(0.1);
  ReleaseA();
  ASSERT_EQ(NumRecordedEvents(), 2u);

  // t = 2.6
  AdvanceClockRelativeToBounceDelay(1.1);
  PressA();
  ASSERT_EQ(NumRecordedEvents(), 3u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyPressed);

  // t = 2.7
  AdvanceClockRelativeToBounceDelay(0.1);
  ReleaseA();
  ASSERT_EQ(NumRecordedEvents(), 4u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyReleased);
}

TEST_F(FilterKeysEventRewriterBounceKeysTest,
       SubsequentKeyReleaseExtendsDelay) {
  PressA();
  ASSERT_EQ(NumRecordedEvents(), 1u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyPressed);

  // t = 0.1, delay until t=1.1
  AdvanceClockRelativeToBounceDelay(0.1);
  ReleaseA();
  ASSERT_EQ(NumRecordedEvents(), 2u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyReleased);

  // t = 0.3
  AdvanceClockRelativeToBounceDelay(0.2);
  PressA();
  ASSERT_EQ(NumRecordedEvents(), 2u);

  // t = 0.9, delay extended to t=1.9
  AdvanceClockRelativeToBounceDelay(0.6);
  ReleaseA();
  ASSERT_EQ(NumRecordedEvents(), 2u);

  // t = 1.4
  AdvanceClockRelativeToBounceDelay(0.5);
  PressA();
  ASSERT_EQ(NumRecordedEvents(), 2u);

  // t = 1.5, delay extended to t=2.5
  AdvanceClockRelativeToBounceDelay(0.1);
  ReleaseA();
  ASSERT_EQ(NumRecordedEvents(), 2u);

  // t = 2.6
  AdvanceClockRelativeToBounceDelay(1.1);
  PressA();
  ASSERT_EQ(NumRecordedEvents(), 3u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyPressed);

  // t = 2.7
  AdvanceClockRelativeToBounceDelay(0.1);
  ReleaseA();
  ASSERT_EQ(NumRecordedEvents(), 4u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyReleased);
}

TEST_F(FilterKeysEventRewriterBounceKeysTest, ModifierKeysDebounced) {
  generator()->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
  ASSERT_EQ(NumRecordedEvents(), 1u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyPressed);

  AdvanceClockWithinBounceDelay();
  generator()->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);
  ASSERT_EQ(NumRecordedEvents(), 2u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyReleased);

  AdvanceClockWithinBounceDelay();
  generator()->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
  ASSERT_EQ(NumRecordedEvents(), 2u);

  AdvanceClockWithinBounceDelay();
  generator()->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);
  ASSERT_EQ(NumRecordedEvents(), 2u);
}

TEST_F(FilterKeysEventRewriterBounceKeysTest, DifferentKeysResetDelay) {
  // t = 0.0
  PressA();
  ASSERT_EQ(NumRecordedEvents(), 1u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyPressed);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_A);

  // t = 0.1 (delay extended to 1.1)
  AdvanceClockRelativeToBounceDelay(0.1);
  ReleaseA();
  ASSERT_EQ(NumRecordedEvents(), 2u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyReleased);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_A);

  // t = 0.4 (different key resets delay)
  AdvanceClockRelativeToBounceDelay(0.3);
  PressB();
  ASSERT_EQ(NumRecordedEvents(), 3u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyPressed);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_B);

  // t = 0.5 (delay extended to 1.5)
  AdvanceClockRelativeToBounceDelay(0.1);
  ReleaseB();
  ASSERT_EQ(NumRecordedEvents(), 4u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyReleased);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_B);

  // t = 0.7
  AdvanceClockRelativeToBounceDelay(0.2);
  PressB();
  ASSERT_EQ(NumRecordedEvents(), 4u);

  // t = 0.8 (delay extended to 1.8)
  AdvanceClockRelativeToBounceDelay(0.1);
  ReleaseB();
  ASSERT_EQ(NumRecordedEvents(), 4u);

  // t = 0.9 (different key resets delay)
  AdvanceClockRelativeToBounceDelay(0.1);
  PressA();
  ASSERT_EQ(NumRecordedEvents(), 5u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyPressed);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_A);

  // t = 1.0 (delay extended to 2.0)
  AdvanceClockRelativeToBounceDelay(0.1);
  ReleaseA();
  ASSERT_EQ(NumRecordedEvents(), 6u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyReleased);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_A);
}

TEST_F(FilterKeysEventRewriterBounceKeysTest, MultipleKeys_ADownBDownAUpBUp) {
  AdvanceClockWithinBounceDelay();
  PressA();
  ASSERT_EQ(NumRecordedEvents(), 1u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyPressed);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_A);

  AdvanceClockWithinBounceDelay();
  PressB();
  ASSERT_EQ(NumRecordedEvents(), 2u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyPressed);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_B);

  AdvanceClockWithinBounceDelay();
  ReleaseA();
  ASSERT_EQ(NumRecordedEvents(), 3u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyReleased);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_A);

  AdvanceClockWithinBounceDelay();
  ReleaseB();
  ASSERT_EQ(NumRecordedEvents(), 4u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyReleased);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_B);

  // Verify that delay is reset for A.
  AdvanceClockWithinBounceDelay();
  PressA();
  ASSERT_EQ(NumRecordedEvents(), 5u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyPressed);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_A);

  AdvanceClockWithinBounceDelay();
  PressB();
  ASSERT_EQ(NumRecordedEvents(), 6u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyPressed);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_B);

  AdvanceClockWithinBounceDelay();
  ReleaseA();
  ASSERT_EQ(NumRecordedEvents(), 7u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyReleased);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_A);

  AdvanceClockWithinBounceDelay();
  ReleaseB();
  ASSERT_EQ(NumRecordedEvents(), 8u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyReleased);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_B);
}

TEST_F(FilterKeysEventRewriterBounceKeysTest, MultipleKeys_ADownBDownBUpAUp) {
  AdvanceClockWithinBounceDelay();
  PressA();
  ASSERT_EQ(NumRecordedEvents(), 1u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyPressed);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_A);

  AdvanceClockWithinBounceDelay();
  PressB();
  ASSERT_EQ(NumRecordedEvents(), 2u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyPressed);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_B);

  AdvanceClockWithinBounceDelay();
  ReleaseB();
  ASSERT_EQ(NumRecordedEvents(), 3u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyReleased);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_B);

  AdvanceClockWithinBounceDelay();
  ReleaseA();
  ASSERT_EQ(NumRecordedEvents(), 4u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyReleased);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_A);

  // Verify that delay is reset for A.
  AdvanceClockWithinBounceDelay();
  PressA();
  ASSERT_EQ(NumRecordedEvents(), 5u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyPressed);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_A);

  AdvanceClockWithinBounceDelay();
  PressB();
  ASSERT_EQ(NumRecordedEvents(), 6u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyPressed);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_B);

  AdvanceClockWithinBounceDelay();
  ReleaseB();
  ASSERT_EQ(NumRecordedEvents(), 7u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyReleased);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_B);

  AdvanceClockWithinBounceDelay();
  ReleaseA();
  ASSERT_EQ(NumRecordedEvents(), 8u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyReleased);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_A);
}

TEST_F(FilterKeysEventRewriterBounceKeysTest,
       MultipleKeys_HoldAThenPressBMultipleTimes) {
  PressA();
  ASSERT_EQ(NumRecordedEvents(), 1u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyPressed);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_A);

  AdvanceClockWithinBounceDelay();
  PressB();
  ASSERT_EQ(NumRecordedEvents(), 2u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyPressed);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_B);

  AdvanceClockWithinBounceDelay();
  ReleaseB();
  ASSERT_EQ(NumRecordedEvents(), 3u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyReleased);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_B);

  AdvanceClockWithinBounceDelay();
  PressB();
  ASSERT_EQ(NumRecordedEvents(), 3u);

  AdvanceClockWithinBounceDelay();
  ReleaseB();
  ASSERT_EQ(NumRecordedEvents(), 3u);

  AdvanceClockWithinBounceDelay();
  ReleaseA();
  ASSERT_EQ(NumRecordedEvents(), 4u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyReleased);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_A);

  // Release of A should not reset delay.
  AdvanceClockWithinBounceDelay();
  PressB();
  ASSERT_EQ(NumRecordedEvents(), 4u);

  AdvanceClockWithinBounceDelay();
  ReleaseB();
  ASSERT_EQ(NumRecordedEvents(), 4u);
}

TEST_F(FilterKeysEventRewriterBounceKeysTest, AutoRepeatKeysAccepted) {
  PressA();
  ASSERT_EQ(NumRecordedEvents(), 1u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyPressed);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_A);
  EXPECT_FALSE(LastKeyEvent()->is_repeat());

  AdvanceClockWithinBounceDelay();
  PressA(ui::EF_IS_REPEAT);
  ASSERT_EQ(NumRecordedEvents(), 2u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyPressed);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_A);
  EXPECT_TRUE(LastKeyEvent()->is_repeat());

  AdvanceClockWithinBounceDelay();
  ReleaseA();
  ASSERT_EQ(NumRecordedEvents(), 3u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyReleased);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_A);
  EXPECT_FALSE(LastKeyEvent()->is_repeat());

  AdvanceClockWithinBounceDelay();
  PressA();
  ASSERT_EQ(NumRecordedEvents(), 3u);

  // This happens because repeat keys are generated earlier.
  AdvanceClockWithinBounceDelay();
  PressA(ui::EF_IS_REPEAT);
  ASSERT_EQ(NumRecordedEvents(), 4u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyPressed);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_A);
  EXPECT_TRUE(LastKeyEvent()->is_repeat());

  AdvanceClockWithinBounceDelay();
  ReleaseA();
  ASSERT_EQ(NumRecordedEvents(), 4u);
}

TEST_F(FilterKeysEventRewriterBounceKeysTest, SynthesizedKeysAccepted) {
  PressA();
  ASSERT_EQ(NumRecordedEvents(), 1u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyPressed);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_A);

  AdvanceClockWithinBounceDelay();
  ReleaseA();
  ASSERT_EQ(NumRecordedEvents(), 2u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyReleased);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_A);

  AdvanceClockWithinBounceDelay();
  PressA(ui::EF_IS_SYNTHESIZED);
  ASSERT_EQ(NumRecordedEvents(), 3u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyPressed);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_A);
  EXPECT_TRUE(LastKeyEvent()->IsSynthesized());

  ReleaseA(ui::EF_IS_SYNTHESIZED);
  ASSERT_EQ(NumRecordedEvents(), 4u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyReleased);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_A);
  EXPECT_TRUE(LastKeyEvent()->IsSynthesized());
}

TEST_F(FilterKeysEventRewriterBounceKeysTest, DisablingBounceKeysResetsState) {
  PressA();
  ASSERT_EQ(NumRecordedEvents(), 1u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyPressed);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_A);

  AdvanceClockWithinBounceDelay();
  ReleaseA();
  ASSERT_EQ(NumRecordedEvents(), 2u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyReleased);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_A);

  // Toggle bounce keys off and on should reset internal state.
  event_rewriter()->SetBounceKeysEnabled(false);
  event_rewriter()->SetBounceKeysEnabled(true);

  AdvanceClockWithinBounceDelay();
  PressA();
  ASSERT_EQ(NumRecordedEvents(), 3u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyPressed);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_A);

  ReleaseA();
  ASSERT_EQ(NumRecordedEvents(), 4u);
  EXPECT_EQ(LastEventType(), ui::EventType::kKeyReleased);
  EXPECT_EQ(LastEventKeyCode(), ui::VKEY_A);
}

}  // namespace ash
