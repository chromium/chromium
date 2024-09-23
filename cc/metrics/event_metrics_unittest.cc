// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/event_metrics.h"

#include "base/test/simple_test_tick_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

using TraceId = base::IdType64<class ui::LatencyInfo>;
class EventMetricsTest : public testing::Test {
 private:
  base::SimpleTestTickClock test_tick_clock_;

 protected:
  base::TimeTicks AdvanceNowByMs(int advance_ms) {
    test_tick_clock_.Advance(base::Microseconds(advance_ms));
    return test_tick_clock_.NowTicks();
  }
};

TEST_F(EventMetricsTest, ScrollBeginCreateWithNullBeginRwhTime) {
  // Arrange
  base::TimeTicks event_time = base::TimeTicks::Now() - base::Microseconds(100);
  base::TimeTicks blocking_touch_dispatched_to_renderer_timestamp;
  base::TimeTicks arrived_in_browser_main_timestamp;
  base::TimeTicks now = base::TimeTicks::Now();

  // Act
  std::unique_ptr<ScrollEventMetrics> scroll_event_metric =
      ScrollEventMetrics::Create(
          ui::EventType::kGestureScrollBegin, ui::ScrollInputType::kTouchscreen,
          /*is_inertial=*/false, event_time, arrived_in_browser_main_timestamp,
          blocking_touch_dispatched_to_renderer_timestamp, std::nullopt);

  // Assert
  EXPECT_EQ(event_time, scroll_event_metric->GetDispatchStageTimestamp(
                            EventMetrics::DispatchStage::kGenerated));
  EXPECT_LE(now,
            scroll_event_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kArrivedInRendererCompositor));
  // not set
  EXPECT_TRUE(scroll_event_metric
                  ->GetDispatchStageTimestamp(
                      EventMetrics::DispatchStage::
                          kScrollsBlockingTouchDispatchedToRenderer)
                  .is_null());
  EXPECT_TRUE(scroll_event_metric
                  ->GetDispatchStageTimestamp(
                      EventMetrics::DispatchStage::kArrivedInBrowserMain)
                  .is_null());
  EXPECT_TRUE(scroll_event_metric
                  ->GetDispatchStageTimestamp(
                      EventMetrics::DispatchStage::kRendererCompositorStarted)
                  .is_null());
  EXPECT_TRUE(scroll_event_metric
                  ->GetDispatchStageTimestamp(
                      EventMetrics::DispatchStage::kRendererCompositorFinished)
                  .is_null());
  EXPECT_TRUE(scroll_event_metric
                  ->GetDispatchStageTimestamp(
                      EventMetrics::DispatchStage::kRendererMainStarted)
                  .is_null());
  EXPECT_TRUE(scroll_event_metric
                  ->GetDispatchStageTimestamp(
                      EventMetrics::DispatchStage::kRendererMainFinished)
                  .is_null());
}

TEST_F(EventMetricsTest, ScrollBeginCreate) {
  // Arrange
  base::TimeTicks event_time = base::TimeTicks::Now() - base::Microseconds(100);
  base::TimeTicks blocking_touch_dispatched_to_renderer_timestamp =
      base::TimeTicks::Now() - base::Microseconds(70);
  base::TimeTicks arrived_in_browser_main_timestamp =
      base::TimeTicks::Now() - base::Microseconds(50);
  base::TimeTicks now = base::TimeTicks::Now();

  // Act
  std::unique_ptr<ScrollEventMetrics> scroll_event_metric =
      ScrollEventMetrics::Create(
          ui::EventType::kGestureScrollBegin, ui::ScrollInputType::kTouchscreen,
          /*is_inertial=*/false, event_time, arrived_in_browser_main_timestamp,
          blocking_touch_dispatched_to_renderer_timestamp, std::nullopt);

  // Assert
  EXPECT_EQ(event_time, scroll_event_metric->GetDispatchStageTimestamp(
                            EventMetrics::DispatchStage::kGenerated));
  EXPECT_EQ(blocking_touch_dispatched_to_renderer_timestamp,
            scroll_event_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::
                    kScrollsBlockingTouchDispatchedToRenderer));
  EXPECT_EQ(arrived_in_browser_main_timestamp,
            scroll_event_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kArrivedInBrowserMain));
  EXPECT_LE(now,
            scroll_event_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kArrivedInRendererCompositor));
  // not set
  EXPECT_TRUE(scroll_event_metric
                  ->GetDispatchStageTimestamp(
                      EventMetrics::DispatchStage::kRendererCompositorStarted)
                  .is_null());
  EXPECT_TRUE(scroll_event_metric
                  ->GetDispatchStageTimestamp(
                      EventMetrics::DispatchStage::kRendererCompositorFinished)
                  .is_null());
  EXPECT_TRUE(scroll_event_metric
                  ->GetDispatchStageTimestamp(
                      EventMetrics::DispatchStage::kRendererMainStarted)
                  .is_null());
  EXPECT_TRUE(scroll_event_metric
                  ->GetDispatchStageTimestamp(
                      EventMetrics::DispatchStage::kRendererMainFinished)
                  .is_null());
}

