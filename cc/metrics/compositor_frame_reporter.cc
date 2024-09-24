// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/metrics/compositor_frame_reporter.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "base/cpu_reduction_experiment.h"
#include "base/debug/alias.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "base/trace_event/typed_macros.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "cc/base/rolling_time_delta_history.h"
#include "cc/metrics/dropped_frame_counter.h"
#include "cc/metrics/event_latency_tracing_recorder.h"
#include "cc/metrics/event_latency_tracker.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/frame_sequence_tracker.h"
#include "cc/metrics/latency_ukm_reporter.h"
#include "services/tracing/public/cpp/perfetto/macros.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_frame_reporter.pbzero.h"
#include "ui/events/types/event_type.h"

namespace cc {
namespace {

using StageType = CompositorFrameReporter::StageType;
using FrameReportType = CompositorFrameReporter::FrameReportType;
using BlinkBreakdown = CompositorFrameReporter::BlinkBreakdown;
using VizBreakdown = CompositorFrameReporter::VizBreakdown;
using FrameFinalState = FrameInfo::FrameFinalState;

constexpr int kFrameReportTypeCount =
    static_cast<int>(FrameReportType::kMaxValue) + 1;
constexpr int kStageTypeCount = static_cast<int>(StageType::kStageTypeCount);
constexpr int kAllBreakdownCount =
    static_cast<int>(VizBreakdown::kBreakdownCount) +
    static_cast<int>(BlinkBreakdown::kBreakdownCount);

constexpr int kVizBreakdownInitialIndex = kStageTypeCount;
constexpr int kBlinkBreakdownInitialIndex =
    kVizBreakdownInitialIndex + static_cast<int>(VizBreakdown::kBreakdownCount);

// For each possible FrameSequenceTrackerType there will be a UMA histogram
// plus one for general case.
constexpr int kFrameSequenceTrackerTypeCount =
    static_cast<int>(FrameSequenceTrackerType::kMaxType) + 1;

// Maximum number of partial update dependents a reporter can own. When a
// reporter with too many dependents is terminated, it will terminate all its
// dependents which will block the pipeline for a long time. Too many dependents
// also means too much memory usage.
constexpr size_t kMaxOwnedPartialUpdateDependents = 300u;

// Names for CompositorFrameReporter::FrameReportType, which should be
// updated in case of changes to the enum.
constexpr const char* kReportTypeNames[]{
    "", "MissedDeadlineFrame.", "DroppedFrame.", "CompositorOnlyFrame."};

static_assert(std::size(kReportTypeNames) == kFrameReportTypeCount,
              "Compositor latency report types has changed.");

// This value should be recalculated in case of changes to the number of values
// in CompositorFrameReporter::DroppedFrameReportType or in
// CompositorFrameReporter::StageType
constexpr int kStagesWithBreakdownCount = kStageTypeCount + kAllBreakdownCount;
constexpr int kMaxCompositorLatencyHistogramIndex =
    kFrameReportTypeCount *
    (kFrameSequenceTrackerTypeCount + kStagesWithBreakdownCount);

constexpr base::TimeDelta kCompositorLatencyHistogramMin =
    base::Microseconds(1);
constexpr base::TimeDelta kCompositorLatencyHistogramMax =
    base::Milliseconds(350);
constexpr int kCompositorLatencyHistogramBucketCount = 50;

constexpr const char kEventLatencyBaseHistogramName[] = "EventLatency";
constexpr int kEventLatencyEventTypeCount =
    static_cast<int>(EventMetrics::EventType::kMaxValue) + 1;
constexpr const char kGenerationToBrowserMainName[] = "GenerationToBrowserMain";

// Scroll and pinch events report a separate metrics for each input type. Scroll
// events also report an aggregate metric over all input types. Other event
// types just report one aggregate metric. So, maximum possible metrics for an
// event type is `max(scroll-types-count, pinch-types-count) + 1`.
constexpr int kEventLatencyScrollTypeCount =
    static_cast<int>(ScrollEventMetrics::ScrollType::kMaxValue) + 1;
constexpr int kEventLatencyPinchTypeCount =
    static_cast<int>(PinchEventMetrics::PinchType::kMaxValue) + 1;
constexpr int kEventLatencyGestureTypeCount =
    std::max(kEventLatencyScrollTypeCount, kEventLatencyPinchTypeCount) + 1;

constexpr int kMaxEventLatencyHistogramIndex =
    kEventLatencyEventTypeCount * kEventLatencyGestureTypeCount;
constexpr base::TimeDelta kEventLatencyHistogramMin = base::Microseconds(1);
constexpr base::TimeDelta kEventLatencyHistogramMax = base::Seconds(5);
constexpr int kEventLatencyHistogramBucketCount = 100;
constexpr base::TimeDelta kHighLatencyMin = base::Milliseconds(75);

// Number of breakdown stages of the current PipelineReporter
constexpr int kNumOfCompositorStages =
    static_cast<int>(StageType::kStageTypeCount) - 1;
// Number of breakdown stages of the blink
constexpr int kNumOfBlinkStages =
    static_cast<int>(BlinkBreakdown::kBreakdownCount);
// Number of breakdown stages of the viz
constexpr int kNumOfVizStages = static_cast<int>(VizBreakdown::kBreakdownCount);
// Number of dispatch stages of the current EventLatency
constexpr int kNumDispatchStages =
    static_cast<int>(EventMetrics::DispatchStage::kMaxValue);
// Stores the weight of the most recent data point used in percentage when
// predicting substages' latency. (It is stored and calculated in percentage
// since TimeDelta calculate based on microseconds instead of nanoseconds,
// therefore, decimals of stage durations in microseconds may be lost.)
constexpr double kWeightOfCurStageInPercent = 25;
// Used for comparing doubles
constexpr double kEpsilon = 0.001;

std::string GetCompositorLatencyHistogramName(
    FrameReportType report_type,
    FrameSequenceTrackerType frame_sequence_tracker_type,
    StageType stage_type,
    std::optional<VizBreakdown> viz_breakdown,
    std::optional<BlinkBreakdown> blink_breakdown) {
  DCHECK_LE(frame_sequence_tracker_type, FrameSequenceTrackerType::kMaxType);
  const char* tracker_type_name =
      FrameSequenceTracker::GetFrameSequenceTrackerTypeName(
          frame_sequence_tracker_type);
  DCHECK(tracker_type_name);
  bool impl_only_frame = report_type == FrameReportType::kCompositorOnlyFrame;
  return base::StrCat(
      {"CompositorLatency.", kReportTypeNames[static_cast<int>(report_type)],
       tracker_type_name, *tracker_type_name ? "." : "",
       CompositorFrameReporter::GetStageName(
           stage_type, viz_breakdown, blink_breakdown, impl_only_frame)});
}

// Helper function to record UMA histogram for an EventLatency metric. There
// should be a 1:1 mapping between `name` and `index` to allow the use of
// `STATIC_HISTOGRAM_POINTER_GROUP()` to cache histogram objects.
void ReportEventLatencyMetric(
    const std::string& name,
    int index,
    base::TimeDelta latency,
    const std::optional<EventMetrics::HistogramBucketing>& bucketing,
    bool guiding_metric = false) {
  // Various scrolling metrics have been updated to V2 bucketing
  if (bucketing) {
    std::string versioned_name = name + bucketing->version_suffix;
    STATIC_HISTOGRAM_POINTER_GROUP(
        versioned_name, index, kMaxEventLatencyHistogramIndex,
        AddTimeMicrosecondsGranularity(latency),
        base::Histogram::FactoryMicrosecondsTimeGet(
            versioned_name, bucketing->min, bucketing->max, bucketing->count,
            base::HistogramBase::kUmaTargetedHistogramFlag));
  }

  // Other metrics still used default bucketting. With validation done we no
  // longer want to emit the V1 variants for metrics with bucketing. With the
  // exception of `guiding_metric`. Which should emit both until such a time as
  // we update the list of guiding metrics.
  if (!bucketing || guiding_metric) {
    STATIC_HISTOGRAM_POINTER_GROUP(
        name, index, kMaxEventLatencyHistogramIndex,
        AddTimeMicrosecondsGranularity(latency),
        base::Histogram::FactoryMicrosecondsTimeGet(
            name, kEventLatencyHistogramMin, kEventLatencyHistogramMax,
            kEventLatencyHistogramBucketCount,
            base::HistogramBase::kUmaTargetedHistogramFlag));
  }
}

constexpr char kTraceCategory[] =
    "cc,benchmark," TRACE_DISABLED_BY_DEFAULT("devtools.timeline.frame");

bool IsTracingEnabled() {
  bool enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(kTraceCategory, &enabled);
  return enabled;
}

base::TimeTicks ComputeSafeDeadlineForFrame(const viz::BeginFrameArgs& args) {
  return args.frame_time + (args.interval * 1.5);
}

// Returns the value of the exponentially weighted average for
// SetEventLatencyPredictions.
base::TimeDelta CalculateWeightedAverage(base::TimeDelta previous_value,
                                         base::TimeDelta current_value) {
  if (previous_value.is_negative())
    return current_value;
  return (kWeightOfCurStageInPercent * current_value +
          (100 - kWeightOfCurStageInPercent) * previous_value) /
         100;
}

// Calculate new prediction of latency based on old prediction and current
// latency
base::TimeDelta PredictLatency(base::TimeDelta previous_prediction,
                               base::TimeDelta current_latency) {
  return (kWeightOfCurStageInPercent * current_latency +
          (100 - kWeightOfCurStageInPercent) * previous_prediction) /
         100;
}

double DetermineHighestContribution(
    double contribution_change,
    double highest_contribution_change,
    const std::string& stage_name,
    std::vector<std::string>& high_latency_stages) {
  if (std::abs(contribution_change - highest_contribution_change) < kEpsilon) {
    high_latency_stages.push_back(stage_name);
  } else if (contribution_change > highest_contribution_change) {
    highest_contribution_change = contribution_change;
    high_latency_stages = {stage_name};
  }
  return highest_contribution_change;
}

void TraceScrollJankMetrics(const EventMetrics::List& events_metrics,
                            int32_t fling_input_count,
                            int32_t normal_input_count,
                            perfetto::EventContext& ctx) {
  auto* track_event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
  auto* scroll_data = track_event->set_scroll_deltas();
  float delta = 0;
  float predicted_delta = 0;

  for (const auto& event : events_metrics) {
    auto type = event->type();
    if (type != EventMetrics::EventType::kGestureScrollUpdate &&
        type != EventMetrics::EventType::kFirstGestureScrollUpdate &&
        type != EventMetrics::EventType::kInertialGestureScrollUpdate)
        [[unlikely]] {
      continue;
    }
    auto* scroll_update_event = event->AsScrollUpdate();
    if (scroll_update_event->trace_id().has_value()) {
      scroll_data->add_trace_ids_in_gpu_frame(
          scroll_update_event->trace_id()->value());
      scroll_data->add_segregated_original_deltas_in_gpu_frame_y(
          scroll_update_event->delta());
      scroll_data->add_segregated_predicted_deltas_in_gpu_frame_y(
          scroll_update_event->predicted_delta());
    }
    delta += scroll_update_event->delta();
    predicted_delta += scroll_update_event->predicted_delta();
  }
  scroll_data->set_event_count_in_gpu_frame(fling_input_count +
                                            normal_input_count);
  scroll_data->set_original_delta_in_gpu_frame_y(delta);
  scroll_data->set_predicted_delta_in_gpu_frame_y(predicted_delta);
}

// For measuring the queuing issues with GenerationToBrowserMain we are only
// looking at scrolling events. So we will not create a histogram that
// encompasses all EventMetrics::EventType options.
constexpr int kMaxGestureScrollHistogramIndex = 5;
int GetGestureScrollIndex(EventMetrics::EventType type) {
  switch (type) {
    case EventMetrics::EventType::kFirstGestureScrollUpdate:
      return 0;
    case EventMetrics::EventType::kGestureScrollBegin:
      return 1;
    case EventMetrics::EventType::kGestureScrollEnd:
      return 2;
    case EventMetrics::EventType::kGestureScrollUpdate:
      return 3;
    case EventMetrics::EventType::kInertialGestureScrollUpdate:
      return 4;
    default:
      // We are only interested in 5 categories of EventType for scroll input
      NOTREACHED();
  }
}

// For measuring the ratio of scrolling event generation, as well as arrival in
// the Renderer. Compared to the active VSync at the time of their arrival.
constexpr int kMaxVSyncRatioHistogramIndex =
    kMaxGestureScrollHistogramIndex *
    static_cast<int>(
        CompositorFrameReporter::VSyncRatioType::kVSyncRatioTypeCount);
const char* GetVSyncRatioTypeName(
    CompositorFrameReporter::VSyncRatioType type) {
  switch (type) {
    case CompositorFrameReporter::VSyncRatioType::
        kArrivedInRendererVsVSyncRatioAfterVSync:
      return "ArrivedInRendererVsVSyncRatio.AfterVSync";
    case CompositorFrameReporter::VSyncRatioType::
        kArrivedInRendererVsVSyncRatioBeforeVSync:
      return "ArrivedInRendererVsVSyncRatio.BeforeVSync";
    case CompositorFrameReporter::VSyncRatioType::
        kGenerationVsVsyncRatioAfterVSync:
      return "GenerationVsVsyncRatio.AfterVSync";
    case CompositorFrameReporter::VSyncRatioType::
        kGenerationVsVsyncRatioBeforeVSync:
      return "GenerationVsVsyncRatio.BeforeVSync";
    case CompositorFrameReporter::VSyncRatioType::kVSyncRatioTypeCount:
      NOTREACHED();
  }
}

void ReportVSyncRatioMetric(const std::string& base_histogram_name,
                            int gesture_scroll_index,
                            CompositorFrameReporter::VSyncRatioType type,
                            int percentage) {
  const std::string vsync_ratio_type_name = GetVSyncRatioTypeName(type);
  const std::string histogram_name =
      base::JoinString({base_histogram_name, vsync_ratio_type_name}, ".");
  STATIC_HISTOGRAM_POINTER_GROUP(
      histogram_name,
      gesture_scroll_index +
          static_cast<int>(type) * kMaxGestureScrollHistogramIndex,
      kMaxVSyncRatioHistogramIndex, Add(percentage),
      base::LinearHistogram::FactoryGet(
          histogram_name, 1, 100, 101,
          base::HistogramBase::kUmaTargetedHistogramFlag));
}

#if BUILDFLAG(IS_ANDROID)
constexpr const char kTopControlsMovedName[] = ".TopControlsMoved";
constexpr const char kTopControlsDidNotMoveName[] = ".TopControlsDidNotMove";
void ReportTopControlsMetric(
    const std::string& name,
    bool top_controls_moved,
    base::TimeDelta latency,
    EventMetrics::EventType type,
    const std::optional<EventMetrics::HistogramBucketing>& bucketing) {
  if (!bucketing) {
    return;
  }
  if (top_controls_moved) {
    std::string versioned_name = name + kTopControlsMovedName;
    STATIC_HISTOGRAM_POINTER_GROUP(
        versioned_name, GetGestureScrollIndex(type),
        kMaxGestureScrollHistogramIndex,
        AddTimeMicrosecondsGranularity(latency),
        base::Histogram::FactoryMicrosecondsTimeGet(
            versioned_name, bucketing->min, bucketing->max, bucketing->count,
            base::HistogramBase::kUmaTargetedHistogramFlag));
  } else if (base::ShouldLogHistogramForCpuReductionExperiment()) {
    // We want to sub-sample the reports with top controls not moving. As they
    // dominate in volume.
    std::string versioned_name = name + kTopControlsDidNotMoveName;
    STATIC_HISTOGRAM_POINTER_GROUP(
        versioned_name, GetGestureScrollIndex(type),
        kMaxGestureScrollHistogramIndex,
        AddTimeMicrosecondsGranularity(latency),
        base::Histogram::FactoryMicrosecondsTimeGet(
            versioned_name, bucketing->min, bucketing->max, bucketing->count,
            base::HistogramBase::kUmaTargetedHistogramFlag));
  }
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

// CompositorFrameReporter::ProcessedBlinkBreakdown::Iterator ==================

CompositorFrameReporter::ProcessedBlinkBreakdown::Iterator::Iterator(
    const ProcessedBlinkBreakdown* owner)
    : owner_(owner) {}

CompositorFrameReporter::ProcessedBlinkBreakdown::Iterator::~Iterator() =
    default;

bool CompositorFrameReporter::ProcessedBlinkBreakdown::Iterator::IsValid()
    const {
  return index_ < std::size(owner_->list_);
}

void CompositorFrameReporter::ProcessedBlinkBreakdown::Iterator::Advance() {
  DCHECK(IsValid());
  index_++;
}

BlinkBreakdown
CompositorFrameReporter::ProcessedBlinkBreakdown::Iterator::GetBreakdown()
    const {
  DCHECK(IsValid());
  return static_cast<BlinkBreakdown>(index_);
}

base::TimeDelta
CompositorFrameReporter::ProcessedBlinkBreakdown::Iterator::GetLatency() const {
  DCHECK(IsValid());
  return owner_->list_[index_];
}

// CompositorFrameReporter::ProcessedBlinkBreakdown ============================

CompositorFrameReporter::ProcessedBlinkBreakdown::ProcessedBlinkBreakdown(
    base::TimeTicks blink_start_time,
    base::TimeTicks begin_main_frame_start,
    const BeginMainFrameMetrics& blink_breakdown) {
  if (blink_start_time.is_null())
    return;

  list_[static_cast<int>(BlinkBreakdown::kHandleInputEvents)] =
      blink_breakdown.handle_input_events;
  list_[static_cast<int>(BlinkBreakdown::kAnimate)] = blink_breakdown.animate;
  list_[static_cast<int>(BlinkBreakdown::kStyleUpdate)] =
      blink_breakdown.style_update;
  list_[static_cast<int>(BlinkBreakdown::kLayoutUpdate)] =
      blink_breakdown.layout_update;
  list_[static_cast<int>(BlinkBreakdown::kAccessibility)] =
      blink_breakdown.accessibility;
  list_[static_cast<int>(BlinkBreakdown::kPrepaint)] = blink_breakdown.prepaint;
  list_[static_cast<int>(BlinkBreakdown::kCompositingInputs)] =
      blink_breakdown.compositing_inputs;
  list_[static_cast<int>(BlinkBreakdown::kPaint)] = blink_breakdown.paint;
  list_[static_cast<int>(BlinkBreakdown::kCompositeCommit)] =
      blink_breakdown.composite_commit;
  list_[static_cast<int>(BlinkBreakdown::kUpdateLayers)] =
      blink_breakdown.update_layers;
  list_[static_cast<int>(BlinkBreakdown::kBeginMainSentToStarted)] =
      begin_main_frame_start - blink_start_time;
}

CompositorFrameReporter::ProcessedBlinkBreakdown::~ProcessedBlinkBreakdown() =
    default;

CompositorFrameReporter::ProcessedBlinkBreakdown::Iterator
CompositorFrameReporter::ProcessedBlinkBreakdown::CreateIterator() const {
  return Iterator(this);
}

// CompositorFrameReporter::ProcessedVizBreakdown::Iterator ====================

CompositorFrameReporter::ProcessedVizBreakdown::Iterator::Iterator(
    const ProcessedVizBreakdown* owner,
    bool skip_swap_start_to_swap_end)
    : owner_(owner), skip_swap_start_to_swap_end_(skip_swap_start_to_swap_end) {
  DCHECK(owner_);
  SkipBreakdownsIfNecessary();
}

CompositorFrameReporter::ProcessedVizBreakdown::Iterator::~Iterator() = default;

bool CompositorFrameReporter::ProcessedVizBreakdown::Iterator::IsValid() const {
  return index_ < std::size(owner_->list_);
}

void CompositorFrameReporter::ProcessedVizBreakdown::Iterator::Advance() {
  DCHECK(HasValue());
  index_++;
  SkipBreakdownsIfNecessary();
}

VizBreakdown
CompositorFrameReporter::ProcessedVizBreakdown::Iterator::GetBreakdown() const {
  DCHECK(HasValue());
  return static_cast<VizBreakdown>(index_);
}

base::TimeTicks
CompositorFrameReporter::ProcessedVizBreakdown::Iterator::GetStartTime() const {
  DCHECK(HasValue());
  return owner_->list_[index_]->first;
}

base::TimeTicks
CompositorFrameReporter::ProcessedVizBreakdown::Iterator::GetEndTime() const {
  DCHECK(HasValue());
  return owner_->list_[index_]->second;
}

base::TimeDelta
CompositorFrameReporter::ProcessedVizBreakdown::Iterator::GetDuration() const {
  DCHECK(HasValue());
  return owner_->list_[index_]->second - owner_->list_[index_]->first;
}

bool CompositorFrameReporter::ProcessedVizBreakdown::Iterator::HasValue()
    const {
  DCHECK(IsValid());
  return owner_->list_[index_].has_value();
}

void CompositorFrameReporter::ProcessedVizBreakdown::Iterator::
    SkipBreakdownsIfNecessary() {
  while (IsValid() &&
         (!HasValue() ||
          (GetBreakdown() ==
               CompositorFrameReporter::VizBreakdown::kSwapStartToSwapEnd &&
           skip_swap_start_to_swap_end_))) {
    index_++;
  }
}

// CompositorFrameReporter::ProcessedVizBreakdown ==============================

CompositorFrameReporter::ProcessedVizBreakdown::ProcessedVizBreakdown(
    base::TimeTicks viz_start_time,
    const viz::FrameTimingDetails& viz_breakdown) {
  if (viz_start_time.is_null())
    return;

  // Check if `viz_breakdown` is set. Testing indicates that sometimes the
  // received_compositor_frame_timestamp can be earlier than the given
  // `viz_start_time`. Avoid reporting negative times.
  if (viz_breakdown.received_compositor_frame_timestamp.is_null() ||
      viz_breakdown.received_compositor_frame_timestamp < viz_start_time) {
    return;
  }
  list_[static_cast<int>(VizBreakdown::kSubmitToReceiveCompositorFrame)] =
      std::make_pair(viz_start_time,
                     viz_breakdown.received_compositor_frame_timestamp);

  if (viz_breakdown.draw_start_timestamp.is_null())
    return;
  list_[static_cast<int>(VizBreakdown::kReceivedCompositorFrameToStartDraw)] =
      std::make_pair(viz_breakdown.received_compositor_frame_timestamp,
                     viz_breakdown.draw_start_timestamp);

  if (viz_breakdown.swap_timings.is_null())
    return;
  list_[static_cast<int>(VizBreakdown::kStartDrawToSwapStart)] =
      std::make_pair(viz_breakdown.draw_start_timestamp,
                     viz_breakdown.swap_timings.swap_start);

  list_[static_cast<int>(VizBreakdown::kSwapStartToSwapEnd)] =
      std::make_pair(viz_breakdown.swap_timings.swap_start,
                     viz_breakdown.swap_timings.swap_end);

  list_[static_cast<int>(VizBreakdown::kSwapEndToPresentationCompositorFrame)] =
      std::make_pair(viz_breakdown.swap_timings.swap_end,
                     viz_breakdown.presentation_feedback.timestamp);
  swap_start_ = viz_breakdown.swap_timings.swap_start;

  if (viz_breakdown.presentation_feedback.ready_timestamp.is_null())
    return;
  buffer_ready_available_ = true;
  list_[static_cast<int>(VizBreakdown::kSwapStartToBufferAvailable)] =
      std::make_pair(viz_breakdown.swap_timings.swap_start,
                     viz_breakdown.presentation_feedback.available_timestamp);
  list_[static_cast<int>(VizBreakdown::kBufferAvailableToBufferReady)] =
      std::make_pair(viz_breakdown.presentation_feedback.available_timestamp,
                     viz_breakdown.presentation_feedback.ready_timestamp);
  list_[static_cast<int>(VizBreakdown::kBufferReadyToLatch)] =
      std::make_pair(viz_breakdown.presentation_feedback.ready_timestamp,
                     viz_breakdown.presentation_feedback.latch_timestamp);
  list_[static_cast<int>(VizBreakdown::kLatchToSwapEnd)] =
      std::make_pair(viz_breakdown.presentation_feedback.latch_timestamp,
                     viz_breakdown.swap_timings.swap_end);
}

CompositorFrameReporter::ProcessedVizBreakdown::~ProcessedVizBreakdown() =
    default;

CompositorFrameReporter::ProcessedVizBreakdown::Iterator
CompositorFrameReporter::ProcessedVizBreakdown::CreateIterator(
    bool skip_swap_start_to_swap_end_if_breakdown_available) const {
  return Iterator(this, skip_swap_start_to_swap_end_if_breakdown_available &&
                            buffer_ready_available_);
}

// CompositorFrameReporter::CompositorLatencyInfo ==============================

CompositorFrameReporter::CompositorLatencyInfo::CompositorLatencyInfo() =
    default;
CompositorFrameReporter::CompositorLatencyInfo::CompositorLatencyInfo(
    base::TimeDelta init_value)
    : top_level_stages(kNumOfCompositorStages, init_value),
      blink_breakdown_stages(kNumOfBlinkStages, init_value),
      viz_breakdown_stages(kNumOfVizStages, init_value),
      total_latency(init_value),
      total_blink_latency(init_value),
      total_viz_latency(init_value) {}
CompositorFrameReporter::CompositorLatencyInfo::~CompositorLatencyInfo() =
    default;

// CompositorFrameReporter =====================================================

CompositorFrameReporter::CompositorFrameReporter(
    const ActiveTrackers& active_trackers,
    const viz::BeginFrameArgs& args,
    bool should_report_histograms,
    SmoothThread smooth_thread,
    FrameInfo::SmoothEffectDrivingThread scrolling_thread,
    int layer_tree_host_id,
    const GlobalMetricsTrackers& trackers)
    : should_report_histograms_(should_report_histograms),
      args_(args),
      active_trackers_(active_trackers),
      scrolling_thread_(scrolling_thread),
      smooth_thread_(smooth_thread),
      layer_tree_host_id_(layer_tree_host_id),
      global_trackers_(trackers) {
  DCHECK(global_trackers_.dropped_frame_counter);
  global_trackers_.dropped_frame_counter->OnBeginFrame(args);
  DCHECK(IsScrollActive(active_trackers_) ||
         scrolling_thread_ == FrameInfo::SmoothEffectDrivingThread::kUnknown);
  if (scrolling_thread_ == FrameInfo::SmoothEffectDrivingThread::kCompositor) {
    DCHECK(smooth_thread_ == SmoothThread::kSmoothCompositor ||
           smooth_thread_ == SmoothThread::kSmoothBoth);
  } else if (scrolling_thread_ == FrameInfo::SmoothEffectDrivingThread::kMain) {
    DCHECK(smooth_thread_ == SmoothThread::kSmoothMain ||
           smooth_thread_ == SmoothThread::kSmoothBoth);
  }
  // If we have a SET version of the animation, then we should also have a
  // non-SET version of the same animation.
  DCHECK(!active_trackers_.test(static_cast<size_t>(
             FrameSequenceTrackerType::kSETCompositorAnimation)) ||
         active_trackers_.test(static_cast<size_t>(
             FrameSequenceTrackerType::kCompositorAnimation)));
  DCHECK(!active_trackers_.test(static_cast<size_t>(
             FrameSequenceTrackerType::kSETMainThreadAnimation)) ||
         active_trackers_.test(static_cast<size_t>(
             FrameSequenceTrackerType::kMainThreadAnimation)));
  is_forked_ = false;
  is_backfill_ = false;
}

// static
const char* CompositorFrameReporter::GetStageName(
    StageType stage_type,
    std::optional<VizBreakdown> viz_breakdown,
    std::optional<BlinkBreakdown> blink_breakdown,
    bool impl_only) {
  DCHECK(!viz_breakdown ||
         stage_type ==
             StageType::kSubmitCompositorFrameToPresentationCompositorFrame);
  DCHECK(!blink_breakdown ||
         stage_type == StageType::kSendBeginMainFrameToCommit);
  switch (stage_type) {
    case StageType::kBeginImplFrameToSendBeginMainFrame:
      return impl_only ? "BeginImplFrameToFinishImpl"
                       : "BeginImplFrameToSendBeginMainFrame";
    case StageType::kSendBeginMainFrameToCommit:
      if (!blink_breakdown) {
        return impl_only ? "SendBeginMainFrameToBeginMainAbort"
                         : "SendBeginMainFrameToCommit";
      }
      switch (*blink_breakdown) {
        case BlinkBreakdown::kHandleInputEvents:
          return "SendBeginMainFrameToCommit.HandleInputEvents";
        case BlinkBreakdown::kAnimate:
          return "SendBeginMainFrameToCommit.Animate";
        case BlinkBreakdown::kStyleUpdate:
          return "SendBeginMainFrameToCommit.StyleUpdate";
        case BlinkBreakdown::kLayoutUpdate:
          return "SendBeginMainFrameToCommit.LayoutUpdate";
        case BlinkBreakdown::kAccessibility:
          return "SendBeginMainFrameToCommit.AccessibiltyUpdate";
        case BlinkBreakdown::kPrepaint:
          return "SendBeginMainFrameToCommit.Prepaint";
        case BlinkBreakdown::kCompositingInputs:
          return "SendBeginMainFrameToCommit.CompositingInputs";
        case BlinkBreakdown::kPaint:
          return "SendBeginMainFrameToCommit.Paint";
        case BlinkBreakdown::kCompositeCommit:
          return "SendBeginMainFrameToCommit.CompositeCommit";
        case BlinkBreakdown::kUpdateLayers:
          return "SendBeginMainFrameToCommit.UpdateLayers";
        case BlinkBreakdown::kBeginMainSentToStarted:
          return "SendBeginMainFrameToCommit.BeginMainSentToStarted";
        case BlinkBreakdown::kBreakdownCount:
          NOTREACHED();
      }
    case StageType::kCommit:
      return "Commit";
    case StageType::kEndCommitToActivation:
      return "EndCommitToActivation";
    case StageType::kActivation:
      return "Activation";
    case StageType::kEndActivateToSubmitCompositorFrame:
      return impl_only ? "ImplFrameDoneToSubmitCompositorFrame"
                       : "EndActivateToSubmitCompositorFrame";
    case StageType::kSubmitCompositorFrameToPresentationCompositorFrame:
      if (!viz_breakdown)
        return "SubmitCompositorFrameToPresentationCompositorFrame";
      switch (*viz_breakdown) {
        case VizBreakdown::kSubmitToReceiveCompositorFrame:
          return "SubmitCompositorFrameToPresentationCompositorFrame."
                 "SubmitToReceiveCompositorFrame";
        case VizBreakdown::kReceivedCompositorFrameToStartDraw:
          return "SubmitCompositorFrameToPresentationCompositorFrame."
                 "ReceivedCompositorFrameToStartDraw";
        case VizBreakdown::kStartDrawToSwapStart:
          return "SubmitCompositorFrameToPresentationCompositorFrame."
                 "StartDrawToSwapStart";
        case VizBreakdown::kSwapStartToSwapEnd:
          return "SubmitCompositorFrameToPresentationCompositorFrame."
                 "SwapStartToSwapEnd";
        case VizBreakdown::kSwapEndToPresentationCompositorFrame:
          return "SubmitCompositorFrameToPresentationCompositorFrame."
                 "SwapEndToPresentationCompositorFrame";
        case VizBreakdown::kSwapStartToBufferAvailable:
          return "SubmitCompositorFrameToPresentationCompositorFrame."
                 "SwapStartToBufferAvailable";
        case VizBreakdown::kBufferAvailableToBufferReady:
          return "SubmitCompositorFrameToPresentationCompositorFrame."
                 "BufferAvailableToBufferReady";
        case VizBreakdown::kBufferReadyToLatch:
          return "SubmitCompositorFrameToPresentationCompositorFrame."
                 "BufferReadyToLatch";
        case VizBreakdown::kLatchToSwapEnd:
          return "SubmitCompositorFrameToPresentationCompositorFrame."
                 "LatchToSwapEnd";
        case VizBreakdown::kBreakdownCount:
          NOTREACHED();
      }
    case StageType::kTotalLatency:
      return "TotalLatency";
    case StageType::kStageTypeCount:
      NOTREACHED();
  }
}

// static
const char* CompositorFrameReporter::GetVizBreakdownName(
    VizBreakdown breakdown) {
  switch (breakdown) {
    case VizBreakdown::kSubmitToReceiveCompositorFrame:
      return "SubmitToReceiveCompositorFrame";
    case VizBreakdown::kReceivedCompositorFrameToStartDraw:
      return "ReceiveCompositorFrameToStartDraw";
    case VizBreakdown::kStartDrawToSwapStart:
      return "StartDrawToSwapStart";
    case VizBreakdown::kSwapStartToSwapEnd:
      return "Swap";
    case VizBreakdown::kSwapEndToPresentationCompositorFrame:
      return "SwapEndToPresentationCompositorFrame";
    case VizBreakdown::kSwapStartToBufferAvailable:
      return "SwapStartToBufferAvailable";
    case VizBreakdown::kBufferAvailableToBufferReady:
      return "BufferAvailableToBufferReady";
    case VizBreakdown::kBufferReadyToLatch:
      return "BufferReadyToLatch";
    case VizBreakdown::kLatchToSwapEnd:
      return "LatchToSwapEnd";
    case VizBreakdown::kBreakdownCount:
      NOTREACHED();
  }
}

std::unique_ptr<CompositorFrameReporter>
CompositorFrameReporter::CopyReporterAtBeginImplStage() {
  // If |this| reporter is dependent on another reporter to decide about partial
  // update, then |this| should not have any such dependents.
  DCHECK(!partial_update_decider_);

  if (stage_history_.empty() ||
      stage_history_.front().stage_type !=
          StageType::kBeginImplFrameToSendBeginMainFrame ||
      (!did_finish_impl_frame() && !did_not_produce_frame_time_.has_value())) {
    return nullptr;
  }
  auto new_reporter = std::make_unique<CompositorFrameReporter>(
      active_trackers_, args_, should_report_histograms_, smooth_thread_,
      scrolling_thread_, layer_tree_host_id_, global_trackers_);
  new_reporter->did_finish_impl_frame_ = did_finish_impl_frame_;
  new_reporter->impl_frame_finish_time_ = impl_frame_finish_time_;
  new_reporter->main_frame_abort_time_ = main_frame_abort_time_;
  new_reporter->current_stage_.stage_type =
      StageType::kBeginImplFrameToSendBeginMainFrame;
  new_reporter->current_stage_.start_time = stage_history_.front().start_time;
  new_reporter->set_tick_clock(tick_clock_);
  new_reporter->set_is_forked(true);

  // Set up the new reporter so that it depends on |this| for partial update
  // information.
  new_reporter->SetPartialUpdateDecider(this);

  return new_reporter;
}

CompositorFrameReporter::~CompositorFrameReporter() {
  TerminateReporter();
}

CompositorFrameReporter::StageData::StageData() = default;
CompositorFrameReporter::StageData::StageData(StageType stage_type,
                                              base::TimeTicks start_time,
                                              base::TimeTicks end_time)
    : stage_type(stage_type), start_time(start_time), end_time(end_time) {}
CompositorFrameReporter::StageData::StageData(const StageData&) = default;
CompositorFrameReporter::StageData::~StageData() = default;

CompositorFrameReporter::EventLatencyInfo::EventLatencyInfo(
    const int num_dispatch_stages,
    const int num_compositor_stages)
    : dispatch_durations(num_dispatch_stages, base::Microseconds(-1)),
      transition_duration(base::Microseconds(-1)),
      compositor_durations(num_compositor_stages, base::Microseconds(-1)),
      total_duration(base::Microseconds(-1)),
      transition_name("") {}
CompositorFrameReporter::EventLatencyInfo::~EventLatencyInfo() = default;

void CompositorFrameReporter::StartStage(
    CompositorFrameReporter::StageType stage_type,
    base::TimeTicks start_time) {
  if (frame_termination_status_ != FrameTerminationStatus::kUnknown)
    return;
  EndCurrentStage(start_time);
  current_stage_.stage_type = stage_type;
  current_stage_.start_time = start_time;
  switch (stage_type) {
    case StageType::kSendBeginMainFrameToCommit:
      DCHECK(blink_start_time_.is_null());
      blink_start_time_ = start_time;
      break;
    case StageType::kSubmitCompositorFrameToPresentationCompositorFrame:
      DCHECK(viz_start_time_.is_null());
      viz_start_time_ = start_time;
      break;
    default:
      break;
  }
}

void CompositorFrameReporter::TerminateFrame(
    FrameTerminationStatus termination_status,
    base::TimeTicks termination_time) {
  // If the reporter is already terminated, (possibly as a result of no damage)
  // then we don't need to do anything here, otherwise the reporter will be
  // terminated.
  if (frame_termination_status_ != FrameTerminationStatus::kUnknown)
    return;
  frame_termination_status_ = termination_status;
  frame_termination_time_ = termination_time;
  EndCurrentStage(frame_termination_time_);
}

void CompositorFrameReporter::OnFinishImplFrame(base::TimeTicks timestamp) {
  DCHECK(!did_finish_impl_frame_);

  did_finish_impl_frame_ = true;
  impl_frame_finish_time_ = timestamp;
}

void CompositorFrameReporter::OnAbortBeginMainFrame(base::TimeTicks timestamp) {
  DCHECK(!main_frame_abort_time_.has_value());
  main_frame_abort_time_ = timestamp;
  impl_frame_finish_time_ = timestamp;
  // impl_frame_finish_time_ can be used for the end of BeginMain to Commit
  // stage.
}

void CompositorFrameReporter::OnDidNotProduceFrame(
    FrameSkippedReason skip_reason) {
  did_not_produce_frame_time_ = Now();
  frame_skip_reason_ = skip_reason;
}

void CompositorFrameReporter::EnableCompositorOnlyReporting() {
  EnableReportType(FrameReportType::kCompositorOnlyFrame);
}

void CompositorFrameReporter::SetBlinkBreakdown(
    std::unique_ptr<BeginMainFrameMetrics> blink_breakdown,
    base::TimeTicks begin_main_start) {
  DCHECK(blink_breakdown_.paint.is_zero());
  if (blink_breakdown)
    blink_breakdown_ = *blink_breakdown;
  else
    blink_breakdown_ = BeginMainFrameMetrics();

  DCHECK(begin_main_frame_start_.is_null());
  begin_main_frame_start_ = begin_main_start;
}

void CompositorFrameReporter::SetVizBreakdown(
    const viz::FrameTimingDetails& viz_breakdown) {
  DCHECK(viz_breakdown_.received_compositor_frame_timestamp.is_null());
  viz_breakdown_ = viz_breakdown;
}

void CompositorFrameReporter::AddEventsMetrics(
    EventMetrics::List events_metrics) {
  events_metrics_.insert(events_metrics_.end(),
                         std::make_move_iterator(events_metrics.begin()),
                         std::make_move_iterator(events_metrics.end()));
}

EventMetrics::List CompositorFrameReporter::TakeEventsMetrics() {
  EventMetrics::List result = std::move(events_metrics_);
  events_metrics_.clear();
  return result;
}

EventMetrics::List CompositorFrameReporter::TakeMainBlockedEventsMetrics() {
  auto mid = std::partition(events_metrics_.begin(), events_metrics_.end(),
                            [](std::unique_ptr<EventMetrics>& metrics) {
                              DCHECK(metrics);
                              bool is_blocked_on_main =
                                  metrics->requires_main_thread_update();
                              // Invert so we can take from the end.
                              return !is_blocked_on_main;
                            });
  EventMetrics::List result(std::make_move_iterator(mid),
                            std::make_move_iterator(events_metrics_.end()));
  events_metrics_.erase(mid, events_metrics_.end());
  return result;
}

void CompositorFrameReporter::DidSuccessfullyPresentFrame() {
  ReportScrollJankMetrics();
}

void CompositorFrameReporter::TerminateReporter() {
  if (frame_termination_status_ == FrameTerminationStatus::kUnknown)
    TerminateFrame(FrameTerminationStatus::kUnknown, Now());

  if (!processed_blink_breakdown_)
    processed_blink_breakdown_ = std::make_unique<ProcessedBlinkBreakdown>(
        blink_start_time_, begin_main_frame_start_, blink_breakdown_);
  if (!processed_viz_breakdown_)
    processed_viz_breakdown_ = std::make_unique<ProcessedVizBreakdown>(
        viz_start_time_, viz_breakdown_);

  DCHECK_EQ(current_stage_.start_time, base::TimeTicks());
  const FrameInfo frame_info = GenerateFrameInfo();
  switch (frame_info.final_state) {
    case FrameFinalState::kDropped:
      EnableReportType(FrameReportType::kDroppedFrame);
      break;

    case FrameFinalState::kNoUpdateDesired:
      // If this reporter was cloned, and the cloned reporter was marked as
      // containing 'partial update' (i.e. missing desired updates from the
      // main-thread), but this reporter terminated with 'no damage', then reset
      // the 'partial update' flag from the cloned reporter (as well as other
      // depending reporters).
      while (!partial_update_dependents_.empty()) {
        auto dependent = partial_update_dependents_.front();
        if (dependent)
          dependent->set_has_partial_update(false);
        partial_update_dependents_.pop_front();
      }
      break;

    case FrameFinalState::kPresentedAll:
    case FrameFinalState::kPresentedPartialNewMain:
    case FrameFinalState::kPresentedPartialOldMain:
      EnableReportType(FrameReportType::kNonDroppedFrame);
      if (ComputeSafeDeadlineForFrame(args_) < frame_termination_time_)
        EnableReportType(FrameReportType::kMissedDeadlineFrame);
      break;
  }

  ReportCompositorLatencyTraceEvents(frame_info);
  if (TestReportType(FrameReportType::kNonDroppedFrame))
    ReportEventLatencyTraceEvents();

  // Only report compositor latency metrics if the frame was produced.
  if (report_types_.any() &&
      (should_report_histograms_ || global_trackers_.latency_ukm_reporter ||
       global_trackers_.event_latency_tracker)) {
    DCHECK(stage_history_.size());
    DCHECK_EQ(SumOfStageHistory(), stage_history_.back().end_time -
                                       stage_history_.front().start_time);
    stage_history_.emplace_back(StageType::kTotalLatency,
                                stage_history_.front().start_time,
                                stage_history_.back().end_time);

    ReportCompositorLatencyMetrics();

    // Only report event latency metrics if the frame was presented.
    if (TestReportType(FrameReportType::kNonDroppedFrame)) {
      ReportEventLatencyMetrics();
    }
  }

  if (TestReportType(FrameReportType::kDroppedFrame)) {
    global_trackers_.dropped_frame_counter->AddDroppedFrame();
  } else {
    if (has_partial_update_)
      global_trackers_.dropped_frame_counter->AddPartialFrame();
    else
      global_trackers_.dropped_frame_counter->AddGoodFrame();
  }
  global_trackers_.dropped_frame_counter->OnEndFrame(args_, frame_info);
}

void CompositorFrameReporter::EndCurrentStage(base::TimeTicks end_time) {
  if (current_stage_.start_time == base::TimeTicks())
    return;
  current_stage_.end_time = end_time;
  stage_history_.push_back(current_stage_);
  current_stage_.start_time = base::TimeTicks();
}

void CompositorFrameReporter::ReportCompositorLatencyMetrics() const {
  // Subsampling these metrics reduced CPU utilization (crbug.com/1295441).
  if (!base::ShouldLogHistogramForCpuReductionExperiment()) {
    return;
  }

  if (global_trackers_.latency_ukm_reporter) {
    global_trackers_.latency_ukm_reporter->ReportCompositorLatencyUkm(
        report_types_, stage_history_, active_trackers_,
        *processed_blink_breakdown_, *processed_viz_breakdown_);
  }

  if (!should_report_histograms_)
    return;

  for (const StageData& stage : stage_history_) {
    ReportStageHistogramWithBreakdown(stage);

    if (stage.stage_type == StageType::kTotalLatency) {
      for (size_t type = 0; type < active_trackers_.size(); ++type) {
        if (active_trackers_.test(type)) {
          // Report stage breakdowns.
          ReportStageHistogramWithBreakdown(
              stage, static_cast<FrameSequenceTrackerType>(type));
        }
      }
    }
  }

  for (size_t type = 0; type < report_types_.size(); ++type) {
    if (!report_types_.test(type))
      continue;
    FrameReportType report_type = static_cast<FrameReportType>(type);
    UMA_HISTOGRAM_ENUMERATION("CompositorLatency.Type", report_type);
    bool any_active_interaction = false;
    for (size_t fst_type = 0; fst_type < active_trackers_.size(); ++fst_type) {
      const auto tracker_type = static_cast<FrameSequenceTrackerType>(fst_type);
      if (!active_trackers_.test(fst_type) ||
          tracker_type == FrameSequenceTrackerType::kCustom ||
          tracker_type == FrameSequenceTrackerType::kMaxType) {
        continue;
      }
      any_active_interaction = true;
      switch (tracker_type) {
        case FrameSequenceTrackerType::kCompositorAnimation:
          UMA_HISTOGRAM_ENUMERATION(
              "CompositorLatency.Type.CompositorAnimation", report_type);
          break;
        case FrameSequenceTrackerType::kMainThreadAnimation:
          UMA_HISTOGRAM_ENUMERATION(
              "CompositorLatency.Type.MainThreadAnimation", report_type);
          break;
        case FrameSequenceTrackerType::kPinchZoom:
          UMA_HISTOGRAM_ENUMERATION("CompositorLatency.Type.PinchZoom",
                                    report_type);
          break;
        case FrameSequenceTrackerType::kRAF:
          UMA_HISTOGRAM_ENUMERATION("CompositorLatency.Type.RAF", report_type);
          break;
        case FrameSequenceTrackerType::kTouchScroll:
          UMA_HISTOGRAM_ENUMERATION("CompositorLatency.Type.TouchScroll",
                                    report_type);
          break;
        case FrameSequenceTrackerType::kVideo:
          UMA_HISTOGRAM_ENUMERATION("CompositorLatency.Type.Video",
                                    report_type);
          break;
        case FrameSequenceTrackerType::kWheelScroll:
          UMA_HISTOGRAM_ENUMERATION("CompositorLatency.Type.WheelScroll",
                                    report_type);
          break;
        case FrameSequenceTrackerType::kScrollbarScroll:
          UMA_HISTOGRAM_ENUMERATION("CompositorLatency.Type.ScrollbarScroll",
                                    report_type);
          break;
        case FrameSequenceTrackerType::kCanvasAnimation:
          UMA_HISTOGRAM_ENUMERATION("CompositorLatency.Type.CanvasAnimation",
                                    report_type);
          break;
        case FrameSequenceTrackerType::kJSAnimation:
          UMA_HISTOGRAM_ENUMERATION("CompositorLatency.Type.JSAnimation",
                                    report_type);
          break;
        case FrameSequenceTrackerType::kSETCompositorAnimation:
          UMA_HISTOGRAM_ENUMERATION(
              "CompositorLatency.Type.SETCompositorAnimation", report_type);
          break;
        case FrameSequenceTrackerType::kSETMainThreadAnimation:
          UMA_HISTOGRAM_ENUMERATION(
              "CompositorLatency.Type.SETMainThreadAnimation", report_type);
          break;
        case FrameSequenceTrackerType::kCustom:
        case FrameSequenceTrackerType::kMaxType:
          NOTREACHED();
      }
    }
    if (any_active_interaction) {
      UMA_HISTOGRAM_ENUMERATION("CompositorLatency.Type.AnyInteraction",
                                report_type);
    } else {
      UMA_HISTOGRAM_ENUMERATION("CompositorLatency.Type.NoInteraction",
                                report_type);
    }
  }

  // Only report the IPC and Thread latency when we have valid timestamps.
  if (args_.frame_time.is_null() || args_.dispatch_time.is_null() ||
      args_.client_arrival_time.is_null()) {
    return;
  }
  // Only report if `frame_time` is earlier than `dispatch_time` to avoid cases
  // where we are dispatching is advance of the expected frame start.
  base::TimeDelta vsync_viz_delta;
  if (args_.dispatch_time > args_.frame_time) {
    vsync_viz_delta = args_.dispatch_time - args_.frame_time;
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "CompositorLatency.IpcThread.FrameTimeToDispatch", vsync_viz_delta,
        kCompositorLatencyHistogramMin, kCompositorLatencyHistogramMax,
        kCompositorLatencyHistogramBucketCount);
  }
  const base::TimeDelta viz_cc_delta =
      args_.client_arrival_time - args_.dispatch_time;
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "CompositorLatency.IpcThread.DispatchToRenderer", viz_cc_delta,
      kCompositorLatencyHistogramMin, kCompositorLatencyHistogramMax,
      kCompositorLatencyHistogramBucketCount);

