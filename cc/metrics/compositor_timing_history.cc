// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/compositor_timing_history.h"

#include <stddef.h>
#include <stdint.h>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/metrics/compositor_frame_reporting_controller.h"

namespace cc {

class CompositorTimingHistory::UMAReporter {
 public:
  virtual ~UMAReporter() = default;

  // Throughput measurements
  virtual void AddBeginMainFrameIntervalCritical(base::TimeDelta interval) = 0;
  virtual void AddBeginMainFrameIntervalNotCritical(
      base::TimeDelta interval) = 0;
  virtual void AddCommitInterval(base::TimeDelta interval) = 0;
  virtual void AddDrawInterval(base::TimeDelta interval) = 0;

  // Latency measurements
  virtual void AddBeginImplFrameLatency(base::TimeDelta delta) = 0;
  virtual void AddBeginMainFrameQueueDurationCriticalDuration(
      base::TimeDelta duration) = 0;
  virtual void AddBeginMainFrameQueueDurationNotCriticalDuration(
      base::TimeDelta duration) = 0;
  virtual void AddBeginMainFrameStartToCommitDuration(
      base::TimeDelta duration) = 0;
  virtual void AddCommitToReadyToActivateDuration(base::TimeDelta duration,
                                                  TreePriority priority) = 0;
  virtual void AddInvalidationToReadyToActivateDuration(
      base::TimeDelta duration,
      TreePriority priority) = 0;
  virtual void AddReadyToActivateToWillActivateDuration(
      base::TimeDelta duration,
      bool pending_tree_is_impl_side) = 0;
  virtual void AddPrepareTilesDuration(base::TimeDelta duration) = 0;
  virtual void AddActivateDuration(base::TimeDelta duration) = 0;
  virtual void AddDrawDuration(base::TimeDelta duration) = 0;
  virtual void AddSubmitToAckLatency(base::TimeDelta duration) = 0;

  // crbug.com/758439: the following functions are used to report timing in
  // certain conditions targeting blink / compositor animations.
  // Only the renderer would get the meaningful data.
  virtual void AddDrawIntervalWithCompositedAnimations(
      base::TimeDelta duration) = 0;
  virtual void AddDrawIntervalWithMainThreadAnimations(
      base::TimeDelta duration) = 0;
  virtual void AddDrawIntervalWithCustomPropertyAnimations(
      base::TimeDelta duration) = 0;

