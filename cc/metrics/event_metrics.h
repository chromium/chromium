// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_EVENT_METRICS_H_
#define CC_METRICS_EVENT_METRICS_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "cc/cc_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/events/types/event_type.h"
#include "ui/events/types/scroll_input_type.h"

namespace cc {
class PinchEventMetrics;
class ScrollEventMetrics;
class ScrollUpdateEventMetrics;

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
  // Otherwise, returns `nullptr`. For scroll and pinch events, use the
  // appropriate subcalss instead.
  static std::unique_ptr<EventMetrics> Create(ui::EventType type,
                                              base::TimeTicks timestamp);

  // Similar to `Create()` with an extra `base::TickClock` to use in tests.
  static std::unique_ptr<EventMetrics> CreateForTesting(
      ui::EventType type,
      base::TimeTicks timestamp,
      const base::TickClock* tick_clock);

  // Used to create an instance for an event generated based on an existing
  // event. If the new event is of an interesting type, we expect that the
  // existing event is also of an interesting type in which case `existing` is
  // not `nullptr` and timestamps (up to and including `last_dispatch_stage`)
  // and tick clock from `existing` will be used for the new metrics object. If
  // the new event is not an interesting one, return value would be `nullptr`.
  // For scroll and pinch events, use the appropriate subclass instead.
  static std::unique_ptr<EventMetrics> CreateFromExisting(
      ui::EventType type,
      DispatchStage last_dispatch_stage,
      const EventMetrics* existing);

  virtual ~EventMetrics();

  EventMetrics& operator=(const EventMetrics&) = delete;

  EventType type() const { return type_; }

  // Returns a string representing event type.
  const char* GetTypeName() const;

  void SetDispatchStageTimestamp(DispatchStage stage);
  base::TimeTicks GetDispatchStageTimestamp(DispatchStage stage) const;

  // Resets the metrics object to dispatch stage `stage` by setting timestamps
  // of dispatch stages after `stage` to null timestamp,
  void ResetToDispatchStage(DispatchStage stage);

  bool HasSmoothInputEvent() const;

  virtual ScrollEventMetrics* AsScroll();
  const ScrollEventMetrics* AsScroll() const;

  virtual ScrollUpdateEventMetrics* AsScrollUpdate();
  const ScrollUpdateEventMetrics* AsScrollUpdate() const;

  virtual PinchEventMetrics* AsPinch();
  const PinchEventMetrics* AsPinch() const;

  virtual std::unique_ptr<EventMetrics> Clone() const;

  bool is_tracing_recorded() const { return is_tracing_recorded_; }
  void set_tracing_recorded() {
    DCHECK(!is_tracing_recorded_);
    is_tracing_recorded_ = true;
  }

 protected:
  EventMetrics(EventType type,
               base::TimeTicks timestamp,
               const base::TickClock* tick_clock);
  EventMetrics(const EventMetrics& other);

  // Copy timestamps of dispatch stages (up to and including
  // `last_dispatch_stage`) from `other`.
  void CopyTimestampsFrom(const EventMetrics& other,
                          DispatchStage last_dispatch_stage);

 private:
  friend class ScrollEventMetrics;
  friend class ScrollUpdateEventMetrics;

  static std::unique_ptr<EventMetrics> CreateInternal(
      ui::EventType type,
      base::TimeTicks timestamp,
      const base::TickClock* tick_clock);

  EventType type_;

  const raw_ptr<const base::TickClock> tick_clock_;

  // Timestamps of different stages of event dispatch. Timestamps are set as the
  // event moves forward in the pipeline. In the end, some stages might not have
  // a timestamp which means the event did not pass those stages.
  base::TimeTicks
      dispatch_stage_timestamps_[static_cast<int>(DispatchStage::kMaxValue) +
                                 1];

  bool is_tracing_recorded_ = false;
};

class CC_EXPORT ScrollEventMetrics : public EventMetrics {
 public:
  // Type of scroll events. This list should be in the same order as values of
  // `EventLatencyScrollInputType` enum from enums.xml file.
  enum class ScrollType {
    kAutoscroll,
    kScrollbar,
    kTouchscreen,
    kWheel,
    kMaxValue = kWheel,
  };

  // Returns a new instance if the event is of a type we are interested in.
  // Otherwise, returns `nullptr`. Should only be used for scroll events other
  // than scroll-update.
  static std::unique_ptr<ScrollEventMetrics> Create(
      ui::EventType type,
      ui::ScrollInputType input_type,
      bool is_inertial,
      base::TimeTicks timestamp);

  // Similar to `Create()` with an extra `base::TickClock` to use in tests.
  // Should only be used for scroll events other than scroll-update.
  static std::unique_ptr<ScrollEventMetrics> CreateForTesting(
      ui::EventType type,
      ui::ScrollInputType input_type,
      bool is_inertial,
      base::TimeTicks timestamp,
      const base::TickClock* tick_clock);

  // Used to create an instance for an event generated based on an existing
  // event. If the new event is of an interesting type, we expect that the
  // existing event is also of an interesting type in which case `existing` is
  // not `nullptr` and timestamps (up to and including `last_dispatch_stage`)
  // and tick clock from `existing` will be used for the new metrics object. If
  // the new event is not an interesting one, return value would be `nullptr`.
  // Should only be used for scroll events other than scroll-update.
  static std::unique_ptr<ScrollEventMetrics> CreateFromExisting(
      ui::EventType type,
      ui::ScrollInputType input_type,
      bool is_inertial,
      DispatchStage last_dispatch_stage,
      const EventMetrics* existing);

  ~ScrollEventMetrics() override;

  ScrollType scroll_type() const { return scroll_type_; }

  // Returns a string representing input type for a scroll event.
  const char* GetScrollTypeName() const;