  // If we don't have Main thread work, report just Impl-thread total latency.
  if (begin_main_frame_start_.is_null() || blink_start_time_.is_null()) {
    const base::TimeDelta impl_total_latency = vsync_viz_delta + viz_cc_delta;
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "CompositorLatency.IpcThread.ImplThreadTotalLatency",
        impl_total_latency, kCompositorLatencyHistogramMin,
        kCompositorLatencyHistogramMax, kCompositorLatencyHistogramBucketCount);
    return;
  }
  const base::TimeDelta impl_main_delta =
      begin_main_frame_start_ - blink_start_time_;
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "CompositorLatency.IpcThread.BeginMainFrameQueuing", impl_main_delta,
      kCompositorLatencyHistogramMin, kCompositorLatencyHistogramMax,
      kCompositorLatencyHistogramBucketCount);
  const base::TimeDelta main_total_latency =
      vsync_viz_delta + viz_cc_delta + impl_main_delta;
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "CompositorLatency.IpcThread.MainThreadTotalLatency", main_total_latency,
      kCompositorLatencyHistogramMin, kCompositorLatencyHistogramMax,
      kCompositorLatencyHistogramBucketCount);
}

void CompositorFrameReporter::ReportStageHistogramWithBreakdown(
    const CompositorFrameReporter::StageData& stage,
    FrameSequenceTrackerType frame_sequence_tracker_type) const {
  base::TimeDelta stage_delta = stage.end_time - stage.start_time;
  ReportCompositorLatencyHistogram(
      frame_sequence_tracker_type, stage.stage_type,
      /*viz_breakdown=*/std::nullopt,
      /*blink_breakdown=*/std::nullopt, stage_delta);
  switch (stage.stage_type) {
    case StageType::kSendBeginMainFrameToCommit:
      ReportCompositorLatencyBlinkBreakdowns(frame_sequence_tracker_type);
      break;
    case StageType::kSubmitCompositorFrameToPresentationCompositorFrame:
      ReportCompositorLatencyVizBreakdowns(frame_sequence_tracker_type);
      break;
    default:
      break;
  }
}

