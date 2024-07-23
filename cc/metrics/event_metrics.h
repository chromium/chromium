// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_EVENT_METRICS_H_
#define CC_METRICS_EVENT_METRICS_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/types/id_type.h"
#include "cc/cc_export.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "ui/events/types/event_type.h"
#include "ui/events/types/scroll_input_type.h"
#include "ui/latency/latency_info.h"
namespace cc {
class PinchEventMetrics;
class ScrollEventMetrics;
class ScrollUpdateEventMetrics;

// Data about an event used by CompositorFrameReporter in generating event
// latency metrics.
class CC_EXPORT EventMetrics {
 public:
  using List = std::vector<std::unique_ptr<EventMetrics>>;
  using TraceId = base::IdType64<class ui::LatencyInfo>;
  // Event types we are interested in. This list should be in the same order as
  // values of EventLatencyEventType enum from enums.xml file.
  enum class EventType {
    kMousePressed,
    kMouseReleased,
    kMouseWheel,
    // TODO(crbug.com/40126863): Currently, all EventType::kKeyPressed events
    // are reported under EventLatency.KeyPressed histogram. This includes both
    // key-down and key-char events. Consider reporting them separately.
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
    kMouseMoved,
    kMaxValue = kMouseMoved,
  };

  // Stages of event dispatch in different processes/threads.
  enum class DispatchStage {
    kGenerated,
    // 'kScrollsBlockingTouchDispatchedToRenderer' is used by Scroll events to
    // understand when a corresponding TouchMove event arrived in the Browser
    // Main. If the related TouchMove wasn't blocking, this stage field is not
    // set.
    kScrollsBlockingTouchDispatchedToRenderer,
    kArrivedInBrowserMain,
    kArrivedInRendererCompositor,
    kRendererCompositorStarted,
    kRendererCompositorFinished,
    kRendererMainStarted,
    kRendererMainFinished,
    kMaxValue = kRendererMainFinished,
  };

  static std::unique_ptr<EventMetrics> Create(ui::EventType type,
                                              base::TimeTicks timestamp,
                                              std::optional<TraceId> trace_id);

  // Returns a new instance if the event is of a type we are interested in.
  // Otherwise, returns `nullptr`. For scroll and pinch events, use the
  // appropriate subcalss instead.
  static std::unique_ptr<EventMetrics> Create(
      ui::EventType type,
      base::TimeTicks timestamp,
      base::TimeTicks arrived_in_browser_main_timestamp,
      std::optional<TraceId> trace_id);

  // Similar to `Create()` with an extra `base::TickClock` to use in tests.
  static std::unique_ptr<EventMetrics> CreateForTesting(
      ui::EventType type,
      base::TimeTicks timestamp,
      base::TimeTicks arrived_in_browser_main_timestamp,
      const base::TickClock* tick_clock,
      std::optional<TraceId> trace_id);

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

  std::optional<TraceId> trace_id() const { return trace_id_; }

  // Returns a string representing event type.
  const char* GetTypeName() const;
  static const char* GetTypeName(EventType type);

  // Returns custom histogram bucketing for the metric. If returns `nullopt`,
  // default bucketing will be used.
  struct HistogramBucketing {
    base::TimeDelta min;
    base::TimeDelta max;
    size_t count;
    const char* version_suffix;
  };
  const std::optional<HistogramBucketing>& GetHistogramBucketing() const;

  void SetHighLatencyStage(const std::string& stage);
  const std::vector<std::string>& GetHighLatencyStages() const {
    return high_latency_stages_;
  }
  void ClearHighLatencyStagesForTesting() { high_latency_stages_.clear(); }

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

  bool should_record_tracing() const { return should_record_tracing_; }
  void tracing_recorded() {
    DCHECK(should_record_tracing_);
    should_record_tracing_ = false;
  }

  bool requires_main_thread_update() const {
    return requires_main_thread_update_;
  }
  void set_requires_main_thread_update() {
    DCHECK(!requires_main_thread_update_);
    requires_main_thread_update_ = true;
  }

 protected:
  EventMetrics(EventType type,
               base::TimeTicks timestamp,
               const base::TickClock* tick_clock,
               std::optional<TraceId> trace_id);

  EventMetrics(EventType type,
               base::TimeTicks timestamp,
               base::TimeTicks arrived_in_browser_main_timestamp,
               const base::TickClock* tick_clock,
               std::optional<TraceId> trace_id);