TEST_F(EventMetricsTest, ScrollBeginCreateFromExisting) {
  // Arrange
  base::TimeTicks event_time = base::TimeTicks::Now() - base::Microseconds(100);
  base::TimeTicks blocking_touch_dispatched_to_renderer_timestamp =
      base::TimeTicks::Now() - base::Microseconds(70);
  base::TimeTicks arrived_in_browser_main_timestamp =
      base::TimeTicks::Now() - base::Microseconds(50);
  std::unique_ptr<ScrollEventMetrics> scroll_metric =
      ScrollEventMetrics::Create(
          ui::EventType::kGestureScrollBegin, ui::ScrollInputType::kTouchscreen,
          /*is_inertial=*/false, event_time, arrived_in_browser_main_timestamp,
          blocking_touch_dispatched_to_renderer_timestamp, std::nullopt);

  // Act
  std::unique_ptr<ScrollEventMetrics> copy_scroll_metric =
      ScrollEventMetrics::CreateFromExisting(
          ui::EventType::kGestureScrollBegin, ui::ScrollInputType::kTouchscreen,
          /*is_inertial=*/false,
          EventMetrics::DispatchStage::kRendererMainFinished,
          scroll_metric.get());

  // Assert
  EXPECT_EQ(scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kGenerated),
            copy_scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kGenerated));
  EXPECT_EQ(scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::
                    kScrollsBlockingTouchDispatchedToRenderer),
            copy_scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::
                    kScrollsBlockingTouchDispatchedToRenderer));
  EXPECT_EQ(scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kArrivedInBrowserMain),
            copy_scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kArrivedInBrowserMain));

  EXPECT_EQ(scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kArrivedInRendererCompositor),
            copy_scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kArrivedInRendererCompositor));

  EXPECT_EQ(scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kRendererCompositorStarted),
            copy_scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kRendererCompositorStarted));

  EXPECT_EQ(scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kRendererCompositorFinished),
            copy_scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kRendererCompositorFinished));

  EXPECT_EQ(scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kRendererMainStarted),
            copy_scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kRendererMainStarted));

  EXPECT_EQ(scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kRendererMainFinished),
            copy_scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kRendererMainFinished));
}

TEST_F(EventMetricsTest, ScrollUpdateCreateWithNullBeginRwhTime) {
  // Arrange
  base::TimeTicks event_time = base::TimeTicks::Now() - base::Microseconds(100);
  base::TimeTicks blocking_touch_dispatched_to_renderer_timestamp;
  base::TimeTicks arrived_in_browser_main_timestamp;
  base::TimeTicks now = base::TimeTicks::Now();
  TraceId trace_id(123);

  // Act
  std::unique_ptr<ScrollUpdateEventMetrics> scroll_event_metric =
      ScrollUpdateEventMetrics::Create(
          ui::EventType::kGestureScrollUpdate,
          ui::ScrollInputType::kTouchscreen,
          /*is_inertial=*/false,
          ScrollUpdateEventMetrics::ScrollUpdateType::kContinued, /*delta=*/0.4,
          event_time, arrived_in_browser_main_timestamp,
          blocking_touch_dispatched_to_renderer_timestamp, trace_id);

  // Assert
  EXPECT_EQ(trace_id, scroll_event_metric->trace_id());
  EXPECT_EQ(event_time, scroll_event_metric->GetDispatchStageTimestamp(
                            EventMetrics::DispatchStage::kGenerated));
  EXPECT_LE(now,
            scroll_event_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kArrivedInRendererCompositor));
  // not set
  EXPECT_TRUE(scroll_event_metric
                  ->GetDispatchStageTimestamp(
                      EventMetrics::DispatchStage::
                          kScrollsBlockingTouchDispatchedToRenderer)
                  .is_null());
  EXPECT_TRUE(scroll_event_metric
                  ->GetDispatchStageTimestamp(
                      EventMetrics::DispatchStage::kArrivedInBrowserMain)
                  .is_null());
  EXPECT_TRUE(scroll_event_metric
                  ->GetDispatchStageTimestamp(
                      EventMetrics::DispatchStage::kRendererCompositorStarted)
                  .is_null());
  EXPECT_TRUE(scroll_event_metric
                  ->GetDispatchStageTimestamp(
                      EventMetrics::DispatchStage::kRendererCompositorFinished)
                  .is_null());
  EXPECT_TRUE(scroll_event_metric
                  ->GetDispatchStageTimestamp(
                      EventMetrics::DispatchStage::kRendererMainStarted)
                  .is_null());
  EXPECT_TRUE(scroll_event_metric
                  ->GetDispatchStageTimestamp(
                      EventMetrics::DispatchStage::kRendererMainFinished)
                  .is_null());
}