  // Synchronization measurements
  virtual void AddMainAndImplFrameTimeDelta(base::TimeDelta delta) = 0;
};

namespace {

// Using the 90th percentile will disable latency recovery
// if we are missing the deadline approximately ~6 times per
// second.
// TODO(brianderson): Fine tune the percentiles below.
const size_t kDurationHistorySize = 60;
const double kBeginMainFrameQueueDurationEstimationPercentile = 90.0;
const double kBeginMainFrameQueueDurationCriticalEstimationPercentile = 90.0;
const double kBeginMainFrameQueueDurationNotCriticalEstimationPercentile = 90.0;
const double kBeginMainFrameStartToReadyToCommitEstimationPercentile = 90.0;
const double kCommitEstimatePercentile = 90.0;
const double kCommitToReadyToActivateEstimationPercentile = 90.0;
const double kPrepareTilesEstimationPercentile = 90.0;
const double kActivateEstimationPercentile = 90.0;
const double kDrawEstimationPercentile = 90.0;

// This macro is deprecated since its bucket count uses too much bandwidth.
// It also has sub-optimal range and bucket distribution.
// TODO(brianderson): Delete this macro and associated UMAs once there is
// sufficient overlap with the re-bucketed UMAs.
#define UMA_HISTOGRAM_CUSTOM_TIMES_MICROS(name, sample)                     \
  UMA_HISTOGRAM_CUSTOM_COUNTS(name, sample.InMicroseconds(),                \
                              kUmaDurationMinMicros, kUmaDurationMaxMicros, \
                              kUmaDurationBucketCount);

// ~90 VSync aligned UMA buckets.
const int kUMAVSyncBuckets[] = {
    // Powers of two from 0 to 2048 us @ 50% precision
    1,
    2,
    4,
    8,
    16,
    32,
    64,
    128,
    256,
    512,
    1024,
    2048,
    // Every 8 Hz from 256 Hz to 128 Hz @ 3-6% precision
    3906,
    4032,
    4167,
    4310,
    4464,
    4630,
    4808,
    5000,
    5208,
    5435,
    5682,
    5952,
    6250,
    6579,
    6944,
    7353,
    // Every 4 Hz from 128 Hz to 64 Hz @ 3-6% precision
    7813,
    8065,
    8333,
    8621,
    8929,
    9259,
    9615,
    10000,
    10417,
    10870,
    11364,
    11905,
    12500,
    13158,
    13889,
    14706,
    // Every 2 Hz from 64 Hz to 32 Hz @ 3-6% precision
    15625,
    16129,
    16667,
    17241,
    17857,
    18519,
    19231,
    20000,
    20833,
    21739,
    22727,
    23810,
    25000,
    26316,
    27778,
    29412,
    // Every 1 Hz from 32 Hz to 1 Hz @ 3-33% precision
    31250,
    32258,
    33333,
    34483,
    35714,
    37037,
    38462,
    40000,
    41667,
    43478,
    45455,
    47619,
    50000,
    52632,
    55556,
    58824,
    62500,
    66667,
    71429,
    76923,
    83333,
    90909,
    100000,
    111111,
    125000,
    142857,
    166667,
    200000,
    250000,
    333333,
    500000,
    // Powers of two from 1s to 32s @ 50% precision
    1000000,
    2000000,
    4000000,
    8000000,
    16000000,
    32000000,
};

// ~50 UMA buckets with high precision from ~100 us to 1s.
const int kUMADurationBuckets[] = {
    // Powers of 2 from 1 us to 64 us @ 50% precision.
    1,
    2,
    4,
    8,
    16,
    32,
    64,
    // 1.25^20, 1.25^21, ..., 1.25^62 @ 20% precision.
    87,
    108,
    136,
    169,
    212,
    265,
    331,
    414,
    517,
    646,
    808,
    1010,
    1262,
    1578,
    1972,
    2465,
    3081,
    3852,
    4815,
    6019,
    7523,
    9404,
    11755,
    14694,
    18367,
    22959,
    28699,
    35873,
    44842,
    56052,
    70065,
    87581,
    109476,
    136846,
    171057,
    213821,
    267276,
    334096,
    417619,
    522024,
    652530,
    815663,
    1019579,
    // Powers of 2 from 2s to 32s @ 50% precision.
    2000000,
    4000000,
    8000000,
    16000000,
    32000000,
};

#define UMA_HISTOGRAM_CUSTOM_TIMES_VSYNC_ALIGNED(name, sample)              \
  do {                                                                      \
    UMA_HISTOGRAM_CUSTOM_ENUMERATION(                                       \
        name "2", sample.InMicroseconds(),                                  \
        std::vector<int>(kUMAVSyncBuckets,                                  \
                         kUMAVSyncBuckets + base::size(kUMAVSyncBuckets))); \
  } while (false)

#define UMA_HISTOGRAM_CUSTOM_TIMES_DURATION_SUFFIX(name, suffix, sample) \
  do {                                                                   \
    UMA_HISTOGRAM_CUSTOM_ENUMERATION(                                    \
        name "2" suffix, sample.InMicroseconds(),                        \
        std::vector<int>(                                                \
            kUMADurationBuckets,                                         \
            kUMADurationBuckets + base::size(kUMADurationBuckets)));     \
  } while (false)

#define UMA_HISTOGRAM_CUSTOM_TIMES_DURATION(name, sample) \
  UMA_HISTOGRAM_CUSTOM_TIMES_DURATION_SUFFIX(name, "", sample)

#define UMA_HISTOGRAM_READY_TO_ACTIVATE(name, sample, priority)            \
  do {                                                                     \
    UMA_HISTOGRAM_CUSTOM_TIMES_DURATION(name, sample);                     \
    switch (priority) {                                                    \
      case SAME_PRIORITY_FOR_BOTH_TREES:                                   \
        UMA_HISTOGRAM_CUSTOM_TIMES_DURATION_SUFFIX(name, ".Same", sample); \
        break;                                                             \
      case SMOOTHNESS_TAKES_PRIORITY:                                      \
        UMA_HISTOGRAM_CUSTOM_TIMES_DURATION_SUFFIX(name, ".Smoothness",    \
                                                   sample);                \
        break;                                                             \
      case NEW_CONTENT_TAKES_PRIORITY:                                     \
        UMA_HISTOGRAM_CUSTOM_TIMES_DURATION_SUFFIX(name, ".NewContent",    \
                                                   sample);                \
        break;                                                             \
    }                                                                      \
  } while (false)

class RendererUMAReporter : public CompositorTimingHistory::UMAReporter {
 public:
  ~RendererUMAReporter() override = default;

  void AddBeginMainFrameIntervalCritical(base::TimeDelta interval) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_VSYNC_ALIGNED(
        "Scheduling.Renderer.BeginMainFrameIntervalCritical", interval);
  }

  void AddBeginMainFrameIntervalNotCritical(base::TimeDelta interval) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_VSYNC_ALIGNED(
        "Scheduling.Renderer.BeginMainFrameIntervalNotCritical", interval);
  }

