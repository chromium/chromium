// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_processor.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/time/time.h"
#include "cc/base/features.h"
#include "cc/metrics/event_metrics.h"

namespace cc {

void ScrollJankV4Processor::OnFramePresented(
    ScrollUpdateEventMetrics& earliest_event,
    base::TimeTicks last_input_generation_ts,
    base::TimeTicks presentation_ts,
    base::TimeDelta vsync_interval,
    bool has_inertial_input,
    float abs_total_raw_delta_pixels,
    float max_abs_inertial_raw_delta_pixels) {
  static const bool scroll_jank_v4_metric_enabled =
      base::FeatureList::IsEnabled(features::kScrollJankV4Metric);
  if (!scroll_jank_v4_metric_enabled) {
    return;
  }

  base::TimeTicks first_input_generation_ts =
      earliest_event.GetDispatchStageTimestamp(
          EventMetrics::DispatchStage::kGenerated);
  std::optional<ScrollUpdateEventMetrics::ScrollJankV4Result> result =
      decider_.DecideJankForPresentedFrame(
          first_input_generation_ts, last_input_generation_ts, presentation_ts,
          vsync_interval, has_inertial_input, abs_total_raw_delta_pixels,
          max_abs_inertial_raw_delta_pixels);
  if (!result.has_value()) {
    return;
  }

  histogram_emitter_.OnFramePresented(result->missed_vsyncs_per_reason);
  CHECK(!earliest_event.scroll_jank_v4().has_value());
  earliest_event.set_scroll_jank_v4(std::move(result));
}

void ScrollJankV4Processor::OnScrollStarted() {
  decider_.OnScrollStarted();
  histogram_emitter_.OnScrollStarted();
}

void ScrollJankV4Processor::OnScrollEnded() {
  decider_.OnScrollEnded();
  histogram_emitter_.OnScrollEnded();
}

}  // namespace cc