void CompositorFrameReporter::ReportCompositorLatencyBlinkBreakdowns(
    FrameSequenceTrackerType frame_sequence_tracker_type) const {
  DCHECK(processed_blink_breakdown_);
  for (auto it = processed_blink_breakdown_->CreateIterator(); it.IsValid();
       it.Advance()) {
    ReportCompositorLatencyHistogram(
        frame_sequence_tracker_type, StageType::kSendBeginMainFrameToCommit,
        /*viz_breakdown=*/std::nullopt, it.GetBreakdown(), it.GetLatency());
  }
}

void CompositorFrameReporter::ReportCompositorLatencyVizBreakdowns(
    FrameSequenceTrackerType frame_sequence_tracker_type) const {
  DCHECK(processed_viz_breakdown_);
  for (auto it = processed_viz_breakdown_->CreateIterator(false); it.IsValid();
       it.Advance()) {
    ReportCompositorLatencyHistogram(
        frame_sequence_tracker_type,
        StageType::kSubmitCompositorFrameToPresentationCompositorFrame,
        it.GetBreakdown(), /*blink_breakdown=*/std::nullopt, it.GetDuration());
  }
}

void CompositorFrameReporter::ReportCompositorLatencyHistogram(
    FrameSequenceTrackerType frame_sequence_tracker_type,
    StageType stage_type,
    std::optional<VizBreakdown> viz_breakdown,
    std::optional<BlinkBreakdown> blink_breakdown,
    base::TimeDelta time_delta) const {
  DCHECK(!viz_breakdown ||
         stage_type ==
             StageType::kSubmitCompositorFrameToPresentationCompositorFrame);
  DCHECK(!blink_breakdown ||
         stage_type == StageType::kSendBeginMainFrameToCommit);
  for (size_t type = 0; type < report_types_.size(); ++type) {
    if (!report_types_.test(type))
      continue;
    FrameReportType report_type = static_cast<FrameReportType>(type);
    const int report_type_index = static_cast<int>(report_type);
    const int frame_sequence_tracker_type_index =
        static_cast<int>(frame_sequence_tracker_type);
    const int stage_type_index =
        blink_breakdown
            ? kBlinkBreakdownInitialIndex + static_cast<int>(*blink_breakdown)
        : viz_breakdown
            ? kVizBreakdownInitialIndex + static_cast<int>(*viz_breakdown)
            : static_cast<int>(stage_type);
    const int histogram_index =
        (stage_type_index == static_cast<int>(StageType::kTotalLatency)
             ? kStagesWithBreakdownCount + frame_sequence_tracker_type_index
             : stage_type_index) *
            kFrameReportTypeCount +
        report_type_index;

    CHECK_LT(stage_type_index, kStagesWithBreakdownCount);
    CHECK_GE(stage_type_index, 0);
    CHECK_LT(report_type_index, kFrameReportTypeCount);
    CHECK_GE(report_type_index, 0);
    CHECK_LT(histogram_index, kMaxCompositorLatencyHistogramIndex);
    CHECK_GE(histogram_index, 0);

    // Note: There's a 1:1 mapping between `histogram_index` and the name
    // returned by `GetCompositorLatencyHistogramName()` which allows the use of
    // `STATIC_HISTOGRAM_POINTER_GROUP()` to cache histogram objects.
    STATIC_HISTOGRAM_POINTER_GROUP(
        GetCompositorLatencyHistogramName(
            report_type, frame_sequence_tracker_type, stage_type, viz_breakdown,
            blink_breakdown),
        histogram_index, kMaxCompositorLatencyHistogramIndex,
        AddTimeMicrosecondsGranularity(time_delta),
        base::Histogram::FactoryMicrosecondsTimeGet(
            GetCompositorLatencyHistogramName(
                report_type, frame_sequence_tracker_type, stage_type,
                viz_breakdown, blink_breakdown),
            kCompositorLatencyHistogramMin, kCompositorLatencyHistogramMax,
            kCompositorLatencyHistogramBucketCount,
            base::HistogramBase::kUmaTargetedHistogramFlag));
  }
}

