// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_EVENT_METRICS_TEST_CREATOR_H_
#define CC_TEST_EVENT_METRICS_TEST_CREATOR_H_

#include <functional>
#include <memory>
#include <optional>

#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "cc/metrics/event_metrics.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "ui/events/types/event_type.h"

namespace cc {

// A helper class for creating `EventMetrics` in unit tests.
class EventMetricsTestCreator {
 public:
  struct EventParams {
    ui::EventType type = ui::EventType::kUnknown;
    base::TimeTicks timestamp = kDefaultTimestamp;
    std::optional<bool> caused_frame_update = std::nullopt;
  };

  std::unique_ptr<EventMetrics> CreateEventMetrics(EventParams params);

  struct ScrollEventParams {
    base::TimeTicks timestamp = kDefaultTimestamp;
    std::optional<bool> caused_frame_update = std::nullopt;
    std::optional<ScrollEventMetrics::DispatchBeginFrameArgs> dispatch_args =
        std::nullopt;
  };

  std::unique_ptr<ScrollEventMetrics> CreateGestureScrollBegin(
      ScrollEventParams params);
  std::unique_ptr<ScrollEventMetrics> CreateGestureScrollEnd(
      ScrollEventParams params);
  std::unique_ptr<ScrollEventMetrics> CreateInertialGestureScrollEnd(
      ScrollEventParams params);

  struct ScrollUpdateEventParams {
    base::TimeTicks timestamp = kDefaultTimestamp;
    float delta = 0.0f;
    std::optional<float> predicted_delta = std::nullopt;
    std::optional<bool> caused_frame_update = std::nullopt;
    std::optional<bool> did_scroll = std::nullopt;
    std::optional<bool> is_synthetic = std::nullopt;
    std::optional<EventMetrics::TraceId> trace_id = std::nullopt;
    std::optional<ScrollEventMetrics::DispatchBeginFrameArgs> dispatch_args =
        std::nullopt;
  };

  std::unique_ptr<ScrollUpdateEventMetrics> CreateFirstGestureScrollUpdate(
      ScrollUpdateEventParams params);
  std::unique_ptr<ScrollUpdateEventMetrics> CreateGestureScrollUpdate(
      ScrollUpdateEventParams params);
  std::unique_ptr<ScrollUpdateEventMetrics> CreateInertialGestureScrollUpdate(
      ScrollUpdateEventParams params);

 private:
  static inline constexpr base::TimeTicks kDefaultTimestamp =
      base::TimeTicks() + base::Milliseconds(1337);

  std::unique_ptr<ScrollEventMetrics> CreateScrollEventMetrics(
      ui::EventType type,
      bool is_inertial,
      ScrollEventParams params);
  std::unique_ptr<ScrollUpdateEventMetrics> CreateScrollUpdateEventMetrics(
      bool is_inertial,
      ScrollUpdateEventMetrics::ScrollUpdateType scroll_update_type,
      ScrollUpdateEventParams params);

  base::SimpleTestTickClock test_tick_clock_;
};

}  // namespace cc

#endif  // CC_TEST_EVENT_METRICS_TEST_CREATOR_H_
