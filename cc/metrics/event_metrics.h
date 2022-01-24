// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_EVENT_METRICS_H_
#define CC_METRICS_EVENT_METRICS_H_

#include <memory>
#include <vector>

#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "cc/cc_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
    kInertialGestureScrollUpdate,
    kMaxValue = kInertialGestureScrollUpdate,
  };

  // Type of scroll events. This list should be in the same order as values of
  // `EventLatencyScrollInputType` enum from enums.xml file.
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

  // Type of pinch events. This list should be in the same order as values of
  // `EventLatencyPinchInputType` enum from enums.xml file.
  enum class PinchType {
    kTouchpad,
    kTouchscreen,
    kMaxValue = kTouchscreen,
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

  // Parameters to initialize an `EventMetrics` object for a scroll or pinch
  // gesture event.
  struct CC_EXPORT GestureParams {
    // Extra parameters for scroll events.
    struct CC_EXPORT ScrollParams {
      // Constructor for all scroll events.
      explicit ScrollParams(bool is_inertial);

      // Constructor for scroll update events only.
      ScrollParams(bool is_inertial, ScrollUpdateType update_type);

      ScrollParams(const ScrollParams&);
      ScrollParams& operator=(const ScrollParams&);

      // Determines whether the scroll event is an inertial phase event (caused
      // by a fling).
      bool is_inertial;

      // Determines whether the scroll update event is the first one in a
      // sequence.
      absl::optional<ScrollUpdateType> update_type;
    };

    // Constructor for all gesture (scroll and pinch) events.
    explicit GestureParams(ui::ScrollInputType input_type);

    // Constructor for scroll events only.
    GestureParams(ui::ScrollInputType input_type, bool scroll_is_inertial);

    // Constructor for scroll update events only.
    GestureParams(ui::ScrollInputType input_type,
                  bool scroll_is_inertial,
                  ScrollUpdateType scroll_update_type);

    GestureParams(const GestureParams&);
    GestureParams& operator=(const GestureParams&);

    // Determines the type of input device generating the event.
    ui::ScrollInputType input_type;

    // Extra parameters for scroll events.
    absl::optional<ScrollParams> scroll_params;
  };

  // Returns a new instance if the event is of a type we are interested in.
  // Otherwise, returns nullptr.
  static std::unique_ptr<EventMetrics> Create(
      ui::EventType type,
      absl::optional<GestureParams> gesture_params,
      base::TimeTicks timestamp);

  // Similar to `Create()` with an extra `base::TickClock` to use in tests.
  static std::unique_ptr<EventMetrics> CreateForTesting(
      ui::EventType type,
      absl::optional<GestureParams> gesture_params,
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
      absl::optional<GestureParams> gesture_params,
      DispatchStage last_dispatch_stage,
      const EventMetrics* existing);

  EventMetrics(const EventMetrics&) = delete;
  EventMetrics& operator=(const EventMetrics&) = delete;

  ~EventMetrics();

  EventType type() const { return type_; }

  // Returns a string representing event type.
  const char* GetTypeName() const;

  const absl::optional<ScrollType>& scroll_type() const { return scroll_type_; }

  // Returns a string representing input type for a scroll event. Should only be
  // called for scroll events.
  const char* GetScrollTypeName() const;

  const absl::optional<PinchType>& pinch_type() const { return pinch_type_; }

  // Returns a string representing input type for a pinch event. Should only be
  // called for pinch events.
  const char* GetPinchTypeName() const;

  void SetDispatchStageTimestamp(DispatchStage stage);
  base::TimeTicks GetDispatchStageTimestamp(DispatchStage stage) const;

  // Resets the metrics object to dispatch stage `stage` by setting timestamps
  // of dispatch stages after `stage` to null timestamp,
  void ResetToDispatchStage(DispatchStage stage);

  bool HasSmoothInputEvent() const;

  std::unique_ptr<EventMetrics> Clone() const;

  // Used in tests to check expectations on EventMetrics objects.
  bool operator==(const EventMetrics& other) const;

 private:
  static std::unique_ptr<EventMetrics> CreateInternal(
      ui::EventType type,
      const absl::optional<GestureParams>& gesture_params,
      base::TimeTicks timestamp,
      const base::TickClock* tick_clock);

  EventMetrics(EventType type,
               absl::optional<ScrollType> scroll_type,
               absl::optional<PinchType> pinch_type,
               base::TimeTicks timestamp,
               const base::TickClock* tick_clock);

  EventType type_;

  // Only available for scroll events and represents the type of input device
  // for the event.
  absl::optional<ScrollType> scroll_type_;

  // Only available for pinch events and represents the type of input device for
  // the event.
  absl::optional<PinchType> pinch_type_;

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
