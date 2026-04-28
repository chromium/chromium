// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/event_metrics_test_creator.h"

#include <memory>

#include "base/time/time.h"
#include "cc/metrics/event_metrics.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/types/event_type.h"

namespace cc {

namespace {

base::TimeTicks MillisecondsTicks(int ms) {
  return base::TimeTicks() + base::Milliseconds(ms);
}

}  // namespace

class EventMetricsTestCreatorTest : public testing::Test {
 protected:
  EventMetricsTestCreator metrics_creator_;
};

// Tests for `EventMetricsTestCreator::CreateEventMetrics()`.
class EventMetricsTestCreatorEventTest : public EventMetricsTestCreatorTest {};

TEST_F(EventMetricsTestCreatorEventTest, NoParams) {
  std::unique_ptr<EventMetrics> event =
      metrics_creator_.CreateEventBuilder(ui::EventType::kUnknown).Build();
  EXPECT_EQ(event, nullptr);
}

TEST_F(EventMetricsTestCreatorEventTest, TypeParam) {
  std::unique_ptr<EventMetrics> event =
      metrics_creator_.CreateEventBuilder(ui::EventType::kTouchMoved).Build();
  EXPECT_EQ(event->type(), EventMetrics::EventType::kTouchMoved);
}

TEST_F(EventMetricsTestCreatorEventTest, TimestampParam) {
  std::unique_ptr<EventMetrics> event =
      metrics_creator_.CreateEventBuilder(ui::EventType::kTouchMoved)
          .SetTimestamp(MillisecondsTicks(12))
          .Build();
  EXPECT_EQ(event->type(), EventMetrics::EventType::kTouchMoved);
  EXPECT_EQ(
      event->GetDispatchStageTimestamp(EventMetrics::DispatchStage::kGenerated),
      MillisecondsTicks(12));
}

TEST_F(EventMetricsTestCreatorEventTest,
       ArrivedInRendererCompositorTimestampParam) {
  std::unique_ptr<EventMetrics> event =
      metrics_creator_.CreateEventBuilder(ui::EventType::kTouchMoved)
          .SetArrivedInRendererCompositorTimestamp(MillisecondsTicks(4321))
          .Build();
  EXPECT_EQ(event->type(), EventMetrics::EventType::kTouchMoved);
  EXPECT_EQ(event->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kArrivedInRendererCompositor),
            MillisecondsTicks(4321));
}

TEST_F(EventMetricsTestCreatorEventTest, CausedFrameUpdateParam) {
  std::unique_ptr<EventMetrics> event1 =
      metrics_creator_.CreateEventBuilder(ui::EventType::kTouchMoved)
          .SetCausedFrameUpdate(false)
          .Build();
  EXPECT_EQ(event1->type(), EventMetrics::EventType::kTouchMoved);
  EXPECT_FALSE(event1->caused_frame_update());
  std::unique_ptr<EventMetrics> event2 =
      metrics_creator_.CreateEventBuilder(ui::EventType::kTouchMoved)
          .SetCausedFrameUpdate(true)
          .Build();
  EXPECT_EQ(event2->type(), EventMetrics::EventType::kTouchMoved);
  EXPECT_TRUE(event2->caused_frame_update());
}

TEST_F(EventMetricsTestCreatorEventTest, AllParams) {
  std::unique_ptr<EventMetrics> event =
      metrics_creator_.CreateEventBuilder(ui::EventType::kTouchMoved)
          .SetTimestamp(MillisecondsTicks(12))
          .SetArrivedInRendererCompositorTimestamp(MillisecondsTicks(15))
          .SetCausedFrameUpdate(true)
          .Build();
  EXPECT_EQ(event->type(), EventMetrics::EventType::kTouchMoved);
  EXPECT_EQ(
      event->GetDispatchStageTimestamp(EventMetrics::DispatchStage::kGenerated),
      MillisecondsTicks(12));
  EXPECT_EQ(event->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kArrivedInRendererCompositor),
            MillisecondsTicks(15));
  EXPECT_TRUE(event->caused_frame_update());
}

template <typename MetricsType, typename BuilderType>
struct EventMetricsParameterizedTestCase {
  BuilderType (EventMetricsTestCreator::*builder_function)();
  EventMetrics::EventType expected_type;
};

template <typename MetricsType, typename BuilderType>
class EventMetricsTestCreatorParametrizedTest
    : public EventMetricsTestCreatorTest,
      public testing::WithParamInterface<
          EventMetricsParameterizedTestCase<MetricsType, BuilderType>> {
 protected:
  BuilderType CreateEventBuilder() {
    const auto& test_param = this->GetParam();
    return (metrics_creator_.*(test_param.builder_function))();
  }
};

