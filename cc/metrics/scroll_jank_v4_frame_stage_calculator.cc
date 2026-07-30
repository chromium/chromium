// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_frame_stage_calculator.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <vector>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/scroll_jank_v4_frame.h"

namespace cc {

namespace {


// Implementation of `ScrollJankV4FrameStageCalculator` which takes the scroll
// ID (`ScrollEventMetrics::scroll_begin_arrival_timestamp()`) into account when
// calculating the `ScrollJankV4Frame::Stage`s that happened in a single frame.
//
// Rationale for using the scroll ID: We want to make sure that, in the rare
// case when scroll events arrive out of order, the scroll jank v4 metric
// doesn't emit blatantly incorrect data. Most importantly, if a GSU (gesture
// scroll update) arrives after a GSE (gesture scroll end) from the same scroll,
// the metric shouldn't treat the GSU as the beginning of a new scroll.
//
// The calculator therefore keeps track of the current / most recent scroll ID
// and whether it has already encountered GSUs and/or a GSE for that scroll. It
// filters out events as follows:
//
// 1. Once the calculator has encountered a GSE with a particular scroll ID, it
//    will ignore GSUs/GSEs with the SAME OR LOWER scroll ID in all subsequent
//    frames.
// 2. Once the calculator has encountered a GSU with a particular scroll ID, it
//    will ignore GSUs/GSUs with a LOWER scroll ID in all subsequent frames.
//
// Furthermore, if two scrolls overlap in a single frame, we want the frame to
// count towards the previous scroll. So if a frame contains GSUs with multiple
// scroll IDs, the calculator will only take into account the GSUs with the
// LOWEST scroll ID.
//
// The calculator takes extra care to ensure that the sequence of
// `ScrollJankV4Frame::Stage`s emitted across all frames matches the regular
// expression `(ScrollStart ScrollUpdate+ ScrollEnd)*`.
class ScrollIdBasedCalculator : public ScrollJankV4FrameStageCalculator {
 public:
  ~ScrollIdBasedCalculator() override = default;

  ScrollJankV4Frame::StageList CalculateStages(
      EventMetrics::List& events_metrics,
      uint64_t result_id) override {
    return CalculateStagesBasedOnScrollId(events_metrics, result_id);
  }

  ScrollJankV4Frame::StageList CalculateStages(
      std::vector<raw_ptr<ScrollEventMetrics>>& events_metrics,
      uint64_t result_id) override {
    return CalculateStagesBasedOnScrollId(events_metrics, result_id);
  }

 private:
  // Information about GSUs and GSEs in a single frame.
  //
  // The calculator considers a GSU/GSE in the frame to be "ineligible" and thus
  // ignores it IF:
  //
  //  * the calculator has already encountered a GSU with a GREATER scroll ID
  //    (`ScrollEventMetrics::scroll_begin_arrival_timestamp()`) in an earlier
  //    frame OR
  //  * the calculator has already encountered a GSE with a GREATER OR EQUAL
  //    scroll ID in an earlier frame.
  //
  // Otherwise, the calculator considers a GSU/GSE "eligible".
  struct FrameScrollEventBounds {
    // Whether the frame contains one or more ineligible GSUs.
    bool has_ineligible_updates = false;

    // The range of scroll IDs of eligible GSUs in the frame.
    //
    // Both endpoints are guaranteed to be greater than or equal to
    // `current_scroll_id_`. If `has_seen_in_current_scroll_` is
    // `HasSeen::kEnd`, both endpoints are guaranteed to be strictly greater
    // than `current_scroll_id_`.
    struct Range {
      base::TimeTicks min;
      base::TimeTicks max;
    };
    std::optional<Range> eligible_updates_scroll_id_range = std::nullopt;

    // The maximum scroll ID of eligible GSEs in the frame.
    //
    // Guaranteed to be greater than or equal to `current_scroll_id_`. If
    // `has_seen_in_current_scroll_` is `HasSeen::kEnd`, it's guaranteed to be
    // strictly greater than `current_scroll_id_`. Can be less than, greater
    // than, or overlap with `eligible_updates_scroll_id_range`.
    std::optional<base::TimeTicks> eligible_end_max_scroll_id = std::nullopt;
  };

