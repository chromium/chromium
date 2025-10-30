// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_frame_stage.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "cc/metrics/event_metrics.h"

namespace cc {

namespace {

template <typename EventMetricsPtr>
ScrollJankV4FrameStage::List CalculateStagesImpl(
    const std::vector<EventMetricsPtr>& events_metrics,
    bool skip_non_damaging_events) {
  ScrollJankV4FrameStage::List stages;

  bool has_inertial_input = false;
  bool had_earliest_gesture_scroll = false;
  bool had_any_gesture_scroll = false;
  std::optional<base::TimeTicks> scroll_start_ts = std::nullopt;
  std::optional<base::TimeTicks> scroll_end_ts = std::nullopt;
  float total_raw_delta_pixels = 0;
  float max_abs_inertial_raw_delta_pixels = 0;

  // This handles cases when we have multiple scroll events. Events for dropped
  // frames are reported by the reporter for next presented frame which could
  // lead to having multiple scroll events.
  ScrollUpdateEventMetrics* earliest_event = nullptr;
  base::TimeTicks earliest_event_generation_ts = base::TimeTicks::Max();
  base::TimeTicks last_input_generation_ts = base::TimeTicks::Min();

  // We expect that `events_metrics` contains:
  //   E. Zero or one scroll ends (`kGestureScrollEnd` or
  //      `kInertialGestureScrollEnd`).
  //   F. Zero or one first scroll updates (`kFirstGestureScrollUpdate`).
  //   U. Zero or more continuing scroll updates (`kGestureScrollUpdate` or
  //      `kInertialGestureScrollUpdate`s).
  // Furthermore, we expect that:
  //   * If there's as scroll end (E), it comes:
  //       * either before all scroll updates (F/U), in which case we assume
  //         that it ends the previous scroll,
  //       * or after all scroll updates (F/U), in which case we assume that
  //         it ends the current scroll.
  //   * If there's a first scroll update (F), it precedes all continuing scroll
  //     updates (U).
  // So E?F?U* and F?U*E? are the two possible orderings. Based on local
  // testing, the first ordering is much more likely.
  for (auto& event : events_metrics) {
    EventMetrics::EventType event_type = event->type();
    base::TimeTicks generation_ts = event->GetDispatchStageTimestamp(
        EventMetrics::DispatchStage::kGenerated);
    if (event_type == EventMetrics::EventType::kGestureScrollEnd ||
        event_type == EventMetrics::EventType::kInertialGestureScrollEnd) {
      if (scroll_end_ts) {
        TRACE_EVENT(
            "input",
            "ProcessFrameEventMetrics: Multiple scroll ends in a frame");
      }
      scroll_end_ts = generation_ts;
      continue;
    }
    if (skip_non_damaging_events && !event->caused_frame_update()) {
      // TODO(crbug.com/444183591): Handle non-damaging inputs in the scroll
      // jank metrics.
      continue;
    }
    auto* scroll_update = event->AsScrollUpdate();
    if (!scroll_update) {
      continue;
    }
    total_raw_delta_pixels += scroll_update->delta();
    // Earliest is always applied, event when the scroll update failed to
    // successfully produce a scroll.
    if (!had_earliest_gesture_scroll ||
        generation_ts < earliest_event_generation_ts) {
      earliest_event = scroll_update;
      earliest_event_generation_ts = generation_ts;
      had_earliest_gesture_scroll = true;
    }

    // We check the type first, as if `is_scroll_start` is true, we need to
    // include `scroll_update` even if `scroll_update->did_scroll()` is false.
    switch (event_type) {
      case EventMetrics::EventType::kFirstGestureScrollUpdate:
        if (scroll_start_ts) {
          TRACE_EVENT("input",
                      "CalculateStages: Multiple scroll starts in a "
                      "single frame (unexpected)");
        }
        scroll_start_ts = generation_ts;
        break;
      case EventMetrics::EventType::kGestureScrollUpdate:
        break;
      case EventMetrics::EventType::kInertialGestureScrollUpdate:
        has_inertial_input = true;
        max_abs_inertial_raw_delta_pixels =
            std::max(max_abs_inertial_raw_delta_pixels,
                     std::abs(scroll_update->delta()));
        break;
      default:
        NOTREACHED();
    }

    if (!skip_non_damaging_events || scroll_update->did_scroll() ||
        scroll_start_ts) {
      had_any_gesture_scroll = true;
    }
    last_input_generation_ts =
        std::max(last_input_generation_ts, scroll_update->last_timestamp());
  }

  // If the generation timestamp of the scroll END is less than or equal to the
  // generation timestamp of all scroll UPDATES, then we assume that the
  // scroll end belongs to the PREVIOUS scroll (the E?F?U* ordering above). Note
  // that this case also covers the scenario where there were no scroll updates
  // in this frame (i.e. `had_any_gesture_scroll` is false).
  if (scroll_end_ts && *scroll_end_ts <= earliest_event_generation_ts) {
    stages.emplace_back(ScrollJankV4FrameStage::ScrollEnd{});
  }

  if (!had_any_gesture_scroll) {
    return stages;
  }

  bool is_scroll_start = scroll_start_ts.has_value();
  if (is_scroll_start && *scroll_start_ts > earliest_event_generation_ts) {
    TRACE_EVENT("input",
                "CalculateStages: First scroll starts after another "
                "scroll update in a single frame (unexpected)");
  }

  stages.emplace_back(ScrollJankV4FrameStage::ScrollUpdates{
      .is_scroll_start = is_scroll_start,
      .earliest_event =
          base::raw_ref<ScrollUpdateEventMetrics>::from_ptr(earliest_event),
      .last_input_generation_ts = last_input_generation_ts,
      .has_inertial_input = has_inertial_input,
      .total_raw_delta_pixels = total_raw_delta_pixels,
      .max_abs_inertial_raw_delta_pixels = max_abs_inertial_raw_delta_pixels,
  });

  // If the generation timestamp of the scroll END is greater than the
  // generation timestamp of at least one scroll UPDATE, then we assume that the
  // scroll end belongs to the CURRENT scroll (the F?U*E? ordering above).
  if (scroll_end_ts && *scroll_end_ts > earliest_event_generation_ts) {
    if (*scroll_end_ts < last_input_generation_ts) {
      // We deliberately treat the unexpected situation where a scroll end
      // appears in the middle of scroll updates (`earliest_event_generation_ts`
      // < `*scroll_end_ts` < `last_input_generation_ts`) as if the scroll end
      // came AFTER all scroll updates here because the situation was most
      // likely caused by scroll updates from the previous scroll being delayed,
      // so we want to evaluate the current frame against the previous scroll
      // (so that the frame would potentially be marked as janky).
      TRACE_EVENT("input",
                  "CalculateStages: Scroll end between two scroll "
                  "updates in a single frame (unexpected)");
    }
    stages.emplace_back(ScrollJankV4FrameStage::ScrollEnd{});
  }

  return stages;
}

}  // namespace

ScrollJankV4FrameStage::ScrollJankV4FrameStage(
    std::variant<ScrollJankV4FrameStage::ScrollUpdates,
                 ScrollJankV4FrameStage::ScrollEnd> stage)
    : stage(stage) {}

ScrollJankV4FrameStage::ScrollJankV4FrameStage(
    const ScrollJankV4FrameStage& stage) = default;

ScrollJankV4FrameStage::~ScrollJankV4FrameStage() = default;

// static
ScrollJankV4FrameStage::List ScrollJankV4FrameStage::CalculateStages(
    const EventMetrics::List& events_metrics,
    bool skip_non_damaging_events) {
  return CalculateStagesImpl(events_metrics, skip_non_damaging_events);
}

// static
ScrollJankV4FrameStage::List ScrollJankV4FrameStage::CalculateStages(
    const std::vector<ScrollEventMetrics*>& events_metrics,
    bool skip_non_damaging_events) {
  return CalculateStagesImpl(events_metrics, skip_non_damaging_events);
}

}  // namespace cc