  // Creates a clone of `other` that might be used in creating `EventMetrics`
  // objects for some injected events. Since this object itself does not
  // directly correspond to an event, it won't be used in recording trace
  // events.
  EventMetrics(const EventMetrics& other);

  // Copy timestamps of dispatch stages (up to and including
  // `last_dispatch_stage`) from `other`.
  void CopyTimestampsFrom(const EventMetrics& other,
                          DispatchStage last_dispatch_stage);

  void SetDispatchStageTimestamp(DispatchStage stage,
                                 base::TimeTicks timestamp);

 private:
  friend class ScrollEventMetrics;
  friend class ScrollUpdateEventMetrics;

  static std::unique_ptr<EventMetrics> CreateInternal(
      ui::EventType type,
      base::TimeTicks timestamp,
      base::TimeTicks arrived_in_browser_main_timestamp,
      const base::TickClock* tick_clock,
      std::optional<TraceId> trace_id);

  EventType type_;

  std::vector<std::string> high_latency_stages_;

  const raw_ptr<const base::TickClock> tick_clock_;

  // Timestamps of different stages of event dispatch. Timestamps are set as the
  // event moves forward in the pipeline. In the end, some stages might not have
  // a timestamp which means the event did not pass those stages.
  base::TimeTicks
      dispatch_stage_timestamps_[static_cast<int>(DispatchStage::kMaxValue) +
                                 1];

  // Determines whether a tracing event should be recorded for this object or
  // not. This is `true` by default and set to `false` after a tracing event is
  // recorded to avoid multiple recordings. Also, it is `false` for cloned
  // objects as they are not meant to be recorded in tracings.
  bool should_record_tracing_ = true;

  // This is set on an EventMetrics object that comes from the impl thread, if
  // the visual update from the event requires the main thread. Currently used
  // for GestureScrollUpdate with scroll unification, when the scroller isn't
  // composited or has main-thread scrolling reasons on the ScrollNode.
  bool requires_main_thread_update_ = false;

  // This is a trace id of an input event. It can be null for events which don't
  // have a corresponding input, for example a generated event based on existing
  // event.
  std::optional<TraceId> trace_id_;
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
  // The |blocking_touch_dispatched_to_renderer| must be not null only for
  // scrolls which corresponding TouchMove was blocking.
  //
  // TODO(b/224960731): Fix tests and stop supporting the case when
  // `arrived_in_browser_main_timestamp` is null.
  static std::unique_ptr<ScrollEventMetrics> Create(
      ui::EventType type,
      ui::ScrollInputType input_type,
      bool is_inertial,
      base::TimeTicks timestamp,
      base::TimeTicks arrived_in_browser_main_timestamp,
      base::TimeTicks blocking_touch_dispatched_to_renderer,
      std::optional<TraceId> trace_id);

  // Prefer to use `Create()` above. This method is used only by the Browser
  // process which have own breakdowns.
  // Similar to `Create()` above but doesn't set kArrivedInBrowserMain and
  // kScrollsBlockingTouchDispatchedToRenderer.
  static std::unique_ptr<ScrollEventMetrics> CreateForBrowser(
      ui::EventType type,
      ui::ScrollInputType input_type,
      bool is_inertial,
      base::TimeTicks timestamp,
      std::optional<TraceId> trace_id);

