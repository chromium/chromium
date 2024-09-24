// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/metrics/event_metrics.h"

#include <algorithm>
#include <ostream>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/time/default_tick_clock.h"
#include "cc/metrics/event_latency_tracing_recorder.h"

namespace cc {
namespace {

// Histogram bucketing for scroll event metrics.
constexpr base::TimeDelta kScrollHistogramMin = base::Milliseconds(4);
constexpr base::TimeDelta kScrollHistogramMax = base::Milliseconds(500);
constexpr size_t kScrollHistogramBucketCount = 50;

constexpr struct {
  EventMetrics::EventType metrics_event_type;
  const char* name;

  ui::EventType ui_event_type;
  std::optional<bool> scroll_is_inertial = std::nullopt;
  std::optional<ScrollUpdateEventMetrics::ScrollUpdateType> scroll_update_type =
      std::nullopt;

  std::optional<EventMetrics::HistogramBucketing> histogram_bucketing =
      std::nullopt;
} kInterestingEvents[] = {
#define EVENT_TYPE(type_name, ui_type, ...)                      \
  {                                                              \
    .metrics_event_type = EventMetrics::EventType::k##type_name, \
    .name = #type_name, .ui_event_type = ui_type, __VA_ARGS__    \
  }
    EVENT_TYPE(MousePressed, ui::EventType::kMousePressed),
    EVENT_TYPE(MouseReleased, ui::EventType::kMouseReleased),
    EVENT_TYPE(MouseWheel, ui::EventType::kMousewheel),
    EVENT_TYPE(KeyPressed, ui::EventType::kKeyPressed),
    EVENT_TYPE(KeyReleased, ui::EventType::kKeyReleased),
    EVENT_TYPE(TouchPressed, ui::EventType::kTouchPressed),
    EVENT_TYPE(TouchReleased, ui::EventType::kTouchReleased),
    EVENT_TYPE(TouchMoved, ui::EventType::kTouchMoved),
    EVENT_TYPE(GestureScrollBegin,
               ui::EventType::kGestureScrollBegin,
               .scroll_is_inertial = false,
               .histogram_bucketing = {{.min = kScrollHistogramMin,
                                        .max = kScrollHistogramMax,
                                        .count = kScrollHistogramBucketCount,
                                        .version_suffix = "2"}}),
    EVENT_TYPE(GestureScrollUpdate,
               ui::EventType::kGestureScrollUpdate,
               .scroll_is_inertial = false,
               .scroll_update_type =
                   ScrollUpdateEventMetrics::ScrollUpdateType::kContinued,
               .histogram_bucketing = {{.min = kScrollHistogramMin,
                                        .max = kScrollHistogramMax,
                                        .count = kScrollHistogramBucketCount,
                                        .version_suffix = "2"}}),
    EVENT_TYPE(GestureScrollEnd,
               ui::EventType::kGestureScrollEnd,
               .scroll_is_inertial = false,
               .histogram_bucketing = {{.min = kScrollHistogramMin,
                                        .max = kScrollHistogramMax,
                                        .count = kScrollHistogramBucketCount,
                                        .version_suffix = "2"}}),
    EVENT_TYPE(GestureDoubleTap, ui::EventType::kGestureDoubleTap),
    EVENT_TYPE(GestureLongPress, ui::EventType::kGestureLongPress),
    EVENT_TYPE(GestureLongTap, ui::EventType::kGestureLongTap),
    EVENT_TYPE(GestureShowPress, ui::EventType::kGestureShowPress),
    EVENT_TYPE(GestureTap, ui::EventType::kGestureTap),
    EVENT_TYPE(GestureTapCancel, ui::EventType::kGestureTapCancel),
    EVENT_TYPE(GestureTapDown, ui::EventType::kGestureTapDown),
    EVENT_TYPE(GestureTapUnconfirmed, ui::EventType::kGestureTapUnconfirmed),
    EVENT_TYPE(GestureTwoFingerTap, ui::EventType::kGestureTwoFingerTap),
    EVENT_TYPE(FirstGestureScrollUpdate,
               ui::EventType::kGestureScrollUpdate,
               .scroll_is_inertial = false,
               .scroll_update_type =
                   ScrollUpdateEventMetrics::ScrollUpdateType::kStarted,
               .histogram_bucketing = {{.min = kScrollHistogramMin,
                                        .max = kScrollHistogramMax,
                                        .count = kScrollHistogramBucketCount,
                                        .version_suffix = "2"}}),
    EVENT_TYPE(MouseDragged, ui::EventType::kMouseDragged),
    EVENT_TYPE(GesturePinchBegin, ui::EventType::kGesturePinchBegin),
    EVENT_TYPE(GesturePinchEnd, ui::EventType::kGesturePinchEnd),
    EVENT_TYPE(GesturePinchUpdate, ui::EventType::kGesturePinchUpdate),
    EVENT_TYPE(InertialGestureScrollUpdate,
               ui::EventType::kGestureScrollUpdate,
               .scroll_is_inertial = true,
               .scroll_update_type =
                   ScrollUpdateEventMetrics::ScrollUpdateType::kContinued,
               .histogram_bucketing = {{.min = kScrollHistogramMin,
                                        .max = kScrollHistogramMax,
                                        .count = kScrollHistogramBucketCount,
                                        .version_suffix = "2"}}),
    EVENT_TYPE(MouseMoved, ui::EventType::kMouseMoved),
#undef EVENT_TYPE
};
static_assert(std::size(kInterestingEvents) ==
                  static_cast<int>(EventMetrics::EventType::kMaxValue) + 1,
              "EventMetrics::EventType has changed.");

constexpr struct {
  ScrollEventMetrics::ScrollType metrics_scroll_type;
  ui::ScrollInputType ui_input_type;
  const char* name;
} kScrollTypes[] = {
#define SCROLL_TYPE(name)                                                  \
  {                                                                        \
    ScrollEventMetrics::ScrollType::k##name, ui::ScrollInputType::k##name, \
        #name                                                              \
  }
    SCROLL_TYPE(Autoscroll),
    SCROLL_TYPE(Scrollbar),
    SCROLL_TYPE(Touchscreen),
    SCROLL_TYPE(Wheel),
#undef SCROLL_TYPE
};
static_assert(std::size(kScrollTypes) ==
                  static_cast<int>(ScrollEventMetrics::ScrollType::kMaxValue) +
                      1,
              "ScrollEventMetrics::ScrollType has changed.");

constexpr struct {
  PinchEventMetrics::PinchType metrics_pinch_type;
  ui::ScrollInputType ui_input_type;
  const char* name;
} kPinchTypes[] = {
#define PINCH_TYPE(metrics_name, ui_name)              \
  {                                                    \
    PinchEventMetrics::PinchType::k##metrics_name,     \
        ui::ScrollInputType::k##ui_name, #metrics_name \
  }
    PINCH_TYPE(Touchpad, Wheel),
    PINCH_TYPE(Touchscreen, Touchscreen),
#undef PINCH_TYPE
};
static_assert(std::size(kPinchTypes) ==
                  static_cast<int>(PinchEventMetrics::PinchType::kMaxValue) + 1,
              "PinchEventMetrics::PinchType has changed.");

std::optional<EventMetrics::EventType> ToInterestingEventType(
    ui::EventType ui_event_type,
    std::optional<bool> scroll_is_inertial,
    std::optional<ScrollUpdateEventMetrics::ScrollUpdateType>
        scroll_update_type) {
  for (size_t i = 0; i < std::size(kInterestingEvents); i++) {
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
  return std::nullopt;
}

ScrollEventMetrics::ScrollType ToScrollType(ui::ScrollInputType ui_input_type) {
  for (size_t i = 0; i < std::size(kScrollTypes); i++) {
    if (ui_input_type == kScrollTypes[i].ui_input_type) {
      auto metrics_scroll_type = static_cast<ScrollEventMetrics::ScrollType>(i);
      DCHECK_EQ(metrics_scroll_type, kScrollTypes[i].metrics_scroll_type);
      return metrics_scroll_type;
    }
  }
  NOTREACHED();
}

PinchEventMetrics::PinchType ToPinchType(ui::ScrollInputType ui_input_type) {
  for (size_t i = 0; i < std::size(kPinchTypes); i++) {
    if (ui_input_type == kPinchTypes[i].ui_input_type) {
      auto metrics_pinch_type = static_cast<PinchEventMetrics::PinchType>(i);
      DCHECK_EQ(metrics_pinch_type, kPinchTypes[i].metrics_pinch_type);
      return metrics_pinch_type;
    }
  }
  NOTREACHED();
}

bool IsGestureScroll(ui::EventType type) {
  return type == ui::EventType::kGestureScrollBegin ||
         type == ui::EventType::kGestureScrollUpdate ||
         type == ui::EventType::kGestureScrollEnd;
}

bool IsGesturePinch(ui::EventType type) {
  return type == ui::EventType::kGesturePinchBegin ||
         type == ui::EventType::kGesturePinchUpdate ||
         type == ui::EventType::kGesturePinchEnd;
}

bool IsGestureScrollUpdate(ui::EventType type) {
  return type == ui::EventType::kGestureScrollUpdate;
}

}  // namespace

// EventMetrics:

// static
std::unique_ptr<EventMetrics> EventMetrics::Create(
    ui::EventType type,
    base::TimeTicks timestamp,
    std::optional<TraceId> trace_id) {
  return Create(type, timestamp, base::TimeTicks(), trace_id);
}

// static
std::unique_ptr<EventMetrics> EventMetrics::Create(
    ui::EventType type,
    base::TimeTicks timestamp,
    base::TimeTicks arrived_in_browser_main_timestamp,
    std::optional<TraceId> trace_id) {
  // TODO(crbug.com/40160689): We expect that `timestamp` is not null, but there
  // seems to be some tests that are emitting events with null timestamp. We
  // should investigate and try to fix those cases and add a `DCHECK` here to
  // assert `timestamp` is not null.

  DCHECK(!IsGestureScroll(type) && !IsGesturePinch(type));

  std::unique_ptr<EventMetrics> metrics =
      CreateInternal(type, timestamp, arrived_in_browser_main_timestamp,
                     base::DefaultTickClock::GetInstance(), trace_id);
  if (!metrics)
    return nullptr;

  metrics->SetDispatchStageTimestamp(
      DispatchStage::kArrivedInRendererCompositor);
  return metrics;
}

// static
std::unique_ptr<EventMetrics> EventMetrics::CreateForTesting(
    ui::EventType type,
    base::TimeTicks timestamp,
    base::TimeTicks arrived_in_browser_main_timestamp,
    const base::TickClock* tick_clock,
    std::optional<TraceId> trace_id) {
  DCHECK(!timestamp.is_null());

  std::unique_ptr<EventMetrics> metrics =
      CreateInternal(type, timestamp, base::TimeTicks(), tick_clock, trace_id);
  if (!metrics)
    return nullptr;

  metrics->SetDispatchStageTimestamp(
      DispatchStage::kArrivedInRendererCompositor);
  return metrics;
}

// static
std::unique_ptr<EventMetrics> EventMetrics::CreateFromExisting(
    ui::EventType type,
    DispatchStage last_dispatch_stage,
    const EventMetrics* existing) {
  // Generally, if `existing` is `nullptr` (the existing event is not of an
  // interesting type), the new event won't be of an interesting type, too, and
  // we can immediately return `nullptr`. The only exception is some tests that
  // are not interested in reporting metrics, in which case we can immediately
  // return `nullptr`, too, as they are not interested in reporting metrics.
  if (!existing)
    return nullptr;

  std::unique_ptr<EventMetrics> metrics =
      CreateInternal(type, base::TimeTicks(), base::TimeTicks(),
                     existing->tick_clock_, std::nullopt);
  if (!metrics)
    return nullptr;

  // Use timestamps of all stages (including "Generated" stage) up to
  // `last_dispatch_stage` from `existing`.
  metrics->CopyTimestampsFrom(*existing, last_dispatch_stage);

  return metrics;
}

// static
std::unique_ptr<EventMetrics> EventMetrics::CreateInternal(
    ui::EventType type,
    base::TimeTicks timestamp,
    base::TimeTicks arrived_in_browser_main_timestamp,
    const base::TickClock* tick_clock,
    std::optional<TraceId> trace_id) {
  std::optional<EventType> interesting_type =
      ToInterestingEventType(type, /*scroll_is_inertial=*/std::nullopt,
                             /*scroll_update_type=*/std::nullopt);
  if (!interesting_type)
    return nullptr;
  return base::WrapUnique(new EventMetrics(*interesting_type, timestamp,
                                           arrived_in_browser_main_timestamp,
                                           tick_clock, trace_id));
}

EventMetrics::EventMetrics(EventType type,
                           base::TimeTicks timestamp,
                           const base::TickClock* tick_clock,
                           std::optional<TraceId> trace_id)
    : type_(type), tick_clock_(tick_clock), trace_id_(trace_id) {
  dispatch_stage_timestamps_[static_cast<int>(DispatchStage::kGenerated)] =
      timestamp;
}

EventMetrics::EventMetrics(EventType type,
                           base::TimeTicks timestamp,
                           base::TimeTicks arrived_in_browser_main_timestamp,
                           const base::TickClock* tick_clock,
                           std::optional<TraceId> trace_id)
    : EventMetrics(type, timestamp, tick_clock, trace_id) {
  dispatch_stage_timestamps_[static_cast<int>(
      DispatchStage::kArrivedInBrowserMain)] =
      arrived_in_browser_main_timestamp;
}

EventMetrics::EventMetrics(const EventMetrics& other)
    : type_(other.type_),
      tick_clock_(other.tick_clock_),
      should_record_tracing_(false) {
  CopyTimestampsFrom(other, DispatchStage::kMaxValue);
}

EventMetrics::~EventMetrics() {
  if (should_record_tracing()) {
    EventLatencyTracingRecorder::RecordEventLatencyTraceEvent(
        this, base::TimeTicks::Now(), base::TimeDelta(), nullptr, nullptr);
  }
}

const char* EventMetrics::GetTypeName() const {
  return GetTypeName(type_);
}

// static
const char* EventMetrics::GetTypeName(EventMetrics::EventType type) {
  return kInterestingEvents[static_cast<int>(type)].name;
}

const std::optional<EventMetrics::HistogramBucketing>&
EventMetrics::GetHistogramBucketing() const {
  return kInterestingEvents[static_cast<int>(type_)].histogram_bucketing;
}

void EventMetrics::SetHighLatencyStage(const std::string& stage) {
  high_latency_stages_.push_back(stage);
}

void EventMetrics::SetDispatchStageTimestamp(DispatchStage stage) {
  DCHECK(dispatch_stage_timestamps_[static_cast<size_t>(stage)].is_null());

  dispatch_stage_timestamps_[static_cast<size_t>(stage)] =
      tick_clock_->NowTicks();
}

void EventMetrics::SetDispatchStageTimestamp(DispatchStage stage,
                                             base::TimeTicks timestamp) {
  DCHECK(dispatch_stage_timestamps_[static_cast<size_t>(stage)].is_null());

  dispatch_stage_timestamps_[static_cast<size_t>(stage)] = timestamp;
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

ScrollEventMetrics* EventMetrics::AsScroll() {
  return nullptr;
}

const ScrollEventMetrics* EventMetrics::AsScroll() const {
  return const_cast<EventMetrics*>(this)->AsScroll();
}

ScrollUpdateEventMetrics* EventMetrics::AsScrollUpdate() {
  return nullptr;
}

const ScrollUpdateEventMetrics* EventMetrics::AsScrollUpdate() const {
  return const_cast<EventMetrics*>(this)->AsScrollUpdate();
}

PinchEventMetrics* EventMetrics::AsPinch() {
  return nullptr;
}

const PinchEventMetrics* EventMetrics::AsPinch() const {
  return const_cast<EventMetrics*>(this)->AsPinch();
}

std::unique_ptr<EventMetrics> EventMetrics::Clone() const {
  return base::WrapUnique(new EventMetrics(*this));
}

void EventMetrics::CopyTimestampsFrom(const EventMetrics& other,
                                      DispatchStage last_dispatch_stage) {
  DCHECK_LE(last_dispatch_stage, DispatchStage::kMaxValue);
  std::copy(other.dispatch_stage_timestamps_,
            other.dispatch_stage_timestamps_ +
                static_cast<size_t>(last_dispatch_stage) + 1,
            dispatch_stage_timestamps_);
}

// ScrollEventMetrics

// static
std::unique_ptr<ScrollEventMetrics> ScrollEventMetrics::Create(
    ui::EventType type,
    ui::ScrollInputType input_type,
    bool is_inertial,
    base::TimeTicks timestamp,
    base::TimeTicks arrived_in_browser_main_timestamp,
    base::TimeTicks blocking_touch_dispatched_to_renderer,
    std::optional<TraceId> trace_id) {
  // TODO(crbug.com/40160689): We expect that `timestamp` is not null, but there
  // seems to be some tests that are emitting events with null timestamp.  We
  // should investigate and try to fix those cases and add a `DCHECK` here to
  // assert `timestamp` is not null.

  DCHECK(IsGestureScroll(type) && !IsGestureScrollUpdate(type));

  std::unique_ptr<ScrollEventMetrics> metrics =
      CreateInternal(type, input_type, is_inertial, timestamp,
                     arrived_in_browser_main_timestamp,
                     base::DefaultTickClock::GetInstance(), trace_id);
  if (!metrics)
    return nullptr;

  metrics->SetDispatchStageTimestamp(
      DispatchStage::kArrivedInRendererCompositor);
  metrics->SetDispatchStageTimestamp(
      DispatchStage::kScrollsBlockingTouchDispatchedToRenderer,
      blocking_touch_dispatched_to_renderer);
  return metrics;
}

// static
std::unique_ptr<ScrollEventMetrics> ScrollEventMetrics::CreateForBrowser(
    ui::EventType type,
    ui::ScrollInputType input_type,
    bool is_inertial,
    base::TimeTicks timestamp,
    std::optional<TraceId> trace_id) {
  return Create(type, input_type, is_inertial, timestamp,
                /*arrived_in_browser_main_timestamp=*/base::TimeTicks(),
                /*blocking_touch_dispatched_to_renderer=*/base::TimeTicks(),
                trace_id);
}

// static
std::unique_ptr<ScrollEventMetrics> ScrollEventMetrics::CreateForTesting(
    ui::EventType type,
    ui::ScrollInputType input_type,
    bool is_inertial,
    base::TimeTicks timestamp,
    base::TimeTicks arrived_in_browser_main_timestamp,
    const base::TickClock* tick_clock) {
  DCHECK(!timestamp.is_null());

  std::unique_ptr<ScrollEventMetrics> metrics = CreateInternal(
      type, input_type, is_inertial, timestamp,
      arrived_in_browser_main_timestamp, tick_clock, std::nullopt);
  if (!metrics)
    return nullptr;

  metrics->SetDispatchStageTimestamp(
      DispatchStage::kArrivedInRendererCompositor);
  return metrics;
}

// static
std::unique_ptr<ScrollEventMetrics> ScrollEventMetrics::CreateFromExisting(
    ui::EventType type,
    ui::ScrollInputType input_type,
    bool is_inertial,
    DispatchStage last_dispatch_stage,
    const EventMetrics* existing) {
  // Generally, if `existing` is `nullptr` (the existing event is not of an
  // interesting type), the new event won't be of an interesting type, too, and
  // we can immediately return `nullptr`. The only exception is some tests that
  // are not interested in reporting metrics, in which case we can immediately
  // return `nullptr`, too, as they are not interested in reporting metrics.
  if (!existing)
    return nullptr;

  std::unique_ptr<ScrollEventMetrics> metrics =
      CreateInternal(type, input_type, is_inertial, base::TimeTicks(),
                     base::TimeTicks(), existing->tick_clock_, std::nullopt);
  if (!metrics)
    return nullptr;

  // Use timestamps of all stages (including "Generated" stage) up to
  // `last_dispatch_stage` from `existing`.
  metrics->CopyTimestampsFrom(*existing, last_dispatch_stage);

  return metrics;
}

// static
std::unique_ptr<ScrollEventMetrics> ScrollEventMetrics::CreateInternal(
    ui::EventType type,
    ui::ScrollInputType input_type,
    bool is_inertial,
    base::TimeTicks timestamp,
    base::TimeTicks arrived_in_browser_main_timestamp,
    const base::TickClock* tick_clock,
    std::optional<TraceId> trace_id) {
  std::optional<EventType> interesting_type =
      ToInterestingEventType(type, is_inertial,
                             /*scroll_update_type=*/std::nullopt);
  if (!interesting_type)
    return nullptr;
  return base::WrapUnique(new ScrollEventMetrics(
      *interesting_type, ToScrollType(input_type), timestamp,
      arrived_in_browser_main_timestamp, tick_clock, trace_id));
}

ScrollEventMetrics::ScrollEventMetrics(
    EventType type,
    ScrollType scroll_type,
    base::TimeTicks timestamp,
    base::TimeTicks arrived_in_browser_main_timestamp,
    const base::TickClock* tick_clock,
    std::optional<TraceId> trace_id)
    : EventMetrics(type,
                   timestamp,
                   arrived_in_browser_main_timestamp,
                   tick_clock,
                   trace_id),
      scroll_type_(scroll_type) {}

ScrollEventMetrics::ScrollEventMetrics(const ScrollEventMetrics&) = default;

ScrollEventMetrics::~ScrollEventMetrics() {
  if (should_record_tracing()) {
    EventLatencyTracingRecorder::RecordEventLatencyTraceEvent(
        this, base::TimeTicks::Now(), base::TimeDelta(), nullptr, nullptr);
  }
}

const char* ScrollEventMetrics::GetScrollTypeName() const {
  return kScrollTypes[static_cast<int>(scroll_type_)].name;
}

ScrollEventMetrics* ScrollEventMetrics::AsScroll() {
  return this;
}

std::unique_ptr<EventMetrics> ScrollEventMetrics::Clone() const {
  return base::WrapUnique(new ScrollEventMetrics(*this));
}

// ScrollUpdateEventMetrics

// static
std::unique_ptr<ScrollUpdateEventMetrics> ScrollUpdateEventMetrics::Create(
    ui::EventType type,
    ui::ScrollInputType input_type,
    bool is_inertial,
    ScrollUpdateType scroll_update_type,
    float delta,
    base::TimeTicks timestamp,
    base::TimeTicks arrived_in_browser_main_timestamp,
    base::TimeTicks blocking_touch_dispatched_to_renderer,
    std::optional<TraceId> trace_id) {
  // TODO(crbug.com/40160689): We expect that `timestamp` is not null, but there
  // seems to be some tests that are emitting events with null timestamp. We
  // should investigate and try to fix those cases and add a `DCHECK` here to
  // assert `timestamp` is not null.

  DCHECK(IsGestureScrollUpdate(type));

  std::unique_ptr<ScrollUpdateEventMetrics> metrics =
      CreateInternal(type, input_type, is_inertial, scroll_update_type, delta,
                     timestamp, arrived_in_browser_main_timestamp,
                     base::DefaultTickClock::GetInstance(), trace_id);
  if (!metrics)
    return nullptr;

  metrics->SetDispatchStageTimestamp(
      DispatchStage::kArrivedInRendererCompositor);
  metrics->SetDispatchStageTimestamp(
      DispatchStage::kScrollsBlockingTouchDispatchedToRenderer,
      blocking_touch_dispatched_to_renderer);
  return metrics;
}

// static
std::unique_ptr<ScrollUpdateEventMetrics>
ScrollUpdateEventMetrics::CreateForBrowser(ui::EventType type,
                                           ui::ScrollInputType input_type,
                                           bool is_inertial,
                                           ScrollUpdateType scroll_update_type,
                                           float delta,
                                           base::TimeTicks timestamp,
                                           TraceId trace_id) {
  return Create(
      type, input_type, is_inertial, scroll_update_type, delta, timestamp,
      /*arrived_in_browser_main_timestamp=*/base::TimeTicks(),
      /*blocking_touch_dispatched_to_renderer=*/base::TimeTicks(), trace_id);
}

// static
std::unique_ptr<ScrollUpdateEventMetrics>
ScrollUpdateEventMetrics::CreateForTesting(
    ui::EventType type,
    ui::ScrollInputType input_type,
    bool is_inertial,
    ScrollUpdateType scroll_update_type,
    float delta,
    base::TimeTicks timestamp,
    base::TimeTicks arrived_in_browser_main_timestamp,
    const base::TickClock* tick_clock,
    std::optional<TraceId> trace_id) {
  DCHECK(!timestamp.is_null());

  std::unique_ptr<ScrollUpdateEventMetrics> metrics = CreateInternal(
      type, input_type, is_inertial, scroll_update_type, delta, timestamp,
      arrived_in_browser_main_timestamp, tick_clock, trace_id);
  if (!metrics)
    return nullptr;

  metrics->SetDispatchStageTimestamp(
      DispatchStage::kArrivedInRendererCompositor);
  return metrics;
}

// static
std::unique_ptr<ScrollUpdateEventMetrics>
ScrollUpdateEventMetrics::CreateFromExisting(
    ui::EventType type,
    ui::ScrollInputType input_type,
    bool is_inertial,
    ScrollUpdateType scroll_update_type,
    float delta,
    DispatchStage last_dispatch_stage,
    const EventMetrics* existing) {
  // Since the new event is of an interesting type, we expect the existing event
  // to be of an interesting type, too; which means `existing` should not be
  // `nullptr`. However, some tests that are not interested in reporting
  // metrics, don't create metrics objects even for events of interesting types.
  // Return `nullptr` if that's the case.
  if (!existing)
    return nullptr;

  base::TimeTicks generation_ts =
      existing->GetDispatchStageTimestamp(DispatchStage::kGenerated);
  std::unique_ptr<ScrollUpdateEventMetrics> metrics = CreateInternal(
      type, input_type, is_inertial, scroll_update_type, delta, generation_ts,
      base::TimeTicks(), existing->tick_clock_, std::nullopt);
  if (!metrics)
    return nullptr;

  // Use timestamps of all stages (including "Generated" stage) up to
  // `last_dispatch_stage` from `existing`.
  metrics->CopyTimestampsFrom(*existing, last_dispatch_stage);

  return metrics;
}

// static
std::unique_ptr<ScrollUpdateEventMetrics>
ScrollUpdateEventMetrics::CreateInternal(
    ui::EventType type,
    ui::ScrollInputType input_type,
    bool is_inertial,
    ScrollUpdateType scroll_update_type,
    float delta,
    base::TimeTicks timestamp,
    base::TimeTicks arrived_in_browser_main_timestamp,
    const base::TickClock* tick_clock,
    std::optional<TraceId> trace_id) {
  std::optional<EventType> interesting_type =
      ToInterestingEventType(type, is_inertial, scroll_update_type);
  if (!interesting_type)
    return nullptr;
  return base::WrapUnique(new ScrollUpdateEventMetrics(
      *interesting_type, ToScrollType(input_type), scroll_update_type, delta,
      timestamp, arrived_in_browser_main_timestamp, tick_clock, trace_id));
}

ScrollUpdateEventMetrics::ScrollUpdateEventMetrics(
    EventType type,
    ScrollType scroll_type,
    ScrollUpdateType scroll_update_type,
    float delta,
    base::TimeTicks timestamp,
    base::TimeTicks arrived_in_browser_main_timestamp,
    const base::TickClock* tick_clock,
    std::optional<TraceId> trace_id)
    : ScrollEventMetrics(type,
                         scroll_type,
                         timestamp,
                         arrived_in_browser_main_timestamp,
                         tick_clock,
                         trace_id),
      delta_(delta),
      predicted_delta_(delta),
      last_timestamp_(timestamp) {}

ScrollUpdateEventMetrics::ScrollUpdateEventMetrics(
    const ScrollUpdateEventMetrics&) = default;

ScrollUpdateEventMetrics::~ScrollUpdateEventMetrics() {
  if (should_record_tracing()) {
    EventLatencyTracingRecorder::RecordEventLatencyTraceEvent(
        this, base::TimeTicks::Now(), base::TimeDelta(), nullptr, nullptr);
  }
}

void ScrollUpdateEventMetrics::CoalesceWith(
    const ScrollUpdateEventMetrics& newer_scroll_update) {
  last_timestamp_ = newer_scroll_update.last_timestamp_;
  delta_ += newer_scroll_update.delta_;
  predicted_delta_ += newer_scroll_update.predicted_delta_;
  coalesced_event_count_ += newer_scroll_update.coalesced_event_count_;
}

ScrollUpdateEventMetrics* ScrollUpdateEventMetrics::AsScrollUpdate() {
  return this;
}

std::unique_ptr<EventMetrics> ScrollUpdateEventMetrics::Clone() const {
  return base::WrapUnique(new ScrollUpdateEventMetrics(*this));
}

// PinchEventMetrics

// static
std::unique_ptr<PinchEventMetrics> PinchEventMetrics::Create(
    ui::EventType type,
    ui::ScrollInputType input_type,
    base::TimeTicks timestamp,
    TraceId trace_id) {
  // TODO(crbug.com/40160689): We expect that `timestamp` is not null, but there
  // seems to be some tests that are emitting events with null timestamp.  We
  // should investigate and try to fix those cases and add a `DCHECK` here to
  // assert `timestamp` is not null.

  DCHECK(IsGesturePinch(type));

  std::unique_ptr<PinchEventMetrics> metrics =
      CreateInternal(type, input_type, timestamp,
                     base::DefaultTickClock::GetInstance(), trace_id);
  if (!metrics)
    return nullptr;

  metrics->SetDispatchStageTimestamp(
      DispatchStage::kArrivedInRendererCompositor);
  return metrics;
}

// static
std::unique_ptr<PinchEventMetrics> PinchEventMetrics::CreateForTesting(
    ui::EventType type,
    ui::ScrollInputType input_type,
    base::TimeTicks timestamp,
    const base::TickClock* tick_clock) {
  DCHECK(!timestamp.is_null());

  std::unique_ptr<PinchEventMetrics> metrics =
      CreateInternal(type, input_type, timestamp, tick_clock, std::nullopt);
  if (!metrics)
    return nullptr;

  metrics->SetDispatchStageTimestamp(
      DispatchStage::kArrivedInRendererCompositor);
  return metrics;
}

// static
std::unique_ptr<PinchEventMetrics> PinchEventMetrics::CreateInternal(
    ui::EventType type,
    ui::ScrollInputType input_type,
    base::TimeTicks timestamp,
    const base::TickClock* tick_clock,
    std::optional<TraceId> trace_id) {
  std::optional<EventType> interesting_type =
      ToInterestingEventType(type, /*scroll_is_inertial=*/std::nullopt,
                             /*scroll_update_type=*/std::nullopt);
  if (!interesting_type)
    return nullptr;
  return base::WrapUnique(
      new PinchEventMetrics(*interesting_type, ToPinchType(input_type),
                            timestamp, tick_clock, trace_id));
}

PinchEventMetrics::PinchEventMetrics(EventType type,
                                     PinchType pinch_type,
                                     base::TimeTicks timestamp,
                                     const base::TickClock* tick_clock,
                                     std::optional<TraceId> trace_id)
    : EventMetrics(type, timestamp, tick_clock, trace_id),
      pinch_type_(pinch_type) {}

PinchEventMetrics::PinchEventMetrics(const PinchEventMetrics&) = default;

PinchEventMetrics::~PinchEventMetrics() {
  if (should_record_tracing()) {
    EventLatencyTracingRecorder::RecordEventLatencyTraceEvent(
        this, base::TimeTicks::Now(), base::TimeDelta(), nullptr, nullptr);
  }
}

const char* PinchEventMetrics::GetPinchTypeName() const {
  return kPinchTypes[static_cast<int>(pinch_type_)].name;
}

PinchEventMetrics* PinchEventMetrics::AsPinch() {
  return this;
}

std::unique_ptr<EventMetrics> PinchEventMetrics::Clone() const {
  return base::WrapUnique(new PinchEventMetrics(*this));
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
