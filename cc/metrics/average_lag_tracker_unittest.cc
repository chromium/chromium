// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/average_lag_tracker.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Bucket;
using testing::ElementsAre;

namespace cc {
namespace {

class AverageLagTrackerTest : public testing::Test {
 public:
  AverageLagTrackerTest() { ResetHistograms(); }

  void ResetHistograms() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  const base::HistogramTester& histogram_tester() { return *histogram_tester_; }

  void SetUp() override {
    average_lag_tracker_ = std::make_unique<AverageLagTracker>();
  }

  void SyntheticTouchScrollBegin(base::TimeTicks event_time,
                                 base::TimeTicks frame_time,
                                 float delta,
                                 float predicted_delta = 0) {
    AverageLagTracker::EventInfo event_info(
        delta, predicted_delta != 0 ? predicted_delta : delta, event_time,
        AverageLagTracker::EventType::kScrollbegin);
    event_info.finish_timestamp = frame_time;
    average_lag_tracker_->AddScrollEventInFrame(event_info);
  }

  void SyntheticTouchScrollUpdate(base::TimeTicks event_time,
                                  base::TimeTicks frame_time,
                                  float delta,
                                  float predicted_delta = 0) {
    AverageLagTracker::EventInfo event_info(
        delta, predicted_delta != 0 ? predicted_delta : delta, event_time,
        AverageLagTracker::EventType::kScrollupdate);
    event_info.finish_timestamp = frame_time;
    average_lag_tracker_->AddScrollEventInFrame(event_info);
  }

  void CheckScrollBeginHistograms(int bucket_value, int count) {
    EXPECT_THAT(histogram_tester().GetAllSamples(
                    "Event.Latency.ScrollBegin.Touch.AverageLagPresentation"),
                ElementsAre(Bucket(bucket_value, count)));

    EXPECT_THAT(
        histogram_tester().GetAllSamples("Event.Latency.ScrollBegin.Touch."
                                         "AverageLagPresentation.NoPrediction"),
        ElementsAre(Bucket(bucket_value, count)));
  }

  void CheckScrollUpdateWithPredictionHistograms(int bucket_value, int count) {
    EXPECT_THAT(histogram_tester().GetAllSamples(
                    "Event.Latency.ScrollUpdate.Touch.AverageLagPresentation"),
                ElementsAre(Bucket(bucket_value, count)));
  }

  void CheckScrollUpdateNoPredictionHistograms(int bucket_value, int count) {
    EXPECT_THAT(
        histogram_tester().GetAllSamples("Event.Latency.ScrollUpdate.Touch."
                                         "AverageLagPresentation.NoPrediction"),
        ElementsAre(Bucket(bucket_value, count)));
  }

  void CheckPredictionPositiveHistograms(int bucket_value, int count) {
    EXPECT_THAT(histogram_tester().GetAllSamples(
                    "Event.Latency.ScrollUpdate.Touch.AverageLagPresentation."
                    "PredictionPositive"),
                ElementsAre(Bucket(bucket_value, count)));
  }

  void CheckRemainingLagPercentageHistograms(int bucket_value, int count) {
    EXPECT_THAT(histogram_tester().GetAllSamples(
                    "Event.Latency.ScrollUpdate.Touch.AverageLagPresentation."
                    "RemainingLagPercentage"),
                ElementsAre(Bucket(bucket_value, count)));
  }

  void CheckPredictionNegativeHistograms(int bucket_value, int count) {
    EXPECT_THAT(histogram_tester().GetAllSamples(
                    "Event.Latency.ScrollUpdate.Touch.AverageLagPresentation."
                    "PredictionNegative"),
                ElementsAre(Bucket(bucket_value, count)));
  }

  void CheckScrollUpdateHistogramsTotalCount(int count) {
    histogram_tester().ExpectTotalCount(
        "Event.Latency.ScrollUpdate.Touch.AverageLagPresentation", count);
    histogram_tester().ExpectTotalCount(
        "Event.Latency.ScrollUpdate.Touch.AverageLagPresentation.NoPrediction",
        count);
  }

  void CheckPredictionPositiveHistogramsTotalCount(int count) {
    histogram_tester().ExpectTotalCount(
        "Event.Latency.ScrollUpdate.Touch.AverageLagPresentation."
        "PredictionPositive",
        count);
  }

  void CheckPredictionNegativeHistogramsTotalCount(int count) {
    histogram_tester().ExpectTotalCount(
        "Event.Latency.ScrollUpdate.Touch.AverageLagPresentation."
        "PredictionNegative",
        count);
  }