// Tests for `EventMetricsTestCreator::CreateGestureScrollBegin()`,
// `EventMetricsTestCreator::CreateGestureScrollEnd()` and
// `EventMetricsTestCreator::CreateInertialGestureScrollEnd()`.
using EventMetricsTestCreatorScrollEventTest =
    EventMetricsTestCreatorParametrizedTest<
        ScrollEventMetrics,
        EventMetricsTestCreator::ScrollEventBuilder>;

INSTANTIATE_TEST_SUITE_P(
    EventMetricsTestCreatorScrollEventTest,
    EventMetricsTestCreatorScrollEventTest,
    testing::ValuesIn<EventMetricsTestCreatorScrollEventTest::ParamType>({
        {
            .builder_function =
                &EventMetricsTestCreator::GestureScrollBeginBuilder,
            .expected_type = EventMetrics::EventType::kGestureScrollBegin,
        },
        {
            .builder_function =
                &EventMetricsTestCreator::GestureScrollEndBuilder,
            .expected_type = EventMetrics::EventType::kGestureScrollEnd,
        },
        {
            .builder_function =
                &EventMetricsTestCreator::InertialGestureScrollEndBuilder,
            .expected_type = EventMetrics::EventType::kInertialGestureScrollEnd,
        },
    }),
    [](const testing::TestParamInfo<
        EventMetricsTestCreatorScrollEventTest::ParamType>& info) {
      return EventMetrics::GetTypeName(info.param.expected_type);
    });

TEST_P(EventMetricsTestCreatorScrollEventTest, NoParams) {
  std::unique_ptr<ScrollEventMetrics> event = CreateEventBuilder().Build();
  EXPECT_EQ(event->type(), GetParam().expected_type);
}

TEST_P(EventMetricsTestCreatorScrollEventTest, TimestampParam) {
  std::unique_ptr<ScrollEventMetrics> event =
      CreateEventBuilder().SetTimestamp(MillisecondsTicks(12)).Build();
  EXPECT_EQ(event->type(), GetParam().expected_type);
  EXPECT_EQ(
      event->GetDispatchStageTimestamp(EventMetrics::DispatchStage::kGenerated),
      MillisecondsTicks(12));
}

TEST_P(EventMetricsTestCreatorScrollEventTest,
       ArrivedInRendererCompositorTimestampParam) {
  std::unique_ptr<ScrollEventMetrics> event =
      CreateEventBuilder()
          .SetArrivedInRendererCompositorTimestamp(MillisecondsTicks(5432))
          .Build();
  EXPECT_EQ(event->type(), GetParam().expected_type);
  EXPECT_EQ(event->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kArrivedInRendererCompositor),
            MillisecondsTicks(5432));
}

TEST_P(EventMetricsTestCreatorScrollEventTest, CausedFrameUpdateParam) {
  std::unique_ptr<ScrollEventMetrics> event1 =
      CreateEventBuilder().SetCausedFrameUpdate(false).Build();
  EXPECT_EQ(event1->type(), GetParam().expected_type);
  EXPECT_FALSE(event1->caused_frame_update());
  std::unique_ptr<ScrollEventMetrics> event2 =
      CreateEventBuilder().SetCausedFrameUpdate(true).Build();
  EXPECT_EQ(event2->type(), GetParam().expected_type);
  EXPECT_TRUE(event2->caused_frame_update());
}

TEST_P(EventMetricsTestCreatorScrollEventTest, DispatchArgsParam) {
  ScrollEventMetrics::DispatchBeginFrameArgs args = {
      .frame_time = MillisecondsTicks(24),
      .interval = base::Milliseconds(16),
      .frame_id = viz::BeginFrameId(123, 456),
  };
  std::unique_ptr<ScrollEventMetrics> event =
      CreateEventBuilder().SetDispatchArgs(args).Build();
  EXPECT_EQ(event->type(), GetParam().expected_type);
  EXPECT_EQ(event->dispatch_args(), args);
}