  void AddCommitInterval(base::TimeDelta interval) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_VSYNC_ALIGNED(
        "Scheduling.Renderer.CommitInterval", interval);
  }

  void AddDrawInterval(base::TimeDelta interval) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_VSYNC_ALIGNED("Scheduling.Renderer.DrawInterval",
                                             interval);
  }

  void AddDrawIntervalWithCompositedAnimations(
      base::TimeDelta interval) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_VSYNC_ALIGNED(
        "Scheduling.Renderer.DrawIntervalWithCompositedAnimations", interval);
  }

  void AddDrawIntervalWithMainThreadAnimations(
      base::TimeDelta interval) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_VSYNC_ALIGNED(
        "Scheduling.Renderer.DrawIntervalWithMainThreadAnimations", interval);
  }

  void AddDrawIntervalWithCustomPropertyAnimations(
      base::TimeDelta interval) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_VSYNC_ALIGNED(
        "Scheduling.Renderer.DrawIntervalWithCustomPropertyAnimations",
        interval);
  }

  void AddBeginImplFrameLatency(base::TimeDelta delta) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_DURATION(
        "Scheduling.Renderer.BeginImplFrameLatency", delta);
  }

  void AddBeginMainFrameQueueDurationCriticalDuration(
      base::TimeDelta duration) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_DURATION(
        "Scheduling.Renderer.BeginMainFrameQueueDurationCritical", duration);
  }

  void AddBeginMainFrameQueueDurationNotCriticalDuration(
      base::TimeDelta duration) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_DURATION(
        "Scheduling.Renderer.BeginMainFrameQueueDurationNotCritical", duration);
  }

  void AddBeginMainFrameStartToCommitDuration(
      base::TimeDelta duration) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_DURATION(
        "Scheduling.Renderer.BeginMainFrameStartToCommitDuration", duration);
  }

  void AddCommitToReadyToActivateDuration(base::TimeDelta duration,
                                          TreePriority priority) override {
    UMA_HISTOGRAM_READY_TO_ACTIVATE(
        "Scheduling.Renderer.CommitToReadyToActivateDuration", duration,
        priority);
  }

  void AddInvalidationToReadyToActivateDuration(
      base::TimeDelta duration,
      TreePriority priority) override {
    UMA_HISTOGRAM_READY_TO_ACTIVATE(
        "Scheduling.Renderer.InvalidationToReadyToActivateDuration", duration,
        priority);
  }

  void AddReadyToActivateToWillActivateDuration(
      base::TimeDelta duration,
      bool pending_tree_is_impl_side) override {
    if (pending_tree_is_impl_side) {
      UMA_HISTOGRAM_CUSTOM_TIMES_DURATION_SUFFIX(
          "Scheduling.Renderer.ReadyToActivateToActivationDuration", ".Impl",
          duration);
    } else {
      UMA_HISTOGRAM_CUSTOM_TIMES_DURATION_SUFFIX(
          "Scheduling.Renderer.ReadyToActivateToActivationDuration", ".Main",
          duration);
    }
  }

  void AddPrepareTilesDuration(base::TimeDelta duration) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_DURATION(
        "Scheduling.Renderer.PrepareTilesDuration", duration);
  }

  void AddActivateDuration(base::TimeDelta duration) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_DURATION("Scheduling.Renderer.ActivateDuration",
                                        duration);
  }

  void AddDrawDuration(base::TimeDelta duration) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_DURATION("Scheduling.Renderer.DrawDuration",
                                        duration);
  }

  void AddSubmitToAckLatency(base::TimeDelta duration) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_DURATION("Scheduling.Renderer.SwapToAckLatency",
                                        duration);
  }

  void AddMainAndImplFrameTimeDelta(base::TimeDelta delta) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_VSYNC_ALIGNED(
        "Scheduling.Renderer.MainAndImplFrameTimeDelta", delta);
  }
};

class BrowserUMAReporter : public CompositorTimingHistory::UMAReporter {
 public:
  ~BrowserUMAReporter() override = default;

  // BeginMainFrameIntervalCritical is not meaningful to measure on browser
  // side because browser rendering fps is not at 60.
  void AddBeginMainFrameIntervalCritical(base::TimeDelta interval) override {}

  void AddBeginMainFrameIntervalNotCritical(base::TimeDelta interval) override {
  }

  // CommitInterval is not meaningful to measure on browser side because
  // browser rendering fps is not at 60.
  void AddCommitInterval(base::TimeDelta interval) override {}

  // DrawInterval is not meaningful to measure on browser side because
  // browser rendering fps is not at 60.
  void AddDrawInterval(base::TimeDelta interval) override {}

  void AddDrawIntervalWithCompositedAnimations(
      base::TimeDelta interval) override {}

  void AddDrawIntervalWithMainThreadAnimations(
      base::TimeDelta interval) override {}

  void AddDrawIntervalWithCustomPropertyAnimations(
      base::TimeDelta interval) override {}

  void AddBeginImplFrameLatency(base::TimeDelta delta) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_DURATION(
        "Scheduling.Browser.BeginImplFrameLatency", delta);
  }

  void AddBeginMainFrameQueueDurationCriticalDuration(
      base::TimeDelta duration) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_DURATION(
        "Scheduling.Browser.BeginMainFrameQueueDurationCritical", duration);
  }

  void AddBeginMainFrameQueueDurationNotCriticalDuration(
      base::TimeDelta duration) override {}

  void AddBeginMainFrameStartToCommitDuration(
      base::TimeDelta duration) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_DURATION(
        "Scheduling.Browser.BeginMainFrameStartToCommitDuration", duration);
  }

  void AddCommitToReadyToActivateDuration(base::TimeDelta duration,
                                          TreePriority priority) override {
    UMA_HISTOGRAM_READY_TO_ACTIVATE(
        "Scheduling.Browser.CommitToReadyToActivateDuration", duration,
        priority);
  }

  void AddInvalidationToReadyToActivateDuration(
      base::TimeDelta duration,
      TreePriority priority) override {
    UMA_HISTOGRAM_READY_TO_ACTIVATE(
        "Scheduling.Browser.InvalidationToReadyToActivateDuration", duration,
        priority);
  }

  void AddReadyToActivateToWillActivateDuration(
      base::TimeDelta duration,
      bool pending_tree_is_impl_side) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_DURATION_SUFFIX(
        "Scheduling.Browser.ReadyToActivateToActivationDuration", ".Main",
        duration);
  }

  void AddPrepareTilesDuration(base::TimeDelta duration) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_DURATION(
        "Scheduling.Browser.PrepareTilesDuration", duration);
  }

  void AddActivateDuration(base::TimeDelta duration) override {}

  void AddDrawDuration(base::TimeDelta duration) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_DURATION("Scheduling.Browser.DrawDuration",
                                        duration);
  }

  void AddSubmitToAckLatency(base::TimeDelta duration) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_DURATION("Scheduling.Browser.SwapToAckLatency",
                                        duration);
  }

  void AddMainAndImplFrameTimeDelta(base::TimeDelta delta) override {}
};

