// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/average_lag_tracking_manager.h"

#include <algorithm>
#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "components/viz/common/frame_timing_details.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

using base::Bucket;
using testing::ElementsAre;
using testing::IsEmpty;

// Helper for TimeTicks usage
base::TimeTicks MillisecondsToTimeTicks(int t_ms) {
  return base::TimeTicks() + base::Milliseconds(t_ms);
}

// Helper function returning a successful `FrameTimingDetails` for use in
// `DidPresentCompositorFrame()`.
viz::FrameTimingDetails PrepareFrameDetails(base::TimeTicks swap_time,
                                            base::TimeTicks presentation_time) {
  viz::FrameTimingDetails details;
  details.swap_timings.swap_start = swap_time;
  details.presentation_feedback.timestamp = presentation_time;
  return details;
}

// Helper function returning a failed `FrameTimingDetails` for use in
// `DidPresentCompositorFrame()`.
viz::FrameTimingDetails PrepareFailedFrameDetails() {
  viz::FrameTimingDetails details;
  details.presentation_feedback = gfx::PresentationFeedback::Failure();
  return details;
}

class AverageLagTrackingManagerTest : public testing::Test {
 protected:
  AverageLagTrackingManagerTest() = default;

  // Creates a scroll event each |scroll_rate| (in ms) of |scroll_delta| px.
  // Collect events at the expected |gpu_swap_times|.
  void SimulateConstantScroll(const std::vector<int>& gpu_swap_times,
                              float scroll_delta,
                              int scroll_rate,
                              ui::ScrollInputType scroll_input_type =
                                  ui::ScrollInputType::kTouchscreen) {
    if (gpu_swap_times.empty() || gpu_swap_times[0] < scroll_rate)
      return;

    // Creates 1st frame with scroll begin.
    int events_count = gpu_swap_times[0] / scroll_rate;
    EventMetricsSet events;
    base::TimeTicks event_time = MillisecondsToTimeTicks(scroll_rate);
    base::TimeDelta time_to_rwh = base::Milliseconds(1);
    events.main_event_metrics.push_back(PrepareScrollUpdateEvent(
        ScrollUpdateEventMetrics::ScrollUpdateType::kStarted, event_time,
        event_time + time_to_rwh, scroll_delta, scroll_input_type));
    for (int i = 1; i < events_count; i++) {
      event_time += base::Milliseconds(scroll_rate);
      events.main_event_metrics.push_back(PrepareScrollUpdateEvent(
          ScrollUpdateEventMetrics::ScrollUpdateType::kContinued, event_time,
          event_time + time_to_rwh, scroll_delta, scroll_input_type));
    }
    average_lag_tracking_manager_.CollectScrollEventsFromFrame(0, events);

    // Creates remaining frames.
    for (size_t frame = 1; frame < gpu_swap_times.size(); frame++) {
      int time_delta = gpu_swap_times[frame] - gpu_swap_times[frame - 1];
      events_count = time_delta / scroll_rate;
      events.main_event_metrics.clear();
      for (int i = 0; i < events_count; i++) {
        event_time += base::Milliseconds(scroll_rate);
        events.main_event_metrics.push_back(PrepareScrollUpdateEvent(
            ScrollUpdateEventMetrics::ScrollUpdateType::kContinued, event_time,
            event_time + time_to_rwh, scroll_delta, scroll_input_type));
      }
      average_lag_tracking_manager_.CollectScrollEventsFromFrame(frame, events);
    }
  }

  // Prepares an `ScrollUpdateEventMetrics` object for a scroll-update event.
  std::unique_ptr<ScrollUpdateEventMetrics> PrepareScrollUpdateEvent(
      ScrollUpdateEventMetrics::ScrollUpdateType scroll_update_type,
      base::TimeTicks event_time,
      base::TimeTicks arrived_in_browser_main_timestamp,
      float delta,
      ui::ScrollInputType scroll_input_type =
          ui::ScrollInputType::kTouchscreen) {
    const bool kScrollIsNotInertial = false;
    const int64_t trace_id = 123;
    return ScrollUpdateEventMetrics::Create(
        ui::EventType::kGestureScrollUpdate, scroll_input_type,
        kScrollIsNotInertial, scroll_update_type, delta, event_time,
        arrived_in_browser_main_timestamp, base::TimeTicks(),
        base::IdType64<class ui::LatencyInfo>(trace_id));
  }

  AverageLagTrackingManager average_lag_tracking_manager_;
};