void CompositorFrameReporter::ReportEventLatencyMetrics() const {
  const StageData& total_latency_stage = stage_history_.back();
  DCHECK_EQ(StageType::kTotalLatency, total_latency_stage.stage_type);

  if (global_trackers_.latency_ukm_reporter) {
    global_trackers_.latency_ukm_reporter->ReportEventLatencyUkm(
        events_metrics_, stage_history_, *processed_blink_breakdown_,
        *processed_viz_breakdown_);
  }

  std::vector<EventLatencyTracker::LatencyData> latencies;

  for (const auto& event_metrics : events_metrics_) {
    DCHECK(event_metrics);
    auto* scroll_metrics = event_metrics->AsScroll();
    auto* pinch_metrics = event_metrics->AsPinch();

    const base::TimeTicks generated_timestamp =
        event_metrics->GetDispatchStageTimestamp(
            EventMetrics::DispatchStage::kGenerated);
    // Generally, we expect that the event timestamp is strictly smaller than
    // the end timestamp of the last stage (i.e. total latency is positive);
    // however, at least in tests, it is possible that the timestamps are the
    // same and total latency is zero.
    DCHECK_LE(generated_timestamp, total_latency_stage.end_time);
    const base::TimeDelta total_latency =
        total_latency_stage.end_time - generated_timestamp;

    if (should_report_histograms_) {
      const std::string histogram_base_name = base::JoinString(
          {kEventLatencyBaseHistogramName, event_metrics->GetTypeName()}, ".");
      const int event_histogram_index = static_cast<int>(event_metrics->type());
      const std::string total_latency_stage_name =
          GetStageName(StageType::kTotalLatency);

      // For pinch events, we only report metrics for each device type and not
      // the aggregate metric over all device types.
      if (!pinch_metrics) {
        const std::string event_total_latency_histogram_name = base::JoinString(
            {histogram_base_name, total_latency_stage_name}, ".");
        ReportEventLatencyMetric(event_total_latency_histogram_name,
                                 event_histogram_index, total_latency,
                                 event_metrics->GetHistogramBucketing());
      }

      // For scroll and pinch events, report metrics for each device type
      // separately.
      if (scroll_metrics || pinch_metrics) {
        const int gesture_type_index =
            1 + (scroll_metrics
                     ? static_cast<int>(scroll_metrics->scroll_type())
                     : static_cast<int>(pinch_metrics->pinch_type()));
        const int gesture_histogram_index =
            event_histogram_index * kEventLatencyGestureTypeCount +
            gesture_type_index;
        const std::string gesture_type_name =
            scroll_metrics ? scroll_metrics->GetScrollTypeName()
                           : pinch_metrics->GetPinchTypeName();

        const std::string gesture_total_latency_histogram_name =
            base::JoinString({histogram_base_name, gesture_type_name,
                              total_latency_stage_name},
                             ".");
        // Currently EventLatency.GestureScrollUpdate.Touchscreen.TotalLatency
        // is a guiding metric. So we want to have it emit both V1 and V2.
        const bool guiding_metric =
            scroll_metrics &&
            event_metrics->type() ==
                EventMetrics::EventType::kGestureScrollUpdate &&
            scroll_metrics->scroll_type() ==
                ScrollEventMetrics::ScrollType::kTouchscreen;
        ReportEventLatencyMetric(gesture_total_latency_histogram_name,
                                 gesture_histogram_index, total_latency,
                                 event_metrics->GetHistogramBucketing(),
                                 guiding_metric);
      }

      if (scroll_metrics) {
        auto& original_args = scroll_metrics->begin_frame_args();
        const base::TimeTicks browser_main_timestamp =
            event_metrics->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kArrivedInBrowserMain);
        const int gesture_scroll_index =
            GetGestureScrollIndex(scroll_metrics->type());
        if (!browser_main_timestamp.is_null()) {
          const std::string generation_to_browser_main_name = base::JoinString(
              {histogram_base_name, kGenerationToBrowserMainName}, ".");
          const base::TimeDelta browser_main_delay =
              browser_main_timestamp - generated_timestamp;
          const std::optional<EventMetrics::HistogramBucketing>& bucketing =
              event_metrics->GetHistogramBucketing();
          if (bucketing) {
            STATIC_HISTOGRAM_POINTER_GROUP(
                generation_to_browser_main_name, gesture_scroll_index,
                kMaxGestureScrollHistogramIndex,
                AddTimeMicrosecondsGranularity(browser_main_delay),
                base::Histogram::FactoryMicrosecondsTimeGet(
                    generation_to_browser_main_name, bucketing->min,
                    bucketing->max, bucketing->count,
                    base::HistogramBase::kUmaTargetedHistogramFlag));
          }
          if (original_args.IsValid()) {
            const base::TimeDelta generation_to_vsync_delta =
                original_args.frame_time - generated_timestamp;
            const double generation_to_vsync_ratio =
                100.f * generation_to_vsync_delta / original_args.interval;
            if (generation_to_vsync_delta.is_negative()) {
              ReportVSyncRatioMetric(histogram_base_name, gesture_scroll_index,
                                     CompositorFrameReporter::VSyncRatioType::
                                         kGenerationVsVsyncRatioBeforeVSync,
                                     std::ceil(generation_to_vsync_ratio * -1));
            } else {
              ReportVSyncRatioMetric(histogram_base_name, gesture_scroll_index,
                                     CompositorFrameReporter::VSyncRatioType::
                                         kGenerationVsVsyncRatioAfterVSync,
                                     std::ceil(generation_to_vsync_ratio));
            }
          }

#if BUILDFLAG(IS_ANDROID)
          ReportTopControlsMetric(histogram_base_name, top_controls_moved_,
                                  total_latency, event_metrics->type(),
                                  event_metrics->GetHistogramBucketing());
#endif  // BUILDFLAG(IS_ANDROID)
        }

        const base::TimeTicks arrived_in_renderer_timestamp =
            event_metrics->GetDispatchStageTimestamp(
                EventMetrics::DispatchStage::kArrivedInRendererCompositor);
        if (original_args.IsValid() &&
            !arrived_in_renderer_timestamp.is_null()) {
          const base::TimeDelta arrived_after_vsync_delta =
              arrived_in_renderer_timestamp - original_args.frame_time;
          const double arrived_after_vsync_ratio =
              100.f * arrived_after_vsync_delta / original_args.interval;
          if (arrived_after_vsync_delta.is_negative()) {
            ReportVSyncRatioMetric(
                histogram_base_name, gesture_scroll_index,
                CompositorFrameReporter::VSyncRatioType::
                    kArrivedInRendererVsVSyncRatioBeforeVSync,
                std::ceil(arrived_after_vsync_ratio * -1));
          } else {
            ReportVSyncRatioMetric(histogram_base_name, gesture_scroll_index,
                                   CompositorFrameReporter::VSyncRatioType::
                                       kArrivedInRendererVsVSyncRatioAfterVSync,
                                   std::ceil(arrived_after_vsync_ratio));
          }
        }
      }

      // Finally, report total latency up to presentation for all event types in
      // a single aggregate histogram.
      const std::string aggregate_total_latency_histogram_name =
          base::JoinString(
              {kEventLatencyBaseHistogramName, total_latency_stage_name}, ".");
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          aggregate_total_latency_histogram_name, total_latency,
          kEventLatencyHistogramMin, kEventLatencyHistogramMax,
          kEventLatencyHistogramBucketCount);
    }

    if (global_trackers_.event_latency_tracker) {
      EventLatencyTracker::LatencyData& latency_data =
          latencies.emplace_back(event_metrics->type(), total_latency);

      if (scroll_metrics)
        latency_data.input_type = scroll_metrics->scroll_type();
      else if (pinch_metrics)
        latency_data.input_type = pinch_metrics->pinch_type();
    }
  }

  if (!latencies.empty()) {
    DCHECK(global_trackers_.event_latency_tracker);
    global_trackers_.event_latency_tracker->ReportEventLatency(
        std::move(latencies));
  }
}