  // What the calculator has seen for the current / most recent scroll.
  // LINT.IfChange(HasSeen)
  enum class HasSeen {
    // The calculator hasn't seen any GSUs or GSE for the current scroll yet.
    //
    // It hasn't emitted a `ScrollJankV4Frame::Stage::ScrollStart` yet.
    //
    // The calculator will ignore any GSUs/GSEs with a scroll ID LOWER than
    // `current_scroll_id_`.
    kNoUpdates,
    // The calculator has seen one or more GSUs for the current scroll (but no
    // GSE).
    //
    // It has emitted one `ScrollJankV4Frame::Stage::ScrollStart` and one or
    // more `ScrollJankV4Frame::Stage::ScrollUpdates`.
    //
    // The calculator will ignore any GSUs/GSEs with a scroll ID LOWER than
    // `current_scroll_id_`.
    kOneOrMoreUpdates,
    // The calculator has seen a GSE for the most recent scroll.
    //
    // Either the calculator hasn't seen any scrolls yet, or it has emitted one
    // `ScrollJankV4Frame::Stage::ScrollStart`, one or more
    // `ScrollJankV4Frame::Stage::ScrollUpdates` and
    // `ScrollJankV4Frame::Stage::ScrollEnd`.
    //
    // The calculator will ignore any GSUs/GSEs with a scroll ID LOWER OR EQUAL
    // to `current_scroll_id_`.
    kEnd
  };
  // LINT.ThenChange(//base/tracing/protos/chrome_track_event.proto:ScrollJankV4FrameStageCalculationHasSeen)