// Ensure that AverageLag metrics are not logged in non-touchscreen scenarios.
TEST_F(AverageLagTrackingManagerTest, EnsureMetricNotLogged) {
  base::HistogramTester histogram_tester;

  std::vector<int> gpu_swap_times = {400, 1400, 1600};
  std::vector<int> presentation_times = {500, 1500, 1700};
  SimulateConstantScroll(gpu_swap_times, 10, 100, ui::ScrollInputType::kWheel);
  for (size_t frame = 0; frame < gpu_swap_times.size(); frame++) {
    average_lag_tracking_manager_.DidPresentCompositorFrame(
        frame, PrepareFrameDetails(
                   MillisecondsToTimeTicks(gpu_swap_times[frame]),
                   MillisecondsToTimeTicks(presentation_times[frame])));
  }

  // Checking the 2 histograms should suffice. If they aren't logged, other
  // AverageLag metrics also won't be logged.
  histogram_tester.ExpectTotalCount(
      "Event.Latency.ScrollBegin.Touch.AverageLagPresentation", 0);
  histogram_tester.ExpectTotalCount(
      "Event.Latency.ScrollUpdate.Touch.AverageLagPresentation", 0);
}

// Simulate a simple situation that generates events at every 10ms starting at
// t=15ms and swaps frames at every 10ms, too, starting at t=20ms. Then tests
// that we record one UMA for ScrollUpdate in one second. Tests usage of
// `CollectScrollEventAtFrame()` (1 event per collection).
TEST_F(AverageLagTrackingManagerTest, OneSecondInterval) {
  base::HistogramTester histogram_tester;

  const float scroll_delta = 10.0f;

  base::TimeTicks event_time = MillisecondsToTimeTicks(5);
  base::TimeTicks arrived_in_browser_main_timestamp =
      MillisecondsToTimeTicks(7);
  base::TimeTicks gpu_swap_time = MillisecondsToTimeTicks(10);
  base::TimeTicks presentation_time = MillisecondsToTimeTicks(13);
  int frame_id = 1;

  // ScrollBegin
  event_time += base::Milliseconds(10);                         // 15ms
  arrived_in_browser_main_timestamp += base::Milliseconds(10);  // 17ms
  gpu_swap_time += base::Milliseconds(10);                      // 20ms
  presentation_time += base::Milliseconds(10);                  // 23ms
  EventMetricsSet events;
  events.main_event_metrics.push_back(PrepareScrollUpdateEvent(
      ScrollUpdateEventMetrics::ScrollUpdateType::kStarted, event_time,
      arrived_in_browser_main_timestamp, scroll_delta));
  average_lag_tracking_manager_.CollectScrollEventsFromFrame(frame_id, events);
  average_lag_tracking_manager_.DidPresentCompositorFrame(
      frame_id, PrepareFrameDetails(gpu_swap_time, presentation_time));

  // Send 101 ScrollUpdate events to verify that there is 1 AverageLag recorded
  // per 1 second.
  const int kUpdates = 101;
  for (int i = 0; i < kUpdates; i++) {
    event_time += base::Milliseconds(10);
    arrived_in_browser_main_timestamp += base::Milliseconds(10);
    gpu_swap_time += base::Milliseconds(10);
    presentation_time += base::Milliseconds(10);
    // First 50 has positive delta, others negative delta.
    const int sign = (i < kUpdates / 2) ? 1 : -1;

    events.main_event_metrics.clear();
    events.main_event_metrics.push_back(PrepareScrollUpdateEvent(
        ScrollUpdateEventMetrics::ScrollUpdateType::kContinued, event_time,
        arrived_in_browser_main_timestamp, sign * scroll_delta));
    average_lag_tracking_manager_.CollectScrollEventsFromFrame(frame_id,
                                                               events);
    average_lag_tracking_manager_.DidPresentCompositorFrame(
        frame_id, PrepareFrameDetails(gpu_swap_time, presentation_time));
  }

  // ScrollBegin report time is at 20ms, so the next ScrollUpdate report time is
  // at 1020ms. The last event_time that finish this report should be later than
  // 1020ms.
  EXPECT_EQ(event_time, MillisecondsToTimeTicks(1025));
  EXPECT_EQ(arrived_in_browser_main_timestamp, MillisecondsToTimeTicks(1027));
  EXPECT_EQ(gpu_swap_time, MillisecondsToTimeTicks(1030));
  EXPECT_EQ(presentation_time, MillisecondsToTimeTicks(1033));

  // Using the presentation time (25ms) instead of gpu swap (20ms) the expected
  // finger position is delta = 16px. Then (0.5*(10px+18px)*10ms)/10ms = 14px.
  histogram_tester.ExpectBucketCount(
      "Event.Latency.ScrollBegin.Touch.AverageLagPresentation", 14, 1);

  // As the presentation times are at 80% of the gap between 2 scroll events,
  // the Lag Area between 2 frames is defined by the trapezoids: (time=event-2,
  // delta=8px), (time=event, delta=10px), (time=event+8, delta=18). This makes
  // 99 trapezoids with an area of 0.5*2*(8+10) + 0.5*8*(10+18) = 130px.
  // For scroll up/down frame, the Lag at the last frame swap is 2px, and Lag
  // at this frame swap is 12px. For the one changing direction, the Lag is
  // from 8 to 10 and down to 8 again. So total LagArea is 99 * 130, plus
  // 0.5*8*(10+2) + 0.5*2*(8+10) = 66. This makes 12,936, Caled by 1 sec.
  histogram_tester.ExpectBucketCount(
      "Event.Latency.ScrollUpdate.Touch.AverageLagPresentation", 12.936, 1);
  histogram_tester.ExpectBucketCount(
      "Event.Latency.ScrollUpdate.Touch.AverageLagPresentation."
      "PredictionPositive",
      0, 1);
  histogram_tester.ExpectTotalCount(
      "Event.Latency.ScrollUpdate.Touch.AverageLagPresentation."
      "PredictionNegative",
      0);
}