class NullUMAReporter : public CompositorTimingHistory::UMAReporter {
 public:
  ~NullUMAReporter() override = default;
  void AddBeginMainFrameIntervalCritical(base::TimeDelta interval) override {}
  void AddBeginMainFrameIntervalNotCritical(base::TimeDelta interval) override {
  }
  void AddCommitInterval(base::TimeDelta interval) override {}
  void AddDrawInterval(base::TimeDelta interval) override {}
  void AddDrawIntervalWithCompositedAnimations(
      base::TimeDelta inverval) override {}
  void AddDrawIntervalWithMainThreadAnimations(
      base::TimeDelta inverval) override {}
  void AddDrawIntervalWithCustomPropertyAnimations(
      base::TimeDelta inverval) override {}
  void AddBeginImplFrameLatency(base::TimeDelta delta) override {}
  void AddBeginMainFrameQueueDurationCriticalDuration(
      base::TimeDelta duration) override {}
  void AddBeginMainFrameQueueDurationNotCriticalDuration(
      base::TimeDelta duration) override {}
  void AddBeginMainFrameStartToCommitDuration(
      base::TimeDelta duration) override {}
  void AddCommitToReadyToActivateDuration(base::TimeDelta duration,
                                          TreePriority priority) override {}
  void AddInvalidationToReadyToActivateDuration(
      base::TimeDelta duration,
      TreePriority priority) override {}
  void AddReadyToActivateToWillActivateDuration(
      base::TimeDelta duration,
      bool pending_tree_is_impl_side) override {}
  void AddPrepareTilesDuration(base::TimeDelta duration) override {}
  void AddActivateDuration(base::TimeDelta duration) override {}
  void AddDrawDuration(base::TimeDelta duration) override {}
  void AddSubmitToAckLatency(base::TimeDelta duration) override {}
  void AddMainAndImplFrameTimeDelta(base::TimeDelta delta) override {}
};

}  // namespace

CompositorTimingHistory::CompositorTimingHistory(
    bool using_synchronous_renderer_compositor,
    UMACategory uma_category,
    RenderingStatsInstrumentation* rendering_stats_instrumentation,
    CompositorFrameReportingController* compositor_frame_reporting_controller)
    : using_synchronous_renderer_compositor_(
          using_synchronous_renderer_compositor),
      enabled_(false),
      did_send_begin_main_frame_(false),
      begin_main_frame_needed_continuously_(false),
      begin_main_frame_committing_continuously_(false),
      compositor_drawing_continuously_(false),
      begin_main_frame_queue_duration_history_(kDurationHistorySize),
      begin_main_frame_queue_duration_critical_history_(kDurationHistorySize),
      begin_main_frame_queue_duration_not_critical_history_(
          kDurationHistorySize),
      begin_main_frame_start_to_ready_to_commit_duration_history_(
          kDurationHistorySize),
      commit_duration_history_(kDurationHistorySize),
      commit_to_ready_to_activate_duration_history_(kDurationHistorySize),
      prepare_tiles_duration_history_(kDurationHistorySize),
      activate_duration_history_(kDurationHistorySize),
      draw_duration_history_(kDurationHistorySize),
      begin_main_frame_on_critical_path_(false),
      uma_reporter_(CreateUMAReporter(uma_category)),
      rendering_stats_instrumentation_(rendering_stats_instrumentation),
      compositor_frame_reporting_controller_(
          compositor_frame_reporting_controller) {}

CompositorTimingHistory::~CompositorTimingHistory() = default;

std::unique_ptr<CompositorTimingHistory::UMAReporter>
CompositorTimingHistory::CreateUMAReporter(UMACategory category) {
  switch (category) {
    case RENDERER_UMA:
      return base::WrapUnique(new RendererUMAReporter);
      break;
    case BROWSER_UMA:
      return base::WrapUnique(new BrowserUMAReporter);
      break;
    case NULL_UMA:
      return base::WrapUnique(new NullUMAReporter);
      break;
  }
  NOTREACHED();
  return base::WrapUnique<CompositorTimingHistory::UMAReporter>(nullptr);
}

void CompositorTimingHistory::AsValueInto(
    base::trace_event::TracedValue* state) const {
  state->SetDouble(
      "begin_main_frame_queue_critical_estimate_ms",
      BeginMainFrameQueueDurationCriticalEstimate().InMillisecondsF());
  state->SetDouble(
      "begin_main_frame_queue_not_critical_estimate_ms",
      BeginMainFrameQueueDurationNotCriticalEstimate().InMillisecondsF());
  state->SetDouble(
      "begin_main_frame_start_to_ready_to_commit_estimate_ms",
      BeginMainFrameStartToReadyToCommitDurationEstimate().InMillisecondsF());
  state->SetDouble("commit_to_ready_to_activate_estimate_ms",
                   CommitToReadyToActivateDurationEstimate().InMillisecondsF());
  state->SetDouble("prepare_tiles_estimate_ms",
                   PrepareTilesDurationEstimate().InMillisecondsF());
  state->SetDouble("activate_estimate_ms",
                   ActivateDurationEstimate().InMillisecondsF());
  state->SetDouble("draw_estimate_ms",
                   DrawDurationEstimate().InMillisecondsF());
}

