// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/touch_usage_metrics_recorder.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"

namespace ash {
namespace {

// Test fixture for the UsageMetricsRecorder class.
class TouchUsageMetricsRecorderTest : public AshTestBase {
 public:
  TouchUsageMetricsRecorderTest()
      : AshTestBase{base::test::TaskEnvironment::TimeSource::MOCK_TIME} {}
  TouchUsageMetricsRecorderTest(const TouchUsageMetricsRecorderTest&) = delete;
  TouchUsageMetricsRecorderTest& operator=(
      const TouchUsageMetricsRecorderTest&) = delete;

  ~TouchUsageMetricsRecorderTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    event_generator_ = GetEventGenerator();
  }

 protected:
  // Used to verify recorded data.
  base::HistogramTester histogram_tester_;

  // Used to generate input events.
  raw_ptr<ui::test::EventGenerator, DanglingUntriaged> event_generator_;
};

// Verifies that a singular TouchscreenUsageRecorder, tested in isolation, does
// not generate a usage session after a singular touch is detected.
TEST_F(TouchUsageMetricsRecorderTest, NoUsageSingleTouch) {
  constexpr char dummy_histogram_name[] =
      "ash.metrics.touch_usage_metrics_recorder_unittest.no_usage_single_touch";
  base::TimeDelta timer_duration = base::Minutes(10);
  base::TimeDelta max_time = base::Hours(1);
  TouchscreenUsageRecorder recorder = TouchscreenUsageRecorder(
      dummy_histogram_name, timer_duration, max_time, false);
  recorder.RecordTouch();
  task_environment()->FastForwardBy(timer_duration + base::Seconds(1));
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(dummy_histogram_name, false), 0);
}

// Verifies that a singular TouchscreenUsageRecorder, tested in isolation,
// generates a usage session after two touches are detected within
// `timer_duration` of one another.
TEST_F(TouchUsageMetricsRecorderTest, DoubleTouch) {
  constexpr char dummy_histogram_name[] =
      "ash.metrics.touch_usage_metrics_recorder_unittest.double_touch";
  base::TimeDelta timer_duration = base::Minutes(10);
  base::TimeDelta max_time = base::Hours(1);
  TouchscreenUsageRecorder recorder = TouchscreenUsageRecorder(
      dummy_histogram_name, timer_duration, max_time, false);
  recorder.RecordTouch();
  task_environment()->FastForwardBy(timer_duration - base::Seconds(1));
  recorder.RecordTouch();
  task_environment()->FastForwardBy(timer_duration + base::Seconds(1));
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(dummy_histogram_name, false), 1);
}

// Verifies that a singular TouchscreenUsageRecorder, tested in isolation, does
// not generate a usage session after two touches are detected that are more
// than `timer_duration` apart from one another.
TEST_F(TouchUsageMetricsRecorderTest, ExtendedDoubleTouch) {
  constexpr char dummy_histogram_name[] =
      "ash.metrics.touch_usage_metrics_recorder_unittest.extended_double_touch";
  base::TimeDelta timer_duration = base::Minutes(10);
  base::TimeDelta max_time = base::Hours(1);
  TouchscreenUsageRecorder recorder = TouchscreenUsageRecorder(
      dummy_histogram_name, timer_duration, max_time, false);
  recorder.RecordTouch();
  task_environment()->FastForwardBy(timer_duration + base::Seconds(1));
  recorder.RecordTouch();
  task_environment()->FastForwardBy(timer_duration + base::Seconds(1));
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(dummy_histogram_name, false), 0);
}

// Verifies that a singular TouchscreenUsageRecorder, tested in isolation,
// generates one usage session after three touches are detected, with each being
// no more than `timer_duration` apart from the prior.
TEST_F(TouchUsageMetricsRecorderTest, SingleUsageTripleTouch) {
  constexpr char dummy_histogram_name[] =
      "ash.metrics.touch_usage_metrics_recorder_unittest.single_usage_triple_"
      "touch";
  base::TimeDelta timer_duration = base::Minutes(10);
  base::TimeDelta max_time = base::Hours(1);
  TouchscreenUsageRecorder recorder = TouchscreenUsageRecorder(
      dummy_histogram_name, timer_duration, max_time, false);
  recorder.RecordTouch();
  task_environment()->FastForwardBy(timer_duration - base::Seconds(1));
  recorder.RecordTouch();
  task_environment()->FastForwardBy(timer_duration - base::Seconds(1));
  recorder.RecordTouch();
  task_environment()->FastForwardBy(timer_duration + base::Seconds(1));
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(dummy_histogram_name, false), 1);
}