// This test creates 3 frames in order to check the submission of ScrollBegin
// and ScrollUpdate events sent using `CollectScrollEventsAtFrame()` (multiple
// events per collection)
TEST_F(AverageLagTrackingManagerTest, MultipleEventsInSameFrame) {
  base::HistogramTester histogram_tester;

  std::vector<int> gpu_swap_times = {400, 1400, 1600};
  std::vector<int> presentation_times = {500, 1500, 1700};
  SimulateConstantScroll(gpu_swap_times, 10, 100);
  for (size_t frame = 0; frame < gpu_swap_times.size(); frame++) {
    average_lag_tracking_manager_.DidPresentCompositorFrame(
        frame, PrepareFrameDetails(
                   MillisecondsToTimeTicks(gpu_swap_times[frame]),
                   MillisecondsToTimeTicks(presentation_times[frame])));
  }

  // As the first frame is the ScrollBegin frame, the average lag is, using the
  // presentation time, 0.5*(10 + 50) * 40 / 40 = 30.
  histogram_tester.ExpectBucketCount(
      "Event.Latency.ScrollBegin.Touch.AverageLagPresentation", 30, 1);

  // Only the ScrollUpdate events from frame 2 are sent (as the frame 3 is
  // waiting for the next frame for sumission).
  // As there is a scroll update right at the same time as the frame submission,
  // using presentation time, frame 2 starts with 10 lag at 0.5s and finishes
  // with 110 at 1.5, thus: 0.5 * (10 + 110) = 60.
  histogram_tester.ExpectBucketCount(
      "Event.Latency.ScrollUpdate.Touch.AverageLagPresentation", 60, 1);
}