 protected:
  std::unique_ptr<AverageLagTracker> average_lag_tracker_;

  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

base::TimeTicks MillisecondsToTimeTicks(float t_ms) {
  return base::TimeTicks() + base::Milliseconds(t_ms);
}

// Simulate a simple situation that events at every 10ms and start at t=15ms,
// frame swaps at every 10ms too and start at t=20ms and test we record one
// UMA for ScrollUpdate in one second.
TEST_F(AverageLagTrackerTest, OneSecondInterval) {
  base::TimeTicks event_time = base::TimeTicks() + base::Milliseconds(5);
  base::TimeTicks frame_time = base::TimeTicks() + base::Milliseconds(10);
  float scroll_delta = 10;

  // ScrollBegin
  event_time += base::Milliseconds(10);  // 15ms
  frame_time += base::Milliseconds(10);  // 20ms
  SyntheticTouchScrollBegin(event_time, frame_time, scroll_delta);

  // Send 101 ScrollUpdate events to verify that there is 1 AverageLag record
  // per 1 second.
  const int kUpdates = 101;
  for (int i = 0; i < kUpdates; i++) {
    event_time += base::Milliseconds(10);
    frame_time += base::Milliseconds(10);
    // First 50 has positive delta, others negetive delta.
    const int sign = (i < kUpdates / 2) ? 1 : -1;
    SyntheticTouchScrollUpdate(event_time, frame_time, sign * scroll_delta);
  }

  // ScrollBegin report_time is at 20ms, so the next ScrollUpdate report_time is
  // at 1020ms. The last event_time that finish this report should be later than
  // 1020ms.
  EXPECT_EQ(event_time, base::TimeTicks() + base::Milliseconds(1025));
  EXPECT_EQ(frame_time, base::TimeTicks() + base::Milliseconds(1030));

  // ScrollBegin AverageLag are the area between the event original component
  // (time=15ms, delta=10px) to the frame swap time (time=20ms, expect finger
  // position at delta=15px). The AverageLag scaled to 1 second is
  // (0.5*(10px+15px)*5ms)/5ms = 12.5px.
  CheckScrollBeginHistograms(12, 1);

  // This ScrollUpdate AverageLag are calculated as the finger uniformly scroll
  // 10px each frame. For scroll up/down frame, the Lag at the last frame swap
  // is 5px, and Lag at this frame swap is 15px. For the one changing direction,
  // the Lag is from 5 to 10 and down to 5 again. So total LagArea is 99 * 100,
  // plus 75. the AverageLag in 1 second is 9.975px.
  CheckScrollUpdateWithPredictionHistograms(9, 1);
  CheckScrollUpdateNoPredictionHistograms(9, 1);
  CheckPredictionPositiveHistograms(0, 1);
  CheckPredictionNegativeHistogramsTotalCount(0);
  CheckRemainingLagPercentageHistograms(100 - 0, 1);

  ResetHistograms();

  // Send another ScrollBegin to end the unfinished ScrollUpdate report.
  event_time += base::Milliseconds(10);
  frame_time += base::Milliseconds(10);
  SyntheticTouchScrollBegin(event_time, frame_time, scroll_delta);

  // The last ScrollUpdate's lag is 8.75px and truncated to 8.
  CheckScrollUpdateWithPredictionHistograms(8, 1);
  CheckScrollUpdateNoPredictionHistograms(8, 1);
  CheckPredictionPositiveHistograms(0, 1);
  CheckPredictionNegativeHistogramsTotalCount(0);
  CheckRemainingLagPercentageHistograms(100 - 0, 1);
}

// Test the case that event's frame swap time is later than next event's
// creation time. (i.e, event at t=10ms will be dispatch at t=30ms, while next
// event is at t=20ms).
TEST_F(AverageLagTrackerTest, LargerLatency) {
  base::TimeTicks event_time = MillisecondsToTimeTicks(10);
  base::TimeTicks frame_time = event_time + base::Milliseconds(20);
  float scroll_delta = 10;

  SyntheticTouchScrollBegin(event_time, frame_time, scroll_delta);

  // Send 2 ScrollUpdate. The second one will record AverageLag.ScrollBegin as
  // it's event_time is larger or equal to ScrollBegin's frame_time.
  for (int i = 0; i < 2; i++) {
    event_time += base::Milliseconds(10);
    frame_time = event_time + base::Milliseconds(20);
    SyntheticTouchScrollUpdate(event_time, frame_time, scroll_delta);
  }

  // ScrollBegin AveragLag are from t=10ms to t=30ms, with absolute scroll
  // position from 10 to 30. The AverageLag should be:
  // (0.5*(10px + 30px)*20ms/20ms) = 20px.
  CheckScrollBeginHistograms(20, 1);

  // Another ScrollBegin to flush unfinished frames.
  // event_time doesn't matter here because the previous frames' lag are
  // compute from their frame_time.
  event_time = MillisecondsToTimeTicks(1000);
  frame_time = MillisecondsToTimeTicks(1000);
  SyntheticTouchScrollBegin(event_time, frame_time, scroll_delta);
  // The to unfinished frames' lag are (finger_positon-rendered_position)*time,
  // AverageLag is ((30px-10px)*10ms+(30px-20px)*10ms)/20ms = 15px.
  CheckScrollUpdateWithPredictionHistograms(14, 1);
  CheckScrollUpdateNoPredictionHistograms(14, 1);
}

// Test that multiple latency being flush in the same frame swap.
TEST_F(AverageLagTrackerTest, TwoLatencyInfoInSameFrame) {
  // ScrollBegin
  base::TimeTicks event_time = MillisecondsToTimeTicks(10);
  base::TimeTicks frame_time = MillisecondsToTimeTicks(20);
  SyntheticTouchScrollBegin(event_time, frame_time, -10 /* scroll_delta */);

  // ScrollUpdate with event_time >= ScrollBegin frame_time will generate
  // a histogram for AverageLag.ScrollBegin.
  event_time = MillisecondsToTimeTicks(20);
  frame_time = MillisecondsToTimeTicks(30);
  SyntheticTouchScrollUpdate(event_time, frame_time, -10 /* scroll_delta */);

  // Absolute position from -10 to -20. The AverageLag should be:
  // (0.5*(10px + 20px)*10ms/10ms) = 15px.
  CheckScrollBeginHistograms(14, 1);

  event_time = MillisecondsToTimeTicks(25);
  frame_time = MillisecondsToTimeTicks(30);
  SyntheticTouchScrollUpdate(event_time, frame_time, 5 /* scroll_delta */);

  // Another ScrollBegin to flush unfinished frames.
  event_time = MillisecondsToTimeTicks(1000);
  frame_time = MillisecondsToTimeTicks(1000);
  SyntheticTouchScrollBegin(event_time, frame_time, 0);

  // The ScrollUpdates are at t=20ms, finger_pos=-20px, rendered_pos=-10px,
  // at t=25ms, finger_pos=-15px, rendered_pos=-10px;
  // To t=30ms both events get flush.
  // AverageLag is (0.5*(10px+5px)*5ms + 5px*5ms)/10ms = 6.25px
  CheckScrollUpdateWithPredictionHistograms(6, 1);
  CheckScrollUpdateNoPredictionHistograms(6, 1);
}

// Test the case that switching direction causes lag at current frame
// time and previous frame time are in different direction.
TEST_F(AverageLagTrackerTest, ChangeDirectionInFrame) {
  // ScrollBegin
  base::TimeTicks event_time = MillisecondsToTimeTicks(10);
  base::TimeTicks frame_time = MillisecondsToTimeTicks(20);
  SyntheticTouchScrollBegin(event_time, frame_time, 10 /* scroll_delta */);

  // At t=20, lag = 10px.
  event_time = MillisecondsToTimeTicks(20);
  frame_time = MillisecondsToTimeTicks(30);
  SyntheticTouchScrollUpdate(event_time, frame_time, 10 /* scroll_delta */);

  // At t=30, lag = -10px.
  event_time = MillisecondsToTimeTicks(30);
  frame_time = MillisecondsToTimeTicks(40);
  SyntheticTouchScrollUpdate(event_time, frame_time, -20 /* scroll_delta */);

  // Another ScrollBegin to flush unfinished frames.
  event_time = MillisecondsToTimeTicks(1000);
  frame_time = MillisecondsToTimeTicks(1000);
  SyntheticTouchScrollBegin(event_time, frame_time, 0);

  // From t=20 to t=30, lag_area=2*(0.5*10px*5ms)=50px*ms.
  // From t=30 to t=40, lag_area=20px*10ms=200px*ms
  // AverageLag = (50+200)/20 = 12.5px.
  CheckScrollUpdateWithPredictionHistograms(12, 1);
  CheckScrollUpdateNoPredictionHistograms(12, 1);
}

// A simple case without scroll prediction to compare with the two with
// prediction cases below.
TEST_F(AverageLagTrackerTest, NoScrollPrediction) {
  // ScrollBegin, at t=5, finger_pos=5px.
  base::TimeTicks event_time = MillisecondsToTimeTicks(5);
  base::TimeTicks frame_time = MillisecondsToTimeTicks(10);
  SyntheticTouchScrollBegin(event_time, frame_time, 5 /* scroll_delta */);

  // ScrollUpdate, at t=15, finger_pos=15px.
  event_time = MillisecondsToTimeTicks(15);
  frame_time = MillisecondsToTimeTicks(20);
  SyntheticTouchScrollUpdate(event_time, frame_time, 10 /* scroll_delta */);

  // ScrollUpdate, at t=25, finger_pos=25px.
  event_time = MillisecondsToTimeTicks(25);
  frame_time = MillisecondsToTimeTicks(30);
  SyntheticTouchScrollUpdate(event_time, frame_time, 10 /* scroll_delta */);

  // Another ScrollBegin to flush unfinished frames.
  event_time = MillisecondsToTimeTicks(1000);
  frame_time = MillisecondsToTimeTicks(1000);
  SyntheticTouchScrollBegin(event_time, frame_time, 0);

  // Prediction hasn't take affect on ScrollBegin so it'll stay the same.
  CheckScrollBeginHistograms(7, 1);
  // At t=10, finger_pos = 10px, rendered_pos = 5px.
  // At t=20, finger_pos = 20px, rendered_pos = 15px.
  // At t=30, finger_pos = 25px, rendered_pos = 25px.
  // AverageLag = ((5px+15px)*10ms/2 + (5px+10px)*5ms/2 + 10px*5ms)/20ms
  //            = 9.375
  CheckScrollUpdateWithPredictionHistograms(9, 1);
  CheckScrollUpdateNoPredictionHistograms(9, 1);
}

// Test AverageLag with perfect scroll prediction.
TEST_F(AverageLagTrackerTest, ScrollPrediction) {
  // ScrollBegin, at t=5, finger_pos=5px.
  // Predict frame_time=10, predicted_pos = 10px.
  base::TimeTicks event_time = MillisecondsToTimeTicks(5);
  base::TimeTicks frame_time = MillisecondsToTimeTicks(10);
  SyntheticTouchScrollBegin(event_time, frame_time, 5 /* scroll_delta */,
                            10 /* predicted_delta */);

  // ScrollUpdate, at t=15, finger_pos=15px.
  // Predict frame_time=20, predicted_pos = 20px.
  event_time = MillisecondsToTimeTicks(15);
  frame_time = MillisecondsToTimeTicks(20);
  SyntheticTouchScrollUpdate(event_time, frame_time, 10 /* scroll_delta */,
                             10 /* predicted_delta */);

  // ScrollUpdate, at t=25, finger_pos=25px.
  // Predict frame_time=30, predicted_pos = 30px.
  event_time = MillisecondsToTimeTicks(25);
  frame_time = MillisecondsToTimeTicks(30);
  SyntheticTouchScrollUpdate(event_time, frame_time, 10 /* scroll_delta */,
                             10 /* predicted_delta */);

  // Another ScrollBegin to flush unfinished frames.
  event_time = MillisecondsToTimeTicks(1000);
  frame_time = MillisecondsToTimeTicks(1000);
  SyntheticTouchScrollBegin(event_time, frame_time, 0);

  // Prediction hasn't take affect on ScrollBegin so it'll stay the same.
  CheckScrollBeginHistograms(7, 1);
  // At t=10, finger_pos = 10px, rendered_pos = 10px.
  // At t=20, finger_pos = 20px, rendered_pos = 20px.
  // At t=30, finger_pos = 25px, rendered_pos = 30px.
  // AverageLag = ((0px+10px)*10ms/2 + (0px+5px)*10ms/2 + 5px*5ms)/20ms
  //            = 4.375px
  CheckScrollUpdateWithPredictionHistograms(4, 1);
  // AverageLag (w/o prediction)
  //              ((5px+15px)*10ms/2 + (5px+10px)*5ms/2 + 10px*5ms)/20ms
  //            = 9.375px
  CheckScrollUpdateNoPredictionHistograms(9, 1);
  // Positive effect of prediction = 5px
  CheckPredictionPositiveHistograms(5, 1);
  CheckPredictionNegativeHistogramsTotalCount(0);
  CheckRemainingLagPercentageHistograms(100 * 4.375 / 9.375, 1);
}

// Test AverageLag with imperfect scroll prediction.
TEST_F(AverageLagTrackerTest, ImperfectScrollPrediction) {
  // ScrollBegin, at t=5, finger_pos=5px.
  // Predict frame_time=10, predicted_pos(over) = 12px.
  base::TimeTicks event_time = MillisecondsToTimeTicks(5);
  base::TimeTicks frame_time = MillisecondsToTimeTicks(10);
  SyntheticTouchScrollBegin(event_time, frame_time, 5 /* scroll_delta */,
                            12 /* predicted_delta */);

  // ScrollUpdate, at t=15, finger_pos=15px.
  // Predict frame_time=20, predicted_pos(under) = 17px.
  event_time = MillisecondsToTimeTicks(15);
  frame_time = MillisecondsToTimeTicks(20);
  SyntheticTouchScrollUpdate(event_time, frame_time, 10 /* scroll_delta */,
                             5 /* predicted_delta */);

  // ScrollUpdate, at t=25, finger_pos=25px.
  // Predict frame_time=30, predicted_pos(over) = 31px.
  event_time = MillisecondsToTimeTicks(25);
  frame_time = MillisecondsToTimeTicks(30);
  SyntheticTouchScrollUpdate(event_time, frame_time, 10 /* scroll_delta */,
                             14 /* predicted_delta */);

  // Another ScrollBegin to flush unfinished frames.
  event_time = MillisecondsToTimeTicks(1000);
  frame_time = MillisecondsToTimeTicks(1000);
  SyntheticTouchScrollBegin(event_time, frame_time, 0);

  CheckScrollBeginHistograms(7, 1);
  // AverageLag = ((2px*2ms/2+8px*8ms/2)+ ((3px+8px)*5ms/2+8px*5ms))/20ms
  //            = 5.075px
  CheckScrollUpdateWithPredictionHistograms(5, 1);
  // AverageLag (w/o prediction =
  //              ((5px+15px)*10ms/2 + (5px+10px)*5ms/2 + 10px*5ms)/20ms
  //            = 9.375px
  CheckScrollUpdateNoPredictionHistograms(9, 1);
  // Positive effect of prediction = 4.3px
  CheckPredictionPositiveHistograms(4, 1);
  CheckPredictionNegativeHistogramsTotalCount(0);
  CheckRemainingLagPercentageHistograms(100 * 5.075 / 9.375, 1);
}

TEST_F(AverageLagTrackerTest, NegativePredictionEffect) {
  // ScrollBegin, at t=5, finger_pos=5px.
  // Predict frame_time=10, predicted_pos(over) = 20px.
  base::TimeTicks event_time = MillisecondsToTimeTicks(5);
  base::TimeTicks frame_time = MillisecondsToTimeTicks(10);
  SyntheticTouchScrollBegin(event_time, frame_time, 5 /* scroll_delta */,
                            20 /* predicted_delta */);

  // ScrollUpdate, at t=15, finger_pos=15px.
  // Predict frame_time=20, predicted_pos(over) = 60px.
  event_time = MillisecondsToTimeTicks(15);
  frame_time = MillisecondsToTimeTicks(20);
  SyntheticTouchScrollUpdate(event_time, frame_time, 10 /* scroll_delta */,
                             40 /* predicted_delta */);

  // ScrollUpdate, at t=25, finger_pos=25px.
  // Predict frame_time=30, predicted_pos(over) = 60px.
  event_time = MillisecondsToTimeTicks(25);
  frame_time = MillisecondsToTimeTicks(30);
  SyntheticTouchScrollUpdate(event_time, frame_time, 10 /* scroll_delta */,
                             0 /* predicted_delta */);

  // Another ScrollBegin to flush unfinished frames.
  event_time = MillisecondsToTimeTicks(1000);
  frame_time = MillisecondsToTimeTicks(1000);
  SyntheticTouchScrollBegin(event_time, frame_time, 0);

  CheckScrollBeginHistograms(7, 1);
  // AverageLag = ((10px+0px)*10ms/2)+ ((40px+35px)*5ms/2+35px*5ms))/20ms
  //            = 20.625px
  CheckScrollUpdateWithPredictionHistograms(20, 1);
  // AverageLag (w/o prediction =
  //              ((5px+15px)*10ms/2 + (5px+10px)*5ms/2 + 10px*5ms)/20ms
  //            = 9.375px
  CheckScrollUpdateNoPredictionHistograms(9, 1);
  // Negative effect of prediction = 11.25
  CheckPredictionPositiveHistogramsTotalCount(0);
  CheckPredictionNegativeHistograms(11, 1);

  // 100 * 20.625 / 9.375 = 220 is logged into bucket 219.
  CheckRemainingLagPercentageHistograms(219, 1);
}

TEST_F(AverageLagTrackerTest, NoPredictionEffect) {
  // ScrollBegin, at t=5, finger_pos=5px.
  // Predict frame_time=10, predicted_pos(over) = 25px.
  base::TimeTicks event_time = MillisecondsToTimeTicks(5);
  base::TimeTicks frame_time = MillisecondsToTimeTicks(10);
  SyntheticTouchScrollBegin(event_time, frame_time, 5 /* scroll_delta */,
                            25 /* predicted_delta */);

  // ScrollUpdate, at t=15, finger_pos=15px.
  // Predict frame_time=20, predicted_pos(over) = 32px.
  event_time = MillisecondsToTimeTicks(15);
  frame_time = MillisecondsToTimeTicks(20);
  SyntheticTouchScrollUpdate(event_time, frame_time, 10 /* scroll_delta */,
                             7 /* predicted_delta */);

  // ScrollUpdate, at t=25, finger_pos=25px.
  // Predict frame_time=30, predicted_pos(over) = 37px.
  event_time = MillisecondsToTimeTicks(25);
  frame_time = MillisecondsToTimeTicks(30);
  SyntheticTouchScrollUpdate(event_time, frame_time, 10 /* scroll_delta */,
                             5 /* predicted_delta */);

  // Another ScrollBegin to flush unfinished frames.
  event_time = MillisecondsToTimeTicks(1000);
  frame_time = MillisecondsToTimeTicks(1000);
  SyntheticTouchScrollBegin(event_time, frame_time, 0);

  CheckScrollBeginHistograms(7, 1);
  // AverageLag = ((15px+5px)*10ms/2 + (12px+7px)*5ms/2 + 7px*5ms)/20ms
  //            = 9.125px
  CheckScrollUpdateWithPredictionHistograms(9, 1);
  // AverageLag (w/o prediction) =
  //              ((5px+15px)*10ms/2 + (5px+10px)*5ms/2 + 10px*5ms)/20ms
  //            = 9.375px
  CheckScrollUpdateNoPredictionHistograms(9, 1);
  // Prediction slightly positive, we should see a 0 bucket in
  // PredictionPositive UMA
  CheckPredictionPositiveHistograms(0, 1);
  CheckPredictionNegativeHistogramsTotalCount(0);
}

// Tests that when an event arrives out-of-order, the average lag tracker
// properly ignores it.
TEST_F(AverageLagTrackerTest, EventOutOfOrder) {
  base::TimeTicks event_time = MillisecondsToTimeTicks(5);
  base::TimeTicks frame_time = MillisecondsToTimeTicks(10);
  float scroll_delta = 5.f;
  SyntheticTouchScrollBegin(event_time, frame_time, scroll_delta);

  event_time = MillisecondsToTimeTicks(15);
  frame_time = MillisecondsToTimeTicks(20);
  SyntheticTouchScrollUpdate(event_time, frame_time, scroll_delta);

  event_time = MillisecondsToTimeTicks(25);
  frame_time = MillisecondsToTimeTicks(30);
  SyntheticTouchScrollUpdate(event_time, frame_time, scroll_delta);

  // A ScrollBegin to flush unfinished frames.
  event_time = MillisecondsToTimeTicks(1000);
  frame_time = MillisecondsToTimeTicks(1000);
  SyntheticTouchScrollBegin(event_time, frame_time, 0);

  CheckScrollUpdateHistogramsTotalCount(1);

  // Send an event whose timestamp is earlier than the most recent event,
  // representing an event that gets process out of order.
  base::TimeTicks earlier_event_time = MillisecondsToTimeTicks(15);
  frame_time = MillisecondsToTimeTicks(1010);
  SyntheticTouchScrollUpdate(earlier_event_time, frame_time, scroll_delta);

  // Another ScrollBegin to flush unfinished frames.
  event_time = MillisecondsToTimeTicks(2000);
  frame_time = MillisecondsToTimeTicks(2000);
  SyntheticTouchScrollBegin(event_time, frame_time, 0);

  // Ensure that the event was ignored.
  CheckScrollUpdateHistogramsTotalCount(1);
}

}  // namespace
}  // namespace cc