// Verifies that a single touch, in clamshell mode, will not create a usage
// session.
TEST_F(TouchUsageMetricsRecorderTest, NoUsageSingleTouchClamshell) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  event_generator_->PressTouchId(0);
  task_environment()->FastForwardBy(base::Minutes(11));

  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage5SecondsHistogramName, false),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage15SecondsHistogramName, false),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage30SecondsHistogramName, false),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage1MinuteHistogramName, false),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage3MinutesHistogramName, false),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage5MinutesHistogramName, false),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage10MinutesHistogramName, false),
      0);
}

// Verifies that a single touch, in tablet mode, will not create a usage
// session.
TEST_F(TouchUsageMetricsRecorderTest, NoUsageSingleTouchTablet) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  event_generator_->PressTouchId(0);
  task_environment()->FastForwardBy(base::Minutes(11));

  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage5SecondsHistogramName, true),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage15SecondsHistogramName, true),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage30SecondsHistogramName, true),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage1MinuteHistogramName, true), 0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage3MinutesHistogramName, true),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage5MinutesHistogramName, true),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage10MinutesHistogramName, true),
      0);
}

// Verifies that touches in clamshell mode and tablet mode are independent of
// one another with regards to usage tracking. This means that two touches,
// one in each mode, will not create a usage session.
TEST_F(TouchUsageMetricsRecorderTest, NoUsageClamshellAndTabletSingleTouch) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  event_generator_->PressTouchId(0);
  task_environment()->FastForwardBy(base::Seconds(1));
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  event_generator_->PressTouchId(1);
  task_environment()->FastForwardBy(base::Minutes(11));

  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage5SecondsHistogramName, false),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage15SecondsHistogramName, false),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage30SecondsHistogramName, false),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage1MinuteHistogramName, false),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage3MinutesHistogramName, false),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage5MinutesHistogramName, false),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage10MinutesHistogramName, false),
      0);

  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage5SecondsHistogramName, true),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage15SecondsHistogramName, true),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage30SecondsHistogramName, true),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage1MinuteHistogramName, true), 0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage3MinutesHistogramName, true),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage5MinutesHistogramName, true),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage10MinutesHistogramName, true),
      0);
}

// Verifies that two touches, in clamshell mode, in quick succession, will
// generate a usage session in clamshell mode, and not one in tablet mode.
TEST_F(TouchUsageMetricsRecorderTest, DoubleTouchClamshell) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  event_generator_->PressTouchId(0);
  task_environment()->FastForwardBy(base::Seconds(1));
  event_generator_->PressTouchId(1);
  task_environment()->FastForwardBy(base::Minutes(11));

  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage5SecondsHistogramName, false),
      1);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage15SecondsHistogramName, false),
      1);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage30SecondsHistogramName, false),
      1);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage1MinuteHistogramName, false),
      1);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage3MinutesHistogramName, false),
      1);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage5MinutesHistogramName, false),
      1);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage10MinutesHistogramName, false),
      1);

  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage5SecondsHistogramName, true),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage15SecondsHistogramName, true),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage30SecondsHistogramName, true),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage1MinuteHistogramName, true), 0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage3MinutesHistogramName, true),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage5MinutesHistogramName, true),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage10MinutesHistogramName, true),
      0);
}

// Verifies that two touches, in tablet mode, in quick succession, will
// generate a usage session in tablet mode, and not one in clamshell mode.
TEST_F(TouchUsageMetricsRecorderTest, DoubleTouchTablet) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  event_generator_->PressTouchId(0);
  task_environment()->FastForwardBy(base::Seconds(1));
  event_generator_->PressTouchId(1);
  task_environment()->FastForwardBy(base::Minutes(11));

  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage5SecondsHistogramName, false),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage15SecondsHistogramName, false),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage30SecondsHistogramName, false),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage1MinuteHistogramName, false),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage3MinutesHistogramName, false),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage5MinutesHistogramName, false),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage10MinutesHistogramName, false),
      0);

  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage5SecondsHistogramName, true),
      1);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage15SecondsHistogramName, true),
      1);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage30SecondsHistogramName, true),
      1);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage1MinuteHistogramName, true), 1);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage3MinutesHistogramName, true),
      1);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage5MinutesHistogramName, true),
      1);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage10MinutesHistogramName, true),
      1);
}

