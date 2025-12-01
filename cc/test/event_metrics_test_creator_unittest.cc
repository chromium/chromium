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
  std::unique_ptr<EventMetrics> event = metrics_creator_.CreateEventMetrics({});
  EXPECT_EQ(event, nullptr);
}

TEST_F(EventMetricsTestCreatorEventTest, TypeParam) {
  std::unique_ptr<EventMetrics> event =
      metrics_creator_.CreateEventMetrics({.type = ui::EventType::kTouchMoved});
  EXPECT_EQ(event->type(), EventMetrics::EventType::kTouchMoved);
}

TEST_F(EventMetricsTestCreatorEventTest, TimestampParam) {
  std::unique_ptr<EventMetrics> event = metrics_creator_.CreateEventMetrics(
      {.type = ui::EventType::kTouchMoved, .timestamp = MillisecondsTicks(12)});
  EXPECT_EQ(event->type(), EventMetrics::EventType::kTouchMoved);
  EXPECT_EQ(
      event->GetDispatchStageTimestamp(EventMetrics::DispatchStage::kGenerated),
      MillisecondsTicks(12));
}

TEST_F(EventMetricsTestCreatorEventTest, CausedFrameUpdateParam) {
  std::unique_ptr<EventMetrics> event1 = metrics_creator_.CreateEventMetrics(
      {.type = ui::EventType::kTouchMoved, .caused_frame_update = false});
  EXPECT_EQ(event1->type(), EventMetrics::EventType::kTouchMoved);
  EXPECT_FALSE(event1->caused_frame_update());
  std::unique_ptr<EventMetrics> event2 = metrics_creator_.CreateEventMetrics(
      {.type = ui::EventType::kTouchMoved, .caused_frame_update = true});
  EXPECT_EQ(event2->type(), EventMetrics::EventType::kTouchMoved);
  EXPECT_TRUE(event2->caused_frame_update());
}

TEST_F(EventMetricsTestCreatorEventTest, AllParams) {
  std::unique_ptr<EventMetrics> event =
      metrics_creator_.CreateEventMetrics({.type = ui::EventType::kTouchMoved,
                                           .timestamp = MillisecondsTicks(12),
                                           .caused_frame_update = true});
  EXPECT_EQ(event->type(), EventMetrics::EventType::kTouchMoved);
  EXPECT_EQ(
      event->GetDispatchStageTimestamp(EventMetrics::DispatchStage::kGenerated),
      MillisecondsTicks(12));
  EXPECT_TRUE(event->caused_frame_update());
}

template <typename MetricsType, typename ParamsType>
struct EventMetricsParameterizedTestCase {
  std::unique_ptr<MetricsType> (EventMetricsTestCreator::*create_function)(
      ParamsType);
  EventMetrics::EventType expected_type;
};

template <typename MetricsType, typename ParamsType>
class EventMetricsTestCreatorParametrizedTest
    : public EventMetricsTestCreatorTest,
      public testing::WithParamInterface<
          EventMetricsParameterizedTestCase<MetricsType, ParamsType>> {
 protected:
  std::unique_ptr<MetricsType> CreateEvent(ParamsType params) {
    const auto& test_param = this->GetParam();
    return (metrics_creator_.*(test_param.create_function))(params);
  }
};

// Tests for `EventMetricsTestCreator::CreateGestureScrollBegin()`,
// `EventMetricsTestCreator::CreateGestureScrollEnd()` and
// `EventMetricsTestCreator::CreateInertialGestureScrollEnd()`.
using EventMetricsTestCreatorScrollEventTest =
    EventMetricsTestCreatorParametrizedTest<
        ScrollEventMetrics,
        EventMetricsTestCreator::ScrollEventParams>;

INSTANTIATE_TEST_SUITE_P(
    EventMetricsTestCreatorScrollEventTest,
    EventMetricsTestCreatorScrollEventTest,
    testing::ValuesIn<EventMetricsTestCreatorScrollEventTest::ParamType>({
        {
            .create_function =
                &EventMetricsTestCreator::CreateGestureScrollBegin,
            .expected_type = EventMetrics::EventType::kGestureScrollBegin,
        },
        {
            .create_function = &EventMetricsTestCreator::CreateGestureScrollEnd,
            .expected_type = EventMetrics::EventType::kGestureScrollEnd,
        },
        {
            .create_function =
                &EventMetricsTestCreator::CreateInertialGestureScrollEnd,
            .expected_type = EventMetrics::EventType::kInertialGestureScrollEnd,
        },
    }),
    [](const testing::TestParamInfo<
        EventMetricsTestCreatorScrollEventTest::ParamType>& info) {
      return EventMetrics::GetTypeName(info.param.expected_type);
    });

TEST_P(EventMetricsTestCreatorScrollEventTest, NoParams) {
  std::unique_ptr<ScrollEventMetrics> event = CreateEvent({});
  EXPECT_EQ(event->type(), GetParam().expected_type);
}

