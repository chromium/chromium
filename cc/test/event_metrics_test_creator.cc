// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/event_metrics_test_creator.h"

#include <memory>
#include <optional>

#include "base/check_op.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "cc/metrics/event_metrics.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "ui/events/types/event_type.h"

namespace cc {

namespace {

enum class EventMetricsType {
  kEventMetrics,
  kScrollEventMetrics,
  kScrollUpdateEventMetrics,
  // TODO: Add `kPinchEventMetrics` if we ever need it.
};

constexpr EventMetricsType GetEventMetricsTypeFor(ui::EventType type) {
  using enum ui::EventType;
  switch (type) {
    case kGestureScrollUpdate:
      return EventMetricsType::kScrollUpdateEventMetrics;
    case kGestureScrollBegin:
    case kGestureScrollEnd:
      return EventMetricsType::kScrollEventMetrics;
    default:
      return EventMetricsType::kEventMetrics;
  }
}

}  // namespace

std::unique_ptr<EventMetrics> EventMetricsTestCreator::CreateEventMetrics(
    EventParams params) {
  CHECK_EQ(GetEventMetricsTypeFor(params.type),
           EventMetricsType::kEventMetrics);
  auto event = EventMetrics::CreateForTesting(
      params.type, params.timestamp,
      /* arrived_in_browser_main_timestamp= */ params.timestamp +
          base::Nanoseconds(1),
      &test_tick_clock_, /* trace_id= */ std::nullopt);
  if (event == nullptr) {
    return event;
  }
  if (params.caused_frame_update.has_value()) {
    event->set_caused_frame_update(*params.caused_frame_update);
  }
  return event;
}

std::unique_ptr<ScrollEventMetrics>
EventMetricsTestCreator::CreateGestureScrollBegin(ScrollEventParams params) {
  return CreateScrollEventMetrics(ui::EventType::kGestureScrollBegin,
                                  /* is_inertial= */ false, params);
}

std::unique_ptr<ScrollEventMetrics>
EventMetricsTestCreator::CreateGestureScrollEnd(ScrollEventParams params) {
  return CreateScrollEventMetrics(ui::EventType::kGestureScrollEnd,
                                  /* is_inertial= */ false, params);
}

std::unique_ptr<ScrollEventMetrics>
EventMetricsTestCreator::CreateInertialGestureScrollEnd(
    ScrollEventParams params) {
  return CreateScrollEventMetrics(ui::EventType::kGestureScrollEnd,
                                  /* is_inertial= */ true, params);
}

std::unique_ptr<ScrollUpdateEventMetrics>
EventMetricsTestCreator::CreateFirstGestureScrollUpdate(
    ScrollUpdateEventParams params) {
  return CreateScrollUpdateEventMetrics(
      /* is_inertial= */ false,
      ScrollUpdateEventMetrics::ScrollUpdateType::kStarted, params);
}

std::unique_ptr<ScrollUpdateEventMetrics>
EventMetricsTestCreator::CreateGestureScrollUpdate(
    ScrollUpdateEventParams params) {
  return CreateScrollUpdateEventMetrics(
      /* is_inertial= */ false,
      ScrollUpdateEventMetrics::ScrollUpdateType::kContinued, params);
}

std::unique_ptr<ScrollUpdateEventMetrics>
EventMetricsTestCreator::CreateInertialGestureScrollUpdate(
    ScrollUpdateEventParams params) {
  return CreateScrollUpdateEventMetrics(
      /* is_inertial= */ true,
      ScrollUpdateEventMetrics::ScrollUpdateType::kContinued, params);
}

std::unique_ptr<ScrollEventMetrics>
EventMetricsTestCreator::CreateScrollEventMetrics(ui::EventType type,
                                                  bool is_inertial,
                                                  ScrollEventParams params) {
  CHECK_EQ(GetEventMetricsTypeFor(type), EventMetricsType::kScrollEventMetrics);
  auto event = ScrollEventMetrics::CreateForTesting(
      type, ui::ScrollInputType::kTouchscreen, is_inertial, params.timestamp,
      /* arrived_in_browser_main_timestamp= */ params.timestamp +
          base::Nanoseconds(1),
      &test_tick_clock_);
  if (params.caused_frame_update.has_value()) {
    event->set_caused_frame_update(*params.caused_frame_update);
  }
  if (params.begin_frame_args.has_value()) {
    event->set_begin_frame_args(*params.begin_frame_args);
  }
  return event;
}

std::unique_ptr<ScrollUpdateEventMetrics>
EventMetricsTestCreator::CreateScrollUpdateEventMetrics(
    bool is_inertial,
    ScrollUpdateEventMetrics::ScrollUpdateType scroll_update_type,
    ScrollUpdateEventParams params) {
  auto event = ScrollUpdateEventMetrics::CreateForTesting(
      ui::EventType::kGestureScrollUpdate, ui::ScrollInputType::kTouchscreen,
      is_inertial, scroll_update_type, params.delta, params.timestamp,
      /* arrived_in_browser_main_timestamp= */ params.timestamp +
          base::Nanoseconds(1),
      &test_tick_clock_, params.trace_id);
  if (params.predicted_delta.has_value()) {
    event->set_predicted_delta(*params.predicted_delta);
  }
  if (params.caused_frame_update.has_value()) {
    event->set_caused_frame_update(*params.caused_frame_update);
  }
  if (params.did_scroll.has_value()) {
    event->set_did_scroll(*params.did_scroll);
  }
  if (params.is_synthetic.has_value()) {
    event->set_is_synthetic(*params.is_synthetic);
  }
  if (params.begin_frame_args.has_value()) {
    event->set_begin_frame_args(*params.begin_frame_args);
  }
  return event;
}

}  // namespace cc
