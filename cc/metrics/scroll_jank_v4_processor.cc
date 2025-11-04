// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_processor.h"

#include <optional>
#include <utility>
#include <variant>

#include "base/check.h"
#include "base/time/time.h"
#include "cc/base/features.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/scroll_jank_v4_frame.h"
#include "cc/metrics/scroll_jank_v4_frame_stage.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace cc {

namespace {

using ScrollDamage = ScrollJankV4Frame::ScrollDamage;
using DamagingFrame = ScrollJankV4Frame::DamagingFrame;

}  // namespace

void ScrollJankV4Processor::ProcessEventsMetricsForPresentedFrame(
    EventMetrics::List& events_metrics,
    base::TimeTicks presentation_ts,
    const viz::BeginFrameArgs& args) {
  static const bool scroll_jank_v4_metric_enabled =
      base::FeatureList::IsEnabled(features::kScrollJankV4Metric);
  if (!scroll_jank_v4_metric_enabled) {
    return;
  }

  if (!base::FeatureList::IsEnabled(
          features::kHandleNonDamagingInputsInScrollJankV4Metric)) {
    // Ignore non-damaging events (legacy behavior).
    ScrollJankV4FrameStage::List stages =
        ScrollJankV4FrameStage::CalculateStages(
            events_metrics, /* skip_non_damaging_events= */ true);
    HandleFrame(stages, DamagingFrame(presentation_ts), args,
                /* counts_towards_histogram_frame_count= */ true);
    return;
  }

  ScrollJankV4Frame::Timeline timeline = ScrollJankV4Frame::CalculateTimeline(
      events_metrics, args, presentation_ts);
  bool count_non_damaging_frames_towards_histogram_frame_count =
      features::kCountNonDamagingFramesTowardsHistogramFrameCount.Get();
  for (auto& frame : timeline) {
    bool counts_towards_histogram_frame_count =
        count_non_damaging_frames_towards_histogram_frame_count ||
        std::holds_alternative<DamagingFrame>(frame.damage);
    HandleFrame(frame.stages, frame.damage, *frame.args,
                counts_towards_histogram_frame_count);
  }
}

void ScrollJankV4Processor::HandleFrame(
    ScrollJankV4FrameStage::List& stages,
    const ScrollDamage& damage,
    const viz::BeginFrameArgs& args,
    bool counts_towards_histogram_frame_count) {
  for (ScrollJankV4FrameStage& stage : stages) {
    std::visit(absl::Overload{
                   [&](ScrollJankV4FrameStage::ScrollUpdates& updates) {
                     if (updates.is_scroll_start) {
                       HandleScrollStarted();
                     }
                     HandleFrameWithScrollUpdates(
                         updates, damage, args,
                         counts_towards_histogram_frame_count);
                   },
                   [&](ScrollJankV4FrameStage::ScrollEnd& end) {
                     HandleScrollEnded();
                   },
               },
               stage.stage);
  }
}

void ScrollJankV4Processor::HandleFrameWithScrollUpdates(
    ScrollJankV4FrameStage::ScrollUpdates& updates,
    const ScrollDamage& damage,
    const viz::BeginFrameArgs& args,
    bool counts_towards_histogram_frame_count) {
  ScrollUpdateEventMetrics& earliest_event = *updates.earliest_event;
  base::TimeTicks first_input_generation_ts =
      earliest_event.GetDispatchStageTimestamp(
          EventMetrics::DispatchStage::kGenerated);
  std::optional<ScrollUpdateEventMetrics::ScrollJankV4Result> result =
      decider_.DecideJankForFrameWithScrollUpdates(
          first_input_generation_ts, updates.last_input_generation_ts, damage,
          args, updates.has_inertial_input,
          std::abs(updates.total_raw_delta_pixels),
          updates.max_abs_inertial_raw_delta_pixels);
  if (!result.has_value()) {
    return;
  }

  histogram_emitter_.OnFrameWithScrollUpdates(
      result->missed_vsyncs_per_reason, counts_towards_histogram_frame_count);

  CHECK(!earliest_event.scroll_jank_v4().has_value());
  earliest_event.set_scroll_jank_v4(std::move(result));
}

void ScrollJankV4Processor::HandleScrollStarted() {
  decider_.OnScrollStarted();
  histogram_emitter_.OnScrollStarted();
}

void ScrollJankV4Processor::HandleScrollEnded() {
  decider_.OnScrollEnded();
  histogram_emitter_.OnScrollEnded();
}

}  // namespace cc
