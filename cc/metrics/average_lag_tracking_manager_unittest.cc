// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/average_lag_tracking_manager.h"

#include <algorithm>
#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "components/viz/common/frame_timing_details.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/latency/latency_info.h"

namespace cc {
namespace {

using base::Bucket;
using testing::ElementsAre;

// Helper for TimeTicks usage
base::TimeTicks MillisecondsToTimeTicks(float t_ms) {
  return base::TimeTicks() + base::TimeDelta::FromMilliseconds(t_ms);
}

// Helper for FrameTimingDetails usage in DidPresentCompositorFrame
viz::FrameTimingDetails PrepareFrameDetails(base::TimeTicks swap_time,
                                            base::TimeTicks presentation_time) {
  gfx::SwapTimings timings;
  timings.swap_start = swap_time;
  viz::FrameTimingDetails details;
  details.presentation_feedback.timestamp = presentation_time;
  details.swap_timings = timings;
  return details;
}

class AverageLagTrackingManagerTest : public testing::Test {
 protected:
  AverageLagTrackingManagerTest() = default;

  void SetUp() override { ResetHistograms(); }

  void ResetHistograms() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  // Creates a scroll event each |scroll_rate| (in ms) of |scroll_delta| px.
  // Collect events at the expected |gpu_swap_times|.
  void SimulateConstantScroll(const std::vector<unsigned int>& gpu_swap_times,
                              float scroll_delta,
                              unsigned int scroll_rate) {
    if (gpu_swap_times.size() == 0 || gpu_swap_times[0] < scroll_rate)
      return;

    // Creates 1st frame with scroll begin
    std::vector<ui::LatencyInfo> events(gpu_swap_times[0] / scroll_rate);
    base::TimeTicks event_time = MillisecondsToTimeTicks(scroll_rate);
    events[0] = PrepareScrollEvent(AverageLagTracker::EventType::ScrollBegin,
                                   event_time, 0, scroll_delta);
    for (size_t i = 1; i < events.size(); i++) {
      event_time += base::TimeDelta::FromMilliseconds(scroll_rate);
      events[i] = PrepareScrollEvent(AverageLagTracker::EventType::ScrollUpdate,
                                     event_time, i, scroll_delta);
    }
    average_lag_tracking_manager_.CollectScrollEventsFromFrame(0, events);

    // Creates remaining frames
    for (size_t frame = 1; frame < gpu_swap_times.size(); frame++) {
      unsigned int time_delta =
          gpu_swap_times[frame] - gpu_swap_times[frame - 1];
      events = std::vector<ui::LatencyInfo>(time_delta / scroll_rate);
      for (size_t i = 0; i < events.size(); i++) {
        event_time += base::TimeDelta::FromMilliseconds(scroll_rate);
        events[i] =
            PrepareScrollEvent(AverageLagTracker::EventType::ScrollUpdate,
                               event_time, i, scroll_delta);
      }
      average_lag_tracking_manager_.CollectScrollEventsFromFrame(frame, events);
    }
  }

  // Prepares a ui::LatencyInfo object for a ScrollEvent
  ui::LatencyInfo PrepareScrollEvent(AverageLagTracker::EventType event_type,
                                     base::TimeTicks event_time,
                                     int trace_id,
                                     float delta,
                                     float predicted_delta = 0) {
    ui::LatencyInfo info;
    info.set_trace_id(trace_id);
    info.set_source_event_type(ui::SourceEventType::TOUCH);

    info.AddLatencyNumberWithTimestamp(
        event_type == AverageLagTracker::EventType::ScrollBegin
            ? ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT
            : ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT,
        event_time);

    info.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_LAST_EVENT_COMPONENT, event_time);

    info.set_scroll_update_delta(delta);
    info.set_predicted_scroll_update_delta(
        predicted_delta != 0 ? predicted_delta : delta);

    return info;
  }

