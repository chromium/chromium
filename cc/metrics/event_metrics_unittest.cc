// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/event_metrics.h"

#include "base/test/simple_test_tick_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {
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
  base::TimeTicks arrived_in_browser_main_timestamp;
  base::TimeTicks now = base::TimeTicks::Now();

  // Act
  std::unique_ptr<ScrollEventMetrics> scroll_event_metric =
      ScrollEventMetrics::Create(
          ui::ET_GESTURE_SCROLL_BEGIN, ui::ScrollInputType::kTouchscreen,
          /*is_inertial=*/false, event_time, arrived_in_browser_main_timestamp);

  // Assert
  EXPECT_EQ(event_time, scroll_event_metric->GetDispatchStageTimestamp(
                            EventMetrics::DispatchStage::kGenerated));
  EXPECT_LE(now,
            scroll_event_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kArrivedInRendererCompositor));
  // not set
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
  base::TimeTicks arrived_in_browser_main_timestamp =
      base::TimeTicks::Now() - base::Microseconds(50);
  base::TimeTicks now = base::TimeTicks::Now();

  // Act
  std::unique_ptr<ScrollEventMetrics> scroll_event_metric =
      ScrollEventMetrics::Create(
          ui::ET_GESTURE_SCROLL_BEGIN, ui::ScrollInputType::kTouchscreen,
          /*is_inertial=*/false, event_time, arrived_in_browser_main_timestamp);

  // Assert
  EXPECT_EQ(event_time, scroll_event_metric->GetDispatchStageTimestamp(
                            EventMetrics::DispatchStage::kGenerated));
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
  base::TimeTicks arrived_in_browser_main_timestamp =
      base::TimeTicks::Now() - base::Microseconds(50);
  std::unique_ptr<ScrollEventMetrics> scroll_metric =
      ScrollEventMetrics::Create(
          ui::ET_GESTURE_SCROLL_BEGIN, ui::ScrollInputType::kTouchscreen,
          /*is_inertial=*/false, event_time, arrived_in_browser_main_timestamp);

  // Act
  std::unique_ptr<ScrollEventMetrics> copy_scroll_metric =
      ScrollEventMetrics::CreateFromExisting(
          ui::ET_GESTURE_SCROLL_BEGIN, ui::ScrollInputType::kTouchscreen,
          /*is_inertial=*/false,
          EventMetrics::DispatchStage::kRendererMainFinished,
          scroll_metric.get());

  // Assert
  EXPECT_EQ(scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kGenerated),
            copy_scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kGenerated));
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
  base::TimeTicks arrived_in_browser_main_timestamp;
  base::TimeTicks now = base::TimeTicks::Now();

  // Act
  std::unique_ptr<ScrollUpdateEventMetrics> scroll_event_metric =
      ScrollUpdateEventMetrics::Create(
          ui::ET_GESTURE_SCROLL_UPDATE, ui::ScrollInputType::kTouchscreen,
          /*is_inertial=*/false,
          ScrollUpdateEventMetrics::ScrollUpdateType::kContinued, /*delta=*/0.4,
          event_time, arrived_in_browser_main_timestamp);

  // Assert
  EXPECT_EQ(event_time, scroll_event_metric->GetDispatchStageTimestamp(
                            EventMetrics::DispatchStage::kGenerated));
  EXPECT_LE(now,
            scroll_event_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kArrivedInRendererCompositor));
  // not set
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
  base::TimeTicks arrived_in_browser_main_timestamp =
      base::TimeTicks::Now() - base::Microseconds(50);
  base::TimeTicks now = base::TimeTicks::Now();

  // Act
  std::unique_ptr<ScrollUpdateEventMetrics> scroll_event_metric =
      ScrollUpdateEventMetrics::Create(
          ui::ET_GESTURE_SCROLL_UPDATE, ui::ScrollInputType::kTouchscreen,
          /*is_inertial=*/false,
          ScrollUpdateEventMetrics::ScrollUpdateType::kContinued, /*delta=*/0.4,
          event_time, arrived_in_browser_main_timestamp);

  // Assert
  EXPECT_EQ(event_time, scroll_event_metric->GetDispatchStageTimestamp(
                            EventMetrics::DispatchStage::kGenerated));
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
  base::TimeTicks arrived_in_browser_main_timestamp =
      base::TimeTicks::Now() - base::Microseconds(50);
  std::unique_ptr<ScrollUpdateEventMetrics> scroll_metric =
      ScrollUpdateEventMetrics::Create(
          ui::ET_GESTURE_SCROLL_UPDATE, ui::ScrollInputType::kTouchscreen,
          /*is_inertial=*/false,
          ScrollUpdateEventMetrics::ScrollUpdateType::kContinued, /*delta=*/0.4,
          event_time, arrived_in_browser_main_timestamp);

  // Act
  std::unique_ptr<ScrollUpdateEventMetrics> copy_scroll_metric =
      ScrollUpdateEventMetrics::CreateFromExisting(
          ui::ET_GESTURE_SCROLL_UPDATE, ui::ScrollInputType::kTouchscreen,
          /*is_inertial=*/false,
          ScrollUpdateEventMetrics::ScrollUpdateType::kContinued, /*delta=*/0.4,
          EventMetrics::DispatchStage::kRendererMainFinished,
          scroll_metric.get());

  // Assert
  EXPECT_EQ(scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kGenerated),
            copy_scroll_metric->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kGenerated));
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

}  // namespace
}  // namespace cc