base::TimeTicks CompositorTimingHistory::Now() const {
  return base::TimeTicks::Now();
}

void CompositorTimingHistory::SetRecordingEnabled(bool enabled) {
  enabled_ = enabled;
}

void CompositorTimingHistory::SetBeginMainFrameNeededContinuously(bool active) {
  if (active == begin_main_frame_needed_continuously_)
    return;
  begin_main_frame_end_time_prev_ = base::TimeTicks();
  begin_main_frame_needed_continuously_ = active;
}

void CompositorTimingHistory::SetBeginMainFrameCommittingContinuously(
    bool active) {
  if (active == begin_main_frame_committing_continuously_)
    return;
  new_active_tree_draw_end_time_prev_committing_continuously_ =
      base::TimeTicks();
  begin_main_frame_committing_continuously_ = active;
}

void CompositorTimingHistory::SetCompositorDrawingContinuously(bool active) {
  if (active == compositor_drawing_continuously_)
    return;
  draw_end_time_prev_ = base::TimeTicks();
  compositor_drawing_continuously_ = active;
}

base::TimeDelta
CompositorTimingHistory::BeginMainFrameQueueDurationCriticalEstimate() const {
  base::TimeDelta all = begin_main_frame_queue_duration_history_.Percentile(
      kBeginMainFrameQueueDurationEstimationPercentile);
  base::TimeDelta critical =
      begin_main_frame_queue_duration_critical_history_.Percentile(
          kBeginMainFrameQueueDurationCriticalEstimationPercentile);
  // Return the min since critical BeginMainFrames are likely fast if
  // the non critical ones are.
  return std::min(critical, all);
}

base::TimeDelta
CompositorTimingHistory::BeginMainFrameQueueDurationNotCriticalEstimate()
    const {
  base::TimeDelta all = begin_main_frame_queue_duration_history_.Percentile(
      kBeginMainFrameQueueDurationEstimationPercentile);
  base::TimeDelta not_critical =
      begin_main_frame_queue_duration_not_critical_history_.Percentile(
          kBeginMainFrameQueueDurationNotCriticalEstimationPercentile);
  // Return the max since, non critical BeginMainFrames are likely slow if
  // the critical ones are.
  return std::max(not_critical, all);
}

base::TimeDelta
CompositorTimingHistory::BeginMainFrameStartToReadyToCommitDurationEstimate()
    const {
  return begin_main_frame_start_to_ready_to_commit_duration_history_.Percentile(
      kBeginMainFrameStartToReadyToCommitEstimationPercentile);
}

base::TimeDelta CompositorTimingHistory::CommitDurationEstimate() const {
  return commit_duration_history_.Percentile(kCommitEstimatePercentile);
}

base::TimeDelta
CompositorTimingHistory::CommitToReadyToActivateDurationEstimate() const {
  return commit_to_ready_to_activate_duration_history_.Percentile(
      kCommitToReadyToActivateEstimationPercentile);
}

base::TimeDelta CompositorTimingHistory::PrepareTilesDurationEstimate() const {
  return prepare_tiles_duration_history_.Percentile(
      kPrepareTilesEstimationPercentile);
}

base::TimeDelta CompositorTimingHistory::ActivateDurationEstimate() const {
  return activate_duration_history_.Percentile(kActivateEstimationPercentile);
}

base::TimeDelta CompositorTimingHistory::DrawDurationEstimate() const {
  return draw_duration_history_.Percentile(kDrawEstimationPercentile);
}

void CompositorTimingHistory::DidCreateAndInitializeLayerTreeFrameSink() {
  // After we get a new output surface, we won't get a spurious
  // CompositorFrameAck from the old output surface.
  submit_start_time_ = base::TimeTicks();
}

void CompositorTimingHistory::WillBeginImplFrame(
    const viz::BeginFrameArgs& args,
    bool new_active_tree_is_likely,
    base::TimeTicks now) {
  viz::BeginFrameArgs::BeginFrameArgsType frame_type = args.type;
  base::TimeTicks frame_time = args.frame_time;

  compositor_frame_reporting_controller_->WillBeginImplFrame();

  // The check for whether a BeginMainFrame was sent anytime between two
  // BeginImplFrames protects us from not detecting a fast main thread that
  // does all it's work and goes idle in between BeginImplFrames.
  // For example, this may happen if an animation is being driven with
  // setInterval(17) or if input events just happen to arrive in the
  // middle of every frame.
  if (!new_active_tree_is_likely && !did_send_begin_main_frame_) {
    SetBeginMainFrameNeededContinuously(false);
    SetBeginMainFrameCommittingContinuously(false);
  }

  if (frame_type == viz::BeginFrameArgs::NORMAL)
    uma_reporter_->AddBeginImplFrameLatency(now - frame_time);

  did_send_begin_main_frame_ = false;
}

void CompositorTimingHistory::WillFinishImplFrame(bool needs_redraw) {
  if (!needs_redraw)
    SetCompositorDrawingContinuously(false);

  compositor_frame_reporting_controller_->OnFinishImplFrame();
}

void CompositorTimingHistory::BeginImplFrameNotExpectedSoon() {
  SetBeginMainFrameNeededContinuously(false);
  SetBeginMainFrameCommittingContinuously(false);
  SetCompositorDrawingContinuously(false);
}