TEST_P(EventMetricsTestCreatorScrollEventTest, AllParams) {
  ScrollEventMetrics::DispatchBeginFrameArgs args = {
      .frame_time = MillisecondsTicks(24),
      .interval = base::Milliseconds(16),
      .frame_id = viz::BeginFrameId(123, 456),
  };
  std::unique_ptr<ScrollEventMetrics> event =
      CreateEventBuilder()
          .SetTimestamp(MillisecondsTicks(99))
          .SetArrivedInRendererCompositorTimestamp(MillisecondsTicks(101))
          .SetCausedFrameUpdate(false)
          .SetDispatchArgs(args)
          .Build();
  EXPECT_EQ(event->type(), GetParam().expected_type);
  EXPECT_EQ(
      event->GetDispatchStageTimestamp(EventMetrics::DispatchStage::kGenerated),
      MillisecondsTicks(99));
  EXPECT_EQ(event->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kArrivedInRendererCompositor),
            MillisecondsTicks(101));
  EXPECT_FALSE(event->caused_frame_update());
  EXPECT_EQ(event->dispatch_args(), args);
}

// Tests for `EventMetricsTestCreator::CreateFirstGestureScrollUpdate()`,
// `EventMetricsTestCreator::CreateGestureScrollUpdate()` and
// `EventMetricsTestCreator::CreateInertialGestureScrollUpdate()`.
using EventMetricsTestCreatorScrollUpdateEventTest =
    EventMetricsTestCreatorParametrizedTest<
        ScrollUpdateEventMetrics,
        EventMetricsTestCreator::ScrollUpdateEventBuilder>;

INSTANTIATE_TEST_SUITE_P(
    EventMetricsTestCreatorScrollUpdateEventTest,
    EventMetricsTestCreatorScrollUpdateEventTest,
    testing::ValuesIn<EventMetricsTestCreatorScrollUpdateEventTest::ParamType>({
        {
            .builder_function =
                &EventMetricsTestCreator::FirstGestureScrollUpdateBuilder,
            .expected_type = EventMetrics::EventType::kFirstGestureScrollUpdate,
        },
        {
            .builder_function =
                &EventMetricsTestCreator::GestureScrollUpdateBuilder,
            .expected_type = EventMetrics::EventType::kGestureScrollUpdate,
        },
        {
            .builder_function =
                &EventMetricsTestCreator::InertialGestureScrollUpdateBuilder,
            .expected_type =
                EventMetrics::EventType::kInertialGestureScrollUpdate,
        },
    }),
    [](const testing::TestParamInfo<
        EventMetricsTestCreatorScrollUpdateEventTest::ParamType>& info) {
      return EventMetrics::GetTypeName(info.param.expected_type);
    });

TEST_P(EventMetricsTestCreatorScrollUpdateEventTest, NoParams) {
  std::unique_ptr<ScrollUpdateEventMetrics> event =
      CreateEventBuilder().Build();
  EXPECT_EQ(event->type(), GetParam().expected_type);
}

TEST_P(EventMetricsTestCreatorScrollUpdateEventTest, TimestampParam) {
  std::unique_ptr<ScrollUpdateEventMetrics> event =
      CreateEventBuilder().SetTimestamp(MillisecondsTicks(12)).Build();
  EXPECT_EQ(event->type(), GetParam().expected_type);
  EXPECT_EQ(
      event->GetDispatchStageTimestamp(EventMetrics::DispatchStage::kGenerated),
      MillisecondsTicks(12));
}

TEST_P(EventMetricsTestCreatorScrollUpdateEventTest,
       ArrivedInRendererCompositorTimestampParam) {
  std::unique_ptr<ScrollUpdateEventMetrics> event =
      CreateEventBuilder()
          .SetArrivedInRendererCompositorTimestamp(MillisecondsTicks(6543))
          .Build();
  EXPECT_EQ(event->type(), GetParam().expected_type);
  EXPECT_EQ(event->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kArrivedInRendererCompositor),
            MillisecondsTicks(6543));
}

TEST_P(EventMetricsTestCreatorScrollUpdateEventTest, DeltaParam) {
  std::unique_ptr<ScrollUpdateEventMetrics> event =
      CreateEventBuilder().SetDelta(-273.15f).Build();
  EXPECT_EQ(event->type(), GetParam().expected_type);
  EXPECT_EQ(event->delta(), -273.15f);
}

TEST_P(EventMetricsTestCreatorScrollUpdateEventTest, PredictedDeltaParam) {
  std::unique_ptr<ScrollUpdateEventMetrics> event =
      CreateEventBuilder().SetPredictedDelta(3.14159f).Build();
  EXPECT_EQ(event->type(), GetParam().expected_type);
  EXPECT_EQ(event->predicted_delta(), 3.14159f);
}