TEST_P(EventMetricsTestCreatorScrollEventTest, TimestampParam) {
  std::unique_ptr<ScrollEventMetrics> event =
      CreateEvent({.timestamp = MillisecondsTicks(12)});
  EXPECT_EQ(event->type(), GetParam().expected_type);
  EXPECT_EQ(
      event->GetDispatchStageTimestamp(EventMetrics::DispatchStage::kGenerated),
      MillisecondsTicks(12));
}

TEST_P(EventMetricsTestCreatorScrollEventTest, CausedFrameUpdateParam) {
  std::unique_ptr<ScrollEventMetrics> event1 =
      CreateEvent({.caused_frame_update = false});
  EXPECT_EQ(event1->type(), GetParam().expected_type);
  EXPECT_FALSE(event1->caused_frame_update());
  std::unique_ptr<ScrollEventMetrics> event2 =
      CreateEvent({.caused_frame_update = true});
  EXPECT_EQ(event2->type(), GetParam().expected_type);
  EXPECT_TRUE(event2->caused_frame_update());
}

TEST_P(EventMetricsTestCreatorScrollEventTest, BeginFrameArgsParam) {
  viz::BeginFrameArgs args = viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, /* source_id= */ 123, /* sequence_number= */ 456,
      /* frame_time= */ MillisecondsTicks(16),
      /* deadline= */ MillisecondsTicks(24),
      /* interval= */ base::Milliseconds(16),
      viz::BeginFrameArgs::BeginFrameArgsType::NORMAL);
  std::unique_ptr<ScrollEventMetrics> event =
      CreateEvent({.begin_frame_args = args});
  EXPECT_EQ(event->type(), GetParam().expected_type);
  EXPECT_EQ(event->begin_frame_args().frame_id, viz::BeginFrameId(123, 456));
}

TEST_P(EventMetricsTestCreatorScrollEventTest, AllParams) {
  viz::BeginFrameArgs args = viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, /* source_id= */ 123, /* sequence_number= */ 456,
      /* frame_time= */ MillisecondsTicks(16),
      /* deadline= */ MillisecondsTicks(24),
      /* interval= */ base::Milliseconds(16),
      viz::BeginFrameArgs::BeginFrameArgsType::NORMAL);
  std::unique_ptr<ScrollEventMetrics> event =
      CreateEvent({.timestamp = MillisecondsTicks(99),
                   .caused_frame_update = false,
                   .begin_frame_args = args});
  EXPECT_EQ(event->type(), GetParam().expected_type);
  EXPECT_EQ(
      event->GetDispatchStageTimestamp(EventMetrics::DispatchStage::kGenerated),
      MillisecondsTicks(99));
  EXPECT_FALSE(event->caused_frame_update());
  EXPECT_EQ(event->begin_frame_args().frame_id, viz::BeginFrameId(123, 456));
}

// Tests for `EventMetricsTestCreator::CreateFirstGestureScrollUpdate()`,
// `EventMetricsTestCreator::CreateGestureScrollUpdate()` and
// `EventMetricsTestCreator::CreateInertialGestureScrollUpdate()`.
using EventMetricsTestCreatorScrollUpdateEventTest =
    EventMetricsTestCreatorParametrizedTest<
        ScrollUpdateEventMetrics,
        EventMetricsTestCreator::ScrollUpdateEventParams>;

INSTANTIATE_TEST_SUITE_P(
    EventMetricsTestCreatorScrollUpdateEventTest,
    EventMetricsTestCreatorScrollUpdateEventTest,
    testing::ValuesIn<EventMetricsTestCreatorScrollUpdateEventTest::ParamType>({
        {
            .create_function =
                &EventMetricsTestCreator::CreateFirstGestureScrollUpdate,
            .expected_type = EventMetrics::EventType::kFirstGestureScrollUpdate,
        },
        {
            .create_function =
                &EventMetricsTestCreator::CreateGestureScrollUpdate,
            .expected_type = EventMetrics::EventType::kGestureScrollUpdate,
        },
        {
            .create_function =
                &EventMetricsTestCreator::CreateInertialGestureScrollUpdate,
            .expected_type =
                EventMetrics::EventType::kInertialGestureScrollUpdate,
        },
    }),
    [](const testing::TestParamInfo<
        EventMetricsTestCreatorScrollUpdateEventTest::ParamType>& info) {
      return EventMetrics::GetTypeName(info.param.expected_type);
    });

TEST_P(EventMetricsTestCreatorScrollUpdateEventTest, NoParams) {
  std::unique_ptr<ScrollUpdateEventMetrics> event = CreateEvent({});
  EXPECT_EQ(event->type(), GetParam().expected_type);
}

TEST_P(EventMetricsTestCreatorScrollUpdateEventTest, TimestampParam) {
  std::unique_ptr<ScrollUpdateEventMetrics> event =
      CreateEvent({.timestamp = MillisecondsTicks(12)});
  EXPECT_EQ(event->type(), GetParam().expected_type);
  EXPECT_EQ(
      event->GetDispatchStageTimestamp(EventMetrics::DispatchStage::kGenerated),
      MillisecondsTicks(12));
}

TEST_P(EventMetricsTestCreatorScrollUpdateEventTest, DeltaParam) {
  std::unique_ptr<ScrollUpdateEventMetrics> event =
      CreateEvent({.delta = -273.15f});
  EXPECT_EQ(event->type(), GetParam().expected_type);
  EXPECT_EQ(event->delta(), -273.15f);
}

