// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/omnibox/suggestions_omt_input_anchor.h"

#include <android/input.h>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace omnibox {
namespace {

constexpr const char kTouchDownDelayHistogram[] =
    "Android.Omnibox.OMTPrefetch.TouchDownDelay";

class SuggestionsOmtInputAnchorNativeTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
};

TEST_F(SuggestionsOmtInputAnchorNativeTest, NonMotionEvent_IgnoredAndNoMetric) {
  int64_t event_time_ns = (base::TimeTicks::Now() - base::Milliseconds(50))
                              .since_origin()
                              .InNanoseconds();

  // Key events or non-motion events should be ignored without histogram
  // emission.
  bool handled = ProcessMotionEvent(
      /*context=*/nullptr, AINPUT_EVENT_TYPE_KEY, AMOTION_EVENT_ACTION_DOWN,
      event_time_ns, /*trigger_transfer=*/false);

  EXPECT_FALSE(handled);
  histogram_tester_.ExpectTotalCount(kTouchDownDelayHistogram, 0);
}

TEST_F(SuggestionsOmtInputAnchorNativeTest,
       MotionEventNonDownAction_IgnoredAndNoMetric) {
  int64_t event_time_ns = (base::TimeTicks::Now() - base::Milliseconds(50))
                              .since_origin()
                              .InNanoseconds();

  const int32_t kNonDownActions[] = {
      AMOTION_EVENT_ACTION_UP,         AMOTION_EVENT_ACTION_MOVE,
      AMOTION_EVENT_ACTION_CANCEL,     AMOTION_EVENT_ACTION_POINTER_DOWN,
      AMOTION_EVENT_ACTION_POINTER_UP, AMOTION_EVENT_ACTION_HOVER_MOVE,
  };

  for (int32_t action : kNonDownActions) {
    bool handled = ProcessMotionEvent(
        /*context=*/nullptr, AINPUT_EVENT_TYPE_MOTION, action, event_time_ns,
        /*trigger_transfer=*/false);
    EXPECT_FALSE(handled);
  }

  histogram_tester_.ExpectTotalCount(kTouchDownDelayHistogram, 0);
}

TEST_F(SuggestionsOmtInputAnchorNativeTest,
       MotionEventDown_CalculatesDelayAndEmitsMetric) {
  int64_t event_time_ns = (base::TimeTicks::Now() - base::Milliseconds(42))
                              .since_origin()
                              .InNanoseconds();

  // Valid ACTION_DOWN event should compute hardware touch-down delay (42ms)
  // and record to UMA.
  bool handled = ProcessMotionEvent(
      /*context=*/nullptr, AINPUT_EVENT_TYPE_MOTION, AMOTION_EVENT_ACTION_DOWN,
      event_time_ns, /*trigger_transfer=*/false);

  EXPECT_TRUE(handled);
  histogram_tester_.ExpectUniqueTimeSample(kTouchDownDelayHistogram,
                                           base::Milliseconds(42), 1);
}

TEST_F(SuggestionsOmtInputAnchorNativeTest,
       MotionEventDown_MaskedAction_HandledCorrectly) {
  int64_t event_time_ns = (base::TimeTicks::Now() - base::Milliseconds(15))
                              .since_origin()
                              .InNanoseconds();

  // ACTION_DOWN with pointer index flags.
  int32_t masked_action = (1 << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT) |
                          AMOTION_EVENT_ACTION_DOWN;

  bool handled = ProcessMotionEvent(
      /*context=*/nullptr, AINPUT_EVENT_TYPE_MOTION, masked_action,
      event_time_ns, /*trigger_transfer=*/false);

  EXPECT_TRUE(handled);
  histogram_tester_.ExpectUniqueTimeSample(kTouchDownDelayHistogram,
                                           base::Milliseconds(15), 1);
}

TEST_F(SuggestionsOmtInputAnchorNativeTest,
       MotionEventDown_GuardsAgainstNegativeDelay) {
  // Event time in the future relative to current mock clock.
  int64_t event_time_ns = (base::TimeTicks::Now() + base::Milliseconds(50))
                              .since_origin()
                              .InNanoseconds();

  bool handled = ProcessMotionEvent(
      /*context=*/nullptr, AINPUT_EVENT_TYPE_MOTION, AMOTION_EVENT_ACTION_DOWN,
      event_time_ns, /*trigger_transfer=*/false);

  EXPECT_TRUE(handled);
  // Negative duration must be clamped to 0ms.
  histogram_tester_.ExpectUniqueTimeSample(kTouchDownDelayHistogram,
                                           base::Milliseconds(0), 1);
}

}  // namespace
}  // namespace omnibox