void CompositorFrameReporter::ReportCompositorLatencyTraceEvents(
    const FrameInfo& info) const {
  if (stage_history_.empty())
    return;

  if (info.IsDroppedAffectingSmoothness()) {
    devtools_instrumentation::DidDropSmoothnessFrame(
        layer_tree_host_id_, args_.frame_time, args_.frame_id.sequence_number,
        has_partial_update_);
  }

  if (!IsTracingEnabled()) {
    return;
  }

  const auto trace_track =
      perfetto::Track(base::trace_event::GetNextGlobalTraceId());
  TRACE_EVENT_BEGIN(
      kTraceCategory, "PipelineReporter", trace_track, args_.frame_time,
      [&](perfetto::EventContext context) {
        using perfetto::protos::pbzero::ChromeFrameReporter;
        ChromeFrameReporter::State state;
        switch (info.final_state) {
          case FrameInfo::FrameFinalState::kPresentedAll:
            state = ChromeFrameReporter::STATE_PRESENTED_ALL;
            break;
          case FrameInfo::FrameFinalState::kPresentedPartialNewMain:
          case FrameInfo::FrameFinalState::kPresentedPartialOldMain:
            state = ChromeFrameReporter::STATE_PRESENTED_PARTIAL;
            break;
          case FrameInfo::FrameFinalState::kNoUpdateDesired:
            state = ChromeFrameReporter::STATE_NO_UPDATE_DESIRED;
            break;
          case FrameInfo::FrameFinalState::kDropped:
            state = ChromeFrameReporter::STATE_DROPPED;
            break;
        }

        auto* reporter = context.event()->set_chrome_frame_reporter();
        reporter->set_state(state);
        reporter->set_frame_source(args_.frame_id.source_id);
        reporter->set_frame_sequence(args_.frame_id.sequence_number);
        reporter->set_layer_tree_host_id(layer_tree_host_id_);
        reporter->set_has_missing_content(info.checkerboarded_needs_raster ||
                                          info.checkerboarded_needs_record);
        reporter->set_checkerboarded_needs_raster(
            info.checkerboarded_needs_raster);
        reporter->set_checkerboarded_needs_record(
            info.checkerboarded_needs_record);
        if (info.IsDroppedAffectingSmoothness()) {
          DCHECK(state == ChromeFrameReporter::STATE_DROPPED ||
                 state == ChromeFrameReporter::STATE_PRESENTED_PARTIAL);
        }
        reporter->set_affects_smoothness(info.IsDroppedAffectingSmoothness());
        ChromeFrameReporter::ScrollState scroll_state;
        switch (info.scroll_thread) {
          case FrameInfo::SmoothEffectDrivingThread::kMain:
            scroll_state = ChromeFrameReporter::SCROLL_MAIN_THREAD;
            break;
          case FrameInfo::SmoothEffectDrivingThread::kCompositor:
            scroll_state = ChromeFrameReporter::SCROLL_COMPOSITOR_THREAD;
            break;
          case FrameInfo::SmoothEffectDrivingThread::kUnknown:
            scroll_state = ChromeFrameReporter::SCROLL_NONE;
            break;
        }
        reporter->set_scroll_state(scroll_state);
        reporter->set_has_main_animation(
            HasMainThreadAnimation(active_trackers_));
        reporter->set_has_compositor_animation(
            HasCompositorThreadAnimation(active_trackers_));

        bool has_smooth_input_main = false;
        for (const auto& event_metrics : events_metrics_) {
          has_smooth_input_main |= event_metrics->HasSmoothInputEvent();
        }
        reporter->set_has_smooth_input_main(has_smooth_input_main);
        reporter->set_has_high_latency(
            (frame_termination_time_ - args_.frame_time) > kHighLatencyMin);

        if (is_forked_) {
          reporter->set_frame_type(ChromeFrameReporter::FORKED);
        } else if (is_backfill_) {
          reporter->set_frame_type(ChromeFrameReporter::BACKFILL);
        }

        for (auto stage : high_latency_substages_) {
          reporter->add_high_latency_contribution_stage(stage);
        }

        // TODO(crbug.com/40132773): Set 'drop reason' if applicable.
      });

  for (const auto& stage : stage_history_) {
    if (stage.start_time >= frame_termination_time_)
      break;
    DCHECK_GE(stage.end_time, stage.start_time);
    if (stage.start_time == stage.end_time)
      continue;

    const char* stage_name = GetStageName(stage.stage_type);

    if (stage.stage_type == StageType::kSendBeginMainFrameToCommit) {
      TRACE_EVENT_BEGIN(
          kTraceCategory, perfetto::StaticString{stage_name}, trace_track,
          stage.start_time, [&](perfetto::EventContext context) {
            DCHECK(processed_blink_breakdown_);
            auto* reporter =
                context.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                    ->set_send_begin_mainframe_to_commit_breakdown();
            for (auto it = processed_blink_breakdown_->CreateIterator();
                 it.IsValid(); it.Advance()) {
              int64_t latency = it.GetLatency().InMicroseconds();
              int curr_breakdown = static_cast<int>(it.GetBreakdown());
              switch (curr_breakdown) {
                case static_cast<int>(BlinkBreakdown::kHandleInputEvents):
                  reporter->set_handle_input_events_us(latency);
                  break;
                case static_cast<int>(BlinkBreakdown::kAnimate):
                  reporter->set_animate_us(latency);
                  break;
                case static_cast<int>(BlinkBreakdown::kStyleUpdate):
                  reporter->set_style_update_us(latency);
                  break;
                case static_cast<int>(BlinkBreakdown::kLayoutUpdate):
                  reporter->set_layout_update_us(latency);
                  break;
                case static_cast<int>(BlinkBreakdown::kAccessibility):
                  reporter->set_accessibility_update_us(latency);
                  break;
                case static_cast<int>(BlinkBreakdown::kPrepaint):
                  reporter->set_prepaint_us(latency);
                  break;
                case static_cast<int>(BlinkBreakdown::kCompositingInputs):
                  reporter->set_compositing_inputs_us(latency);
                  break;
                case static_cast<int>(BlinkBreakdown::kPaint):
                  reporter->set_paint_us(latency);
                  break;
                case static_cast<int>(BlinkBreakdown::kCompositeCommit):
                  reporter->set_composite_commit_us(latency);
                  break;
                case static_cast<int>(BlinkBreakdown::kUpdateLayers):
                  reporter->set_update_layers_us(latency);
                  break;
                case static_cast<int>(BlinkBreakdown::kBeginMainSentToStarted):
                  reporter->set_begin_main_sent_to_started_us(latency);
                  break;
                default:
                  break;
              }
            }
          });
    } else {
      TRACE_EVENT_BEGIN(kTraceCategory, perfetto::StaticString{stage_name},
                        trace_track, stage.start_time);
    }

    if (stage.stage_type ==
        StageType::kSubmitCompositorFrameToPresentationCompositorFrame) {
      DCHECK(processed_viz_breakdown_);
      for (auto it = processed_viz_breakdown_->CreateIterator(true);
           it.IsValid(); it.Advance()) {
        base::TimeTicks start_time = it.GetStartTime();
        base::TimeTicks end_time = it.GetEndTime();
        if (start_time >= end_time)
          continue;
        const char* breakdown_name = GetVizBreakdownName(it.GetBreakdown());
        TRACE_EVENT_BEGIN(kTraceCategory,
                          perfetto::StaticString{breakdown_name}, trace_track,
                          start_time);
        TRACE_EVENT_END(kTraceCategory, trace_track, end_time);
      }
    }
    TRACE_EVENT_END(kTraceCategory, trace_track, stage.end_time);
  }

  TRACE_EVENT_END(kTraceCategory, trace_track, frame_termination_time_);
}