void CompositorTimingHistory::WillBeginMainFrame(
    bool on_critical_path,
    base::TimeTicks main_frame_time) {
  DCHECK_EQ(base::TimeTicks(), begin_main_frame_sent_time_);
  DCHECK_EQ(base::TimeTicks(), begin_main_frame_frame_time_);

  compositor_frame_reporting_controller_->WillBeginMainFrame();

  begin_main_frame_on_critical_path_ = on_critical_path;
  begin_main_frame_sent_time_ = Now();
  begin_main_frame_frame_time_ = main_frame_time;

  did_send_begin_main_frame_ = true;
  SetBeginMainFrameNeededContinuously(true);
}

void CompositorTimingHistory::BeginMainFrameStarted(
    base::TimeTicks main_thread_start_time) {
  DCHECK_NE(base::TimeTicks(), begin_main_frame_sent_time_);
  DCHECK_EQ(base::TimeTicks(), begin_main_frame_start_time_);
  begin_main_frame_start_time_ = main_thread_start_time;
}

void CompositorTimingHistory::BeginMainFrameAborted() {
  compositor_frame_reporting_controller_->BeginMainFrameAborted();
  SetBeginMainFrameCommittingContinuously(false);
  base::TimeTicks begin_main_frame_end_time = Now();
  DidBeginMainFrame(begin_main_frame_end_time);
  begin_main_frame_frame_time_ = base::TimeTicks();
}

void CompositorTimingHistory::NotifyReadyToCommit(
    std::unique_ptr<BeginMainFrameMetrics> details) {
  DCHECK_NE(begin_main_frame_start_time_, base::TimeTicks());
  compositor_frame_reporting_controller_->SetBlinkBreakdown(std::move(details));
  begin_main_frame_start_to_ready_to_commit_duration_history_.InsertSample(
      Now() - begin_main_frame_start_time_);
}

void CompositorTimingHistory::WillCommit() {
  DCHECK_NE(begin_main_frame_start_time_, base::TimeTicks());
  compositor_frame_reporting_controller_->WillCommit();
  commit_start_time_ = Now();
}

void CompositorTimingHistory::DidCommit() {
  DCHECK_EQ(base::TimeTicks(), pending_tree_main_frame_time_);
  DCHECK_EQ(pending_tree_creation_time_, base::TimeTicks());
  DCHECK_NE(commit_start_time_, base::TimeTicks());

  compositor_frame_reporting_controller_->DidCommit();

  SetBeginMainFrameCommittingContinuously(true);
  base::TimeTicks begin_main_frame_end_time = Now();
  DidBeginMainFrame(begin_main_frame_end_time);
  commit_duration_history_.InsertSample(begin_main_frame_end_time -
                                        commit_start_time_);

  pending_tree_is_impl_side_ = false;
  pending_tree_creation_time_ = begin_main_frame_end_time;
  pending_tree_main_frame_time_ = begin_main_frame_frame_time_;
  begin_main_frame_frame_time_ = base::TimeTicks();
}

void CompositorTimingHistory::DidBeginMainFrame(
    base::TimeTicks begin_main_frame_end_time) {
  DCHECK_NE(base::TimeTicks(), begin_main_frame_sent_time_);

  // If the BeginMainFrame start time isn't know, assume it was immediate
  // for scheduling purposes, but don't report it for UMA to avoid skewing
  // the results.
  bool begin_main_frame_start_time_is_valid =
      !begin_main_frame_start_time_.is_null();
  if (!begin_main_frame_start_time_is_valid)
    begin_main_frame_start_time_ = begin_main_frame_sent_time_;

  base::TimeDelta begin_main_frame_sent_to_commit_duration =
      begin_main_frame_end_time - begin_main_frame_sent_time_;
  base::TimeDelta begin_main_frame_queue_duration =
      begin_main_frame_start_time_ - begin_main_frame_sent_time_;
  base::TimeDelta begin_main_frame_start_to_commit_duration =
      begin_main_frame_end_time - begin_main_frame_start_time_;

  rendering_stats_instrumentation_->AddBeginMainFrameToCommitDuration(
      begin_main_frame_sent_to_commit_duration);

  if (begin_main_frame_start_time_is_valid) {
    if (begin_main_frame_on_critical_path_) {
      uma_reporter_->AddBeginMainFrameQueueDurationCriticalDuration(
          begin_main_frame_queue_duration);
    } else {
      uma_reporter_->AddBeginMainFrameQueueDurationNotCriticalDuration(
          begin_main_frame_queue_duration);
    }
  }

  uma_reporter_->AddBeginMainFrameStartToCommitDuration(
      begin_main_frame_start_to_commit_duration);

  if (enabled_) {
    begin_main_frame_queue_duration_history_.InsertSample(
        begin_main_frame_queue_duration);
    if (begin_main_frame_on_critical_path_) {
      begin_main_frame_queue_duration_critical_history_.InsertSample(
          begin_main_frame_queue_duration);
    } else {
      begin_main_frame_queue_duration_not_critical_history_.InsertSample(
          begin_main_frame_queue_duration);
    }
  }

  if (begin_main_frame_needed_continuously_) {
    if (!begin_main_frame_end_time_prev_.is_null()) {
      base::TimeDelta commit_interval =
          begin_main_frame_end_time - begin_main_frame_end_time_prev_;
      if (begin_main_frame_on_critical_path_)
        uma_reporter_->AddBeginMainFrameIntervalCritical(commit_interval);
      else
        uma_reporter_->AddBeginMainFrameIntervalNotCritical(commit_interval);
    }
    begin_main_frame_end_time_prev_ = begin_main_frame_end_time;
  }

  begin_main_frame_sent_time_ = base::TimeTicks();
  begin_main_frame_start_time_ = base::TimeTicks();
}

