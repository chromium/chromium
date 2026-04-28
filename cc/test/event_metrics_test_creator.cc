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

template <typename Derived>
EventMetricsTestCreator::EventBuilderBase<Derived>::EventBuilderBase(
    base::SimpleTestTickClock& clock,
    ui::EventType type)
    : clock_(clock), type_(type) {}

template <typename Derived>
Derived& EventMetricsTestCreator::EventBuilderBase<Derived>::SetTimestamp(
    base::TimeTicks timestamp) {
  timestamp_ = timestamp;
  return static_cast<Derived&>(*this);
}

template <typename Derived>
Derived& EventMetricsTestCreator::EventBuilderBase<Derived>::
    SetArrivedInRendererCompositorTimestamp(
        base::TimeTicks arrived_in_renderer_compositor_timestamp) {
  arrived_in_renderer_compositor_timestamp_ =
      arrived_in_renderer_compositor_timestamp;
  return static_cast<Derived&>(*this);
}

template <typename Derived>
Derived&
EventMetricsTestCreator::EventBuilderBase<Derived>::SetCausedFrameUpdate(
    bool caused_frame_update) {
  caused_frame_update_ = caused_frame_update;
  return static_cast<Derived&>(*this);
}

template <typename Derived>
EventMetricsTestCreator::ScrollEventBuilderBase<
    Derived>::ScrollEventBuilderBase(base::SimpleTestTickClock& clock,
                                     ui::EventType type,
                                     bool is_inertial)
    : EventBuilderBase<Derived>(clock, type), is_inertial_(is_inertial) {}

template <typename Derived>
Derived&
EventMetricsTestCreator::ScrollEventBuilderBase<Derived>::SetDispatchArgs(
    ScrollEventMetrics::DispatchBeginFrameArgs dispatch_args) {
  dispatch_args_ = dispatch_args;
  return static_cast<Derived&>(*this);
}

template <typename Derived>
EventMetricsTestCreator::ScrollUpdateEventBuilderBase<Derived>::
    ScrollUpdateEventBuilderBase(
        base::SimpleTestTickClock& clock,
        ScrollUpdateEventMetrics::ScrollUpdateType scroll_update_type,
        bool is_inertial)
    : ScrollEventBuilderBase<Derived>(clock,
                                      ui::EventType::kGestureScrollUpdate,
                                      is_inertial),
      scroll_update_type_(scroll_update_type) {}

template <typename Derived>
Derived&
EventMetricsTestCreator::ScrollUpdateEventBuilderBase<Derived>::SetDelta(
    float delta) {
  delta_ = delta;
  return static_cast<Derived&>(*this);
}

template <typename Derived>
Derived& EventMetricsTestCreator::ScrollUpdateEventBuilderBase<
    Derived>::SetPredictedDelta(float predicted_delta) {
  predicted_delta_ = predicted_delta;
  return static_cast<Derived&>(*this);
}

template <typename Derived>
Derived&
EventMetricsTestCreator::ScrollUpdateEventBuilderBase<Derived>::SetDidScroll(
    bool did_scroll) {
  did_scroll_ = did_scroll;
  return static_cast<Derived&>(*this);
}

template <typename Derived>
Derived&
EventMetricsTestCreator::ScrollUpdateEventBuilderBase<Derived>::SetIsSynthetic(
    bool is_synthetic) {
  is_synthetic_ = is_synthetic;
  return static_cast<Derived&>(*this);
}

template <typename Derived>
Derived&
EventMetricsTestCreator::ScrollUpdateEventBuilderBase<Derived>::SetTraceId(
    EventMetrics::TraceId trace_id) {
  trace_id_ = trace_id;
  return static_cast<Derived&>(*this);
}

EventMetricsTestCreator::EventBuilder::EventBuilder(
    base::SimpleTestTickClock& clock,
    ui::EventType type)
    : EventBuilderBase<EventBuilder>(clock, type) {}

EventMetricsTestCreator::EventBuilder::~EventBuilder() = default;