// Verifies that two touches, in clamshell mode, with significant time
// between them, will not generate a usage session.
TEST_F(TouchUsageMetricsRecorderTest, ExtendedDoubleTouchClamshell) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  event_generator_->PressTouchId(0);
  task_environment()->FastForwardBy(base::Minutes(11));
  event_generator_->PressTouchId(1);
  task_environment()->FastForwardBy(base::Minutes(11));

  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage5SecondsHistogramName, false),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage15SecondsHistogramName, false),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage30SecondsHistogramName, false),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage1MinuteHistogramName, false),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage3MinutesHistogramName, false),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage5MinutesHistogramName, false),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage10MinutesHistogramName, false),
      0);
}

// Verifies that two touches, in tablet mode, with significant time between
// them, will not generate a usage session.
TEST_F(TouchUsageMetricsRecorderTest, ExtendedDoubleTouchTablet) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  event_generator_->PressTouchId(0);
  task_environment()->FastForwardBy(base::Minutes(11));
  event_generator_->PressTouchId(1);
  task_environment()->FastForwardBy(base::Minutes(11));

  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage5SecondsHistogramName, true),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage15SecondsHistogramName, true),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage30SecondsHistogramName, true),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage1MinuteHistogramName, true), 0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage3MinutesHistogramName, true),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage5MinutesHistogramName, true),
      0);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage10MinutesHistogramName, true),
      0);
}

// Verifies that multiple touches, in clamshell mode, with varying times
// between them, will create the correct number of usage session in their
// respective histograms.
TEST_F(TouchUsageMetricsRecorderTest, MultiTouchClamshell) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  event_generator_->PressTouchId(0);
  task_environment()->FastForwardBy(base::Seconds(1));
  event_generator_->PressTouchId(1);
  task_environment()->FastForwardBy(base::Seconds(31));
  event_generator_->PressTouchId(2);
  task_environment()->FastForwardBy(base::Seconds(1));
  event_generator_->PressTouchId(3);
  task_environment()->FastForwardBy(base::Minutes(4));
  event_generator_->PressTouchId(4);
  task_environment()->FastForwardBy(base::Seconds(1));
  event_generator_->PressTouchId(5);
  task_environment()->FastForwardBy(base::Minutes(11));

  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage5SecondsHistogramName, false),
      3);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage15SecondsHistogramName, false),
      3);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage30SecondsHistogramName, false),
      3);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage1MinuteHistogramName, false),
      2);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage3MinutesHistogramName, false),
      2);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage5MinutesHistogramName, false),
      1);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage10MinutesHistogramName, false),
      1);
}

// Verifies that multiple touches, in tablet mode, with varying times between
// them, will create the correct number of usage session in their respective
// histograms.
TEST_F(TouchUsageMetricsRecorderTest, MultiTouchTabletMode) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  event_generator_->PressTouchId(0);
  task_environment()->FastForwardBy(base::Seconds(1));
  event_generator_->PressTouchId(1);
  task_environment()->FastForwardBy(base::Seconds(31));
  event_generator_->PressTouchId(2);
  task_environment()->FastForwardBy(base::Seconds(1));
  event_generator_->PressTouchId(3);
  task_environment()->FastForwardBy(base::Minutes(4));
  event_generator_->PressTouchId(4);
  task_environment()->FastForwardBy(base::Seconds(1));
  event_generator_->PressTouchId(5);
  task_environment()->FastForwardBy(base::Minutes(11));

  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage5SecondsHistogramName, true),
      3);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage15SecondsHistogramName, true),
      3);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage30SecondsHistogramName, true),
      3);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage1MinuteHistogramName, true), 2);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage3MinutesHistogramName, true),
      2);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage5MinutesHistogramName, true),
      1);
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithMode(kTouchscreenUsage10MinutesHistogramName, true),
      1);
}

}  // namespace

}  // namespace ash