  template <typename EventMetricsPtr>
  ScrollJankV4Frame::StageList CalculateStagesBasedOnScrollId(
      std::vector<EventMetricsPtr>& events_metrics,
      uint64_t result_id) {
    TRACE_EVENT(
        "input.scrolling",
        "Processing ScrollJankV4Frame stages (based on scroll IDs)",
        [&](perfetto::EventContext context) {
          auto* scroll_jank_v4 =
              context.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                  ->set_scroll_jank_v4();
          scroll_jank_v4->set_result_id(result_id);
          auto* frame_stage_calculation =
              scroll_jank_v4->set_frame_stage_calculation();
          frame_stage_calculation->set_current_scroll_begin_arrival_us(
              current_scroll_id_.since_origin().InMicroseconds());
          // The perfetto `HasSeen` proto enum values are incremented by 1 to
          // leave 0 for the `UNKNOWN` value.
          frame_stage_calculation->set_has_seen_in_current_scroll(
              static_cast<
                  perfetto::protos::pbzero::EventLatency::ScrollJankV4Result::
                      FrameStageCalculation::HasSeen>(
                  static_cast<int>(has_seen_in_current_scroll_) + 1));
        });

    ScrollJankV4Frame::StageList stages;

    auto [has_ineligible_updates, eligible_update_scroll_id_range,
          eligible_end_max_scroll_id] =
        CalculateFrameScrollEventBoundsAndSetResultId(events_metrics,
                                                      result_id);

    // If `event_metrics` contains at least one GSU (eligible or ineligible),
    // emit the "Event.ScrollJank.FrameStageScrollIdBasedCalculationIssues" UMA
    // histogram with 1% probability.
    if ((has_ineligible_updates ||
         eligible_update_scroll_id_range.has_value()) &&
        base::ShouldRecordSubsampledMetric(0.01)) {
      UMA_HISTOGRAM_ENUMERATION(
          "Event.ScrollJank.FrameStageScrollIdBasedCalculationIssues", [&] {
            bool overlapping_scrolls =
                eligible_update_scroll_id_range.has_value() &&
                eligible_update_scroll_id_range->max >
                    eligible_update_scroll_id_range->min;
            if (overlapping_scrolls) {
              return has_ineligible_updates
                         ? ScrollIdBasedCalculationIssues::
                               kOverlappingScrollsAndLateUpdate
                         : ScrollIdBasedCalculationIssues::kOverlappingScrolls;
            }
            return has_ineligible_updates
                       ? ScrollIdBasedCalculationIssues::kLateUpdate
                       : ScrollIdBasedCalculationIssues::kNoIssues;
          }());
    }

    if (has_ineligible_updates) {
      TRACE_EVENT("input",
                  "CalculateStagesBasedOnScrollId: Ignoring GSUs from already "
                  "ended scrolls");
    }

    // There are many possible relationships between `current_scroll_id_`
    // (CS), `eligible_update_scroll_id_range` (GSU-min, GSU-max) and
    // `eligible_end_max_scroll_id` (GSE-max):
    //
    // 1.  Frame with no GSUs or GSEs:
    //     CS
    // 2.  Frame with GSUs which continue an existing scroll:
    //     CS = GSU-min = GSU-max
    // 3.  Frame with GSUs which continue an existing scroll and a GSE which
    //     then ends that scroll:
    //     CS = GSU-min = GSU-max <= GSE-max
    // 4.  Frame with GSUs both from the existing and a new scroll:
    //     CS = GSU-min < GSU-max
    // 5.  Frame with GSUs both from the existing and a new scroll as well as a
    //     GSE which ends that scroll:
    //     CS = GSU-min < GSU-max <= GSE-max
    // 6.  Frame with GSUs from a new scroll:
    //     CS < GSU-min = GSU-max
    // 7.  Frame with GSUs from a new scroll and a GSE which immediately ends
    //     that scroll:
    //     CS < GSU-min = GSU-max <= GSE-max
    // 8.  Frame with GSUs from more than one new scroll, the last scroll hasn't
    //     ended yet:
    //     CS < GSU-min < GSU-max
    // 9.  Frame with GSUs from more than one new scroll and a GSE which ends
    //     the last one:
    //     CS < GSU-min < GSU-max <= GSE-max
    // 10. Frame with a GSU which ends an existing scroll.
    //     CS <= GSE-max
    //
    // Note: We can ignore scenarios where GSE-max < GSU-max because the
    // calculator will interpret GSU-max as "any scroll with a lower scroll ID
    // has already ended" and emit any necessary
    // `ScrollJankV4Frame::Stage::ScrollEnd` /
    // `ScrollJankV4Frame::Stage::ScrollStart` events.

    // Are there any eligible GSUs in the frame (scenarios 2-9)?
    if (eligible_update_scroll_id_range.has_value()) {
      if (eligible_update_scroll_id_range->max >
          eligible_update_scroll_id_range->min) {
        // Scenarios 4, 5, 8, 9.
        TRACE_EVENT("input",
                    "CalculateStagesBasedOnScrollId: Multiple scrolls overlap "
                    "in a single frame");
      }
      // If so, the calculator might first need to emit a
      // `ScrollJankV4Frame::Stage::ScrollEnd` and/or
      // `ScrollJankV4Frame::Stage::ScrollStart`, depending on whether the GSUs
      // continue an existing scroll or start a new one.
      if (eligible_update_scroll_id_range->min > current_scroll_id_) {
        // Scenarios 6-9.
        if (has_seen_in_current_scroll_ == HasSeen::kOneOrMoreUpdates) {
          stages.emplace_back(ScrollJankV4Frame::Stage::ScrollEnd{});
        }
        stages.emplace_back(ScrollJankV4Frame::Stage::ScrollStart{});
      } else {
        // Scenarios 2-5.
        DCHECK_EQ(eligible_update_scroll_id_range->min, current_scroll_id_);
        DCHECK_NE(has_seen_in_current_scroll_, HasSeen::kEnd);
        if (has_seen_in_current_scroll_ == HasSeen::kNoUpdates) {
          stages.emplace_back(ScrollJankV4Frame::Stage::ScrollStart{});
        }
      }

      // The calculator then emits a single
      // `ScrollJankV4Frame::Stage::ScrollUpdates`.
      stages.emplace_back(CreateScrollUpdatesStageForScrollId(
          events_metrics, eligible_update_scroll_id_range->min));
    }

    // Should we end the current scroll for any reason (either because of a GSE
    // or because the frame contained GSUs with multiple scroll IDs)?
    if (eligible_end_max_scroll_id.has_value() &&
        !eligible_update_scroll_id_range.has_value()) {
      // Yes, because the frame contained an eligible GSE and no eligible GSUs
      // (scenario 10).
      if (has_seen_in_current_scroll_ == HasSeen::kOneOrMoreUpdates) {
        stages.emplace_back(ScrollJankV4Frame::Stage::ScrollEnd{});
      }
      current_scroll_id_ = *eligible_end_max_scroll_id;
      has_seen_in_current_scroll_ = HasSeen::kEnd;
    } else if (eligible_end_max_scroll_id.has_value() &&
               *eligible_end_max_scroll_id >=
                   eligible_update_scroll_id_range->max) {
      // Yes, because the frame contained an eligible GSE whose scroll ID was
      // greater than or equal to that of all eligible GSUs (scenarios 3, 5, 7,
      // 9).
      stages.emplace_back(ScrollJankV4Frame::Stage::ScrollEnd{});
      current_scroll_id_ = *eligible_end_max_scroll_id;
      has_seen_in_current_scroll_ = HasSeen::kEnd;
    } else if (eligible_update_scroll_id_range.has_value()) {
      if (eligible_update_scroll_id_range->max >
          eligible_update_scroll_id_range->min) {
        // Yes, because the frame contained eligible GSUs with multiple scroll
        // IDs (scenarios 4, 8).
        stages.emplace_back(ScrollJankV4Frame::Stage::ScrollEnd{});
        current_scroll_id_ = eligible_update_scroll_id_range->max;
        has_seen_in_current_scroll_ = HasSeen::kNoUpdates;
      } else {
        // No, all eligible GSUs in the frame had the same scroll ID and there
        // were no GSEs with a scroll ID that's greater or equal (scenarios 2,
        // 6).
        DCHECK_EQ(eligible_update_scroll_id_range->max,
                  eligible_update_scroll_id_range->min);
        current_scroll_id_ = eligible_update_scroll_id_range->min;
        has_seen_in_current_scroll_ = HasSeen::kOneOrMoreUpdates;
      }
    }

    return stages;
  }

