// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_EVENT_METRICS_H_
#define CC_METRICS_EVENT_METRICS_H_

#include <memory>
#include <vector>

#include "base/optional.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "cc/cc_export.h"
#include "ui/events/types/event_type.h"
#include "ui/events/types/scroll_input_type.h"

namespace cc {

// Data about an event used by CompositorFrameReporter in generating event
// latency metrics.
class CC_EXPORT EventMetrics {
 public:
  using List = std::vector<std::unique_ptr<EventMetrics>>;

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
    kMouseDragged,
    kGesturePinchBegin,
    kGesturePinchEnd,
    kGesturePinchUpdate,
    kMaxValue = kGesturePinchUpdate,
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

  // Stages of event dispatch in different processes/threads.
  enum class DispatchStage {
    kGenerated,
    kArrivedInRendererCompositor,
    kRendererCompositorStarted,
    kRendererCompositorFinished,
    kRendererMainStarted,
    kRendererMainFinished,
    kMaxValue = kRendererMainFinished,
  };

  // Returns a new instance if the event is of a type we are interested in.
  // Otherwise, returns nullptr.
  static std::unique_ptr<EventMetrics> Create(
      ui::EventType type,
      base::Optional<ScrollUpdateType> scroll_update_type,
      base::Optional<ui::ScrollInputType> scroll_input_type,
      base::TimeTicks timestamp);

  // Similar to `Create()` with an extra `base::TickClock` to use in tests.
  static std::unique_ptr<EventMetrics> CreateForTesting(
      ui::EventType type,
      base::Optional<ScrollUpdateType> scroll_update_type,
      base::Optional<ui::ScrollInputType> scroll_input_type,
      base::TimeTicks timestamp,
      const base::TickClock* tick_clock);

  // Used to create an instance for an event generated based on an existing
  // event. If the new event is of an interesting type, we expect that the
  // existing event is also of an interesting type in which case `existing` is
  // not nullptr and timestamps (up to and including `last_dispatch_stage`) and
  // tick clock from `existing` will be used for the new metrics object. If the
  // new event is not an interesting one, return value would be nullptr.
  static std::unique_ptr<EventMetrics> CreateFromExisting(
      ui::EventType type,
      base::Optional<ScrollUpdateType> scroll_update_type,
      base::Optional<ui::ScrollInputType> scroll_input_type,
      DispatchStage last_dispatch_stage,
      const EventMetrics* existing);

  EventMetrics(const EventMetrics&) = delete;
  EventMetrics& operator=(const EventMetrics&) = delete;

  ~EventMetrics();

  EventType type() const { return type_; }

  // Returns a string representing event type.
  const char* GetTypeName() const;

  const base::Optional<ScrollType>& scroll_type() const { return scroll_type_; }

  // Returns a string representing input type for a scroll event. Should only be
  // called for scroll events.
  const char* GetScrollTypeName() const;

  void SetDispatchStageTimestamp(DispatchStage stage);
  base::TimeTicks GetDispatchStageTimestamp(DispatchStage stage) const;

  // Resets the metrics object to dispatch stage `stage` by setting timestamps
  // of dispatch stages after `stage` to null timestamp,
  void ResetToDispatchStage(DispatchStage stage);

  // Determines whether TotalLatencyToSwapBegin metric should be reported for
  // this event or not. This metric is only desired for gesture-scroll events.
  bool ShouldReportScrollingTotalLatency() const;

  std::unique_ptr<EventMetrics> Clone() const;

  // Used in tests to check expectations on EventMetrics objects.
  bool operator==(const EventMetrics& other) const;

 private:
  static std::unique_ptr<EventMetrics> CreateInternal(
      ui::EventType type,
      base::Optional<ScrollUpdateType> scroll_update_type,
      base::Optional<ui::ScrollInputType> scroll_input_type,
      base::TimeTicks timestamp,
      const base::TickClock* tick_clock);

  EventMetrics(EventType type,
               base::Optional<ScrollType> scroll_type,
               base::TimeTicks timestamp,
               const base::TickClock* tick_clock);

  EventType type_;

  // Only available for scroll events and represents the type of input device
  // for the event.
  base::Optional<ScrollType> scroll_type_;

  const base::TickClock* const tick_clock_;

  // Timestamps of different stages of event dispatch. Timestamps are set as the
  // event moves forward in the pipeline. In the end, some stages might not have
  // a timestamp which means the event did not pass those stages.
  base::TimeTicks
      dispatch_stage_timestamps_[static_cast<int>(DispatchStage::kMaxValue) +
                                 1];
};

// Struct storing event metrics from both main and impl threads.
struct CC_EXPORT EventMetricsSet {
  EventMetricsSet();
  ~EventMetricsSet();
  EventMetricsSet(EventMetrics::List main_thread_event_metrics,
                  EventMetrics::List impl_thread_event_metrics);
  EventMetricsSet(EventMetricsSet&&);
  EventMetricsSet& operator=(EventMetricsSet&&);

  EventMetricsSet(const EventMetricsSet&) = delete;
  EventMetricsSet& operator=(const EventMetricsSet&) = delete;

  EventMetrics::List main_event_metrics;
  EventMetrics::List impl_event_metrics;
};

}  // namespace cc

#endif  // CC_METRICS_EVENT_METRICS_H_