void CompositorTimingHistory::WillInvalidateOnImplSide() {
  DCHECK(!pending_tree_is_impl_side_);
  DCHECK_EQ(pending_tree_creation_time_, base::TimeTicks());

  compositor_frame_reporting_controller_->WillInvalidateOnImplSide();
  pending_tree_is_impl_side_ = true;
  pending_tree_creation_time_ = base::TimeTicks::Now();
}

void CompositorTimingHistory::WillPrepareTiles() {
  DCHECK_EQ(base::TimeTicks(), prepare_tiles_start_time_);
  prepare_tiles_start_time_ = Now();
}

void CompositorTimingHistory::DidPrepareTiles() {
  DCHECK_NE(base::TimeTicks(), prepare_tiles_start_time_);

  base::TimeDelta prepare_tiles_duration = Now() - prepare_tiles_start_time_;
  uma_reporter_->AddPrepareTilesDuration(prepare_tiles_duration);
  if (enabled_)
    prepare_tiles_duration_history_.InsertSample(prepare_tiles_duration);

  prepare_tiles_start_time_ = base::TimeTicks();
}

void CompositorTimingHistory::ReadyToActivate() {
  DCHECK_NE(pending_tree_creation_time_, base::TimeTicks());
  DCHECK_EQ(pending_tree_ready_to_activate_time_, base::TimeTicks());

  pending_tree_ready_to_activate_time_ = Now();
  if (pending_tree_is_impl_side_) {
    base::TimeDelta time_since_invalidation =
        pending_tree_ready_to_activate_time_ - pending_tree_creation_time_;
    uma_reporter_->AddInvalidationToReadyToActivateDuration(
        time_since_invalidation, tree_priority_);
  } else {
    base::TimeDelta time_since_commit =
        pending_tree_ready_to_activate_time_ - pending_tree_creation_time_;

    // Before adding the new data point to the timing history, see what we would
    // have predicted for this frame. This allows us to keep track of the
    // accuracy of our predictions.

    base::TimeDelta commit_to_ready_to_activate_estimate =
        CommitToReadyToActivateDurationEstimate();
    uma_reporter_->AddCommitToReadyToActivateDuration(time_since_commit,
                                                      tree_priority_);
    rendering_stats_instrumentation_->AddCommitToActivateDuration(
        time_since_commit, commit_to_ready_to_activate_estimate);

    if (enabled_) {
      commit_to_ready_to_activate_duration_history_.InsertSample(
          time_since_commit);
    }
  }
}

void CompositorTimingHistory::WillActivate() {
  DCHECK_EQ(base::TimeTicks(), activate_start_time_);

  compositor_frame_reporting_controller_->WillActivate();
  activate_start_time_ = Now();

  // Its possible to activate the pending tree before it is ready for
  // activation, for instance in the case of a context loss or visibility
  // changes.
  if (pending_tree_ready_to_activate_time_ != base::TimeTicks()) {
    base::TimeDelta time_since_ready =
        activate_start_time_ - pending_tree_ready_to_activate_time_;
    uma_reporter_->AddReadyToActivateToWillActivateDuration(
        time_since_ready, pending_tree_is_impl_side_);
  }

  pending_tree_is_impl_side_ = false;
  pending_tree_creation_time_ = base::TimeTicks();
  pending_tree_ready_to_activate_time_ = base::TimeTicks();
}

void CompositorTimingHistory::DidActivate() {
  DCHECK_NE(base::TimeTicks(), activate_start_time_);
  compositor_frame_reporting_controller_->DidActivate();
  base::TimeDelta activate_duration = Now() - activate_start_time_;

  uma_reporter_->AddActivateDuration(activate_duration);
  if (enabled_)
    activate_duration_history_.InsertSample(activate_duration);

  // The synchronous compositor doesn't necessarily draw every new active tree.
  if (!using_synchronous_renderer_compositor_)
    DCHECK_EQ(base::TimeTicks(), active_tree_main_frame_time_);
  active_tree_main_frame_time_ = pending_tree_main_frame_time_;

  activate_start_time_ = base::TimeTicks();
  pending_tree_main_frame_time_ = base::TimeTicks();
}

void CompositorTimingHistory::WillDraw() {
  DCHECK_EQ(base::TimeTicks(), draw_start_time_);
  draw_start_time_ = Now();
}

void CompositorTimingHistory::DrawAborted() {
  active_tree_main_frame_time_ = base::TimeTicks();
}