  ScrollEventMetrics* AsScroll() override;

  std::unique_ptr<EventMetrics> Clone() const override;

 protected:
  ScrollEventMetrics(EventType type,
                     ScrollType scroll_type,
                     base::TimeTicks timestamp,
                     const base::TickClock* tick_clock);
  ScrollEventMetrics(const ScrollEventMetrics&);

 private:
  static std::unique_ptr<ScrollEventMetrics> CreateInternal(
      ui::EventType type,
      ui::ScrollInputType input_type,
      bool is_inertial,
      base::TimeTicks timestamp,
      const base::TickClock* tick_clock);

  // Type of the input device for the event.
  ScrollType scroll_type_;
};

class CC_EXPORT ScrollUpdateEventMetrics : public ScrollEventMetrics {
 public:
  // Determines whether a scroll-update event is the first one in a gesture
  // scroll sequence or not.
  enum class ScrollUpdateType {
    kStarted,
    kContinued,
    kMaxValue = kContinued,
  };

  // Returns a new instance if the event is of a type we are interested in.
  // Otherwise, returns `nullptr`. Should only be used for scroll-update events.
  static std::unique_ptr<ScrollUpdateEventMetrics> Create(
      ui::EventType type,
      ui::ScrollInputType input_type,
      bool is_inertial,
      ScrollUpdateType scroll_update_type,
      float delta,
      base::TimeTicks timestamp);

  // Similar to `Create()` with an extra `base::TickClock` to use in tests.
  // Should only be used for scroll-update events.
  static std::unique_ptr<ScrollUpdateEventMetrics> CreateForTesting(
      ui::EventType type,
      ui::ScrollInputType input_type,
      bool is_inertial,
      ScrollUpdateType scroll_update_type,
      float delta,
      base::TimeTicks timestamp,
      const base::TickClock* tick_clock);

  // Used to create an instance for an event generated based on an existing
  // event. If the new event is of an interesting type, we expect that the
  // existing event is also of an interesting type in which case `existing` is
  // not `nullptr` and timestamps (up to and including `last_dispatch_stage`)
  // and tick clock from `existing` will be used for the new metrics object. If
  // the new event is not an interesting one, return value would be `nullptr`.
  // Should only be used for scroll-update events.
  static std::unique_ptr<ScrollUpdateEventMetrics> CreateFromExisting(
      ui::EventType type,
      ui::ScrollInputType input_type,
      bool is_inertial,
      ScrollUpdateType scroll_update_type,
      float delta,
      DispatchStage last_dispatch_stage,
      const EventMetrics* existing);

  ~ScrollUpdateEventMetrics() override;

  void CoalesceWith(const ScrollUpdateEventMetrics& newer_scroll_update);

  ScrollUpdateEventMetrics* AsScrollUpdate() override;

  float delta() const { return delta_; }

  float predicted_delta() const { return predicted_delta_; }
  void set_predicted_delta(float predicted_delta) {
    predicted_delta_ = predicted_delta;
  }

  base::TimeTicks last_timestamp() const { return last_timestamp_; }

  std::unique_ptr<EventMetrics> Clone() const override;

 protected:
  ScrollUpdateEventMetrics(EventType type,
                           ScrollType scroll_type,
                           ScrollUpdateType scroll_update_type,
                           float delta,
                           base::TimeTicks timestamp,
                           const base::TickClock* tick_clock);
  ScrollUpdateEventMetrics(const ScrollUpdateEventMetrics&);

 private:
  static std::unique_ptr<ScrollUpdateEventMetrics> CreateInternal(
      ui::EventType type,
      ui::ScrollInputType input_type,
      bool is_inertial,
      ScrollUpdateType scroll_update_type,
      float delta,
      base::TimeTicks timestamp,
      const base::TickClock* tick_clock);

  float delta_;
  float predicted_delta_;

  // Timestamp of the last event coalesced into this one.
  base::TimeTicks last_timestamp_;
};

class CC_EXPORT PinchEventMetrics : public EventMetrics {
 public:
  // Type of pinch events. This list should be in the same order as values of
  // `EventLatencyPinchInputType` enum from enums.xml file.
  enum class PinchType {
    kTouchpad,
    kTouchscreen,
    kMaxValue = kTouchscreen,
  };

  // Returns a new instance if the event is of a type we are interested in.
  // Otherwise, returns `nullptr`. Should only be used for pinch events.
  static std::unique_ptr<PinchEventMetrics> Create(
      ui::EventType type,
      ui::ScrollInputType input_type,
      base::TimeTicks timestamp);

  // Similar to `Create()` with an extra `base::TickClock` to use in tests.
  // Should only be used for pinch events.
  static std::unique_ptr<PinchEventMetrics> CreateForTesting(
      ui::EventType type,
      ui::ScrollInputType input_type,
      base::TimeTicks timestamp,
      const base::TickClock* tick_clock);

  ~PinchEventMetrics() override;

  PinchType pinch_type() const { return pinch_type_; }

  // Returns a string representing input type for a pinch event. Should only be
  // called for pinch events.
  const char* GetPinchTypeName() const;

  PinchEventMetrics* AsPinch() override;

  std::unique_ptr<EventMetrics> Clone() const override;

 protected:
  PinchEventMetrics(EventType type,
                    PinchType pinch_type,
                    base::TimeTicks timestamp,
                    const base::TickClock* tick_clock);
  PinchEventMetrics(const PinchEventMetrics&);

 private:
  static std::unique_ptr<PinchEventMetrics> CreateInternal(
      ui::EventType type,
      ui::ScrollInputType input_type,
      base::TimeTicks timestamp,
      const base::TickClock* tick_clock);

  PinchType pinch_type_;
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