TEST_F(EventMetricsTest, ScrollUpdateCreate) {
  // Arrange
  base::TimeTicks event_time = base::TimeTicks::Now() - base::Microseconds(100);
  base::TimeTicks blocking_touch_dispatched_to_renderer_timestamp =
      base::TimeTicks::Now() - base::Microseconds(70);
  base::TimeTicks arrived_in_browser_main_timestamp =
      base::TimeTicks::Now() - base::Microseconds(50);
  base::TimeTicks now = base::TimeTicks::Now();
  TraceId trace_id(123);

  // Act
  std::unique_ptr<ScrollUpdateEventMetrics> scroll_event_metric =
      ScrollUpdateEventMetrics::Create(
          ui::EventType::kGestureScrollUpdate,
          ui::ScrollInputType::kTouchscreen,
          /*is_inertial=*/false,
          ScrollUpdateEventMetrics::ScrollUpdateType::kContinued, /*delta=*/0.4,
          event_time, arrived_in_browser_main_timestamp,
          blocking_touch_dispatched_to_renderer_timestamp, TraceId(trace_id));

  // Assert
  EXPECT_EQ(trace_id, scroll_event_metric->trace_id());
  EXPECT_EQ(event_time, scroll_event_metric->GetDispatchStageTimestamp(
                            EventMetrics::DispatchStage::kGenerated));
  EXPECT_EQ(blocking_touch_dispatched_to_renderer_timestamp,
            scroll_event_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::
                    kScrollsBlockingTouchDispatchedToRenderer));
  EXPECT_EQ(arrived_in_browser_main_timestamp,
            scroll_event_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kArrivedInBrowserMain));
  EXPECT_LE(now,
            scroll_event_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kArrivedInRendererCompositor));
  // not set
  EXPECT_TRUE(scroll_event_metric
                  ->GetDispatchStageTimestamp(
                      EventMetrics::DispatchStage::kRendererCompositorStarted)
                  .is_null());
  EXPECT_TRUE(scroll_event_metric
                  ->GetDispatchStageTimestamp(
                      EventMetrics::DispatchStage::kRendererCompositorFinished)
                  .is_null());
  EXPECT_TRUE(scroll_event_metric
                  ->GetDispatchStageTimestamp(
                      EventMetrics::DispatchStage::kRendererMainStarted)
                  .is_null());
  EXPECT_TRUE(scroll_event_metric
                  ->GetDispatchStageTimestamp(
                      EventMetrics::DispatchStage::kRendererMainFinished)
                  .is_null());
}

TEST_F(EventMetricsTest, ScrollUpdateCreateFromExisting) {
  // Arrange
  base::TimeTicks event_time = base::TimeTicks::Now() - base::Microseconds(100);
  base::TimeTicks blocking_touch_dispatched_to_renderer_timestamp =
      base::TimeTicks::Now() - base::Microseconds(70);
  base::TimeTicks arrived_in_browser_main_timestamp =
      base::TimeTicks::Now() - base::Microseconds(50);
  TraceId trace_id(123);
  std::unique_ptr<ScrollUpdateEventMetrics> scroll_metric =
      ScrollUpdateEventMetrics::Create(
          ui::EventType::kGestureScrollUpdate,
          ui::ScrollInputType::kTouchscreen,
          /*is_inertial=*/false,
          ScrollUpdateEventMetrics::ScrollUpdateType::kContinued, /*delta=*/0.4,
          event_time, arrived_in_browser_main_timestamp,
          blocking_touch_dispatched_to_renderer_timestamp, trace_id);

  // Act
  std::unique_ptr<ScrollUpdateEventMetrics> copy_scroll_metric =
      ScrollUpdateEventMetrics::CreateFromExisting(
          ui::EventType::kGestureScrollUpdate,
          ui::ScrollInputType::kTouchscreen,
          /*is_inertial=*/false,
          ScrollUpdateEventMetrics::ScrollUpdateType::kContinued, /*delta=*/0.4,
          EventMetrics::DispatchStage::kRendererMainFinished,
          scroll_metric.get());

  // Assert
  EXPECT_NE(scroll_metric->trace_id(), copy_scroll_metric->trace_id());
  EXPECT_EQ(scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kGenerated),
            copy_scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kGenerated));
  EXPECT_EQ(scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::
                    kScrollsBlockingTouchDispatchedToRenderer),
            copy_scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::
                    kScrollsBlockingTouchDispatchedToRenderer));
  EXPECT_EQ(scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kArrivedInBrowserMain),
            copy_scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kArrivedInBrowserMain));

  EXPECT_EQ(scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kArrivedInRendererCompositor),
            copy_scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kArrivedInRendererCompositor));

  EXPECT_EQ(scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kRendererCompositorStarted),
            copy_scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kRendererCompositorStarted));

  EXPECT_EQ(scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kRendererCompositorFinished),
            copy_scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kRendererCompositorFinished));

  EXPECT_EQ(scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kRendererMainStarted),
            copy_scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kRendererMainStarted));

  EXPECT_EQ(scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kRendererMainFinished),
            copy_scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kRendererMainFinished));
}