std::unique_ptr<EventMetrics> EventMetricsTestCreator::EventBuilder::Build() {
  CHECK_EQ(GetEventMetricsTypeFor(type_), EventMetricsType::kEventMetrics);
  // `EventMetrics::CreateForTesting()` sets the dispatch timestamp of the
  // `EventMetrics::DispatchStage::kArrivedInRendererCompositor` stage to
  // `test_tick_clock->NowTicks()`. The statement below ensures that the
  // dispatch timestamp is valid and chronological
  // (`EventMetrics::DispatchStage::kGenerated` <
  // `EventMetrics::DispatchStage::kArrivedInBrowserMain` <
  // `EventMetrics::DispatchStage::kArrivedInRendererCompositor`).
  clock_->SetNowTicks(arrived_in_renderer_compositor_timestamp_.value_or(
      timestamp_ + base::Nanoseconds(2)));

  auto event = EventMetrics::CreateForTesting(
      type_, timestamp_,
      /* arrived_in_browser_main_timestamp= */ timestamp_ +
          base::Nanoseconds(1),
      &*clock_, /* trace_id= */ std::nullopt);
  if (event == nullptr) {
    return event;
  }
  if (caused_frame_update_.has_value()) {
    event->set_caused_frame_update(*caused_frame_update_);
  }
  return event;
}

EventMetricsTestCreator::EventBuilder
EventMetricsTestCreator::CreateEventBuilder(ui::EventType type) {
  return EventBuilder(test_tick_clock_, type);
}

EventMetricsTestCreator::ScrollEventBuilder::ScrollEventBuilder(
    base::SimpleTestTickClock& clock,
    ui::EventType type,
    bool is_inertial)
    : ScrollEventBuilderBase<ScrollEventBuilder>(clock, type, is_inertial) {}

EventMetricsTestCreator::ScrollEventBuilder::~ScrollEventBuilder() = default;

std::unique_ptr<ScrollEventMetrics>
EventMetricsTestCreator::ScrollEventBuilder::Build() {
  CHECK_EQ(GetEventMetricsTypeFor(type_),
           EventMetricsType::kScrollEventMetrics);
  // See `EventMetricsTestCreator::CreateEventMetrics()` for why we do this.
  clock_->SetNowTicks(arrived_in_renderer_compositor_timestamp_.value_or(
      timestamp_ + base::Nanoseconds(2)));
  auto event = ScrollEventMetrics::CreateForTesting(
      type_, ui::ScrollInputType::kTouchscreen, is_inertial_, timestamp_,
      /* arrived_in_browser_main_timestamp= */ timestamp_ +
          base::Nanoseconds(1),
      &*clock_,
      /* scroll_begin_arrival_timestamp= */ timestamp_ - base::Nanoseconds(1));
  if (caused_frame_update_.has_value()) {
    event->set_caused_frame_update(*caused_frame_update_);
  }
  if (dispatch_args_.has_value()) {
    event->set_dispatch_args(*dispatch_args_);
  }
  return event;
}

EventMetricsTestCreator::ScrollEventBuilder
EventMetricsTestCreator::GestureScrollBeginBuilder() {
  return ScrollEventBuilder(test_tick_clock_,
                            ui::EventType::kGestureScrollBegin,
                            /*is_inertial=*/false);
}

EventMetricsTestCreator::ScrollEventBuilder
EventMetricsTestCreator::GestureScrollEndBuilder() {
  return ScrollEventBuilder(test_tick_clock_, ui::EventType::kGestureScrollEnd,
                            /*is_inertial=*/false);
}

EventMetricsTestCreator::ScrollEventBuilder
EventMetricsTestCreator::InertialGestureScrollEndBuilder() {
  return ScrollEventBuilder(test_tick_clock_, ui::EventType::kGestureScrollEnd,
                            /*is_inertial=*/true);
}