TEST_P(EventMetricsTestCreatorScrollUpdateEventTest, PredictedDeltaParam) {
  std::unique_ptr<ScrollUpdateEventMetrics> event =
      CreateEvent({.predicted_delta = 3.14159f});
  EXPECT_EQ(event->type(), GetParam().expected_type);
  EXPECT_EQ(event->predicted_delta(), 3.14159f);
}

TEST_P(EventMetricsTestCreatorScrollUpdateEventTest, DidScrollParam) {
  std::unique_ptr<ScrollUpdateEventMetrics> event1 =
      CreateEvent({.did_scroll = false});
  EXPECT_EQ(event1->type(), GetParam().expected_type);
  EXPECT_FALSE(event1->did_scroll());
  std::unique_ptr<ScrollUpdateEventMetrics> event2 =
      CreateEvent({.did_scroll = true});
  EXPECT_EQ(event2->type(), GetParam().expected_type);
  EXPECT_TRUE(event2->did_scroll());
}

TEST_P(EventMetricsTestCreatorScrollUpdateEventTest, CausedFrameUpdateParam) {
  std::unique_ptr<ScrollUpdateEventMetrics> event1 =
      CreateEvent({.caused_frame_update = false});
  EXPECT_EQ(event1->type(), GetParam().expected_type);
  EXPECT_FALSE(event1->caused_frame_update());
  std::unique_ptr<ScrollUpdateEventMetrics> event2 =
      CreateEvent({.caused_frame_update = true});
  EXPECT_EQ(event2->type(), GetParam().expected_type);
  EXPECT_TRUE(event2->caused_frame_update());
}

TEST_P(EventMetricsTestCreatorScrollUpdateEventTest, IsSyntheticParam) {
  std::unique_ptr<ScrollUpdateEventMetrics> event1 =
      CreateEvent({.is_synthetic = false});
  EXPECT_EQ(event1->type(), GetParam().expected_type);
  EXPECT_FALSE(event1->is_synthetic());
  std::unique_ptr<ScrollUpdateEventMetrics> event2 =
      CreateEvent({.is_synthetic = true});
  EXPECT_EQ(event2->type(), GetParam().expected_type);
  EXPECT_TRUE(event2->is_synthetic());
}

TEST_P(EventMetricsTestCreatorScrollUpdateEventTest, TraceIdParam) {
  std::unique_ptr<ScrollUpdateEventMetrics> event =
      CreateEvent({.trace_id = EventMetrics::TraceId(123)});
  EXPECT_EQ(event->type(), GetParam().expected_type);
  EXPECT_EQ(event->trace_id()->value(), 123);
}

TEST_P(EventMetricsTestCreatorScrollUpdateEventTest, BeginFrameArgsParam) {
  viz::BeginFrameArgs args = viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, /* source_id= */ 123, /* sequence_number= */ 456,
      /* frame_time= */ MillisecondsTicks(16),
      /* deadline= */ MillisecondsTicks(24),
      /* interval= */ base::Milliseconds(16),
      viz::BeginFrameArgs::BeginFrameArgsType::NORMAL);
  std::unique_ptr<ScrollUpdateEventMetrics> event =
      CreateEvent({.begin_frame_args = args});
  EXPECT_EQ(event->type(), GetParam().expected_type);
  EXPECT_EQ(event->begin_frame_args().frame_id, viz::BeginFrameId(123, 456));
}

TEST_P(EventMetricsTestCreatorScrollUpdateEventTest, AllParams) {
  viz::BeginFrameArgs args = viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, /* source_id= */ 123, /* sequence_number= */ 456,
      /* frame_time= */ MillisecondsTicks(16),
      /* deadline= */ MillisecondsTicks(24),
      /* interval= */ base::Milliseconds(16),
      viz::BeginFrameArgs::BeginFrameArgsType::NORMAL);
  std::unique_ptr<ScrollUpdateEventMetrics> event =
      CreateEvent({.timestamp = MillisecondsTicks(99),
                   .delta = 7.0f,
                   .predicted_delta = -7.0f,
                   .caused_frame_update = true,
                   .did_scroll = false,
                   .is_synthetic = true,
                   .trace_id = EventMetrics::TraceId(456),
                   .begin_frame_args = args});
  EXPECT_EQ(event->type(), GetParam().expected_type);
  EXPECT_EQ(
      event->GetDispatchStageTimestamp(EventMetrics::DispatchStage::kGenerated),
      MillisecondsTicks(99));
  EXPECT_EQ(event->delta(), 7.0f);
  EXPECT_EQ(event->predicted_delta(), -7.0f);
  EXPECT_TRUE(event->caused_frame_update());
  EXPECT_FALSE(event->did_scroll());
  EXPECT_TRUE(event->is_synthetic());
  EXPECT_EQ(event->trace_id()->value(), 456);
  EXPECT_EQ(event->begin_frame_args().frame_id, viz::BeginFrameId(123, 456));
}

}  // namespace cc
