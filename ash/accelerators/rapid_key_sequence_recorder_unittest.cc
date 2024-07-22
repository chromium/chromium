// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/rapid_key_sequence_recorder.h"

#include "ash/public/cpp/accelerators_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/event_type.h"

namespace ash {

class RapidKeySequenceRecorderTest : public AshTestBase {
 public:
  RapidKeySequenceRecorderTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    AshTestBase::SetUp();
    rapid_key_sequence_recorder_ = std::make_unique<RapidKeySequenceRecorder>();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    rapid_key_sequence_recorder_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<RapidKeySequenceRecorder> rapid_key_sequence_recorder_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;

  void AdvanceClock(base::TimeDelta time) {
    task_environment()->AdvanceClock(time);
  }

  base::TimeTicks GetNowTimestamp() { return task_environment()->NowTicks(); }

  const ui::KeyEvent LeftShiftKeyEvent() {
    return ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_LSHIFT,
                        ui::DomCode::SHIFT_LEFT, ui::EF_NONE,
                        GetNowTimestamp());
  }
  const ui::KeyEvent RightShiftKeyEvent() {
    return ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_RSHIFT,
                        ui::DomCode::SHIFT_RIGHT, ui::EF_NONE,
                        GetNowTimestamp());
  }
};

TEST_F(RapidKeySequenceRecorderTest, SingleShiftPress) {
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(LeftShiftKeyEvent());

  // Wait a few seconds to reset the double-tap window.
  AdvanceClock(base::Milliseconds(3000));

  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(RightShiftKeyEvent());

  // No samples should have been recorded for any of the above events.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.DoubleTapLeftShiftDuration", 0);
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.DoubleTapRightShiftDuration", 0);
}

TEST_F(RapidKeySequenceRecorderTest, LeftShiftTappedTwice) {
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(LeftShiftKeyEvent());
  AdvanceClock(base::Milliseconds(100));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(LeftShiftKeyEvent());
  histogram_tester_->ExpectUniqueTimeSample(
      "ChromeOS.Inputs.DoubleTapLeftShiftDuration", base::Milliseconds(100), 1);
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.DoubleTapLeftShiftDuration", 1);
  // Right shift metric should not be recorded.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.DoubleTapRightShiftDuration", 0);
}

TEST_F(RapidKeySequenceRecorderTest, RightShiftTappedTwice) {
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(RightShiftKeyEvent());
  AdvanceClock(base::Milliseconds(100));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(RightShiftKeyEvent());
  histogram_tester_->ExpectTimeBucketCount(
      "ChromeOS.Inputs.DoubleTapRightShiftDuration", base::Milliseconds(100),
      1);
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.DoubleTapRightShiftDuration", 1);
  // Left shift metric should not be recorded.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.DoubleTapLeftShiftDuration", 0);
}

TEST_F(RapidKeySequenceRecorderTest, RightShiftOtherKeyPressedTogether) {
  const auto press_other_key_event =
      ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_A, ui::DomCode::US_A,
                   ui::EF_NONE, GetNowTimestamp());
  const auto release_other_key_event =
      ui::KeyEvent(ui::EventType::kKeyReleased, ui::VKEY_A, ui::DomCode::US_A,
                   ui::EF_NONE, GetNowTimestamp());
  const auto release_shift_key_event =
      ui::KeyEvent(ui::EventType::kKeyReleased, ui::VKEY_RSHIFT,
                   ui::DomCode::SHIFT_RIGHT, ui::EF_NONE, GetNowTimestamp());

  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(
      press_other_key_event);
  AdvanceClock(base::Milliseconds(50));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(RightShiftKeyEvent());
  AdvanceClock(base::Milliseconds(50));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(
      release_other_key_event);
  AdvanceClock(base::Milliseconds(50));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(
      release_shift_key_event);
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(RightShiftKeyEvent());
  AdvanceClock(base::Milliseconds(50));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(
      release_shift_key_event);
  // Right shift metric should not be recorded.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.DoubleTapRightShiftDuration", 0);
  // Left shift metric should not be recorded.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.DoubleTapLeftShiftDuration", 0);
}

TEST_F(RapidKeySequenceRecorderTest, AlternatingShiftLocations) {
  // RightShift then LeftShift:
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(RightShiftKeyEvent());
  AdvanceClock(base::Milliseconds(100));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(LeftShiftKeyEvent());

  // Wait a few seconds to reset the double-tap window.
  AdvanceClock(base::Milliseconds(3000));

  // LeftShift then RightShift:
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(LeftShiftKeyEvent());
  AdvanceClock(base::Milliseconds(100));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(RightShiftKeyEvent());

  // No metrics should have been recorded.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.DoubleTapRightShiftDuration", 0);
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.DoubleTapLeftShiftDuration", 0);
}

