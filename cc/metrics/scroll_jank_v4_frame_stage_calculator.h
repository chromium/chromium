// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_SCROLL_JANK_V4_FRAME_STAGE_CALCULATOR_H_
#define CC_METRICS_SCROLL_JANK_V4_FRAME_STAGE_CALCULATOR_H_

#include <cstdint>
#include <memory>
#include <vector>

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

  static std::unique_ptr<ScrollJankV4FrameStageCalculator> Create();

  virtual ~ScrollJankV4FrameStageCalculator() = default;

  // Calculates the scroll jank reporting stages based on `events_metrics`
  // associated with a frame.
  //
  // Sets `ScrollEventMetrics::scroll_jank_v4_result_id()` to `result_id` for
  // all scroll events in `event_metrics`.
  virtual ScrollJankV4Frame::StageList CalculateStages(
      EventMetrics::List& events_metrics,
      uint64_t result_id) = 0;
  virtual ScrollJankV4Frame::StageList CalculateStages(
      std::vector<ScrollEventMetrics*>& events_metrics,
      uint64_t result_id) = 0;

 protected:
  ScrollJankV4FrameStageCalculator() = default;
};

}  // namespace cc

#endif  // CC_METRICS_SCROLL_JANK_V4_FRAME_STAGE_CALCULATOR_H_