void CompositorFrameReporter::ReportScrollJankMetrics() const {
  int32_t fling_input_count = 0;
  int32_t normal_input_count = 0;
  float total_predicted_delta = 0;
  bool had_gesture_scrolls = false;
  bool is_scroll_start = false;

  // This handles cases when we have multiple scroll events. Events for dropped
  // frames are reported by the reporter for next presented frame which could
  // lead to having multiple scroll events.
  EventMetrics* earliest_event = nullptr;
  base::TimeTicks last_coalesced_ts = base::TimeTicks::Min();
  for (const auto& event : events_metrics_) {
    TRACE_EVENT("input", "GestureType", "gesture", event->type());
    const auto* scroll_update = event->AsScrollUpdate();
    if (!scroll_update) {
      continue;
    }

    total_predicted_delta += scroll_update->predicted_delta();
    if (!had_gesture_scrolls) {
      earliest_event = event.get();
    }
    had_gesture_scrolls = true;
    if (earliest_event->GetDispatchStageTimestamp(
            EventMetrics::DispatchStage::kGenerated) <
        event->GetDispatchStageTimestamp(
            EventMetrics::DispatchStage::kGenerated)) {
      earliest_event = event.get();
    }
    last_coalesced_ts =
        std::max(last_coalesced_ts, scroll_update->last_timestamp());

    switch (event->type()) {
      case EventMetrics::EventType::kFirstGestureScrollUpdate:
        is_scroll_start = true;
        ABSL_FALLTHROUGH_INTENDED;
      case EventMetrics::EventType::kGestureScrollUpdate:
        normal_input_count += scroll_update->coalesced_event_count();
        break;
      case EventMetrics::EventType::kInertialGestureScrollUpdate:
        fling_input_count += scroll_update->coalesced_event_count();
        break;
      default:
        NOTREACHED();
    }
  }

  if (!had_gesture_scrolls) {
    return;
  }
  if (is_scroll_start) {
    if (global_trackers_.predictor_jank_tracker) {
      global_trackers_.predictor_jank_tracker->ResetCurrentScrollReporting();
    }
    if (global_trackers_.scroll_jank_dropped_frame_tracker) {
      global_trackers_.scroll_jank_dropped_frame_tracker->OnScrollStarted();
    }
    if (global_trackers_.scroll_jank_ukm_reporter) {
      global_trackers_.scroll_jank_ukm_reporter->EmitScrollJankUkm();
      global_trackers_.scroll_jank_ukm_reporter->SetEarliestScrollEvent(
          *(earliest_event->AsScrollUpdate()));
    }
  }

  TRACE_EVENT("input,input.scrolling", "PresentedFrameInformation",
              [events_metrics = std::cref(events_metrics_), fling_input_count,
               normal_input_count](perfetto::EventContext& ctx) {
                TraceScrollJankMetrics(events_metrics, fling_input_count,
                                       normal_input_count, ctx);
              });

  const auto end_timestamp = viz_breakdown_.presentation_feedback.timestamp;
  if (global_trackers_.predictor_jank_tracker) {
    global_trackers_.predictor_jank_tracker->ReportLatestScrollDelta(
        total_predicted_delta, end_timestamp, args_.interval,
        earliest_event->AsScrollUpdate()->trace_id());
  }
  if (global_trackers_.scroll_jank_dropped_frame_tracker) {
    global_trackers_.scroll_jank_dropped_frame_tracker
        ->ReportLatestPresentationData(*(earliest_event->AsScrollUpdate()),
                                       last_coalesced_ts, end_timestamp,
                                       args_.interval);
  }
  if (global_trackers_.scroll_jank_ukm_reporter) {
    global_trackers_.scroll_jank_ukm_reporter
        ->UpdateLatestFrameAndEmitPredictorJank(end_timestamp);
  }
}

void CompositorFrameReporter::ReportEventLatencyTraceEvents() const {
  for (const auto& event_metrics : events_metrics_) {
    EventLatencyTracingRecorder::RecordEventLatencyTraceEvent(
        event_metrics.get(), frame_termination_time_, args_.interval,
        &stage_history_, processed_viz_breakdown_.get());
  }
}

