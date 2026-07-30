// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_SCROLL_JANK_V4_FRAME_STAGE_CALCULATOR_H_
#define CC_METRICS_SCROLL_JANK_V4_FRAME_STAGE_CALCULATOR_H_

#include <cstdint>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "cc/cc_export.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/scroll_jank_v4_frame.h"

namespace cc {

class CC_EXPORT ScrollJankV4FrameStageCalculator {
 public:
  // The issues encountered by the scroll-ID-based implementation of
  // `ScrollJankV4FrameStageCalculator` when processing scroll events in a
  // single frame. Only emitted for frames which contain at least one GSU.
  // LINT.IfChange(ScrollIdBasedCalculationIssues)
  enum class ScrollIdBasedCalculationIssues {
    // The frame only contained eligible scroll updates from at most one scroll.
    kNoIssues = 0,
    // The frame contained eligible scroll updates from more than one scroll.
    kOverlappingScrolls = 1,
    // The frame contained scroll updates that the calculator ignored because
    // they arrived after the corresponding scroll has already ended.
    kLateUpdate = 2,
    // The frame contained both overlapping scrolls and late updates.
    kOverlappingScrollsAndLateUpdate = 3,
    kMaxValue = kOverlappingScrollsAndLateUpdate,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/event/enums.xml:FrameStageScrollIdBasedCalculationIssues)

  // Calculates the scroll jank reporting stages based on `events_metrics`
  // associated with a frame.
  //
  // This method takes the scroll ID
  // (`ScrollEventMetrics::scroll_begin_arrival_timestamp()`) into account when
  // calculating the `ScrollJankV4Frame::Stage`s that happened in a single
  // frame.
  //
  // Rationale for using the scroll ID: We want to make sure that, in the rare
  // case when scroll events arrive out of order, the scroll jank v4 metric
  // doesn't emit blatantly incorrect data. Most importantly, if a GSU (gesture
  // scroll update) arrives after a GSE (gesture scroll end) from the same
  // scroll, the metric shouldn't treat the GSU as the beginning of a new
  // scroll.
  //
  // The calculator therefore keeps track of the current / most recent scroll ID
  // and whether it has already encountered GSUs and/or a GSE for that scroll.
  // It filters out events as follows:
  //
  // 1. Once the calculator has encountered a GSE with a particular scroll ID,
  //    it will ignore GSUs/GSEs with the SAME OR LOWER scroll ID in all
  //    subsequent frames.
  // 2. Once the calculator has encountered a GSU with a particular scroll ID,
  //    it will ignore GSUs/GSUs with a LOWER scroll ID in all subsequent
  //    frames.
  //
  // Furthermore, if two scrolls overlap in a single frame, we want the frame to
  // count towards the previous scroll. So if a frame contains GSUs with
  // multiple scroll IDs, the calculator will only take into account the GSUs
  // with the LOWEST scroll ID.
  //
  // The calculator takes extra care to ensure that the sequence of
  // `ScrollJankV4Frame::Stage`s emitted across all frames matches the regular
  // expression `(ScrollStart ScrollUpdate+ ScrollEnd)*`.
  //
  // Sets `ScrollEventMetrics::scroll_jank_v4_result_id()` to `result_id` for
  // all scroll events in `event_metrics`, even for GSUs/GSEs that it ignored
  // based on scroll IDs (see above).
  ScrollJankV4Frame::StageList CalculateStages(
      EventMetrics::List& events_metrics,
      uint64_t result_id);
  ScrollJankV4Frame::StageList CalculateStages(
      std::vector<raw_ptr<ScrollEventMetrics>>& events_metrics,
      uint64_t result_id);

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
      uint64_t result_id);

  template <typename EventMetricsPtr>
  FrameScrollEventBounds CalculateFrameScrollEventBoundsAndSetResultId(
      std::vector<EventMetricsPtr>& events_metrics,
      uint64_t result_id);

  bool IsEligible(const ScrollEventMetrics& scroll_event);

  template <typename EventMetricsPtr>
  static ScrollJankV4Frame::Stage::ScrollUpdates
  CreateScrollUpdatesStageForScrollId(
      const std::vector<EventMetricsPtr>& events_metrics,
      base::TimeTicks scroll_id);

  base::TimeTicks current_scroll_id_ = base::TimeTicks::Min();
  HasSeen has_seen_in_current_scroll_ = HasSeen::kEnd;
};

}  // namespace cc

#endif  // CC_METRICS_SCROLL_JANK_V4_FRAME_STAGE_CALCULATOR_H_