TEST_F(RapidKeySequenceRecorderTest, ThreeRapidPressesLeftShift) {
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(LeftShiftKeyEvent());
  AdvanceClock(base::Milliseconds(50));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(LeftShiftKeyEvent());
  AdvanceClock(base::Milliseconds(73));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(LeftShiftKeyEvent());

  // When three keys are consecutively pressed, only one metric for the first
  // two presses should be emitted.
  histogram_tester_->ExpectTimeBucketCount(
      "ChromeOS.Inputs.DoubleTapLeftShiftDuration", base::Milliseconds(50), 1);
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.DoubleTapLeftShiftDuration", 1);
  // Right shift metric should not be recorded.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.DoubleTapRightShiftDuration", 0);
}

TEST_F(RapidKeySequenceRecorderTest, ThreeRapidPressesRightShift) {
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(RightShiftKeyEvent());
  AdvanceClock(base::Milliseconds(50));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(RightShiftKeyEvent());
  AdvanceClock(base::Milliseconds(73));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(RightShiftKeyEvent());

  // When three keys are consecutively pressed, only one metric for the first
  // two presses should be emitted.
  histogram_tester_->ExpectTimeBucketCount(
      "ChromeOS.Inputs.DoubleTapRightShiftDuration", base::Milliseconds(50), 1);
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.DoubleTapRightShiftDuration", 1);
  // Left shift metric should not be recorded.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.DoubleTapLeftShiftDuration", 0);
}

TEST_F(RapidKeySequenceRecorderTest, FiveRapidPressesLeftShift) {
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(LeftShiftKeyEvent());
  AdvanceClock(base::Milliseconds(50));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(LeftShiftKeyEvent());
  AdvanceClock(base::Milliseconds(60));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(LeftShiftKeyEvent());
  AdvanceClock(base::Milliseconds(70));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(LeftShiftKeyEvent());
  AdvanceClock(base::Milliseconds(80));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(LeftShiftKeyEvent());

  // When five keys are consecutively pressed, two metrics (for the first
  // four presses) should be emitted.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.DoubleTapLeftShiftDuration", 2);
  histogram_tester_->ExpectTimeBucketCount(
      "ChromeOS.Inputs.DoubleTapLeftShiftDuration", base::Milliseconds(50), 1);
  histogram_tester_->ExpectTimeBucketCount(
      "ChromeOS.Inputs.DoubleTapLeftShiftDuration", base::Milliseconds(70), 1);
  // Right shift metric should not be recorded.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.DoubleTapRightShiftDuration", 0);
}

TEST_F(RapidKeySequenceRecorderTest, FiveRapidPressesRightShift) {
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(RightShiftKeyEvent());
  AdvanceClock(base::Milliseconds(50));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(RightShiftKeyEvent());
  AdvanceClock(base::Milliseconds(60));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(RightShiftKeyEvent());
  AdvanceClock(base::Milliseconds(70));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(RightShiftKeyEvent());
  AdvanceClock(base::Milliseconds(80));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(RightShiftKeyEvent());

  // When five keys are consecutively pressed, two metrics (for the first
  // four presses) should be emitted.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.DoubleTapRightShiftDuration", 2);
  histogram_tester_->ExpectTimeBucketCount(
      "ChromeOS.Inputs.DoubleTapRightShiftDuration", base::Milliseconds(50), 1);
  histogram_tester_->ExpectTimeBucketCount(
      "ChromeOS.Inputs.DoubleTapRightShiftDuration", base::Milliseconds(70), 1);
  // Left shift metric should not be recorded.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.DoubleTapLeftShiftDuration", 0);
}

TEST_F(RapidKeySequenceRecorderTest, TimingWindowCloseToLimit) {
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(LeftShiftKeyEvent());
  AdvanceClock(base::Milliseconds(499));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(LeftShiftKeyEvent());

  // Only record one sample (for the 499ms delay double-tap).
  histogram_tester_->ExpectUniqueTimeSample(
      "ChromeOS.Inputs.DoubleTapLeftShiftDuration", base::Milliseconds(499), 1);
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.DoubleTapLeftShiftDuration", 1);
}

TEST_F(RapidKeySequenceRecorderTest, TimingWindowAtLimit) {
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(LeftShiftKeyEvent());
  AdvanceClock(base::Milliseconds(500));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(LeftShiftKeyEvent());

  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.DoubleTapLeftShiftDuration", 0);
}

TEST_F(RapidKeySequenceRecorderTest, TimingWindowExceedsLimit) {
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(LeftShiftKeyEvent());
  AdvanceClock(base::Milliseconds(2000));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(LeftShiftKeyEvent());

  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.DoubleTapLeftShiftDuration", 0);
}

