// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_processor.h"

#include <memory>
#include <variant>

#include "base/trace_event/trace_event.h"
#include "cc/base/features.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/scroll_jank_v4_decision_queue.h"
#include "cc/metrics/scroll_jank_v4_frame.h"
#include "cc/metrics/scroll_jank_v4_frame_stage.h"
#include "cc/metrics/scroll_jank_v4_histogram_emitter.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace cc {

namespace {

class ProcessorResultConsumer
    : public ScrollJankV4DecisionQueue::ResultConsumer {
 public:
  void OnFrameResult(ScrollUpdateEventMetrics::ScrollJankV4Result result,
                     ScrollUpdateEventMetrics* earliest_event) override {
    bool counts_towards_histogram_frame_count =
        result.is_damaging_frame ||
        features::kCountNonDamagingFramesTowardsHistogramFrameCount.Get();
    histogram_emitter_.OnFrameWithScrollUpdates(
        result.missed_vsyncs_per_reason, counts_towards_histogram_frame_count);
    if (earliest_event) {
      CHECK(!earliest_event->scroll_jank_v4().has_value());
      earliest_event->set_scroll_jank_v4(result);
    }
  }

  void OnScrollStarted() override { histogram_emitter_.OnScrollStarted(); }

  void OnScrollEnded() override { histogram_emitter_.OnScrollEnded(); }

 private:
  ScrollJankV4HistogramEmitter histogram_emitter_;
};

}  // namespace

ScrollJankV4Processor::ScrollJankV4Processor()
    : decision_queue_(std::make_unique<ProcessorResultConsumer>()) {}

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
    HandleFrame(stages, ScrollJankV4Frame::DamagingFrame(presentation_ts),
                ScrollJankV4Frame::BeginFrameArgsForScrollJank::From(args));
    return;
  }

  ScrollJankV4Frame::Timeline timeline = ScrollJankV4Frame::CalculateTimeline(
      events_metrics, args, presentation_ts);
  for (auto& frame : timeline) {
    HandleFrame(frame.stages, frame.damage, frame.args);
  }
}

void ScrollJankV4Processor::HandleFrame(
    ScrollJankV4FrameStage::List& stages,
    const ScrollJankV4Frame::ScrollDamage& damage,
    const ScrollJankV4Frame::BeginFrameArgsForScrollJank& args) {
  for (ScrollJankV4FrameStage& stage : stages) {
    std::visit(absl::Overload{
                   [&](ScrollJankV4FrameStage::ScrollStart& end) {
                     decision_queue_.OnScrollStarted();
                   },
                   [&](ScrollJankV4FrameStage::ScrollUpdates& updates) {
                     if (!decision_queue_.ProcessFrameWithScrollUpdates(
                             updates, damage, args)) {
                       TRACE_EVENT(
                           "input.scrolling",
                           "ScrollJankV4Processor::HandleFrame: Invalid frame");
                     }
                   },
                   [&](ScrollJankV4FrameStage::ScrollEnd& end) {
                     decision_queue_.OnScrollEnded();
                   },
               },
               stage.stage);
  }
}

}  // namespace cc