EventMetricsTestCreator::ScrollUpdateEventBuilder::ScrollUpdateEventBuilder(
    base::SimpleTestTickClock& clock,
    ScrollUpdateEventMetrics::ScrollUpdateType scroll_update_type,
    bool is_inertial)
    : ScrollUpdateEventBuilderBase<ScrollUpdateEventBuilder>(clock,
                                                             scroll_update_type,
                                                             is_inertial) {}

EventMetricsTestCreator::ScrollUpdateEventBuilder::~ScrollUpdateEventBuilder() =
    default;

std::unique_ptr<ScrollUpdateEventMetrics>
EventMetricsTestCreator::ScrollUpdateEventBuilder::Build() {
  CHECK_EQ(GetEventMetricsTypeFor(ui::EventType::kGestureScrollUpdate),
           EventMetricsType::kScrollUpdateEventMetrics);
  // See `EventMetricsTestCreator::CreateEventMetrics()` for why we do this.
  clock_->SetNowTicks(arrived_in_renderer_compositor_timestamp_.value_or(
      timestamp_ + base::Nanoseconds(2)));
  auto event = ScrollUpdateEventMetrics::CreateForTesting(
      ui::EventType::kGestureScrollUpdate, ui::ScrollInputType::kTouchscreen,
      is_inertial_, scroll_update_type_, delta_, timestamp_,
      /* arrived_in_browser_main_timestamp= */ timestamp_ +
          base::Nanoseconds(1),
      &*clock_, trace_id_,
      /* scroll_begin_arrival_timestamp= */ timestamp_ - base::Nanoseconds(1));
  if (predicted_delta_.has_value()) {
    event->set_predicted_delta(*predicted_delta_);
  }
  if (caused_frame_update_.has_value()) {
    event->set_caused_frame_update(*caused_frame_update_);
  }
  if (did_scroll_.has_value()) {
    event->set_did_scroll(*did_scroll_);
  }
  if (is_synthetic_.has_value()) {
    event->set_is_synthetic(*is_synthetic_);
  }
  if (dispatch_args_.has_value()) {
    event->set_dispatch_args(*dispatch_args_);
  }
  return event;
}

EventMetricsTestCreator::ScrollUpdateEventBuilder
EventMetricsTestCreator::FirstGestureScrollUpdateBuilder() {
  return ScrollUpdateEventBuilder(
      test_tick_clock_, ScrollUpdateEventMetrics::ScrollUpdateType::kStarted,
      /*is_inertial=*/false);
}

EventMetricsTestCreator::ScrollUpdateEventBuilder
EventMetricsTestCreator::GestureScrollUpdateBuilder() {
  return ScrollUpdateEventBuilder(
      test_tick_clock_, ScrollUpdateEventMetrics::ScrollUpdateType::kContinued,
      /*is_inertial=*/false);
}

EventMetricsTestCreator::ScrollUpdateEventBuilder
EventMetricsTestCreator::InertialGestureScrollUpdateBuilder() {
  return ScrollUpdateEventBuilder(
      test_tick_clock_, ScrollUpdateEventMetrics::ScrollUpdateType::kContinued,
      /*is_inertial=*/true);
}

// Explicit template instantiations so that method implementations don't have to
// live in the header file.
template class EventMetricsTestCreator::EventBuilderBase<
    EventMetricsTestCreator::EventBuilder>;
template class EventMetricsTestCreator::EventBuilderBase<
    EventMetricsTestCreator::ScrollEventBuilder>;
template class EventMetricsTestCreator::EventBuilderBase<
    EventMetricsTestCreator::ScrollUpdateEventBuilder>;
template class EventMetricsTestCreator::ScrollEventBuilderBase<
    EventMetricsTestCreator::ScrollEventBuilder>;
template class EventMetricsTestCreator::ScrollEventBuilderBase<
    EventMetricsTestCreator::ScrollUpdateEventBuilder>;
template class EventMetricsTestCreator::ScrollUpdateEventBuilderBase<
    EventMetricsTestCreator::ScrollUpdateEventBuilder>;

}  // namespace cc
