// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/event_metrics.h"

#include <algorithm>
#include <ostream>
#include <utility>

#include "base/check.h"
#include "base/cxx17_backports.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/time/default_tick_clock.h"

namespace cc {
namespace {

constexpr struct {
  EventMetrics::EventType metrics_event_type;
  ui::EventType ui_event_type;
  const char* name;
  absl::optional<bool> scroll_is_inertial = absl::nullopt;
  absl::optional<EventMetrics::ScrollUpdateType> scroll_update_type =
      absl::nullopt;
} kInterestingEvents[] = {
#define EVENT_TYPE(name, ui_type, ...) \
  { EventMetrics::EventType::k##name, ui_type, #name, __VA_ARGS__ }
    EVENT_TYPE(MousePressed, ui::ET_MOUSE_PRESSED),
    EVENT_TYPE(MouseReleased, ui::ET_MOUSE_RELEASED),
    EVENT_TYPE(MouseWheel, ui::ET_MOUSEWHEEL),
    EVENT_TYPE(KeyPressed, ui::ET_KEY_PRESSED),
    EVENT_TYPE(KeyReleased, ui::ET_KEY_RELEASED),
    EVENT_TYPE(TouchPressed, ui::ET_TOUCH_PRESSED),
    EVENT_TYPE(TouchReleased, ui::ET_TOUCH_RELEASED),
    EVENT_TYPE(TouchMoved, ui::ET_TOUCH_MOVED),
    EVENT_TYPE(GestureScrollBegin, ui::ET_GESTURE_SCROLL_BEGIN, false),
    EVENT_TYPE(GestureScrollUpdate,
               ui::ET_GESTURE_SCROLL_UPDATE,
               false,
               EventMetrics::ScrollUpdateType::kContinued),
    EVENT_TYPE(GestureScrollEnd, ui::ET_GESTURE_SCROLL_END, false),
    EVENT_TYPE(GestureDoubleTap, ui::ET_GESTURE_DOUBLE_TAP),
    EVENT_TYPE(GestureLongPress, ui::ET_GESTURE_LONG_PRESS),
    EVENT_TYPE(GestureLongTap, ui::ET_GESTURE_LONG_TAP),
    EVENT_TYPE(GestureShowPress, ui::ET_GESTURE_SHOW_PRESS),
    EVENT_TYPE(GestureTap, ui::ET_GESTURE_TAP),
    EVENT_TYPE(GestureTapCancel, ui::ET_GESTURE_TAP_CANCEL),
    EVENT_TYPE(GestureTapDown, ui::ET_GESTURE_TAP_DOWN),
    EVENT_TYPE(GestureTapUnconfirmed, ui::ET_GESTURE_TAP_UNCONFIRMED),
    EVENT_TYPE(GestureTwoFingerTap, ui::ET_GESTURE_TWO_FINGER_TAP),
    EVENT_TYPE(FirstGestureScrollUpdate,
               ui::ET_GESTURE_SCROLL_UPDATE,
               false,
               EventMetrics::ScrollUpdateType::kStarted),
    EVENT_TYPE(MouseDragged, ui::ET_MOUSE_DRAGGED),
    EVENT_TYPE(GesturePinchBegin, ui::ET_GESTURE_PINCH_BEGIN),
    EVENT_TYPE(GesturePinchEnd, ui::ET_GESTURE_PINCH_END),
    EVENT_TYPE(GesturePinchUpdate, ui::ET_GESTURE_PINCH_UPDATE),
    EVENT_TYPE(InertialGestureScrollUpdate,
               ui::ET_GESTURE_SCROLL_UPDATE,
               true,
               EventMetrics::ScrollUpdateType::kContinued),
#undef EVENT_TYPE
};
static_assert(base::size(kInterestingEvents) ==
                  static_cast<int>(EventMetrics::EventType::kMaxValue) + 1,
              "EventMetrics::EventType has changed.");

constexpr struct {
  EventMetrics::ScrollType metrics_scroll_type;
  ui::ScrollInputType ui_input_type;
  const char* name;
} kScrollTypes[] = {
#define SCROLL_TYPE(name) \
  { EventMetrics::ScrollType::k##name, ui::ScrollInputType::k##name, #name }
    SCROLL_TYPE(Autoscroll),
    SCROLL_TYPE(Scrollbar),
    SCROLL_TYPE(Touchscreen),
    SCROLL_TYPE(Wheel),
#undef SCROLL_TYPE
};
static_assert(base::size(kScrollTypes) ==
                  static_cast<int>(EventMetrics::ScrollType::kMaxValue) + 1,
              "EventMetrics::ScrollType has changed.");

constexpr struct {
  EventMetrics::PinchType metrics_pinch_type;
  ui::ScrollInputType ui_input_type;
  const char* name;
} kPinchTypes[] = {
#define PINCH_TYPE(metrics_name, ui_name)                                      \
  {                                                                            \
    EventMetrics::PinchType::k##metrics_name, ui::ScrollInputType::k##ui_name, \
        #metrics_name                                                          \
  }
    PINCH_TYPE(Touchpad, Wheel),
    PINCH_TYPE(Touchscreen, Touchscreen),
#undef PINCH_TYPE
};
static_assert(base::size(kPinchTypes) ==
                  static_cast<int>(EventMetrics::PinchType::kMaxValue) + 1,
              "EventMetrics::PinchType has changed.");

absl::optional<EventMetrics::EventType> ToInterestingEventType(
    ui::EventType ui_event_type,
    const absl::optional<EventMetrics::GestureParams>& gesture_params) {
  absl::optional<bool> scroll_is_inertial;
  absl::optional<EventMetrics::ScrollUpdateType> scroll_update_type;
  if (gesture_params && gesture_params->scroll_params) {
    scroll_is_inertial = gesture_params->scroll_params->is_inertial;
    scroll_update_type = gesture_params->scroll_params->update_type;
  }

  for (size_t i = 0; i < base::size(kInterestingEvents); i++) {
    const auto& interesting_event = kInterestingEvents[i];
    if (ui_event_type == interesting_event.ui_event_type &&
        scroll_is_inertial == interesting_event.scroll_is_inertial &&
        scroll_update_type == interesting_event.scroll_update_type) {
      EventMetrics::EventType metrics_event_type =
          static_cast<EventMetrics::EventType>(i);
      DCHECK_EQ(metrics_event_type, interesting_event.metrics_event_type);
      return metrics_event_type;
    }
  }
  return absl::nullopt;
}

EventMetrics::ScrollType ToScrollType(ui::ScrollInputType ui_input_type) {
  for (size_t i = 0; i < base::size(kScrollTypes); i++) {
    if (ui_input_type == kScrollTypes[i].ui_input_type) {
      EventMetrics::ScrollType metrics_scroll_type =
          static_cast<EventMetrics::ScrollType>(i);
      DCHECK_EQ(metrics_scroll_type, kScrollTypes[i].metrics_scroll_type);
      return metrics_scroll_type;
    }
  }
  NOTREACHED();
  return EventMetrics::ScrollType::kMaxValue;
}

EventMetrics::PinchType ToPinchType(ui::ScrollInputType ui_input_type) {
  for (size_t i = 0; i < base::size(kPinchTypes); i++) {
    if (ui_input_type == kPinchTypes[i].ui_input_type) {
      EventMetrics::PinchType metrics_pinch_type =
          static_cast<EventMetrics::PinchType>(i);
      DCHECK_EQ(metrics_pinch_type, kPinchTypes[i].metrics_pinch_type);
      return metrics_pinch_type;
    }
  }
  NOTREACHED();
  return EventMetrics::PinchType::kMaxValue;
}

bool IsGestureScroll(ui::EventType type) {
  return type == ui::ET_GESTURE_SCROLL_BEGIN ||
         type == ui::ET_GESTURE_SCROLL_UPDATE ||
         type == ui::ET_GESTURE_SCROLL_END;
}

bool IsGesturePinch(ui::EventType type) {
  return type == ui::ET_GESTURE_PINCH_BEGIN ||
         type == ui::ET_GESTURE_PINCH_UPDATE ||
         type == ui::ET_GESTURE_PINCH_END;
}

bool IsGestureScrollUpdate(ui::EventType type) {
  return type == ui::ET_GESTURE_SCROLL_UPDATE;
}

}  // namespace

// EventMetrics::GestureParams::ScrollParams:

EventMetrics::GestureParams::ScrollParams::ScrollParams(bool is_inertial)
    : is_inertial(is_inertial) {}

EventMetrics::GestureParams::ScrollParams::ScrollParams(
    bool is_inertial,
    ScrollUpdateType update_type)
    : is_inertial(is_inertial), update_type(update_type) {}

EventMetrics::GestureParams::ScrollParams::ScrollParams(const ScrollParams&) =
    default;
EventMetrics::GestureParams::ScrollParams&
EventMetrics::GestureParams::ScrollParams::operator=(const ScrollParams&) =
    default;

// EventMetrics::GestureParams:
EventMetrics::GestureParams::GestureParams(ui::ScrollInputType input_type)
    : input_type(input_type) {}

EventMetrics::GestureParams::GestureParams(ui::ScrollInputType input_type,
                                           bool scroll_is_inertial)
    : input_type(input_type),
      scroll_params(absl::in_place, scroll_is_inertial) {}

EventMetrics::GestureParams::GestureParams(ui::ScrollInputType input_type,
                                           bool scroll_is_inertial,
                                           ScrollUpdateType scroll_update_type)
    : input_type(input_type),
      scroll_params(absl::in_place, scroll_is_inertial, scroll_update_type) {}

EventMetrics::GestureParams::GestureParams(const GestureParams&) = default;
EventMetrics::GestureParams& EventMetrics::GestureParams::operator=(
    const GestureParams&) = default;

// EventMetrics:

// static
std::unique_ptr<EventMetrics> EventMetrics::Create(
    ui::EventType type,
    absl::optional<GestureParams> gesture_params,
    base::TimeTicks timestamp) {
  // TODO(crbug.com/1157090): We expect that `timestamp` is not null, but there
  // seems to be some tests that are emitting events with null timestamp. We
  // should investigate and try to fix those cases and add a `DCHECK` here to
  // assert `timestamp` is not null.

  std::unique_ptr<EventMetrics> metrics = CreateInternal(
      type, gesture_params, timestamp, base::DefaultTickClock::GetInstance());
  if (!metrics)
    return nullptr;

  metrics->SetDispatchStageTimestamp(
      DispatchStage::kArrivedInRendererCompositor);
  return metrics;
}

// static
std::unique_ptr<EventMetrics> EventMetrics::CreateForTesting(
    ui::EventType type,
    absl::optional<GestureParams> gesture_params,
    base::TimeTicks timestamp,
    const base::TickClock* tick_clock) {
  DCHECK(!timestamp.is_null());

  std::unique_ptr<EventMetrics> metrics =
      CreateInternal(type, gesture_params, timestamp, tick_clock);
  if (!metrics)
    return nullptr;

  metrics->SetDispatchStageTimestamp(
      DispatchStage::kArrivedInRendererCompositor);
  return metrics;
}

// static
std::unique_ptr<EventMetrics> EventMetrics::CreateFromExisting(
    ui::EventType type,
    absl::optional<GestureParams> gesture_params,
    DispatchStage last_dispatch_stage,
    const EventMetrics* existing) {
  std::unique_ptr<EventMetrics> metrics =
      CreateInternal(type, gesture_params, base::TimeTicks(),
                     existing ? existing->tick_clock_.get()
                              : base::DefaultTickClock::GetInstance());
  if (!metrics)
    return nullptr;

  // Since the new event is of an interesting type, we expect the existing event
  // to be of an interesting type, too; which means `existing` should not be
  // nullptr. However, some tests that are not interested in reporting metrics,
  // don't create metrics objects even for events of interesting types. Return
  // nullptr if that's the case.
  if (!existing)
    return nullptr;

  // Use timestamps of all stages (including "Generated" stage) up to
  // `last_dispatch_stage` from `existing`.
  for (size_t stage_index = static_cast<size_t>(DispatchStage::kGenerated);
       stage_index <= static_cast<size_t>(last_dispatch_stage); stage_index++) {
    metrics->dispatch_stage_timestamps_[stage_index] =
        existing->dispatch_stage_timestamps_[stage_index];
  }
  return metrics;
}

// static
std::unique_ptr<EventMetrics> EventMetrics::CreateInternal(
    ui::EventType type,
    const absl::optional<GestureParams>& gesture_params,
    base::TimeTicks timestamp,
    const base::TickClock* tick_clock) {
  absl::optional<ScrollType> scroll_type;
  absl::optional<PinchType> pinch_type;
  if (IsGestureScroll(type)) {
    DCHECK(gesture_params);
    DCHECK(gesture_params->scroll_params);
    DCHECK(IsGestureScrollUpdate(type) &&
               gesture_params->scroll_params->update_type ||
           !IsGestureScrollUpdate(type) &&
               !gesture_params->scroll_params->update_type);
    scroll_type = ToScrollType(gesture_params->input_type);
  } else if (IsGesturePinch(type)) {
    DCHECK(gesture_params);
    DCHECK(!gesture_params->scroll_params);
    pinch_type = ToPinchType(gesture_params->input_type);
  } else {
    DCHECK(!gesture_params);
  }

  absl::optional<EventType> interesting_type =
      ToInterestingEventType(type, gesture_params);
  if (!interesting_type)
    return nullptr;
  return base::WrapUnique(new EventMetrics(*interesting_type, scroll_type,
                                           pinch_type, timestamp, tick_clock));
}

EventMetrics::EventMetrics(EventType type,
                           absl::optional<ScrollType> scroll_type,
                           absl::optional<PinchType> pinch_type,
                           base::TimeTicks timestamp,
                           const base::TickClock* tick_clock)
    : type_(type),
      scroll_type_(scroll_type),
      pinch_type_(pinch_type),
      tick_clock_(tick_clock) {
  dispatch_stage_timestamps_[static_cast<int>(DispatchStage::kGenerated)] =
      timestamp;
}

EventMetrics::~EventMetrics() = default;

const char* EventMetrics::GetTypeName() const {
  return kInterestingEvents[static_cast<int>(type_)].name;
}

const char* EventMetrics::GetScrollTypeName() const {
  DCHECK(scroll_type_) << "Event is not a scroll event.";

  return kScrollTypes[static_cast<int>(*scroll_type_)].name;
}

const char* EventMetrics::GetPinchTypeName() const {
  DCHECK(pinch_type_) << "Event is not a pinch event.";

  return kPinchTypes[static_cast<int>(*pinch_type_)].name;
}

void EventMetrics::SetDispatchStageTimestamp(DispatchStage stage) {
  DCHECK(dispatch_stage_timestamps_[static_cast<size_t>(stage)].is_null());

  dispatch_stage_timestamps_[static_cast<size_t>(stage)] =
      tick_clock_->NowTicks();
}

base::TimeTicks EventMetrics::GetDispatchStageTimestamp(
    DispatchStage stage) const {
  return dispatch_stage_timestamps_[static_cast<size_t>(stage)];
}

void EventMetrics::ResetToDispatchStage(DispatchStage stage) {
  for (size_t stage_index = static_cast<size_t>(stage) + 1;
       stage_index <= static_cast<size_t>(DispatchStage::kMaxValue);
       stage_index++) {
    dispatch_stage_timestamps_[stage_index] = base::TimeTicks();
  }
}

bool EventMetrics::HasSmoothInputEvent() const {
  return type_ == EventType::kMouseDragged || type_ == EventType::kTouchMoved;
}

std::unique_ptr<EventMetrics> EventMetrics::Clone() const {
  auto clone = base::WrapUnique(new EventMetrics(
      type_, scroll_type_, pinch_type_, base::TimeTicks(), tick_clock_));
  std::copy(std::begin(dispatch_stage_timestamps_),
            std::end(dispatch_stage_timestamps_),
            std::begin(clone->dispatch_stage_timestamps_));
  return clone;
}

bool EventMetrics::operator==(const EventMetrics& other) const {
  return type_ == other.type_ && scroll_type_ == other.scroll_type_ &&
         pinch_type_ == other.pinch_type_ &&
         std::equal(std::begin(dispatch_stage_timestamps_),
                    std::end(dispatch_stage_timestamps_),
                    std::begin(other.dispatch_stage_timestamps_));
}

// EventMetricsSet
EventMetricsSet::EventMetricsSet() = default;
EventMetricsSet::~EventMetricsSet() = default;
EventMetricsSet::EventMetricsSet(EventMetrics::List main_thread_event_metrics,
                                 EventMetrics::List impl_thread_event_metrics)
    : main_event_metrics(std::move(main_thread_event_metrics)),
      impl_event_metrics(std::move(impl_thread_event_metrics)) {}
EventMetricsSet::EventMetricsSet(EventMetricsSet&& other) = default;
EventMetricsSet& EventMetricsSet::operator=(EventMetricsSet&& other) = default;

}  // namespace cc