// Tests that if failed presentations arrive out-of-order, they don't mark
// previous pending frames as failed since they can still end up in a
// successful presentation.
TEST_F(AverageLagTrackingManagerTest, OutOfOrderPresentationFeedback) {
  base::HistogramTester histogram_tester;

  const float scroll_delta = 100.0f;

  std::vector<int> event_times = {500, 1500, 2500, 3500};
  std::vector<int> arrived_in_browser_main_timestamps = {700, 1700, 2700, 3700};
  std::vector<int> gpu_swap_times = {900, 1900, 2900, 3900};
  std::vector<int> presentation_times = {1000, 2000, 3000, 4000};

  // Create a scroll-begin event. Submit frame 0 with updates from scroll-begin
  // event and present it successfully. No AverageLag metrics should be reported
  // yet.
  EventMetricsSet events;
  events.main_event_metrics.push_back(PrepareScrollUpdateEvent(
      ScrollUpdateEventMetrics::ScrollUpdateType::kStarted,
      MillisecondsToTimeTicks(event_times[0]),
      MillisecondsToTimeTicks(arrived_in_browser_main_timestamps[0]),
      scroll_delta));
  average_lag_tracking_manager_.CollectScrollEventsFromFrame(0, events);
  average_lag_tracking_manager_.DidPresentCompositorFrame(
      0, PrepareFrameDetails(MillisecondsToTimeTicks(gpu_swap_times[0]),
                             MillisecondsToTimeTicks(presentation_times[0])));
  histogram_tester.ExpectTotalCount(
      "Event.Latency.ScrollBegin.Touch.AverageLagPresentation", 0);
  histogram_tester.ExpectTotalCount(
      "Event.Latency.ScrollUpdate.Touch.AverageLagPresentation", 0);

  // Create the first scroll-update event. Submit frame 1 with updates from the
  // first scroll-update event, but don't present it yet. No AverageLag metrics
  // should be recorded.
  events.main_event_metrics.clear();
  events.main_event_metrics.push_back(PrepareScrollUpdateEvent(
      ScrollUpdateEventMetrics::ScrollUpdateType::kContinued,
      MillisecondsToTimeTicks(event_times[1]),
      MillisecondsToTimeTicks(arrived_in_browser_main_timestamps[1]),
      scroll_delta));
  average_lag_tracking_manager_.CollectScrollEventsFromFrame(1, events);
  histogram_tester.ExpectTotalCount(
      "Event.Latency.ScrollBegin.Touch.AverageLagPresentation", 0);
  histogram_tester.ExpectTotalCount(
      "Event.Latency.ScrollUpdate.Touch.AverageLagPresentation", 0);

  // Create the second scroll-update event. Submit frame 2 with updates from the
  // second scroll-update event, but fail to present it. No AverageLag metrics
  // should be reported.
  events.main_event_metrics.clear();
  events.main_event_metrics.push_back(PrepareScrollUpdateEvent(
      ScrollUpdateEventMetrics::ScrollUpdateType::kContinued,
      MillisecondsToTimeTicks(event_times[2]),
      MillisecondsToTimeTicks(arrived_in_browser_main_timestamps[2]),
      scroll_delta));
  average_lag_tracking_manager_.CollectScrollEventsFromFrame(2, events);
  average_lag_tracking_manager_.DidPresentCompositorFrame(
      2, PrepareFailedFrameDetails());
  histogram_tester.ExpectTotalCount(
      "Event.Latency.ScrollBegin.Touch.AverageLagPresentation", 0);
  histogram_tester.ExpectTotalCount(
      "Event.Latency.ScrollUpdate.Touch.AverageLagPresentation", 0);

  // Now present frame 1 successfully. This should report AverageLag metrics for
  // scroll-begin event of frame 0.
  average_lag_tracking_manager_.DidPresentCompositorFrame(
      1, PrepareFrameDetails(MillisecondsToTimeTicks(gpu_swap_times[1]),
                             MillisecondsToTimeTicks(presentation_times[1])));
  histogram_tester.ExpectBucketCount(
      "Event.Latency.ScrollBegin.Touch.AverageLagPresentation", 125, 1);
  histogram_tester.ExpectTotalCount(
      "Event.Latency.ScrollUpdate.Touch.AverageLagPresentation", 0);

  // Create the third scroll-update event. Submit frame 3 with updates from the
  // second scroll-update (which failed to be presented in frame 2) and the
  // third scroll-update events. Since the failure of frame 2 should not have
  // affected events from frame 1, AverageLag metrics for scroll-update event of
  // frame 1 should be reported.
  events.main_event_metrics.push_back(PrepareScrollUpdateEvent(
      ScrollUpdateEventMetrics::ScrollUpdateType::kContinued,
      MillisecondsToTimeTicks(event_times[3]),
      MillisecondsToTimeTicks(arrived_in_browser_main_timestamps[3]),
      scroll_delta));
  average_lag_tracking_manager_.CollectScrollEventsFromFrame(3, events);
  average_lag_tracking_manager_.DidPresentCompositorFrame(
      3, PrepareFrameDetails(MillisecondsToTimeTicks(gpu_swap_times[3]),
                             MillisecondsToTimeTicks(presentation_times[3])));
  histogram_tester.ExpectBucketCount(
      "Event.Latency.ScrollBegin.Touch.AverageLagPresentation", 125, 1);
  histogram_tester.ExpectBucketCount(
      "Event.Latency.ScrollUpdate.Touch.AverageLagPresentation", 100, 1);
}

}  // namespace
}  // namespace cc
