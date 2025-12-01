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

#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "cc/metrics/event_metrics.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace cc {

namespace {

template <typename EventMetricsPtr>
ScrollJankV4FrameStage::List CalculateStagesImpl(
    const std::vector<EventMetricsPtr>& events_metrics,
    bool skip_non_damaging_events) {
  ScrollJankV4FrameStage::List stages;

  // Any scroll updates (real or synthetic).
  ScrollUpdateEventMetrics* earliest_event = nullptr;
  // This handles cases when we have multiple scroll events. Events for dropped
  // frames are reported by the reporter for next presented frame which could
  // lead to having multiple scroll events.
  base::TimeTicks first_input_generation_ts = base::TimeTicks::Max();
  base::TimeTicks last_input_generation_ts = base::TimeTicks::Min();

  // Real scroll updates.
  bool had_real_input = false;
  base::TimeTicks first_real_input_generation_ts = base::TimeTicks::Max();
  base::TimeTicks last_real_input_generation_ts = base::TimeTicks::Min();
  bool has_real_inertial_input = false;
  float total_real_raw_delta_pixels = 0;
  float max_abs_real_inertial_raw_delta_pixels = 0;
  std::optional<EventMetrics::TraceId> first_real_input_trace_id = std::nullopt;

  // Synthetic scroll updates.
  bool had_synthetic_input = false;
  base::TimeTicks first_synthetic_input_begin_frame_ts = base::TimeTicks::Max();
  std::optional<EventMetrics::TraceId> first_synthetic_input_trace_id =
      std::nullopt;

  // Scroll start and end.
  std::optional<base::TimeTicks> scroll_start_ts = std::nullopt;
  std::optional<base::TimeTicks> scroll_end_ts = std::nullopt;

  using FrameStageCalculationResult =
      ScrollJankV4FrameStage::FrameStageCalculationResult;
  std::optional<FrameStageCalculationResult> issue;
  auto add_issue = [&](FrameStageCalculationResult new_issue) {
    if (!issue.has_value() || *issue == new_issue) {
      // If this is a new or a recurring issue, use it.
      issue = new_issue;
    } else {
      // Otherwise, combine different issues into `kMultipleIssues`.
      issue = FrameStageCalculationResult::kMultipleIssues;
    }
  };
  absl::Cleanup maybe_report_result = [&] {
    // Only report for 1% of frames that contain at least one scroll update or
    // scroll end.
    if (stages.empty() || !base::ShouldRecordSubsampledMetric(0.01)) {
      return;
    }
    FrameStageCalculationResult result = [&] {
      if (issue.has_value()) {
        // If there was an issue, report it.
        return *issue;
      }
      // If there were no issues, report the ordering of scroll start, scroll
      // updates and/or scroll end.
      using enum FrameStageCalculationResult;
      return std::visit(
          // Note: We know, by construction, that `stages` contains each stage
          // type (start/updates/end) at most once.
          absl::Overload{
              [&stages](const ScrollJankV4FrameStage::ScrollStart& start) {
                // It's not possible to have a start without updates because
                // both originate from
                // `EventMetrics::EventType::kFirstGestureScrollUpdate`.
                return stages.size() == 2 ? kScrollStartThenUpdates
                                          : kScrollStartThenUpdatesThenEnd;
              },
              [&stages](const ScrollJankV4FrameStage::ScrollUpdates& updates) {
                // We know that `stages` doesn't contain any start. Otherwise,
                // we'd have reported the `kScrollStartAfterUpdate` issue.
                return stages.size() == 1 ? kScrollUpdatesOnly
                                          : kScrollUpdatesThenEnd;
              },
              [&stages](const ScrollJankV4FrameStage::ScrollEnd& end) {
                if (stages.size() == 1) {
                  return kScrollEndOnly;
                }
                // We know that `stages` contains [end, start, updates] in that
                // order. Otherwise, we'd have reported one of the
                // `kScrollEndThenUpdatesWithoutStart` or
                // `kScrollStartAfterUpdate` issues.
                return kScrollEndThenStartThenUpdates;
              }},
          stages[0].stage);
    }();
    UMA_HISTOGRAM_ENUMERATION("Event.ScrollJank.FrameStageCalculationResult",
                              result);
  };

  // We expect that `events_metrics` contains:
  //   E. Zero or one scroll ends (`kGestureScrollEnd` or
  //      `kInertialGestureScrollEnd`).
  //   F. Zero or one first scroll updates (`kFirstGestureScrollUpdate`).
  //   U. Zero or more continuing scroll updates (`kGestureScrollUpdate` or
  //      `kInertialGestureScrollUpdate`s).
  // Furthermore, we expect that:
  //   * If there's a scroll end (E), it comes:
  //       * either before all scroll updates (F/U), in which case we assume
  //         that it ends the previous scroll,
  //       * or after all scroll updates (F/U), in which case we assume that
  //         it ends the current scroll.
  //   * If there's a first scroll update (F), it precedes all continuing
  //   scroll
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
        add_issue(FrameStageCalculationResult::kMultipleScrollEnds);
        TRACE_EVENT("input",
                    "CalculateStages: Multiple scroll ends in a frame");
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

    // Earliest is always applied, even when the scroll update failed to
    // successfully produce a scroll.
    if (generation_ts < first_input_generation_ts) {
      first_input_generation_ts = generation_ts;
      earliest_event = scroll_update;
    }
    bool is_synthetic = scroll_update->is_synthetic();
    if (is_synthetic) {
      base::TimeTicks begin_frame_ts =
          scroll_update->begin_frame_args().frame_time;
      if (begin_frame_ts < first_synthetic_input_begin_frame_ts) {
        first_synthetic_input_begin_frame_ts = begin_frame_ts;
        first_synthetic_input_trace_id = scroll_update->trace_id();
      }
    } else {
      total_real_raw_delta_pixels += scroll_update->delta();
      if (generation_ts < first_real_input_generation_ts) {
        first_real_input_generation_ts = generation_ts;
        first_real_input_trace_id = scroll_update->trace_id();
      }
    }

    // We check the type first, as if `is_scroll_start` is true, we need to
    // include `scroll_update` even if `scroll_update->did_scroll()` is false.
    switch (event_type) {
      case EventMetrics::EventType::kFirstGestureScrollUpdate:
        if (scroll_start_ts) {
          add_issue(FrameStageCalculationResult::kMultipleScrollStarts);
          TRACE_EVENT("input",
                      "CalculateStages: Multiple scroll starts in a "
                      "single frame (unexpected)");
          scroll_start_ts = std::min(generation_ts, *scroll_start_ts);
          break;
        }
        scroll_start_ts = generation_ts;
        break;
      case EventMetrics::EventType::kGestureScrollUpdate:
        break;
      case EventMetrics::EventType::kInertialGestureScrollUpdate:
        DCHECK(!is_synthetic);
        has_real_inertial_input = true;
        max_abs_real_inertial_raw_delta_pixels =
            std::max(max_abs_real_inertial_raw_delta_pixels,
                     std::abs(scroll_update->delta()));
        break;
      default:
        NOTREACHED();
    }

    if (!skip_non_damaging_events || scroll_update->did_scroll() ||
        scroll_start_ts) {
      if (is_synthetic) {
        had_synthetic_input = true;
      } else {
        had_real_input = true;
      }
    }
    last_input_generation_ts =
        std::max(last_input_generation_ts, scroll_update->last_timestamp());
    if (!is_synthetic) {
      last_real_input_generation_ts = std::max(last_real_input_generation_ts,
                                               scroll_update->last_timestamp());
    }
  }