  AverageLagTrackingManager average_lag_tracking_manager_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// Simulate a simple situation that events at every 10ms and start at t=15ms,
// frame swaps at every 10ms too and start at t=20ms and test we record one
// UMA for ScrollUpdate in one second. Tests using CollectScrollEventAtFrame
// (1 event per collection)
TEST_F(AverageLagTrackingManagerTest, OneSecondInterval) {
  base::TimeTicks event_time = MillisecondsToTimeTicks(5);
  base::TimeTicks gpu_swap_time = MillisecondsToTimeTicks(10);
  base::TimeTicks presentation_time = MillisecondsToTimeTicks(13);
  float scroll_delta = 10;
  int frame_id = 1;

  // ScrollBegin
  event_time += base::TimeDelta::FromMilliseconds(10);  // 15ms
  gpu_swap_time += base::TimeDelta::FromMilliseconds(10);      // 20ms
  presentation_time += base::TimeDelta::FromMilliseconds(10);  // 23ms
  ui::LatencyInfo evt = PrepareScrollEvent(
      AverageLagTracker::EventType::ScrollBegin, event_time, 1, scroll_delta);
  average_lag_tracking_manager_.CollectScrollEventsFromFrame(
      frame_id, std::vector<ui::LatencyInfo>{evt});
  average_lag_tracking_manager_.DidPresentCompositorFrame(
      frame_id, PrepareFrameDetails(gpu_swap_time, presentation_time));

  // Send 101 ScrollUpdate events to verify that there is 1 AverageLag record
  // per 1 second.
  const int kUpdates = 101;
  for (int i = 0; i < kUpdates; i++) {
    event_time += base::TimeDelta::FromMilliseconds(10);
    gpu_swap_time += base::TimeDelta::FromMilliseconds(10);
    presentation_time += base::TimeDelta::FromMilliseconds(10);
    // First 50 has positive delta, others negative delta.
    const int sign = (i < kUpdates / 2) ? 1 : -1;

    evt = PrepareScrollEvent(AverageLagTracker::EventType::ScrollUpdate,
                             event_time, 1, sign * scroll_delta);
    average_lag_tracking_manager_.CollectScrollEventsFromFrame(
        frame_id, std::vector<ui::LatencyInfo>{evt});
    average_lag_tracking_manager_.DidPresentCompositorFrame(
        frame_id, PrepareFrameDetails(gpu_swap_time, presentation_time));
  }

  // ScrollBegin report_time is at 20ms, so the next ScrollUpdate report_time is
  // at 1020ms. The last event_time that finish this report should be later than
  // 1020ms.
  EXPECT_EQ(event_time, MillisecondsToTimeTicks(1025));
  EXPECT_EQ(gpu_swap_time, MillisecondsToTimeTicks(1030));
  EXPECT_EQ(presentation_time, MillisecondsToTimeTicks(1033));

  // Using the presentation time (25ms) instead of gpu swap (20ms) the expected
  // finger position is delta = 16px. Then (0.5*(10px+18px)*10ms)/10ms = 14px.
  // UmaHistogramCounts1000's binning will round it to 12.
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "Event.Latency.ScrollBegin.Touch.AverageLagPresentation"),
              ElementsAre(Bucket(14, 1)));

  // As the presentation times are at 80% of the gap between 2 scroll events,
  // the Lag Area between 2 frames is define by the trapezoids: (time=event-2,
  // delta=8px), (time=event, delta=10px), (time=event+8, delta=18). This makes
  // 99 trapezois with an area of 0.5*2*(8+10) + 0.5*8*(10+18) = 130px.
  // For scroll up/down frame, the Lag at the last frame swap is 2px, and Lag
  // at this frame swap is 12px. For the one changing direction, the Lag is
  // from 8 to 10 and down to 8 again. So total LagArea is 99 * 130, plus
  // 0.5*8*(10+2) + 0.5*2*(8+10) = 66. This makes 12,936. Scaled by 1 sec
  // and binned: 12.
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "Event.Latency.ScrollUpdate.Touch.AverageLagPresentation"),
              ElementsAre(Bucket(12, 1)));
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "Event.Latency.ScrollUpdate.Touch.AverageLagPresentation."
                  "PredictionPositive"),
              ElementsAre(Bucket(0, 1)));
  histogram_tester_->ExpectTotalCount(
      "Event.Latency.ScrollUpdate.Touch.AverageLagPresentation."
      "PredictionNegative",
      0);

  ResetHistograms();
}

// This test creates 3 frames in order to check the submission of ScrollBegin
// and ScrollUpdate events sent using CollectScrollEventsAtFrame (multiple
// events per collection)
TEST_F(AverageLagTrackingManagerTest, MultipleEventsInSameFrame) {
  std::vector<unsigned int> gpu_swap_times = {400, 1400, 1600};
  std::vector<unsigned int> presentation_times = {500, 1500, 1700};
  SimulateConstantScroll(gpu_swap_times, 10, 100);
  for (size_t frame = 0; frame < gpu_swap_times.size(); frame++) {
    average_lag_tracking_manager_.DidPresentCompositorFrame(
        frame, PrepareFrameDetails(
                   MillisecondsToTimeTicks(gpu_swap_times[frame]),
                   MillisecondsToTimeTicks(presentation_times[frame])));
  }

  // As the first frame is the ScrollBegin frame, the average lag is, using the
  // presentation time, 0.5*(10 + 50) * 40 / 40 = 30. But
  // UmaHistogramCounts1000's binning will round it to 29.
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "Event.Latency.ScrollBegin.Touch.AverageLagPresentation"),
              ElementsAre(Bucket(29, 1)));

  // Only the ScrollUpdate events from frame 2 are sent (as the frame 3 is
  // waiting for the next frame for sumission).
  // As there is a scroll update right at the same time as the frame submission,
  // using presentation time, frame 2 starts with 10 lag at 0.5s and finishes
  // with 110 at 1.5, thus: 0.5 * (10 + 110) / 2 = 60.
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "Event.Latency.ScrollUpdate.Touch.AverageLagPresentation"),
              ElementsAre(Bucket(60, 1)));
}

}  // namespace
}  // namespace cc
