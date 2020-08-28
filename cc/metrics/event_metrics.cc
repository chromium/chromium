// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/event_metrics.h"

#include <utility>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/stl_util.h"

namespace cc {
namespace {

constexpr struct {
  EventMetrics::EventType metrics_event_type;
  ui::EventType ui_event_type;
  const char* name;
  base::Optional<EventMetrics::ScrollUpdateType> scroll_update_type =
      base::nullopt;
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
    EVENT_TYPE(GestureScrollBegin, ui::ET_GESTURE_SCROLL_BEGIN),
    EVENT_TYPE(GestureScrollUpdate,
               ui::ET_GESTURE_SCROLL_UPDATE,
               EventMetrics::ScrollUpdateType::kContinued),
    EVENT_TYPE(GestureScrollEnd, ui::ET_GESTURE_SCROLL_END),
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
               EventMetrics::ScrollUpdateType::kStarted),
#undef EVENT_TYPE
};
static_assert(base::size(kInterestingEvents) ==
                  static_cast<int>(EventMetrics::EventType::kMaxValue) + 1,
              "EventMetrics::EventType has changed.");

constexpr struct {
  EventMetrics::ScrollType metrics_scroll_type;
  ui::ScrollInputType ui_scroll_type;
  const char* name;
} kScrollTypes[] = {
#define SCROLL_TYPE(name, ui_type) \
  { EventMetrics::ScrollType::k##name, ui_type, #name }
    SCROLL_TYPE(Autoscroll, ui::ScrollInputType::kAutoscroll),
    SCROLL_TYPE(Scrollbar, ui::ScrollInputType::kScrollbar),
    SCROLL_TYPE(Touchscreen, ui::ScrollInputType::kTouchscreen),
    SCROLL_TYPE(Wheel, ui::ScrollInputType::kWheel),
#undef SCROLL_TYPE
};
static_assert(base::size(kScrollTypes) ==
                  static_cast<int>(EventMetrics::ScrollType::kMaxValue) + 1,
              "EventMetrics::ScrollType has changed.");

base::Optional<EventMetrics::EventType> ToInterestingEventType(
    ui::EventType ui_event_type,
    base::Optional<EventMetrics::ScrollUpdateType> scroll_update_type) {
  for (size_t i = 0; i < base::size(kInterestingEvents); i++) {
    const auto& interesting_event = kInterestingEvents[i];
    if (ui_event_type == interesting_event.ui_event_type &&
        scroll_update_type == interesting_event.scroll_update_type) {
      EventMetrics::EventType metrics_event_type =
          static_cast<EventMetrics::EventType>(i);
      DCHECK_EQ(metrics_event_type, interesting_event.metrics_event_type);
      return metrics_event_type;
    }
  }
  return base::nullopt;
}

base::Optional<EventMetrics::ScrollType> ToScrollType(
    const base::Optional<ui::ScrollInputType>& scroll_input_type) {
  if (!scroll_input_type)
    return base::nullopt;

  for (size_t i = 0; i < base::size(kScrollTypes); i++) {
    if (*scroll_input_type == kScrollTypes[i].ui_scroll_type) {
      EventMetrics::ScrollType metrics_scroll_type =
          static_cast<EventMetrics::ScrollType>(i);
      DCHECK_EQ(metrics_scroll_type, kScrollTypes[i].metrics_scroll_type);
      return metrics_scroll_type;
    }
  }
  NOTREACHED();
  return base::nullopt;
}

}  // namespace

std::unique_ptr<EventMetrics> EventMetrics::Create(
    ui::EventType type,
    base::Optional<ScrollUpdateType> scroll_update_type,
    base::TimeTicks time_stamp,
    base::Optional<ui::ScrollInputType> scroll_input_type) {
  // `scroll_update_type` should be set for and only for
  // `ui::ET_GESTURE_SCROLL_UPDATE`.
  DCHECK(type == ui::ET_GESTURE_SCROLL_UPDATE && scroll_update_type ||
         type != ui::ET_GESTURE_SCROLL_UPDATE && !scroll_update_type);
  base::Optional<EventType> interesting_type =
      ToInterestingEventType(type, scroll_update_type);
  if (!interesting_type)
    return nullptr;
  return base::WrapUnique(new EventMetrics(*interesting_type, time_stamp,
                                           ToScrollType(scroll_input_type)));
}

EventMetrics::EventMetrics(EventType type,
                           base::TimeTicks time_stamp,
                           base::Optional<ScrollType> scroll_type)
    : type_(type), time_stamp_(time_stamp), scroll_type_(scroll_type) {}

EventMetrics::EventMetrics(const EventMetrics&) = default;
EventMetrics& EventMetrics::operator=(const EventMetrics&) = default;

const char* EventMetrics::GetTypeName() const {
  return kInterestingEvents[static_cast<int>(type_)].name;
}

const char* EventMetrics::GetScrollTypeName() const {
  DCHECK(scroll_type_) << "Event is not a scroll event.";

  return kScrollTypes[static_cast<int>(*scroll_type_)].name;
}

bool EventMetrics::operator==(const EventMetrics& other) const {
  return std::tie(type_, time_stamp_, scroll_type_) ==
         std::tie(other.type_, other.time_stamp_, other.scroll_type_);
}

// EventMetricsSet
EventMetricsSet::EventMetricsSet() = default;
EventMetricsSet::~EventMetricsSet() = default;
EventMetricsSet::EventMetricsSet(
    std::vector<EventMetrics> main_thread_event_metrics,
    std::vector<EventMetrics> impl_thread_event_metrics)
    : main_event_metrics(std::move(main_thread_event_metrics)),
      impl_event_metrics(std::move(impl_thread_event_metrics)) {}
EventMetricsSet::EventMetricsSet(EventMetricsSet&& other) = default;
EventMetricsSet& EventMetricsSet::operator=(EventMetricsSet&& other) = default;

}  // namespace cc