  // Similar to `Create()` with an extra `base::TickClock` to use in tests.
  // Should only be used for scroll events other than scroll-update.
  static std::unique_ptr<ScrollEventMetrics> CreateForTesting(
      ui::EventType type,
      ui::ScrollInputType input_type,
      bool is_inertial,
      base::TimeTicks timestamp,
      base::TimeTicks arrived_in_browser_main_timestamp,
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

  void set_begin_frame_args(const viz::BeginFrameArgs& args) { args_ = args; }

  const viz::BeginFrameArgs& begin_frame_args() const { return args_; }

 protected:
  ScrollEventMetrics(EventType type,
                     ScrollType scroll_type,
                     base::TimeTicks timestamp,
                     base::TimeTicks arrived_in_browser_main_timestamp,
                     const base::TickClock* tick_clock,
                     std::optional<TraceId> trace_id);
  ScrollEventMetrics(const ScrollEventMetrics&);

 private:
  static std::unique_ptr<ScrollEventMetrics> CreateInternal(
      ui::EventType type,
      ui::ScrollInputType input_type,
      bool is_inertial,
      base::TimeTicks timestamp,
      base::TimeTicks arrived_in_browser_main_timestamp,
      const base::TickClock* tick_clock,
      std::optional<TraceId> trace_id);

  // Type of the input device for the event.
  ScrollType scroll_type_;

  // The active viz::BeginFrameArgs when the event arrived in the Renderer.
  // These may not match those of CompositorFrameReporter for which the event
  // is eventually displayed.
  viz::BeginFrameArgs args_;
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
  // The `arrived_in_browser_main_timestamp` can be null for events that were
  // generated synthetically within the Renderer.  The
  // `blocking_touch_dispatched_to_renderer` must be not null only for
  // scrolls which corresponding TouchMove was blocking.
  //
  // TODO(b/329346768): Build `trace_id` generation for synthetic events.
  static std::unique_ptr<ScrollUpdateEventMetrics> Create(
      ui::EventType type,
      ui::ScrollInputType input_type,
      bool is_inertial,
      ScrollUpdateType scroll_update_type,
      float delta,
      base::TimeTicks timestamp,
      base::TimeTicks arrived_in_browser_main_timestamp,
      base::TimeTicks blocking_touch_dispatched_to_renderer,
      std::optional<TraceId> trace_id);

  // Prefer to use `Create()` above. This method is used only by the Browser
  // process which have own breakdowns.
  // Similar to `Create()` above but doesn't set kArrivedInBrowserMain and
  // kScrollsBlockingTouchDispatchedToRenderer.
  static std::unique_ptr<ScrollUpdateEventMetrics> CreateForBrowser(
      ui::EventType type,
      ui::ScrollInputType input_type,
      bool is_inertial,
      ScrollUpdateType scroll_update_type,
      float delta,
      base::TimeTicks timestamp,
      TraceId trace_id);

  // Similar to `Create()` with an extra `base::TickClock` to use in tests.
  // Should only be used for scroll-update events.
  static std::unique_ptr<ScrollUpdateEventMetrics> CreateForTesting(
      ui::EventType type,
      ui::ScrollInputType input_type,
      bool is_inertial,
      ScrollUpdateType scroll_update_type,
      float delta,
      base::TimeTicks timestamp,
      base::TimeTicks arrived_in_browser_main_timestamp,
      const base::TickClock* tick_clock,
      std::optional<TraceId> trace_id);

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

  int32_t coalesced_event_count() const { return coalesced_event_count_; }

  void set_predicted_delta(float predicted_delta) {
    predicted_delta_ = predicted_delta;
  }

  base::TimeTicks last_timestamp() const { return last_timestamp_; }

  std::unique_ptr<EventMetrics> Clone() const override;

  void set_is_janky_scrolled_frame(std::optional<bool> is_janky) {
    is_janky_scrolled_frame_ = is_janky;
  }
  std::optional<bool> is_janky_scrolled_frame() const {
    return is_janky_scrolled_frame_;
  }

 protected:
  ScrollUpdateEventMetrics(EventType type,
                           ScrollType scroll_type,
                           ScrollUpdateType scroll_update_type,
                           float delta,
                           base::TimeTicks timestamp,
                           base::TimeTicks arrived_in_browser_main_timestamp,
                           const base::TickClock* tick_clock,
                           std::optional<TraceId> trace_id);
  ScrollUpdateEventMetrics(const ScrollUpdateEventMetrics&);

 private:
  static std::unique_ptr<ScrollUpdateEventMetrics> CreateInternal(
      ui::EventType type,
      ui::ScrollInputType input_type,
      bool is_inertial,
      ScrollUpdateType scroll_update_type,
      float delta,
      base::TimeTicks timestamp,
      base::TimeTicks arrived_in_browser_main_timestamp,
      const base::TickClock* tick_clock,
      std::optional<TraceId> trace_id);

  float delta_;
  float predicted_delta_;

  // Timestamp of the last event coalesced into this one.
  base::TimeTicks last_timestamp_;

  // Total events that were coalesced into this into this ScrollUpdate
  int32_t coalesced_event_count_ = 1;

  std::optional<bool> is_janky_scrolled_frame_ = std::nullopt;
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
      base::TimeTicks timestamp,
      TraceId trace_id);

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
                    const base::TickClock* tick_clock,
                    std::optional<TraceId> trace_id);
  PinchEventMetrics(const PinchEventMetrics&);

 private:
  static std::unique_ptr<PinchEventMetrics> CreateInternal(
      ui::EventType type,
      ui::ScrollInputType input_type,
      base::TimeTicks timestamp,
      const base::TickClock* tick_clock,
      std::optional<TraceId> trace_id);

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