base::TimeDelta CompositorFrameReporter::SumOfStageHistory() const {
  base::TimeDelta sum;
  for (const StageData& stage : stage_history_)
    sum += stage.end_time - stage.start_time;
  return sum;
}

base::TimeTicks CompositorFrameReporter::Now() const {
  return tick_clock_->NowTicks();
}

void CompositorFrameReporter::AdoptReporter(
    std::unique_ptr<CompositorFrameReporter> reporter) {
  // If |this| reporter is dependent on another reporter to decide about partial
  // update, then |this| should not have any such dependents.
  DCHECK(!partial_update_decider_);
  DCHECK(!partial_update_dependents_.empty());

  // The adoptee tracks a partial update. If it has metrics that depend on the
  // main thread update, move them into |this| reporter.
  AddEventsMetrics(reporter->TakeMainBlockedEventsMetrics());

  owned_partial_update_dependents_.push(std::move(reporter));
  DiscardOldPartialUpdateReporters();
}

void CompositorFrameReporter::CalculateCompositorLatencyPrediction(
    CompositorLatencyInfo& previous_predictions,
    base::TimeDelta prediction_deviation_threshold) {
  // `stage_history_` should not be empty since we are calling this function
  // from DidPresentCompositorFrame(), which means there has to be some sort of
  // stage data.
  DCHECK(!stage_history_.empty());

  // If the bad case of having `total_latency` of `previous_predictions` happens
  // then it would mess up the prediction calculation, therefore, we want to
  // reset the prediction by setting everything back to -1.
  if (previous_predictions.total_latency.is_zero())
    previous_predictions = CompositorLatencyInfo(base::Microseconds(-1));

  base::TimeDelta total_pipeline_latency =
      stage_history_.back().end_time - stage_history_[0].start_time;

  // Do not record current breakdown stages' duration if the total latency of
  // the current PipelineReporter is 0s. And no further predictions could be
  // made in this case.
  if (total_pipeline_latency.is_zero())
    return;

  processed_blink_breakdown_ = std::make_unique<ProcessedBlinkBreakdown>(
      blink_start_time_, begin_main_frame_start_, blink_breakdown_);
  processed_viz_breakdown_ =
      std::make_unique<ProcessedVizBreakdown>(viz_start_time_, viz_breakdown_);

  // Note that `current_stage_durations` would always have the same length as
  // `previous_predictions`, since each index represent the breakdown stages of
  // the PipelineReporter listed at enum class, StageType.
  CompositorLatencyInfo current_stage_durations(base::Microseconds(0));
  current_stage_durations.total_latency = total_pipeline_latency;

  for (auto stage : stage_history_) {
    if (stage.stage_type == StageType::kTotalLatency)
      continue;
    base::TimeDelta substageLatency = stage.end_time - stage.start_time;
    current_stage_durations
        .top_level_stages[static_cast<int>(stage.stage_type)] = substageLatency;
  }

  for (auto it = processed_blink_breakdown_->CreateIterator(); it.IsValid();
       it.Advance()) {
    current_stage_durations
        .blink_breakdown_stages[static_cast<int>(it.GetBreakdown())] =
        it.GetLatency();
    current_stage_durations.total_blink_latency += it.GetLatency();
  }

  for (auto it = processed_viz_breakdown_->CreateIterator(true); it.IsValid();
       it.Advance()) {
    current_stage_durations
        .viz_breakdown_stages[static_cast<int>(it.GetBreakdown())] =
        it.GetDuration();
    current_stage_durations.total_viz_latency += it.GetDuration();
  }

  // Do not record current pipeline details or update predictions if no frame
  // is submitted.
  if (current_stage_durations
          .top_level_stages[static_cast<int>(
              StageType::kSubmitCompositorFrameToPresentationCompositorFrame)]
          .is_zero())
    return;

  // The previous prediction is initialized to be -1, so check if the current
  // PipelineReporter is the first reporter ever to be calculated.
  if (previous_predictions.total_latency == base::Microseconds(-1)) {
    previous_predictions = current_stage_durations;
  } else {
    if ((current_stage_durations.total_latency -
         previous_predictions.total_latency) >= prediction_deviation_threshold)
      FindHighLatencyAttribution(previous_predictions, current_stage_durations);

    for (int i = 0; i < kNumOfCompositorStages; i++) {
      previous_predictions.top_level_stages[i] =
          PredictLatency(previous_predictions.top_level_stages[i],
                         current_stage_durations.top_level_stages[i]);
    }
    previous_predictions.total_latency =
        PredictLatency(previous_predictions.total_latency,
                       current_stage_durations.total_latency);

    if (!current_stage_durations.total_blink_latency.is_zero()) {
      for (int i = 0; i < kNumOfBlinkStages; i++) {
        previous_predictions.blink_breakdown_stages[i] =
            previous_predictions.total_blink_latency.is_zero()
                ? current_stage_durations.blink_breakdown_stages[i]
                : PredictLatency(
                      previous_predictions.blink_breakdown_stages[i],
                      current_stage_durations.blink_breakdown_stages[i]);
      }
      previous_predictions.total_blink_latency =
          previous_predictions.total_blink_latency.is_zero()
              ? current_stage_durations.total_blink_latency
              : PredictLatency(previous_predictions.total_blink_latency,
                               current_stage_durations.total_blink_latency);
    }

    // TODO(crbug.com/40233949): implement check that ensure the prediction is
    // correct by checking if platform supports breakdown of the stage
    // SubmitCompositorFrameToPresentationCompositorFrame.SwapStartToSwapEnd,
    // then SwapStartToSwapEnd should always be 0s and data for breakdown of it
    // should always be available. (See enum class `VizBreakdown` for stage
    // details.)
    if (!current_stage_durations.total_viz_latency.is_zero()) {
      for (int i = 0; i < kNumOfVizStages; i++) {
        previous_predictions.viz_breakdown_stages[i] =
            previous_predictions.total_viz_latency.is_zero()
                ? current_stage_durations.viz_breakdown_stages[i]
                : PredictLatency(
                      previous_predictions.viz_breakdown_stages[i],
                      current_stage_durations.viz_breakdown_stages[i]);
      }
      previous_predictions.total_viz_latency =
          previous_predictions.total_viz_latency.is_zero()
              ? current_stage_durations.total_viz_latency
              : PredictLatency(previous_predictions.total_viz_latency,
                               current_stage_durations.total_viz_latency);
    }
  }
}

void CompositorFrameReporter::CalculateEventLatencyPrediction(
    CompositorFrameReporter::EventLatencyInfo& predicted_event_latency,
    base::TimeDelta prediction_deviation_threshold) {
  if (events_metrics_.empty())
    return;

  // TODO(crbug.com/40228308): Explore calculating predictions for multiple
  // events. Currently only kGestureScrollUpdate event predictions
  // are being calculated, consider including other stages in future changes.
  auto event_it = base::ranges::find_if(
      events_metrics_, [](const std::unique_ptr<EventMetrics>& event) {
        return event &&
               event->type() == EventMetrics::EventType::kGestureScrollUpdate;
      });
  if (event_it == events_metrics_.end())
    return;
  auto& event_metrics = *event_it;

  base::TimeTicks dispatch_start_time =
      event_metrics->GetDispatchStageTimestamp(
          EventMetrics::DispatchStage::kGenerated);

  // Determine the last valid stage. First check kRendererMainFinished and if it
  // doesn't exist, check kRendererCompositorFinished. If neither of them
  // exists, there is not enough information for the prediction.
  EventMetrics::DispatchStage last_valid_stage =
      EventMetrics::DispatchStage::kGenerated;
  if (event_metrics->GetDispatchStageTimestamp(
          EventMetrics::DispatchStage::kRendererMainFinished) >
      dispatch_start_time) {
    last_valid_stage = EventMetrics::DispatchStage::kRendererMainFinished;
  } else if (event_metrics->GetDispatchStageTimestamp(
                 EventMetrics::DispatchStage::kRendererCompositorFinished) >
             dispatch_start_time) {
    last_valid_stage = EventMetrics::DispatchStage::kRendererCompositorFinished;
  } else {
    return;
  }

  base::TimeTicks dispatch_end_time =
      event_metrics->GetDispatchStageTimestamp(last_valid_stage);
  base::TimeDelta total_dispatch_duration =
      dispatch_end_time - dispatch_start_time;
  if (total_dispatch_duration.is_negative())
    return;

  CompositorFrameReporter::EventLatencyInfo actual_event_latency(
      kNumDispatchStages, kNumOfCompositorStages);
  actual_event_latency.total_duration = base::Microseconds(0);

  // Determine dispatch stage durations.
  base::TimeTicks previous_timetick = dispatch_start_time;
  for (auto stage = EventMetrics::DispatchStage::kGenerated;
       stage <= last_valid_stage;
       stage = static_cast<EventMetrics::DispatchStage>(
           static_cast<int>(stage) + 1)) {
    if (stage != EventMetrics::DispatchStage::kGenerated) {
      base::TimeTicks current_timetick =
          event_metrics->GetDispatchStageTimestamp(stage);
      // Only update stage if the current_timetick is later than the
      // previous_timetick.
      if (current_timetick > previous_timetick) {
        base::TimeDelta stage_duration = current_timetick - previous_timetick;
        actual_event_latency.dispatch_durations[static_cast<int>(stage) - 1] =
            stage_duration;
        actual_event_latency.total_duration += stage_duration;
        previous_timetick = current_timetick;
      }
    }
  }

  // Determine dispatch-to-compositor transition stage duration.
  auto stage_it = base::ranges::lower_bound(
      stage_history_, dispatch_end_time, {},
      &CompositorFrameReporter::StageData::start_time);
  if (stage_it != stage_history_.end()) {
    if (dispatch_end_time < stage_it->start_time) {
      base::TimeDelta stage_duration = stage_it->start_time - dispatch_end_time;
      actual_event_latency.transition_duration = stage_duration;
      actual_event_latency.total_duration += stage_duration;
      actual_event_latency.transition_name =
          EventLatencyTracingRecorder::GetDispatchToCompositorBreakdownName(
              last_valid_stage, stage_it->stage_type);
    }
  }

  // Determine compositor stage durations, which start from stage_it for
  // EventLatency.
  for (auto stage = stage_it; stage != stage_history_.end(); stage++) {
    base::TimeDelta stage_duration = stage->end_time - stage->start_time;
    if (stage_duration.is_positive()) {
      actual_event_latency
          .compositor_durations[static_cast<int>(stage->stage_type)] =
          stage_duration;
      actual_event_latency.total_duration += stage_duration;
    }
  }

  // High latency attribution.
  if (predicted_event_latency.total_duration.is_positive() &&
      actual_event_latency.total_duration -
              predicted_event_latency.total_duration >=
          prediction_deviation_threshold) {
    FindEventLatencyAttribution(event_metrics.get(), predicted_event_latency,
                                actual_event_latency);
  }

  // Calculate new dispatch stage predictions.
  base::TimeDelta predicted_total_duration = base::Microseconds(0);
  for (int i = 0; i < kNumDispatchStages; i++) {
    if (actual_event_latency.dispatch_durations[i].is_positive()) {
      predicted_event_latency.dispatch_durations[i] = CalculateWeightedAverage(
          predicted_event_latency.dispatch_durations[i],
          actual_event_latency.dispatch_durations[i]);
    }
    if (predicted_event_latency.dispatch_durations[i].is_positive()) {
      predicted_total_duration += predicted_event_latency.dispatch_durations[i];
    }
  }

  // Calculate new dispatch-to-compositor transition stage predictions.
  if (actual_event_latency.transition_duration.is_positive()) {
    predicted_event_latency.transition_duration =
        CalculateWeightedAverage(predicted_event_latency.transition_duration,
                                 actual_event_latency.transition_duration);
    if (predicted_event_latency.transition_duration.is_positive())
      predicted_total_duration += predicted_event_latency.transition_duration;
  }

  // Calculate new compositor stage predictions.
  // TODO(crbug.com/40228308): Explore using existing PipelineReporter
  // predictions for the compositor stage.
  for (int i = 0; i < kNumOfCompositorStages; i++) {
    if (actual_event_latency.compositor_durations[i].is_positive()) {
      predicted_event_latency.compositor_durations[i] =
          CalculateWeightedAverage(
              predicted_event_latency.compositor_durations[i],
              actual_event_latency.compositor_durations[i]);
    }
    if (predicted_event_latency.compositor_durations[i].is_positive()) {
      predicted_total_duration +=
          predicted_event_latency.compositor_durations[i];
    }
  }

  predicted_event_latency.total_duration = predicted_total_duration;
}