TEST_F(EventMetricsTest, Create) {
  // Arrange
  base::TimeTicks event_time = base::TimeTicks::Now() - base::Microseconds(100);
  base::TimeTicks arrived_in_browser_main_timestamp =
      base::TimeTicks::Now() - base::Microseconds(50);
  base::TimeTicks now = base::TimeTicks::Now();

  // Act
  std::unique_ptr<EventMetrics> event_metric =
      EventMetrics::Create(ui::EventType::kTouchMoved, event_time,
                           arrived_in_browser_main_timestamp, std::nullopt);

  // Assert
  EXPECT_EQ(event_time, event_metric->GetDispatchStageTimestamp(
                            EventMetrics::DispatchStage::kGenerated));
  EXPECT_EQ(arrived_in_browser_main_timestamp,
            event_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kArrivedInBrowserMain));
  EXPECT_LE(now,
            event_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kArrivedInRendererCompositor));
  // not set
  EXPECT_TRUE(event_metric
                  ->GetDispatchStageTimestamp(
                      EventMetrics::DispatchStage::kRendererCompositorStarted)
                  .is_null());
  EXPECT_TRUE(event_metric
                  ->GetDispatchStageTimestamp(
                      EventMetrics::DispatchStage::kRendererCompositorFinished)
                  .is_null());
  EXPECT_TRUE(event_metric
                  ->GetDispatchStageTimestamp(
                      EventMetrics::DispatchStage::kRendererMainStarted)
                  .is_null());
  EXPECT_TRUE(event_metric
                  ->GetDispatchStageTimestamp(
                      EventMetrics::DispatchStage::kRendererMainFinished)
                  .is_null());
}

TEST_F(EventMetricsTest, CreateFromExisting) {
  // Arrange
  base::TimeTicks event_time = base::TimeTicks::Now() - base::Microseconds(100);
  base::TimeTicks arrived_in_browser_main_timestamp =
      base::TimeTicks::Now() - base::Microseconds(50);
  std::unique_ptr<EventMetrics> event_metric =
      EventMetrics::Create(ui::EventType::kTouchMoved, event_time,
                           arrived_in_browser_main_timestamp, std::nullopt);

  // Act
  std::unique_ptr<EventMetrics> copy_event_metric =
      EventMetrics::CreateFromExisting(
          ui::EventType::kTouchMoved,
          EventMetrics::DispatchStage::kRendererMainFinished,
          event_metric.get());

  // Assert
  EXPECT_EQ(event_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kGenerated),
            copy_event_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kGenerated));
  EXPECT_EQ(event_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kArrivedInBrowserMain),
            copy_event_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kArrivedInBrowserMain));

  EXPECT_EQ(event_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kArrivedInRendererCompositor),
            copy_event_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kArrivedInRendererCompositor));

  EXPECT_EQ(event_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kRendererCompositorStarted),
            copy_event_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kRendererCompositorStarted));

  EXPECT_EQ(event_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kRendererCompositorFinished),
            copy_event_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kRendererCompositorFinished));

  EXPECT_EQ(event_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kRendererMainStarted),
            copy_event_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kRendererMainStarted));

  EXPECT_EQ(event_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kRendererMainFinished),
            copy_event_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kRendererMainFinished));
}

}  // namespace
}  // namespace cc