void CompositorTimingHistory::DidDraw(bool used_new_active_tree,
                                      base::TimeTicks impl_frame_time,
                                      size_t composited_animations_count,
                                      size_t main_thread_animations_count,
                                      bool current_frame_had_raf,
                                      bool next_frame_has_pending_raf,
                                      bool has_custom_property_animations) {
  DCHECK_NE(base::TimeTicks(), draw_start_time_);
  base::TimeTicks draw_end_time = Now();
  base::TimeDelta draw_duration = draw_end_time - draw_start_time_;

  // Before adding the new data point to the timing history, see what we would
  // have predicted for this frame. This allows us to keep track of the accuracy
  // of our predictions.
  base::TimeDelta draw_estimate = DrawDurationEstimate();
  rendering_stats_instrumentation_->AddDrawDuration(draw_duration,
                                                    draw_estimate);

  uma_reporter_->AddDrawDuration(draw_duration);

  if (enabled_) {
    draw_duration_history_.InsertSample(draw_duration);
  }

  SetCompositorDrawingContinuously(true);
  if (!draw_end_time_prev_.is_null()) {
    base::TimeDelta draw_interval = draw_end_time - draw_end_time_prev_;
    uma_reporter_->AddDrawInterval(draw_interval);
    if (composited_animations_count > 0 &&
        previous_frame_had_composited_animations_)
      uma_reporter_->AddDrawIntervalWithCompositedAnimations(draw_interval);
    if (has_custom_property_animations &&
        previous_frame_had_custom_property_animations_)
      uma_reporter_->AddDrawIntervalWithCustomPropertyAnimations(draw_interval);
  }
  previous_frame_had_composited_animations_ = composited_animations_count > 0;
  previous_frame_had_custom_property_animations_ =
      has_custom_property_animations;
  draw_end_time_prev_ = draw_end_time;

  if (used_new_active_tree) {
    DCHECK_NE(base::TimeTicks(), active_tree_main_frame_time_);
    base::TimeDelta main_and_impl_delta =
        impl_frame_time - active_tree_main_frame_time_;
    DCHECK_GE(main_and_impl_delta, base::TimeDelta());
    TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler.frames"),
                 "CompositorTimingHistory::DidDraw",
                 "active_tree_main_frame_time", active_tree_main_frame_time_,
                 "impl_frame_time", impl_frame_time);
    uma_reporter_->AddMainAndImplFrameTimeDelta(main_and_impl_delta);
    active_tree_main_frame_time_ = base::TimeTicks();

    bool current_main_frame_had_visual_update =
        main_thread_animations_count > 0 || current_frame_had_raf;
    bool previous_main_frame_had_visual_update =
        previous_frame_had_main_thread_animations_ || previous_frame_had_raf_;
    if (current_main_frame_had_visual_update &&
        previous_main_frame_had_visual_update) {
      base::TimeDelta draw_interval =
          draw_end_time - new_active_tree_draw_end_time_prev_;
      uma_reporter_->AddDrawIntervalWithMainThreadAnimations(draw_interval);
    }
    previous_frame_had_main_thread_animations_ =
        main_thread_animations_count > 0;
    // It's possible that two consecutive main frames both run a rAF but are
    // separated by idle time (for example: calling requestAnimationFrame from a
    // setInterval function, with nothing else producing a main frame
    // in-between). To avoid incorrectly counting those cases as long draw
    // intervals, we only update previous_frame_had_raf_ if the current frame
    // also already has a future raf scheduled.
    previous_frame_had_raf_ =
        current_frame_had_raf && next_frame_has_pending_raf;

    new_active_tree_draw_end_time_prev_ = draw_end_time;

    if (begin_main_frame_committing_continuously_) {
      if (!new_active_tree_draw_end_time_prev_committing_continuously_
               .is_null()) {
        base::TimeDelta draw_interval =
            draw_end_time -
            new_active_tree_draw_end_time_prev_committing_continuously_;
        uma_reporter_->AddCommitInterval(draw_interval);
      }
      new_active_tree_draw_end_time_prev_committing_continuously_ =
          draw_end_time;
    }
  }
  draw_start_time_ = base::TimeTicks();
}

void CompositorTimingHistory::DidSubmitCompositorFrame(uint32_t frame_token) {
  DCHECK_EQ(base::TimeTicks(), submit_start_time_);
  compositor_frame_reporting_controller_->DidSubmitCompositorFrame(frame_token);
  submit_start_time_ = Now();
}

void CompositorTimingHistory::DidReceiveCompositorFrameAck() {
  DCHECK_NE(base::TimeTicks(), submit_start_time_);
  base::TimeDelta submit_to_ack_duration = Now() - submit_start_time_;
  uma_reporter_->AddSubmitToAckLatency(submit_to_ack_duration);
  submit_start_time_ = base::TimeTicks();
}

void CompositorTimingHistory::DidPresentCompositorFrame(
    uint32_t frame_token,
    const viz::FrameTimingDetails& details) {
  compositor_frame_reporting_controller_->DidPresentCompositorFrame(frame_token,
                                                                    details);
}

void CompositorTimingHistory::SetTreePriority(TreePriority priority) {
  tree_priority_ = priority;
}

void CompositorTimingHistory::ClearHistory() {
  TRACE_EVENT0("cc,benchmark", "CompositorTimingHistory::ClearHistory");

  begin_main_frame_queue_duration_history_.Clear();
  begin_main_frame_queue_duration_critical_history_.Clear();
  begin_main_frame_queue_duration_not_critical_history_.Clear();
  begin_main_frame_start_to_ready_to_commit_duration_history_.Clear();
  commit_duration_history_.Clear();
  commit_to_ready_to_activate_duration_history_.Clear();
  prepare_tiles_duration_history_.Clear();
  activate_duration_history_.Clear();
  draw_duration_history_.Clear();
}

}  // namespace cc