TEST_F(RapidKeySequenceRecorderTest, StartWithShiftThenWait) {
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(LeftShiftKeyEvent());
  AdvanceClock(base::Milliseconds(3000));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(LeftShiftKeyEvent());
  AdvanceClock(base::Milliseconds(50));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(LeftShiftKeyEvent());

  histogram_tester_->ExpectUniqueTimeSample(
      "ChromeOS.Inputs.DoubleTapLeftShiftDuration", base::Milliseconds(50), 1);
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.DoubleTapLeftShiftDuration", 1);
}

TEST_F(RapidKeySequenceRecorderTest, StartWithShiftThenWaitThenThreeShifts) {
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(LeftShiftKeyEvent());
  AdvanceClock(base::Milliseconds(3000));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(LeftShiftKeyEvent());
  AdvanceClock(base::Milliseconds(50));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(LeftShiftKeyEvent());
  AdvanceClock(base::Milliseconds(75));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(LeftShiftKeyEvent());

  // Only one sample should be recorded.
  histogram_tester_->ExpectUniqueTimeSample(
      "ChromeOS.Inputs.DoubleTapLeftShiftDuration", base::Milliseconds(50), 1);
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.DoubleTapLeftShiftDuration", 1);
}

TEST_F(RapidKeySequenceRecorderTest, OtherKeys) {
  ui::KeyEvent alpha_key_no_modifier(ui::EventType::kKeyPressed, ui::VKEY_C,
                                     ui::EF_NONE, GetNowTimestamp());
  // Other key, then left shift
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(
      alpha_key_no_modifier);
  AdvanceClock(base::Milliseconds(50));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(LeftShiftKeyEvent());

  // Wait a few seconds to reset the double-tap window.
  AdvanceClock(base::Milliseconds(3000));

  // Left shift, then other key
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(LeftShiftKeyEvent());
  AdvanceClock(base::Milliseconds(50));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(
      alpha_key_no_modifier);

  // Wait a few seconds to reset the double-tap window.
  AdvanceClock(base::Milliseconds(3000));

  // Other key, then right shift
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(
      alpha_key_no_modifier);
  AdvanceClock(base::Milliseconds(50));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(RightShiftKeyEvent());

  // Wait a few seconds to reset the double-tap window.
  AdvanceClock(base::Milliseconds(3000));

  // Right shift, then other key
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(RightShiftKeyEvent());
  AdvanceClock(base::Milliseconds(50));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(
      alpha_key_no_modifier);

  // Wait a few seconds to reset the double-tap window.
  AdvanceClock(base::Milliseconds(3000));

  // Right shift, then other key, then right shift again
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(RightShiftKeyEvent());
  AdvanceClock(base::Milliseconds(50));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(
      alpha_key_no_modifier);
  AdvanceClock(base::Milliseconds(50));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(RightShiftKeyEvent());

  // No samples should have been recorded for any of the above events.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.DoubleTapLeftShiftDuration", 0);
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.DoubleTapRightShiftDuration", 0);
}

TEST_F(RapidKeySequenceRecorderTest, ShiftAndOtherModifiers) {
  ui::KeyEvent ctrl_shift_t(ui::EventType::kKeyPressed, ui::VKEY_T,
                            ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
                            GetNowTimestamp());

  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(ctrl_shift_t);
  AdvanceClock(base::Milliseconds(50));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(ctrl_shift_t);

  // No samples should have been recorded for any of the above events.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.DoubleTapLeftShiftDuration", 0);
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.DoubleTapRightShiftDuration", 0);
}

TEST_F(RapidKeySequenceRecorderTest, ShiftAndAlpha) {
  ui::KeyEvent shift_t(ui::EventType::kKeyPressed, ui::VKEY_T,
                       ui::EF_SHIFT_DOWN, GetNowTimestamp());

  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(shift_t);
  AdvanceClock(base::Milliseconds(50));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(shift_t);

  // No samples should have been recorded for any of the above events.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.DoubleTapLeftShiftDuration", 0);
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.DoubleTapRightShiftDuration", 0);
}

TEST_F(RapidKeySequenceRecorderTest, UnknownKeyAndShift) {
  ui::KeyEvent non_keypress_with_shift(ui::EventType::kKeyPressed,
                                       ui::VKEY_UNKNOWN, ui::DomCode::NONE,
                                       ui::EF_SHIFT_DOWN, GetNowTimestamp());

  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(
      non_keypress_with_shift);
  AdvanceClock(base::Milliseconds(50));
  rapid_key_sequence_recorder_->OnPrerewriteKeyInputEvent(
      non_keypress_with_shift);

  // No samples should have been recorded for any of the above events.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.DoubleTapLeftShiftDuration", 0);
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Inputs.DoubleTapRightShiftDuration", 0);
}

}  // namespace ash