  template <typename EventMetricsPtr>
  FrameScrollEventBounds CalculateFrameScrollEventBoundsAndSetResultId(
      std::vector<EventMetricsPtr>& events_metrics,
      uint64_t result_id) {
    bool has_ineligible_updates = false;
    std::optional<FrameScrollEventBounds::Range>
        eligible_update_scroll_id_range = std::nullopt;
    std::optional<base::TimeTicks> eligible_end_max_scroll_id = std::nullopt;

    for (auto& event : events_metrics) {
      auto* scroll_event = event->AsScroll();
      if (!scroll_event) {
        continue;
      }
      DCHECK(!scroll_event->scroll_jank_v4_result_id().has_value());
      scroll_event->set_scroll_jank_v4_result_id(result_id);
      if (!IsEligible(*scroll_event)) {
        if (scroll_event->AsScrollUpdate()) {
          has_ineligible_updates = true;
        }
        continue;
      }
      base::TimeTicks scroll_id =
          scroll_event->scroll_begin_arrival_timestamp();
      EventMetrics::EventType event_type = scroll_event->type();
      if (event_type == EventMetrics::EventType::kGestureScrollEnd ||
          event_type == EventMetrics::EventType::kInertialGestureScrollEnd) {
        if (!eligible_end_max_scroll_id.has_value() ||
            scroll_id > *eligible_end_max_scroll_id) {
          eligible_end_max_scroll_id = scroll_id;
        }
      } else if (scroll_event->AsScrollUpdate()) {
        if (!eligible_update_scroll_id_range.has_value()) {
          eligible_update_scroll_id_range = {.min = scroll_id,
                                             .max = scroll_id};
        } else if (scroll_id < eligible_update_scroll_id_range->min) {
          eligible_update_scroll_id_range->min = scroll_id;
        } else if (scroll_id > eligible_update_scroll_id_range->max) {
          eligible_update_scroll_id_range->max = scroll_id;
        }
        DCHECK_LE(eligible_update_scroll_id_range->min,
                  eligible_update_scroll_id_range->max);
      }
    }
    return {
        .has_ineligible_updates = has_ineligible_updates,
        .eligible_updates_scroll_id_range = eligible_update_scroll_id_range,
        .eligible_end_max_scroll_id = eligible_end_max_scroll_id,
    };
  }

  bool IsEligible(const ScrollEventMetrics& scroll_event) {
    if (has_seen_in_current_scroll_ == HasSeen::kEnd) {
      return scroll_event.scroll_begin_arrival_timestamp() > current_scroll_id_;
    }
    return scroll_event.scroll_begin_arrival_timestamp() >= current_scroll_id_;
  }