  bool is_scroll_start = scroll_start_ts.has_value();
  bool had_gesture_scroll = had_real_input || had_synthetic_input;

  // If the generation timestamp of the scroll END is less than or equal to the
  // generation timestamp of all scroll UPDATES, then we assume that the scroll
  // end belongs to the PREVIOUS scroll (the E?F?U* ordering above). Note that
  // this case also covers the scenario where there were no scroll updates in
  // this frame (i.e. `had_gesture_scroll` is false).
  if (scroll_end_ts && *scroll_end_ts <= first_input_generation_ts) {
    if (had_gesture_scroll && !is_scroll_start) {
      add_issue(FrameStageCalculationResult::kScrollEndThenUpdatesWithoutStart);
      TRACE_EVENT("input",
                  "CalculateStages: Scroll end followed by scroll updates "
                  "without a scroll start (unexpected)");
    }
    stages.emplace_back(ScrollJankV4FrameStage::ScrollEnd{});
  }

  if (is_scroll_start) {
    if (*scroll_start_ts > first_input_generation_ts) {
      add_issue(FrameStageCalculationResult::kScrollStartAfterUpdate);
      TRACE_EVENT("input",
                  "CalculateStages: First scroll starts after another "
                  "scroll update in a single frame (unexpected)");
    }
    stages.emplace_back(ScrollJankV4FrameStage::ScrollStart{});
  }

