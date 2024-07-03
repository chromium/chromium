// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/latency_ukm_reporter.h"

#include <climits>
#include <memory>
#include <utility>

#include "base/rand_util.h"
#include "cc/metrics/ukm_manager.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace cc {

// We use a Poisson process with an exponential decay multiplier. The goal is to
// get many randomly distributed samples early during page load and initial
// interaction, then samples at an exponentially decreasing rate to effectively
// cap the number of samples. The particular parameters chosen here give roughly
// 5-10 samples in the first 100 frames, decaying to several hours between
// samples by the 40th sample. The multiplier value should be tuned to achieve a
// total sample count that avoids throttling by the UKM system.
class LatencyUkmReporter::SamplingController {
 public:
  SamplingController() = default;
  ~SamplingController() = default;

  // When a new UKM event is issued, this function should be called (once and
  // only once) by the client to determine whether that event should be recorded
  // or ignored, according to the sampling parameters. The sampling state will
  // be updated to be ready for the next UKM event.
  bool ShouldRecordNextEvent() {
    bool should_record = false;
    if (!frames_to_next_event_) {
      should_record = true;
      frames_to_next_event_ = SampleFramesToNextEvent();
    }
    DCHECK_GT(frames_to_next_event_, 0);
    --frames_to_next_event_;
    return should_record;
  }

 private:
  // The |kSampleRateMultiplier| and |kSampleDecayRate| have been set to meet
  // UKM goals for data volume.
  const double kSampleDecayRate = 1.0;
  const double kSampleRateMultiplier = 2.0;

  int SampleFramesToNextEvent() {
    // Sample from an exponential distribution to give a Poisson distribution
    // of samples per time unit, then weigh it with an exponential multiplier
    // to give a few samples in rapid succession (for frames early in the
    // page's life) then exponentially fewer as the page lives longer.
    // RandDouble() returns [0,1), but we need (0,1]. If RandDouble() is
    // uniformly random, so is 1-RandDouble(), so use it to adjust the range.
    // When RandDouble() returns 0.0, as it could, we will get a float_sample
    // of 0, causing underflow. So rejection sample until we get a positive
    // count.
    double float_sample = 0.0;
    do {
      float_sample = -(kSampleRateMultiplier *
                       std::exp(samples_so_far_ * kSampleDecayRate) *
                       std::log(1.0 - base::RandDouble()));
    } while (float_sample == 0.0);
    // float_sample is positive, so we don't need to worry about underflow.
    // After around 30 samples we will end up with a super high sample. That's
    // OK because it just means we'll stop reporting metrics for that session,
    // but we do need to be careful about overflow and NaN.
    samples_so_far_++;
    int integer_sample =
        std::isnan(float_sample)
            ? INT_MAX
            : base::saturated_cast<int>(std::ceil(float_sample));
    return integer_sample;
  }

  int samples_so_far_ = 0;
  int frames_to_next_event_ = 0;
};

LatencyUkmReporter::LatencyUkmReporter()
    : compositor_latency_sampling_controller_(
          std::make_unique<SamplingController>()),
      event_latency_sampling_controller_(
          std::make_unique<SamplingController>()) {}

LatencyUkmReporter::~LatencyUkmReporter() = default;

void LatencyUkmReporter::ReportCompositorLatencyUkm(
    const CompositorFrameReporter::FrameReportTypes& report_types,
    const std::vector<CompositorFrameReporter::StageData>& stage_history,
    const ActiveTrackers& active_trackers,
    const CompositorFrameReporter::ProcessedBlinkBreakdown&
        processed_blink_breakdown,
    const CompositorFrameReporter::ProcessedVizBreakdown&
        processed_viz_breakdown) {
  if (ukm_manager_ &&
      compositor_latency_sampling_controller_->ShouldRecordNextEvent()) {
    ukm_manager_->RecordCompositorLatencyUKM(
        report_types, stage_history, active_trackers, processed_blink_breakdown,
        processed_viz_breakdown);
  }
}

void LatencyUkmReporter::ReportEventLatencyUkm(
    const EventMetrics::List& events_metrics,
    const std::vector<CompositorFrameReporter::StageData>& stage_history,
    const CompositorFrameReporter::ProcessedBlinkBreakdown&
        processed_blink_breakdown,
    const CompositorFrameReporter::ProcessedVizBreakdown&
        processed_viz_breakdown) {
  if (ukm_manager_ &&
      event_latency_sampling_controller_->ShouldRecordNextEvent()) {
    ukm_manager_->RecordEventLatencyUKM(events_metrics, stage_history,
                                        processed_blink_breakdown,
                                        processed_viz_breakdown);
  }
}

void LatencyUkmReporter::InitializeUkmManager(
    std::unique_ptr<ukm::UkmRecorder> recorder) {
  ukm_manager_ = std::make_unique<UkmManager>(std::move(recorder));
}

void LatencyUkmReporter::SetSourceId(ukm::SourceId source_id) {
  if (ukm_manager_) {
    ukm_manager_->SetSourceId(source_id);
  }
}

}  // namespace cc