  template <typename EventMetricsPtr>
  static ScrollJankV4Frame::Stage::ScrollUpdates
  CreateScrollUpdatesStageForScrollId(
      const std::vector<EventMetricsPtr>& events_metrics,
      base::TimeTicks scroll_id) {
    // Real scroll updates.
    bool had_real_input = false;
    base::TimeTicks first_real_input_generation_ts = base::TimeTicks::Max();
    base::TimeTicks last_real_input_generation_ts = base::TimeTicks::Min();
    bool has_real_inertial_input = false;
    float total_real_raw_delta_pixels = 0;
    float max_abs_real_inertial_raw_delta_pixels = 0;
    std::optional<EventMetrics::TraceId> first_real_input_trace_id =
        std::nullopt;

    // Synthetic scroll updates.
    bool had_synthetic_input = false;
    bool has_synthetic_inertial_input = false;
    base::TimeTicks first_synthetic_input_begin_frame_ts =
        base::TimeTicks::Max();
    std::optional<EventMetrics::TraceId> first_synthetic_input_trace_id =
        std::nullopt;

    for (auto& event : events_metrics) {
      auto* scroll_update = event->AsScrollUpdate();
      if (!scroll_update ||
          scroll_update->scroll_begin_arrival_timestamp() != scroll_id) {
        continue;
      }

      bool is_synthetic = scroll_update->is_synthetic();
      bool is_inertial = event->type() ==
                         EventMetrics::EventType::kInertialGestureScrollUpdate;
      if (is_synthetic) {
        had_synthetic_input = true;
        base::TimeTicks begin_frame_ts =
            scroll_update->dispatch_args().frame_time;
        if (begin_frame_ts < first_synthetic_input_begin_frame_ts) {
          first_synthetic_input_begin_frame_ts = begin_frame_ts;
          first_synthetic_input_trace_id = scroll_update->trace_id();
        }
        if (is_inertial) {
          has_synthetic_inertial_input = true;
        }
      } else {
        had_real_input = true;
        last_real_input_generation_ts = std::max(
            last_real_input_generation_ts, scroll_update->last_timestamp());
        total_real_raw_delta_pixels += scroll_update->delta();
        base::TimeTicks generation_ts = event->GetDispatchStageTimestamp(
            EventMetrics::DispatchStage::kGenerated);
        if (generation_ts < first_real_input_generation_ts) {
          first_real_input_generation_ts = generation_ts;
          first_real_input_trace_id = scroll_update->trace_id();
        }
        if (is_inertial) {
          has_real_inertial_input = true;
          max_abs_real_inertial_raw_delta_pixels =
              std::max(max_abs_real_inertial_raw_delta_pixels,
                       std::abs(scroll_update->delta()));
        }
      }
    }

    CHECK(had_real_input || had_synthetic_input);
    std::optional<ScrollJankV4Frame::Stage::ScrollUpdates::Real> real_updates =
        had_real_input
            ? std::make_optional(ScrollJankV4Frame::Stage::ScrollUpdates::Real{
                  .first_input_generation_ts = first_real_input_generation_ts,
                  .last_input_generation_ts = last_real_input_generation_ts,
                  .has_inertial_input = has_real_inertial_input,
                  .total_raw_delta_pixels = total_real_raw_delta_pixels,
                  .max_abs_inertial_raw_delta_pixels =
                      max_abs_real_inertial_raw_delta_pixels,
                  .first_input_trace_id = first_real_input_trace_id,
              })
            : std::nullopt;
    std::optional<ScrollJankV4Frame::Stage::ScrollUpdates::Synthetic>
        synthetic_updates =
            had_synthetic_input
                ? std::make_optional(
                      ScrollJankV4Frame::Stage::ScrollUpdates::Synthetic{
                          .first_input_begin_frame_ts =
                              first_synthetic_input_begin_frame_ts,
                          .has_inertial_input = has_synthetic_inertial_input,
                          .first_input_trace_id =
                              first_synthetic_input_trace_id,
                      })
                : std::nullopt;
    return ScrollJankV4Frame::Stage::ScrollUpdates(
        real_updates, synthetic_updates, scroll_id);
  }

  base::TimeTicks current_scroll_id_ = base::TimeTicks::Min();
  HasSeen has_seen_in_current_scroll_ = HasSeen::kEnd;
};

}  // namespace

// static
std::unique_ptr<ScrollJankV4FrameStageCalculator>
ScrollJankV4FrameStageCalculator::Create() {
  // TODO: b/496151166 - Merge ScrollIdBasedCalculator into
  // ScrollJankV4FrameStageCalculator.
  return std::make_unique<ScrollIdBasedCalculator>();
}

}  // namespace cc
