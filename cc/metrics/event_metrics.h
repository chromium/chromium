// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_EVENT_METRICS_H_
#define CC_METRICS_EVENT_METRICS_H_

#include <memory>
#include <vector>

#include "base/optional.h"
#include "base/time/time.h"
#include "cc/cc_export.h"
#include "ui/events/types/event_type.h"
#include "ui/events/types/scroll_input_type.h"

namespace cc {

// Data about an event used by CompositorFrameReporter in generating event
// latency metrics.
class CC_EXPORT EventMetrics {
 public:
  // Event types we are interested in. This list should be in the same order as
  // values of EventLatencyEventType enum from enums.xml file.
  enum class EventType {
    kMousePressed,
    kMouseReleased,
    kMouseWheel,
    // TODO(crbug/1071645): Currently, all ET_KEY_PRESSED events are reported
    // under EventLatency.KeyPressed histogram. This includes both key-down and
    // key-char events. Consider reporting them separately.
    kKeyPressed,
    kKeyReleased,
    kTouchPressed,
    kTouchReleased,
    kTouchMoved,
    kGestureScrollBegin,
    kGestureScrollUpdate,
    kGestureScrollEnd,
    kGestureDoubleTap,
    kGestureLongPress,
    kGestureLongTap,
    kGestureShowPress,
    kGestureTap,
    kGestureTapCancel,
    kGestureTapDown,
    kGestureTapUnconfirmed,
    kGestureTwoFingerTap,
    kFirstGestureScrollUpdate,
    kMaxValue = kFirstGestureScrollUpdate,
  };

  // Type of scroll events. This list should be in the same order as values of
  // EventLatencyScrollInputType enum from enums.xml file.
  enum class ScrollType {
    kAutoscroll,
    kScrollbar,
    kTouchscreen,
    kWheel,
    kMaxValue = kWheel,
  };

  // Determines whether a scroll-update event is the first one in a gesture
  // scroll sequence or not.
  enum class ScrollUpdateType {
    kStarted,
    kContinued,
    kMaxValue = kContinued,
  };

  // Returns a new instance if |type| is an event type we are interested in.
  // Otherwise, returns nullptr.
  static std::unique_ptr<EventMetrics> Create(
      ui::EventType type,
      base::Optional<ScrollUpdateType> scroll_update_type,
      base::TimeTicks time_stamp,
      base::Optional<ui::ScrollInputType> scroll_input_type);

  EventMetrics(const EventMetrics&);
  EventMetrics& operator=(const EventMetrics&);

  EventType type() const { return type_; }

  // Returns a string representing event type.
  const char* GetTypeName() const;

  base::TimeTicks time_stamp() const { return time_stamp_; }

  const base::Optional<ScrollType>& scroll_type() const { return scroll_type_; }

  // Returns a string representing input type for a scroll event. Should only be
  // called for scroll events.
  const char* GetScrollTypeName() const;

  // Used in tests to check expectations on EventMetrics objects.
  bool operator==(const EventMetrics& other) const;

 private:
  EventMetrics(EventType type,
               base::TimeTicks time_stamp,
               base::Optional<ScrollType> scroll_type);

  EventType type_;
  base::TimeTicks time_stamp_;

  // Only available for scroll events and represents the type of input device
  // for the event.
  base::Optional<ScrollType> scroll_type_;
};

// Struct storing event metrics from both main and impl threads.
struct CC_EXPORT EventMetricsSet {
  EventMetricsSet();
  ~EventMetricsSet();
  EventMetricsSet(std::vector<EventMetrics> main_thread_event_metrics,
                  std::vector<EventMetrics> impl_thread_event_metrics);
  EventMetricsSet(EventMetricsSet&&);
  EventMetricsSet& operator=(EventMetricsSet&&);

  EventMetricsSet(const EventMetricsSet&) = delete;
  EventMetricsSet& operator=(const EventMetricsSet&) = delete;

  std::vector<EventMetrics> main_event_metrics;
  std::vector<EventMetrics> impl_event_metrics;
};

}  // namespace cc

#endif  // CC_METRICS_EVENT_METRICS_H_