void CompositorFrameReporter::SetPartialUpdateDecider(
    CompositorFrameReporter* decider) {
  DCHECK(decider);
  DCHECK(partial_update_dependents_.empty());
  has_partial_update_ = true;
  partial_update_decider_ = decider->GetWeakPtr();
  size_t size = decider->partial_update_dependents_.size();
  base::debug::Alias(&size);
  decider->partial_update_dependents_.push_back(GetWeakPtr());
}

void CompositorFrameReporter::DiscardOldPartialUpdateReporters() {
  DCHECK_LE(owned_partial_update_dependents_.size(),
            partial_update_dependents_.size());
  // Remove old owned partial update dependents if there are too many.
  bool removed = false;
  while (owned_partial_update_dependents_.size() >
         kMaxOwnedPartialUpdateDependents) {
    auto& dependent = owned_partial_update_dependents_.front();
    dependent->set_has_partial_update(false);
    owned_partial_update_dependents_.pop();
    removed = true;
  }

  if (!removed) {
    return;
  }
  // Remove all destroyed reporters from `partial_update_dependents_`.
  std::erase_if(partial_update_dependents_,
                [](const base::WeakPtr<CompositorFrameReporter>& reporter) {
                  return !reporter;
                });
}

base::WeakPtr<CompositorFrameReporter> CompositorFrameReporter::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

FrameInfo CompositorFrameReporter::GenerateFrameInfo() const {
  FrameFinalState final_state = FrameFinalState::kNoUpdateDesired;
  auto smooth_thread = smooth_thread_;
  auto scrolling_thread = scrolling_thread_;

  switch (frame_termination_status_) {
    case FrameTerminationStatus::kPresentedFrame:
      if (has_partial_update_) {
        final_state = is_accompanied_by_main_thread_update_
                          ? FrameFinalState::kPresentedPartialNewMain
                          : FrameFinalState::kPresentedPartialOldMain;
      } else {
        final_state = FrameFinalState::kPresentedAll;
      }
      break;

    case FrameTerminationStatus::kDidNotPresentFrame:
    case FrameTerminationStatus::kReplacedByNewReporter:
      final_state = FrameFinalState::kDropped;
      break;

    case FrameTerminationStatus::kDidNotProduceFrame: {
      const bool no_update_expected_from_main =
          frame_skip_reason_.has_value() &&
          frame_skip_reason() == FrameSkippedReason::kNoDamage;
      const bool no_update_expected_from_compositor =
          !has_partial_update_ && frame_skip_reason_.has_value() &&
          frame_skip_reason() == FrameSkippedReason::kWaitingOnMain;
      const bool draw_is_throttled =
          frame_skip_reason_.has_value() &&
          frame_skip_reason() == FrameSkippedReason::kDrawThrottled;

      if (!no_update_expected_from_main &&
          !no_update_expected_from_compositor) {
        final_state = FrameFinalState::kDropped;
      } else if (draw_is_throttled) {
        final_state = FrameFinalState::kDropped;
      } else {
        final_state = FrameFinalState::kNoUpdateDesired;
      }

      // If the compositor-thread is running an animation, and it ends with
      // 'did not produce frame', then that implies that the compositor
      // animation did not cause any visual changes. So for such cases, update
      // the `smooth_thread` for the FrameInfo created to exclude the compositor
      // thread. However, it is important to keep `final_state` unchanged,
      // because the main-thread update (if any) did get dropped.
      if (frame_skip_reason_.has_value() &&
          frame_skip_reason() == FrameSkippedReason::kWaitingOnMain) {
        if (smooth_thread == SmoothThread::kSmoothBoth) {
          smooth_thread = SmoothThread::kSmoothMain;
        } else if (smooth_thread == SmoothThread::kSmoothCompositor) {
          smooth_thread = SmoothThread::kSmoothNone;
        }

        if (scrolling_thread ==
            FrameInfo::SmoothEffectDrivingThread::kCompositor) {
          scrolling_thread = FrameInfo::SmoothEffectDrivingThread::kUnknown;
        }
      }

      break;
    }

    case FrameTerminationStatus::kUnknown:
      break;
  }

  FrameInfo info;
  info.final_state = final_state;
  info.smooth_thread = smooth_thread;
  info.scroll_thread = scrolling_thread;
  info.checkerboarded_needs_raster = checkerboarded_needs_raster_;
  info.checkerboarded_needs_record = checkerboarded_needs_record_;
  info.sequence_number = args_.frame_id.sequence_number;

  if (frame_skip_reason_.has_value() &&
      frame_skip_reason() == FrameSkippedReason::kNoDamage) {
    // If the frame was explicitly skipped because of 'no damage', then that
    // means this frame contains the response ('no damage') from the
    // main-thread.
    info.main_thread_response = FrameInfo::MainThreadResponse::kIncluded;
  } else if (partial_update_dependents_.size() > 0) {
    // Only a frame containing a response from the main-thread can have
    // dependent reporters.
    info.main_thread_response = FrameInfo::MainThreadResponse::kIncluded;
  } else if (begin_main_frame_start_.is_null() ||
             (frame_skip_reason_.has_value() &&
              frame_skip_reason() == FrameSkippedReason::kWaitingOnMain)) {
    // If 'begin main frame' never started, or if it started, but it
    // had to be skipped because it was waiting on the main-thread,
    // then the main-thread update is missing from this reporter.
    info.main_thread_response = FrameInfo::MainThreadResponse::kMissing;
  } else {
    info.main_thread_response = FrameInfo::MainThreadResponse::kIncluded;
  }

  info.termination_time = frame_termination_time_;
  return info;
}

void CompositorFrameReporter::FindHighLatencyAttribution(
    CompositorLatencyInfo& previous_predictions,
    CompositorLatencyInfo& current_stage_durations) {
  if (previous_predictions.total_latency.is_zero() ||
      current_stage_durations.total_latency.is_zero())
    return;

  double contribution_change = -1;
  double highest_contribution_change = -1;
  std::vector<int> highest_contribution_change_index;
  std::vector<int> highest_blink_contribution_change_index;
  std::vector<int> highest_viz_contribution_change_index;

  for (int i = 0; i < kNumOfCompositorStages; i++) {
    switch (i) {
      case static_cast<int>(StageType::kSendBeginMainFrameToCommit):
        if (current_stage_durations.top_level_stages[i].is_zero() ||
            previous_predictions.total_blink_latency.is_zero() ||
            current_stage_durations.total_blink_latency.is_zero())
          continue;

        for (int j = 0; j < kNumOfBlinkStages; j++) {
          contribution_change =
              (current_stage_durations.blink_breakdown_stages[j] /
               current_stage_durations.total_latency) -
              (previous_predictions.blink_breakdown_stages[j] /
               previous_predictions.total_latency);

          if (contribution_change > highest_contribution_change) {
            highest_contribution_change = contribution_change;
            highest_contribution_change_index.clear();
            highest_viz_contribution_change_index.clear();
            highest_blink_contribution_change_index = {j};
          } else if (std::abs(contribution_change -
                              highest_contribution_change) < kEpsilon) {
            highest_blink_contribution_change_index.push_back(j);
          }
        }
        break;

      case static_cast<int>(
          StageType::kSubmitCompositorFrameToPresentationCompositorFrame):
        if (current_stage_durations.top_level_stages[i].is_zero() ||
            previous_predictions.total_viz_latency.is_zero() ||
            current_stage_durations.total_viz_latency.is_zero())
          continue;

        for (int j = 0; j < kNumOfVizStages; j++) {
          contribution_change =
              (current_stage_durations.viz_breakdown_stages[j] /
               current_stage_durations.total_latency) -
              (previous_predictions.viz_breakdown_stages[j] /
               previous_predictions.total_latency);

          if (contribution_change > highest_contribution_change) {
            highest_contribution_change = contribution_change;
            highest_contribution_change_index.clear();
            highest_blink_contribution_change_index.clear();
            highest_viz_contribution_change_index = {j};
          } else if (std::abs(contribution_change -
                              highest_contribution_change) < kEpsilon) {
            highest_viz_contribution_change_index.push_back(j);
          }
        }
        break;

      default:
        contribution_change = (current_stage_durations.top_level_stages[i] /
                               current_stage_durations.total_latency) -
                              (previous_predictions.top_level_stages[i] /
                               previous_predictions.total_latency);

        if (contribution_change > highest_contribution_change) {
          highest_contribution_change = contribution_change;
          highest_blink_contribution_change_index.clear();
          highest_viz_contribution_change_index.clear();
          highest_contribution_change_index = {i};
        } else if (std::abs(contribution_change - highest_contribution_change) <
                   kEpsilon) {
          highest_contribution_change_index.push_back(i);
        }
        break;
    }
  }

  // It is not expensive to go through vector of indexes again since it is
  // usually very small (possibilities of breakdown stages having the same
  // change contribution is small).
  for (auto index : highest_contribution_change_index) {
    high_latency_substages_.push_back(
        GetStageName(static_cast<StageType>(index)));
  }
  for (auto index : highest_blink_contribution_change_index) {
    high_latency_substages_.push_back(
        GetStageName(StageType::kSendBeginMainFrameToCommit, std::nullopt,
                     static_cast<BlinkBreakdown>(index)));
  }
  for (auto index : highest_viz_contribution_change_index) {
    high_latency_substages_.push_back(GetStageName(
        StageType::kSubmitCompositorFrameToPresentationCompositorFrame,
        static_cast<VizBreakdown>(index)));
  }
}

void CompositorFrameReporter::FindEventLatencyAttribution(
    EventMetrics* event_metrics,
    CompositorFrameReporter::EventLatencyInfo& predicted_event_latency,
    CompositorFrameReporter::EventLatencyInfo& actual_event_latency) {
  if (!event_metrics)
    return;

  std::vector<std::string> high_latency_stages;
  double contribution_change = -1;
  double highest_contribution_change = -1;

  // Check dispatch stage change
  EventMetrics::DispatchStage dispatch_stage =
      EventMetrics::DispatchStage::kGenerated;
  base::TimeTicks dispatch_timestamp =
      event_metrics->GetDispatchStageTimestamp(dispatch_stage);
  while (dispatch_stage != EventMetrics::DispatchStage::kMaxValue) {
    DCHECK(!dispatch_timestamp.is_null());
    auto end_stage = static_cast<EventMetrics::DispatchStage>(
        static_cast<int>(dispatch_stage) + 1);
    base::TimeTicks end_timestamp =
        event_metrics->GetDispatchStageTimestamp(end_stage);
    while (end_timestamp.is_null() &&
           end_stage != EventMetrics::DispatchStage::kMaxValue) {
      end_stage = static_cast<EventMetrics::DispatchStage>(
          static_cast<int>(end_stage) + 1);
      end_timestamp = event_metrics->GetDispatchStageTimestamp(end_stage);
    }
    if (end_timestamp.is_null())
      break;

    contribution_change =
        (actual_event_latency
             .dispatch_durations[static_cast<int>(end_stage) - 1] /
         actual_event_latency.total_duration) -
        (predicted_event_latency
             .dispatch_durations[static_cast<int>(end_stage) - 1] /
         predicted_event_latency.total_duration);
    std::string dispatch_stage_name =
        EventLatencyTracingRecorder::GetDispatchBreakdownName(dispatch_stage,
                                                              end_stage);
    highest_contribution_change = DetermineHighestContribution(
        contribution_change, highest_contribution_change, dispatch_stage_name,
        high_latency_stages);

    dispatch_stage = end_stage;
    dispatch_timestamp = end_timestamp;
  }

  // Check dispatch-to-compositor stage change
  contribution_change = (actual_event_latency.transition_duration /
                         actual_event_latency.total_duration) -
                        (predicted_event_latency.transition_duration /
                         predicted_event_latency.total_duration);
  highest_contribution_change = DetermineHighestContribution(
      contribution_change, highest_contribution_change,
      actual_event_latency.transition_name, high_latency_stages);

  // Check compositor stage change
  for (int i = 0; i < kNumOfCompositorStages; i++) {
    contribution_change = (actual_event_latency.compositor_durations[i] /
                           actual_event_latency.total_duration) -
                          (predicted_event_latency.compositor_durations[i] /
                           predicted_event_latency.total_duration);
    highest_contribution_change = DetermineHighestContribution(
        contribution_change, highest_contribution_change,
        GetStageName(static_cast<StageType>(i)), high_latency_stages);
  }

  for (auto stage : high_latency_stages) {
    event_metrics->SetHighLatencyStage(stage);
  }
}

}  // namespace cc
