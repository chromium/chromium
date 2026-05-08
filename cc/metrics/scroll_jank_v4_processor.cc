// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/scroll_jank_v4_processor.h"

#include <memory>
#include <variant>

#include "base/feature_list.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "cc/base/features.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/scroll_jank_v4_decision_queue.h"
#include "cc/metrics/scroll_jank_v4_frame.h"
#include "cc/metrics/scroll_jank_v4_frame_timeline_calculator.h"
#include "cc/metrics/scroll_jank_v4_histogram_emitter.h"
#include "cc/metrics/scroll_jank_v4_result.h"
#include "cc/metrics/scroll_jank_v4_tracing_recorder.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace cc {

namespace {

class ProcessorResultConsumer
    : public ScrollJankV4DecisionQueue::ResultConsumer {
 public:
  void OnFrameResult(const ScrollJankV4Frame::Stage::ScrollUpdates& updates,
                     const ScrollJankV4Frame::ScrollDamage& damage,
                     const ScrollJankV4Frame::BeginFrameArgsForScrollJank& args,
                     const ScrollJankV4Result& result) override {
    bool is_damaging =
        std::holds_alternative<ScrollJankV4Frame::DamagingFrame>(damage);
    histogram_emitter_.OnFrameWithScrollUpdates(result.missed_vsyncs_per_reason,
                                                is_damaging);
    ScrollJankV4TracingRecorder::RecordTraceEvents(updates, damage, args,
                                                   result);
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

  ScrollJankV4Frame::Timeline timeline = timeline_calculator_.CalculateTimeline(
      events_metrics, args, presentation_ts);
  for (auto& frame : timeline) {
    HandleFrame(frame.stages, frame.damage, frame.args);
  }
}

void ScrollJankV4Processor::HandleFrame(
    const ScrollJankV4Frame::StageList& stages,
    const ScrollJankV4Frame::ScrollDamage& damage,
    const ScrollJankV4Frame::BeginFrameArgsForScrollJank& args) {
  for (const ScrollJankV4Frame::Stage& stage : stages) {
    std::visit(absl::Overload{
                   [&](const ScrollJankV4Frame::Stage::ScrollStart& end) {
                     decision_queue_.OnScrollStarted();
                   },
                   [&](const ScrollJankV4Frame::Stage::ScrollUpdates& updates) {
                     if (!decision_queue_.ProcessFrameWithScrollUpdates(
                             updates, damage, args)) {
                       TRACE_EVENT(
                           "input.scrolling",
                           "ScrollJankV4Processor::HandleFrame: Invalid frame",
                           [&](perfetto::EventContext context) {
                             auto* scroll_jank_v4 =
                                 context
                                     .event<perfetto::protos::pbzero::
                                                ChromeTrackEvent>()
                                     ->set_scroll_jank_v4();
                             scroll_jank_v4->set_result_id(args.result_id);
                           });
                     }
                   },
                   [&](const ScrollJankV4Frame::Stage::ScrollEnd& end) {
                     decision_queue_.OnScrollEnded();
                   },
               },
               stage.stage);
  }
}

}  // namespace cc