  if (!had_gesture_scroll) {
    return stages;
  }

  std::optional<ScrollJankV4FrameStage::ScrollUpdates::Real> real_updates =
      had_real_input
          ? std::make_optional(ScrollJankV4FrameStage::ScrollUpdates::Real{
                .first_input_generation_ts = first_real_input_generation_ts,
                .last_input_generation_ts = last_real_input_generation_ts,
                .has_inertial_input = has_real_inertial_input,
                .abs_total_raw_delta_pixels =
                    std::abs(total_real_raw_delta_pixels),
                .max_abs_inertial_raw_delta_pixels =
                    max_abs_real_inertial_raw_delta_pixels,
                .first_input_trace_id = first_real_input_trace_id,
            })
          : std::nullopt;
  std::optional<ScrollJankV4FrameStage::ScrollUpdates::Synthetic>
      synthetic_updates =
          had_synthetic_input
              ? std::make_optional(
                    ScrollJankV4FrameStage::ScrollUpdates::Synthetic{
                        .first_input_begin_frame_ts =
                            first_synthetic_input_begin_frame_ts,
                        .first_input_trace_id = first_synthetic_input_trace_id,
                    })
              : std::nullopt;

  stages.emplace_back(ScrollJankV4FrameStage::ScrollUpdates(
      earliest_event, real_updates, synthetic_updates));

  // If the generation timestamp of the scroll END is greater than the
  // generation timestamp of at least one scroll UPDATE, then we assume that the
  // scroll end belongs to the CURRENT scroll (the F?U*E? ordering above).
  if (scroll_end_ts && *scroll_end_ts > first_input_generation_ts) {
    if (*scroll_end_ts < last_input_generation_ts) {
      // We deliberately treat the unexpected situation where a scroll end
      // appears in the middle of scroll updates (`first_input_generation_ts`
      // < `*scroll_end_ts` < `last_input_generation_ts`) as if the scroll end
      // came AFTER all scroll updates here because the situation was most
      // likely caused by scroll updates from the previous scroll being delayed,
      // so we want to evaluate the current frame against the previous scroll
      // (so that the frame would potentially be marked as janky).
      add_issue(FrameStageCalculationResult::kScrollEndBetweenUpdates);
      TRACE_EVENT("input",
                  "CalculateStages: Scroll end between two scroll "
                  "updates in a single frame (unexpected)");
    }
    stages.emplace_back(ScrollJankV4FrameStage::ScrollEnd{});
  }

  return stages;
}

}  // namespace

ScrollJankV4FrameStage::ScrollUpdates::ScrollUpdates(
    ScrollUpdateEventMetrics* earliest_event,
    std::optional<Real> real,
    std::optional<Synthetic> synthetic)
    : earliest_event_(earliest_event),
      real_(std::move(real)),
      synthetic_(std::move(synthetic)) {
  CHECK(real_.has_value() || synthetic_.has_value());
}

ScrollJankV4FrameStage::ScrollJankV4FrameStage(
    std::variant<ScrollJankV4FrameStage::ScrollStart,
                 ScrollJankV4FrameStage::ScrollUpdates,
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
