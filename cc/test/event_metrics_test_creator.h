// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_EVENT_METRICS_TEST_CREATOR_H_
#define CC_TEST_EVENT_METRICS_TEST_CREATOR_H_

#include <functional>
#include <memory>
#include <optional>

#include "base/memory/raw_ref.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "cc/metrics/event_metrics.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "ui/events/types/event_type.h"

namespace cc {

// A helper class for creating `EventMetrics` in unit tests.
class EventMetricsTestCreator {
 public:
  static inline constexpr base::TimeTicks kDefaultTimestamp =
      base::TimeTicks() + base::Milliseconds(1337);

  // ---------------------------------------------------------------------------
  // Base Templates for CRTP
  //
  // We use the Curiously Recurring Template Pattern
  // (https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern) to
  // avoid setter duplication while preserving method chaining and improving
  // type safety. It allows us to implement a single method on a base class, for
  // example:
  //
  // ```
  // template <typename Derived>
  // class EventBuilderBase {
  //  public:
  //   Derived& SetXYZ(...);
  //   ...
  // }
  // ```
  //
  // and then use it with all three builders as if they each implemented their
  // own method:
  //
  // ```
  // EventBuilder& EventBuilder::SetTimestamp(...);
  // ScrollEventBuilder& ScrollEventBuilder::SetTimestamp(...);
  // ScrollUpdateEventBuilder& ScrollUpdateEventBuilder::SetTimestamp(...);
  // ```
  //
  // Inheritance Hierarchy:
  //
  //   Actual Builder Classes         CRTP Base Templates
  //   (to be used by clients)
  //
  //   EventBuilder ----------------> EventBuilderBase<Derived>
  //   (Derived=EventBuilder)               ^
  //                                        |
  //   ScrollEventBuilder ----------> ScrollEventBuilderBase<Derived>
  //   (Derived=ScrollEventBuilder)         ^
  //                                        |
  //   ScrollUpdateEventBuilder ----> ScrollUpdateEventBuilderBase<Derived>
  //   (Derived=ScrollUpdateEventBuilderBase)
  // ---------------------------------------------------------------------------

  template <typename Derived>
  class EventBuilderBase {
   public:
    Derived& SetTimestamp(base::TimeTicks timestamp);
    Derived& SetArrivedInRendererCompositorTimestamp(
        base::TimeTicks arrived_in_renderer_compositor_timestamp);
    Derived& SetCausedFrameUpdate(bool caused_frame_update);

   protected:
    EventBuilderBase(base::SimpleTestTickClock& clock, ui::EventType type);

    raw_ref<base::SimpleTestTickClock> clock_;
    ui::EventType type_;
    base::TimeTicks timestamp_ = kDefaultTimestamp;
    std::optional<base::TimeTicks> arrived_in_renderer_compositor_timestamp_;
    std::optional<bool> caused_frame_update_;
  };

  template <typename Derived>
  class ScrollEventBuilderBase : public EventBuilderBase<Derived> {
   public:
    Derived& SetDispatchArgs(
        ScrollEventMetrics::DispatchBeginFrameArgs dispatch_args);

   protected:
    ScrollEventBuilderBase(base::SimpleTestTickClock& clock,
                           ui::EventType type,
                           bool is_inertial);

    bool is_inertial_;
    std::optional<ScrollEventMetrics::DispatchBeginFrameArgs> dispatch_args_;
  };

  template <typename Derived>
  class ScrollUpdateEventBuilderBase : public ScrollEventBuilderBase<Derived> {
   public:
    Derived& SetDelta(float delta);
    Derived& SetPredictedDelta(float predicted_delta);
    Derived& SetDidScroll(bool did_scroll);
    Derived& SetIsSynthetic(bool is_synthetic);
    Derived& SetTraceId(EventMetrics::TraceId trace_id);

   protected:
    ScrollUpdateEventBuilderBase(
        base::SimpleTestTickClock& clock,
        ScrollUpdateEventMetrics::ScrollUpdateType scroll_update_type,
        bool is_inertial);

    ScrollUpdateEventMetrics::ScrollUpdateType scroll_update_type_;
    float delta_ = 0.0f;
    std::optional<float> predicted_delta_;
    std::optional<bool> did_scroll_;
    std::optional<bool> is_synthetic_;
    std::optional<EventMetrics::TraceId> trace_id_;
  };

  // ---------------------------------------------------------------------------
  // Actual Builder Classes
  // ---------------------------------------------------------------------------

  class EventBuilder : public EventBuilderBase<EventBuilder> {
   public:
    EventBuilder(base::SimpleTestTickClock& clock, ui::EventType type);
    ~EventBuilder();

    std::unique_ptr<EventMetrics> Build();
  };

  EventBuilder CreateEventBuilder(ui::EventType type);

  class ScrollEventBuilder : public ScrollEventBuilderBase<ScrollEventBuilder> {
   public:
    ScrollEventBuilder(base::SimpleTestTickClock& clock,
                       ui::EventType type,
                       bool is_inertial);
    ~ScrollEventBuilder();

    std::unique_ptr<ScrollEventMetrics> Build();
  };

  ScrollEventBuilder GestureScrollBeginBuilder();
  ScrollEventBuilder GestureScrollEndBuilder();
  ScrollEventBuilder InertialGestureScrollEndBuilder();

  class ScrollUpdateEventBuilder
      : public ScrollUpdateEventBuilderBase<ScrollUpdateEventBuilder> {
   public:
    ScrollUpdateEventBuilder(
        base::SimpleTestTickClock& clock,
        ScrollUpdateEventMetrics::ScrollUpdateType scroll_update_type,
        bool is_inertial);
    ~ScrollUpdateEventBuilder();

    std::unique_ptr<ScrollUpdateEventMetrics> Build();
  };

  ScrollUpdateEventBuilder FirstGestureScrollUpdateBuilder();
  ScrollUpdateEventBuilder GestureScrollUpdateBuilder();
  ScrollUpdateEventBuilder InertialGestureScrollUpdateBuilder();

 private:
  base::SimpleTestTickClock test_tick_clock_;
};

}  // namespace cc

#endif  // CC_TEST_EVENT_METRICS_TEST_CREATOR_H_