TEST_P(EventMetricsTestCreatorScrollUpdateEventTest, DidScrollParam) {
  std::unique_ptr<ScrollUpdateEventMetrics> event1 =
      CreateEventBuilder().SetDidScroll(false).Build();
  EXPECT_EQ(event1->type(), GetParam().expected_type);
  EXPECT_FALSE(event1->did_scroll());
  std::unique_ptr<ScrollUpdateEventMetrics> event2 =
      CreateEventBuilder().SetDidScroll(true).Build();
  EXPECT_EQ(event2->type(), GetParam().expected_type);
  EXPECT_TRUE(event2->did_scroll());
}

TEST_P(EventMetricsTestCreatorScrollUpdateEventTest, CausedFrameUpdateParam) {
  std::unique_ptr<ScrollUpdateEventMetrics> event1 =
      CreateEventBuilder().SetCausedFrameUpdate(false).Build();
  EXPECT_EQ(event1->type(), GetParam().expected_type);
  EXPECT_FALSE(event1->caused_frame_update());
  std::unique_ptr<ScrollUpdateEventMetrics> event2 =
      CreateEventBuilder().SetCausedFrameUpdate(true).Build();
  EXPECT_EQ(event2->type(), GetParam().expected_type);
  EXPECT_TRUE(event2->caused_frame_update());
}

TEST_P(EventMetricsTestCreatorScrollUpdateEventTest, IsSyntheticParam) {
  std::unique_ptr<ScrollUpdateEventMetrics> event1 =
      CreateEventBuilder().SetIsSynthetic(false).Build();
  EXPECT_EQ(event1->type(), GetParam().expected_type);
  EXPECT_FALSE(event1->is_synthetic());
  std::unique_ptr<ScrollUpdateEventMetrics> event2 =
      CreateEventBuilder().SetIsSynthetic(true).Build();
  EXPECT_EQ(event2->type(), GetParam().expected_type);
  EXPECT_TRUE(event2->is_synthetic());
}

TEST_P(EventMetricsTestCreatorScrollUpdateEventTest, TraceIdParam) {
  std::unique_ptr<ScrollUpdateEventMetrics> event =
      CreateEventBuilder().SetTraceId(EventMetrics::TraceId(123)).Build();
  EXPECT_EQ(event->type(), GetParam().expected_type);
  EXPECT_EQ(event->trace_id()->value(), 123);
}

TEST_P(EventMetricsTestCreatorScrollUpdateEventTest, DispatchArgsParam) {
  ScrollEventMetrics::DispatchBeginFrameArgs args = {
      .frame_time = MillisecondsTicks(24),
      .interval = base::Milliseconds(16),
      .frame_id = viz::BeginFrameId(123, 456),
  };
  std::unique_ptr<ScrollUpdateEventMetrics> event =
      CreateEventBuilder().SetDispatchArgs(args).Build();
  EXPECT_EQ(event->type(), GetParam().expected_type);
  EXPECT_EQ(event->dispatch_args(), args);
}

TEST_P(EventMetricsTestCreatorScrollUpdateEventTest, AllParams) {
  ScrollEventMetrics::DispatchBeginFrameArgs args = {
      .frame_time = MillisecondsTicks(24),
      .interval = base::Milliseconds(16),
      .frame_id = viz::BeginFrameId(123, 456),
  };
  std::unique_ptr<ScrollUpdateEventMetrics> event =
      CreateEventBuilder()
          .SetTimestamp(MillisecondsTicks(99))
          .SetArrivedInRendererCompositorTimestamp(MillisecondsTicks(102))
          .SetDelta(7.0f)
          .SetPredictedDelta(-7.0f)
          .SetCausedFrameUpdate(true)
          .SetDidScroll(false)
          .SetIsSynthetic(true)
          .SetTraceId(EventMetrics::TraceId(456))
          .SetDispatchArgs(args)
          .Build();
  EXPECT_EQ(event->type(), GetParam().expected_type);
  EXPECT_EQ(
      event->GetDispatchStageTimestamp(EventMetrics::DispatchStage::kGenerated),
      MillisecondsTicks(99));
  EXPECT_EQ(event->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kArrivedInRendererCompositor),
            MillisecondsTicks(102));
  EXPECT_EQ(event->delta(), 7.0f);
  EXPECT_EQ(event->predicted_delta(), -7.0f);
  EXPECT_TRUE(event->caused_frame_update());
  EXPECT_FALSE(event->did_scroll());
  EXPECT_TRUE(event->is_synthetic());
  EXPECT_EQ(event->trace_id()->value(), 456);
  EXPECT_EQ(event->dispatch_args(), args);
}

}  // namespace cc
