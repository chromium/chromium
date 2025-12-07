// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/compositor_frame_reporter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "base/trace_event/typed_macros.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "cc/base/features.h"
#include "cc/base/rolling_time_delta_history.h"
#include "cc/metrics/custom_metrics_recorder.h"
#include "cc/metrics/event_latency_tracing_recorder.h"
#include "cc/metrics/event_latency_tracker.h"
#include "cc/metrics/event_metrics.h"
#include "cc/metrics/frame_sequence_tracker.h"
#include "cc/metrics/latency_ukm_reporter.h"
#include "cc/metrics/submit_info.h"
#include "services/tracing/public/cpp/perfetto/macros.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_frame_reporter.pbzero.h"
#include "ui/events/types/event_type.h"

namespace cc {
namespace {

using StageType = CompositorFrameReporter::StageType;
using FrameReportType = CompositorFrameReporter::FrameReportType;
using BlinkBreakdown = CompositorFrameReporter::BlinkBreakdown;
using VizBreakdown = CompositorFrameReporter::VizBreakdown;
using TreesInVizBreakdown = CompositorFrameReporter::TreesInVizBreakdown;
using FrameFinalState = FrameInfo::FrameFinalState;

constexpr int kStageTypeCount = static_cast<int>(StageType::kStageTypeCount);

// We need double the VizBreakdown count to accommodate TreesInViz and regular
// mode
constexpr int kAllBreakdownCount =
    (static_cast<int>(VizBreakdown::kBreakdownCount) * 2) +
    static_cast<int>(BlinkBreakdown::kBreakdownCount) +
    static_cast<int>(TreesInVizBreakdown::kTreesInVizBreakdownCount);

constexpr int kVizBreakdownInitialIndex = kStageTypeCount;
// The first set of VizBreakdowns is for normal mode, second set for TreesInViz.
constexpr int kVizBreakdownForTreesInVizModeInitialIndex =
    kVizBreakdownInitialIndex + static_cast<int>(VizBreakdown::kBreakdownCount);
constexpr int kBlinkBreakdownInitialIndex =
    kVizBreakdownInitialIndex +
    (static_cast<int>(VizBreakdown::kBreakdownCount) * 2);
constexpr int kTreesInVizBreakdownInitialIndex =
    kBlinkBreakdownInitialIndex +
    static_cast<int>(BlinkBreakdown::kBreakdownCount);

// For each possible FrameSequenceTrackerType there will be a UMA histogram
// plus one for general case.
constexpr int kFrameSequenceTrackerTypeCount =
    static_cast<int>(FrameSequenceTrackerType::kMaxType) + 1;

// Maximum number of partial update dependents a reporter can own. When a
// reporter with too many dependents is terminated, it will terminate all its
// dependents which will block the pipeline for a long time. Too many dependents
// also means too much memory usage.
constexpr size_t kMaxOwnedPartialUpdateDependents = 300u;

// This value should be recalculated in case of changes to the number of values
// in CompositorFrameReporter::DroppedFrameReportType or in
// CompositorFrameReporter::StageType.
constexpr int kStagesWithBreakdownCount = kStageTypeCount + kAllBreakdownCount;
constexpr int kMaxCompositorLatencyHistogramIndex =
    kFrameSequenceTrackerTypeCount * kStagesWithBreakdownCount;

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

std::string GetCompositorLatencyHistogramName(
    FrameSequenceTrackerType frame_sequence_tracker_type,
    StageType stage_type,
    std::optional<VizBreakdown> viz_breakdown,
    std::optional<BlinkBreakdown> blink_breakdown,
    std::optional<TreesInVizBreakdown> trees_in_viz_breakdown) {
  DCHECK_LE(frame_sequence_tracker_type, FrameSequenceTrackerType::kMaxType);
  const char* tracker_type_name =
      FrameSequenceTracker::GetFrameSequenceTrackerTypeName(
          frame_sequence_tracker_type);
  DCHECK(tracker_type_name);
  return base::StrCat(
      {"CompositorLatency2.", tracker_type_name, *tracker_type_name ? "." : "",
       CompositorFrameReporter::GetStageName(stage_type, viz_breakdown,
                                             blink_breakdown,
                                             trees_in_viz_breakdown)});
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

base::TimeTicks ComputeSafeDeadlineForFrame(const viz::BeginFrameArgs& args) {
  return args.frame_time + (args.interval * 1.5);
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
constexpr int kMaxGestureScrollHistogramIndex = 6;
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
    case EventMetrics::EventType::kInertialGestureScrollEnd:
      return 5;
    default:
      // We are only interested in 6 categories of EventType for scroll input
      NOTREACHED();
  }
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
  } else if (base::ShouldRecordSubsampledMetric(0.001)) {
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
  bool trees_in_viz_mode = base::FeatureList::IsEnabled(features::kTreesInViz);
  if (!trees_in_viz_mode && viz_start_time.is_null()) {
    return;
  }

  // Check if `viz_breakdown` is set. Testing indicates that sometimes the
  // received_compositor_frame_timestamp can be earlier than the given
  // `viz_start_time`. Avoid reporting negative times.
  if (viz_breakdown.received_compositor_frame_timestamp.is_null() ||
      (!trees_in_viz_mode &&
       viz_breakdown.received_compositor_frame_timestamp < viz_start_time)) {
    return;
  }

  if (trees_in_viz_mode) {
    list_[static_cast<int>(VizBreakdown::kSubmitToReceiveCompositorFrame)] =
        std::make_pair(viz_breakdown.submit_compositor_frame,
                       viz_breakdown.received_compositor_frame_timestamp);

  } else {
    list_[static_cast<int>(VizBreakdown::kSubmitToReceiveCompositorFrame)] =
        std::make_pair(viz_start_time,
                       viz_breakdown.received_compositor_frame_timestamp);
  }

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

// CompositorFrameReporter::ProcessedBlinkBreakdown::Iterator ==================

CompositorFrameReporter::ProcessedTreesInVizBreakdown::Iterator::Iterator(
    const ProcessedTreesInVizBreakdown* owner)
    : owner_(owner) {
  DCHECK(owner_);
  SkipBreakdownsIfNecessary();
}

CompositorFrameReporter::ProcessedTreesInVizBreakdown::Iterator::~Iterator() =
    default;

bool CompositorFrameReporter::ProcessedTreesInVizBreakdown::Iterator::IsValid()
    const {
  return index_ < std::size(owner_->list_);
}

void CompositorFrameReporter::ProcessedTreesInVizBreakdown::Iterator::
    Advance() {
  DCHECK(IsValid());
  index_++;
  SkipBreakdownsIfNecessary();
}

TreesInVizBreakdown
CompositorFrameReporter::ProcessedTreesInVizBreakdown::Iterator::GetBreakdown()
    const {
  DCHECK(IsValid());
  return static_cast<TreesInVizBreakdown>(index_);
}

base::TimeTicks
CompositorFrameReporter::ProcessedTreesInVizBreakdown::Iterator::GetStartTime()
    const {
  DCHECK(HasValue());
  return owner_->list_[index_]->first;
}

base::TimeTicks
CompositorFrameReporter::ProcessedTreesInVizBreakdown::Iterator::GetEndTime()
    const {
  DCHECK(HasValue());
  return owner_->list_[index_]->second;
}

base::TimeDelta
CompositorFrameReporter::ProcessedTreesInVizBreakdown::Iterator::GetDuration()
    const {
  DCHECK(HasValue());
  return owner_->list_[index_]->second - owner_->list_[index_]->first;
}

bool CompositorFrameReporter::ProcessedTreesInVizBreakdown::Iterator::HasValue()
    const {
  DCHECK(IsValid());
  return owner_->list_[index_].has_value();
}

void CompositorFrameReporter::ProcessedTreesInVizBreakdown::Iterator::
    SkipBreakdownsIfNecessary() {
  while (IsValid() && (!HasValue())) {
    index_++;
  }
}

// CompositorFrameReporter::ProcessedBlinkBreakdown ============================

CompositorFrameReporter::ProcessedTreesInVizBreakdown::
    ProcessedTreesInVizBreakdown(base::TimeTicks activate_time,
                                 base::TimeTicks trees_in_viz_branch_time,
                                 base::TimeTicks trees_in_viz_viz_time,
                                 const viz::FrameTimingDetails& viz_breakdown) {
  // Check if `viz_breakdown` is set, avoid reporting negative times.
  // See VizBreakdown().
  if (viz_breakdown.received_compositor_frame_timestamp.is_null()) {
    return;
  }

  // New stages introduced by CC.
  list_[static_cast<int>(TreesInVizBreakdown::kEndActivateToDrawLayers)] =
      std::make_pair(activate_time,              // end activate to
                     trees_in_viz_branch_time);  // branch time
  list_[static_cast<int>(
      TreesInVizBreakdown::kDrawLayersToSubmitUpdateDisplayTree)] =
      std::make_pair(trees_in_viz_branch_time,  // branch time to
                     trees_in_viz_viz_time);    // send over wire

  // CC -> Viz mojo time
  list_[static_cast<int>(
      TreesInVizBreakdown::kSendUpdateDisplayTreeToReceiveUpdateDisplayTree)] =
      std::make_pair(
          trees_in_viz_viz_time,                     // send over wire to
          viz_breakdown.start_update_display_tree);  // receive over wire.

  // New stages introduced in Viz.
  list_[static_cast<int>(
      TreesInVizBreakdown::
          kReceiveUpdateDisplayTreeToStartPrepareToDraw)] =  // receive over the
                                                             // wire to start
                                                             // prepare to draw
      std::make_pair(viz_breakdown.start_update_display_tree,
                     viz_breakdown.start_prepare_to_draw);
  list_[static_cast<int>(
      TreesInVizBreakdown::
          kStartPrepareToDrawToStartDrawLayers)] =  // start prepare to draw to
                                                    // start draw layers
      std::make_pair(viz_breakdown.start_prepare_to_draw,
                     viz_breakdown.start_draw_layers);
  list_[static_cast<int>(
      TreesInVizBreakdown::
          kStartDrawLayersToSubmitCompositorFrame)] =  // start draw layers to
                                                       // submit cf
      std::make_pair(viz_breakdown.start_draw_layers,
                     viz_breakdown.submit_compositor_frame);

  return;
}

CompositorFrameReporter::ProcessedTreesInVizBreakdown::
    ~ProcessedTreesInVizBreakdown() = default;

CompositorFrameReporter::ProcessedTreesInVizBreakdown::Iterator
CompositorFrameReporter::ProcessedTreesInVizBreakdown::CreateIterator() const {
  return Iterator(this);
}

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
  DCHECK(global_trackers_.frame_sorter);
  if (global_trackers_.frame_sorter->first_contentful_paint_received()) {
    global_trackers_.frame_sorter->AddNewFrame(args);
  }
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
             FrameSequenceTrackerType::kCompositorNativeAnimation)) ||
         active_trackers_.test(static_cast<size_t>(
             FrameSequenceTrackerType::kCompositorRasterAnimation)));
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
    std::optional<TreesInVizBreakdown> trees_in_viz_breakdown) {
  DCHECK(!viz_breakdown ||
         stage_type ==
             StageType::kSubmitCompositorFrameToPresentationCompositorFrame ||
         stage_type ==
             StageType::kSubmitUpdateDisplayTreeToPresentationCompositorFrame);
  DCHECK(!blink_breakdown ||
         stage_type == StageType::kSendBeginMainFrameToCommit);
  DCHECK(
      !trees_in_viz_breakdown ||
      (stage_type == StageType::kEndActivateToSubmitUpdateDisplayTree ||
       stage_type ==
           StageType::kSubmitUpdateDisplayTreeToPresentationCompositorFrame));
  switch (stage_type) {
    case StageType::kBeginImplFrameToSendBeginMainFrame:
      return "BeginImplFrameToSendBeginMainFrame";
    case StageType::kSendBeginMainFrameToCommit:
      if (!blink_breakdown) {
        return "SendBeginMainFrameToCommit";
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
      return "EndActivateToSubmitCompositorFrame";
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
    case StageType::kEndActivateToSubmitUpdateDisplayTree:
      if (!trees_in_viz_breakdown) {
        return "EndActivateToSubmitUpdateDisplayTree";
      }
      switch (*trees_in_viz_breakdown) {
        case TreesInVizBreakdown::kEndActivateToDrawLayers:
          return "EndActivateToSubmitUpdateDisplayTree."
                 "EndActivateToDrawLayers";
        case TreesInVizBreakdown::kDrawLayersToSubmitUpdateDisplayTree:
          return "EndActivateToSubmitUpdateDisplayTree."
                 "DrawLayersToSubmitUpdateDisplayTree";
        case TreesInVizBreakdown::
            kSendUpdateDisplayTreeToReceiveUpdateDisplayTree:
        case TreesInVizBreakdown::kReceiveUpdateDisplayTreeToStartPrepareToDraw:
        case TreesInVizBreakdown::kStartPrepareToDrawToStartDrawLayers:
        case TreesInVizBreakdown::kStartDrawLayersToSubmitCompositorFrame:
        case TreesInVizBreakdown::kTreesInVizBreakdownCount:
          NOTREACHED();
      }
    case StageType::kSubmitUpdateDisplayTreeToPresentationCompositorFrame:
      if (!trees_in_viz_breakdown) {
        if (!viz_breakdown) {
          return "SubmitUpdateDisplayTreeToPresentationCompositorFrame";
        }
        switch (*viz_breakdown) {
          case VizBreakdown::kSubmitToReceiveCompositorFrame:
            return "SubmitUpdateDisplayTreeToPresentationCompositorFrame."
                   "SubmitToReceiveCompositorFrame";
          case VizBreakdown::kReceivedCompositorFrameToStartDraw:
            return "SubmitUpdateDisplayTreeToPresentationCompositorFrame."
                   "ReceivedCompositorFrameToStartDraw";
          case VizBreakdown::kStartDrawToSwapStart:
            return "SubmitUpdateDisplayTreeToPresentationCompositorFrame."
                   "StartDrawToSwapStart";
          case VizBreakdown::kSwapStartToSwapEnd:
            return "SubmitUpdateDisplayTreeToPresentationCompositorFrame."
                   "SwapStartToSwapEnd";
          case VizBreakdown::kSwapEndToPresentationCompositorFrame:
            return "SubmitUpdateDisplayTreeToPresentationCompositorFrame."
                   "SwapEndToPresentationCompositorFrame";
          case VizBreakdown::kSwapStartToBufferAvailable:
            return "SubmitUpdateDisplayTreeToPresentationCompositorFrame."
                   "SwapStartToBufferAvailable";
          case VizBreakdown::kBufferAvailableToBufferReady:
            return "SubmitUpdateDisplayTreeToPresentationCompositorFrame."
                   "BufferAvailableToBufferReady";
          case VizBreakdown::kBufferReadyToLatch:
            return "SubmitUpdateDisplayTreeToPresentationCompositorFrame."
                   "BufferReadyToLatch";
          case VizBreakdown::kLatchToSwapEnd:
            return "SubmitUpdateDisplayTreeToPresentationCompositorFrame."
                   "LatchToSwapEnd";
          case VizBreakdown::kBreakdownCount:
            NOTREACHED();
        }
      }
      switch (*trees_in_viz_breakdown) {
        case TreesInVizBreakdown::kEndActivateToDrawLayers:
        case TreesInVizBreakdown::kDrawLayersToSubmitUpdateDisplayTree:
          NOTREACHED();
        case TreesInVizBreakdown::
            kSendUpdateDisplayTreeToReceiveUpdateDisplayTree:
          return "SubmitUpdateDisplayTreeToPresentationCompositorFrame."
                 "SendUpdateDisplayTreeToReceiveUpdateDisplayTree";
        case TreesInVizBreakdown::kReceiveUpdateDisplayTreeToStartPrepareToDraw:
          return "SubmitUpdateDisplayTreeToPresentationCompositorFrame."
                 "ReceiveUpdateDisplayTreeToStartPrepareToDraw";
        case TreesInVizBreakdown::kStartPrepareToDrawToStartDrawLayers:
          return "SubmitUpdateDisplayTreeToPresentationCompositorFrame."
                 "StartPrepareToDrawToStartDrawLayers";
        case TreesInVizBreakdown::kStartDrawLayersToSubmitCompositorFrame:
          return "SubmitUpdateDisplayTreeToPresentationCompositorFrame."
                 "StartDrawLayersToSubmitCompositorFrame";
        case TreesInVizBreakdown::kTreesInVizBreakdownCount:
          NOTREACHED();
      }

    case StageType::kTotalLatency:
      return "TotalLatency";
    case StageType::kStageTypeCount:
      NOTREACHED();
  }
}

const char* CompositorFrameReporter::GetTreesInVizBreakdownName(
    TreesInVizBreakdown breakdown) {
  switch (breakdown) {
    case TreesInVizBreakdown::kEndActivateToDrawLayers:
      return "EndActivateToDrawLayers";
    case TreesInVizBreakdown::kDrawLayersToSubmitUpdateDisplayTree:
      return "DrawLayersToSendUpdateDisplayTree";
    case TreesInVizBreakdown::kSendUpdateDisplayTreeToReceiveUpdateDisplayTree:
      return "SendUpdateDisplayTreeToReceiveUpdateDisplayTree";
    case TreesInVizBreakdown::kReceiveUpdateDisplayTreeToStartPrepareToDraw:
      return "ReceiveUpdateDisplayTreeToStartPrepareToDraw";
    case TreesInVizBreakdown::kStartPrepareToDrawToStartDrawLayers:
      return "StartPrepareToDrawToStartDrawLayers";
    case TreesInVizBreakdown::kStartDrawLayersToSubmitCompositorFrame:
      return "StartDrawLayersToSubmitCompositorFrame";
    case TreesInVizBreakdown::kTreesInVizBreakdownCount:
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
  new_reporter->set_will_throttle_main(will_throttle_main_);
  new_reporter->waiting_for_main(waiting_for_main_);

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
    case StageType::kEndActivateToSubmitUpdateDisplayTree: {
      TreesInVizTimestamps timestamps =
          trees_in_viz_timestamps_.value_or(TreesInVizTimestamps{});
      DCHECK(timestamps.trees_in_viz_activate_time_.is_null());
      timestamps.trees_in_viz_activate_time_ = start_time;
      trees_in_viz_timestamps_ = timestamps;
      break;
    }
    case StageType::kSubmitUpdateDisplayTreeToPresentationCompositorFrame: {
      TreesInVizTimestamps timestamps =
          trees_in_viz_timestamps_.value_or(TreesInVizTimestamps{});
      DCHECK(timestamps.trees_in_viz_viz_start_time_.is_null());
      DCHECK(!timestamps.trees_in_viz_branch_time_.is_null());
      DCHECK(timestamps.trees_in_viz_branch_time_ <=
             start_time);  // branch time expected to happen before we sent
                           // the update to Viz.
      timestamps.trees_in_viz_viz_start_time_ = start_time;
      trees_in_viz_timestamps_ = timestamps;
      break;
    }
    default:
      break;
  }
}

void CompositorFrameReporter::SetTreesInVizBranchTime(
    base::TimeTicks timestamp) {
  // We expect trees_in_viz_timestamps_to be set most of the time because the
  // reporter is expected to move through the activate stage prior to
  // the TreesInViz branch point.
  // TODO(crbug.com/445500514): Sometimes, activation stage does not occur
  // because the CFR represents a partial update and in this case we make a new
  // TreesInVizTimestamps value.
  TreesInVizTimestamps timestamps =
      trees_in_viz_timestamps_.value_or(TreesInVizTimestamps{});
  timestamps.trees_in_viz_branch_time_ = timestamp;
  trees_in_viz_timestamps_ = timestamps;
}

void CompositorFrameReporter::StartStageUpdateDisplayTree(
    SubmitInfo& submit_info) {
  // TODO(crbug.com/445970842):
  // If main the main frame has been aborted, the impl_frame_finish_time() might
  // be after the UpdateLayerTree has already been sent over.
  if (!main_frame_abort_time_.has_value()) {
    DCHECK(impl_frame_finish_time() < submit_info.time);
  }
  StartStage(StageType::kEndActivateToSubmitUpdateDisplayTree,
             impl_frame_finish_time());
  SetTreesInVizBranchTime(submit_info.time);
}

void CompositorFrameReporter::StartStagePresentationCompositorFrame(
    SubmitInfo& submit_info) {
  SetTreesInVizBranchTime(submit_info.time);
  StartStage(StageType::kSubmitUpdateDisplayTreeToPresentationCompositorFrame,
             submit_info.trees_in_viz_submit_time);
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

void CompositorFrameReporter::OnFinishImplFrame(base::TimeTicks timestamp,
                                                bool waiting_for_main) {
  DCHECK(!did_finish_impl_frame_);

  did_finish_impl_frame_ = true;
  impl_frame_finish_time_ = timestamp;
  waiting_for_main_ = waiting_for_main;
}

// TODO(crbug.com/445970842): If impl_frame_finish_time_ is set, do not
// override it.
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
  DCHECK(!dropped_non_damaging_events_metrics_);
  events_metrics_.insert(events_metrics_.end(),
                         std::make_move_iterator(events_metrics.begin()),
                         std::make_move_iterator(events_metrics.end()));
}

EventMetrics::List CompositorFrameReporter::TakeEventsMetrics() {
  DCHECK(!dropped_non_damaging_events_metrics_);
  EventMetrics::List result = std::move(events_metrics_);
  events_metrics_.clear();
  return result;
}

void CompositorFrameReporter::set_normalized_invalidated_area(
    std::optional<float> normalized_invalidated_area) {
  paint_metric_ = normalized_invalidated_area;
}

EventMetrics::List CompositorFrameReporter::TakeMainBlockedEventsMetrics() {
  DCHECK(!dropped_non_damaging_events_metrics_);
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
  // Remove all `EventMetrics` which didn't cause a frame update so that they
  // wouldn't affect UMA metrics like EventLatency.TotalLatency. It's safe to do
  // here because the scroll jank metric, which needs to be informed about
  // non-damaging `EventMetrics`, has already been reported in
  // `ReportScrollJankMetrics()`, so there's no more need to keep the
  // non-damaging event metrics around.
  DropEventMetricsWhichDidNotCauseFrameUpdate();

  if (frame_termination_status_ == FrameTerminationStatus::kUnknown)
    TerminateFrame(FrameTerminationStatus::kUnknown, Now());

  if (!processed_blink_breakdown_)
    processed_blink_breakdown_ = std::make_unique<ProcessedBlinkBreakdown>(
        blink_start_time_, begin_main_frame_start_, blink_breakdown_);
  if (!processed_viz_breakdown_)
    processed_viz_breakdown_ = std::make_unique<ProcessedVizBreakdown>(
        viz_start_time_, viz_breakdown_);

  if (!processed_trees_in_viz_breakdown_) {
    // TODO(crbug.com/445500514): Should be possible to report breakdowns for
    // partial updates.
    TreesInVizTimestamps cc_timestamps =
        trees_in_viz_timestamps_.value_or(TreesInVizTimestamps{});
    processed_trees_in_viz_breakdown_ =
        std::make_unique<ProcessedTreesInVizBreakdown>(
            cc_timestamps.trees_in_viz_activate_time_,
            cc_timestamps.trees_in_viz_branch_time_,
            cc_timestamps.trees_in_viz_viz_start_time_, viz_breakdown_);
  }

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
    case FrameFinalState::kPresentedPartialWithoutWaiting:
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

  // Paint metrics are only reported for UI compositors.
  if (paint_metric_) {
    ReportPaintMetric();
  }

  global_trackers_.frame_sorter->AddFrameInfoToBuffer(frame_info);
  if (global_trackers_.frame_sorter->first_contentful_paint_received()) {
    global_trackers_.frame_sorter->AddFrameResult(args_, frame_info);
  }
}

void CompositorFrameReporter::EndCurrentStage(base::TimeTicks end_time) {
  if (current_stage_.start_time == base::TimeTicks()) {
    return;
  }
  current_stage_.end_time = end_time;
  stage_history_.push_back(current_stage_);
  current_stage_.start_time = base::TimeTicks();
}

void CompositorFrameReporter::ReportCompositorLatencyMetrics() const {
  // Subsampling these metrics to reduce CPU utilization.
  if (!base::ShouldRecordSubsampledMetric(0.001)) {
    return;
  }

  if (global_trackers_.latency_ukm_reporter) {
    global_trackers_.latency_ukm_reporter->ReportCompositorLatencyUkm(
        report_types_, stage_history_, active_trackers_,
        *processed_blink_breakdown_, *processed_viz_breakdown_,
        *processed_trees_in_viz_breakdown_);
  }

  if (!should_report_histograms_)
    return;

  for (const StageData& stage : stage_history_) {
    // Top-level breakdowns are only reported for presented frames.
    if (TestReportType(FrameReportType::kNonDroppedFrame)) {
      ReportStageHistogramWithBreakdown(stage);
    }
    for (size_t type = 0; type < active_trackers_.size(); ++type) {
      if (active_trackers_.test(type)) {
        // Report stage breakdowns for each `FrameSequenceTrackerType`
        ReportStageHistogramWithBreakdown(
            stage, static_cast<FrameSequenceTrackerType>(type));
      }
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
      /*blink_breakdown=*/std::nullopt,
      /*trees_in_viz_breakdown=*/std::nullopt, stage_delta);
  switch (stage.stage_type) {
    case StageType::kSendBeginMainFrameToCommit:
      ReportCompositorLatencyBlinkBreakdowns(frame_sequence_tracker_type);
      break;
    case StageType::kSubmitUpdateDisplayTreeToPresentationCompositorFrame:
      ReportCompositorLatencyTreesInVizBreakdowns(frame_sequence_tracker_type);
      ReportCompositorLatencyVizBreakdowns(frame_sequence_tracker_type,
                                           stage.stage_type);
      break;
    case StageType::kSubmitCompositorFrameToPresentationCompositorFrame:
      ReportCompositorLatencyVizBreakdowns(frame_sequence_tracker_type,
                                           stage.stage_type);
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
        /*viz_breakdown=*/std::nullopt, it.GetBreakdown(),
        /*trees_in_viz_breakdown=*/std::nullopt, it.GetLatency());
  }
}

void CompositorFrameReporter::ReportCompositorLatencyVizBreakdowns(
    FrameSequenceTrackerType frame_sequence_tracker_type,
    StageType stage_type) const {
  DCHECK(processed_viz_breakdown_);
  for (auto it = processed_viz_breakdown_->CreateIterator(false); it.IsValid();
       it.Advance()) {
    ReportCompositorLatencyHistogram(
        frame_sequence_tracker_type, stage_type, it.GetBreakdown(),
        /*blink_breakdown=*/std::nullopt,
        /*trees_in_viz_breakdown*/ std::nullopt, it.GetDuration());
  }
}

void CompositorFrameReporter::ReportCompositorLatencyTreesInVizBreakdowns(
    FrameSequenceTrackerType frame_sequence_tracker_type) const {
  DCHECK(processed_trees_in_viz_breakdown_);
  for (auto it = processed_trees_in_viz_breakdown_->CreateIterator();
       it.IsValid(); it.Advance()) {
    if (it.GetBreakdown() == TreesInVizBreakdown::kEndActivateToDrawLayers ||
        it.GetBreakdown() ==
            TreesInVizBreakdown::kDrawLayersToSubmitUpdateDisplayTree) {
      ReportCompositorLatencyHistogram(
          frame_sequence_tracker_type,
          StageType::kEndActivateToSubmitUpdateDisplayTree,
          /*viz_breakdown=*/std::nullopt, /*blink_breakdown=*/std::nullopt,
          it.GetBreakdown(), it.GetDuration());
    } else {
      ReportCompositorLatencyHistogram(
          frame_sequence_tracker_type,
          StageType::kSubmitUpdateDisplayTreeToPresentationCompositorFrame,
          /*viz_breakdown=*/std::nullopt, /*blink_breakdown=*/std::nullopt,
          it.GetBreakdown(), it.GetDuration());
    }
  }
}

// To accommodate TreesInViz and normal modes in parallel, we need to
// separately bucket the viz breakdowns because histogram exports cache metric
// names. The viz substage categories are mutually-exclusive.
//
// Metric histograms should look something like this:
//
// /---------------------\
// | Stage1              | <- Start with Stage breakdowns
// | Stage2              |
// | ...                 |
// | VizSubstage1        | <- Allocated for Viz substages for normal path,
// | VizSubstage2        | SubmitCompositorFrameToPresentationCompositorFrame
// | ...                 |
// | VizSubstage1        | <- Repeat the substages for breakdowns of
// | VizSubstage2        | SubmitUpdateDisplayTreeToPresentationCompositorFrame
// | ---                 |
// | BlinkSubstage1      | <- Blink substages valid for both modes
// | BlinkSubstage2      |
// | ...                 |
// | TreesInVizSubStage1 | <- TreesInViz cc-specific substages
// | TreesInVizSubstage2 |
// \---------------------/
//
static int GetSubstageIndex(
    StageType stage_type,
    std::optional<VizBreakdown> viz_breakdown,
    std::optional<BlinkBreakdown> blink_breakdown,
    std::optional<TreesInVizBreakdown> trees_in_viz_breakdown) {
  if (blink_breakdown) {
    return kBlinkBreakdownInitialIndex + static_cast<int>(*blink_breakdown);
  }

  if (viz_breakdown) {
    if (stage_type ==
        StageType::kSubmitUpdateDisplayTreeToPresentationCompositorFrame) {
      return kVizBreakdownForTreesInVizModeInitialIndex +
             static_cast<int>(*viz_breakdown);
    }
    return kVizBreakdownInitialIndex + static_cast<int>(*viz_breakdown);
  }

  if (trees_in_viz_breakdown) {
    return kTreesInVizBreakdownInitialIndex +
           static_cast<int>(*trees_in_viz_breakdown);
  }

  return static_cast<int>(stage_type);
}

void CompositorFrameReporter::ReportCompositorLatencyHistogram(
    FrameSequenceTrackerType frame_sequence_tracker_type,
    StageType stage_type,
    std::optional<VizBreakdown> viz_breakdown,
    std::optional<BlinkBreakdown> blink_breakdown,
    std::optional<TreesInVizBreakdown> trees_in_viz_breakdown,
    base::TimeDelta time_delta) const {
  DCHECK(!viz_breakdown ||
         stage_type ==
             StageType::kSubmitCompositorFrameToPresentationCompositorFrame ||
         stage_type ==
             StageType::kSubmitUpdateDisplayTreeToPresentationCompositorFrame);
  DCHECK(!blink_breakdown ||
         stage_type == StageType::kSendBeginMainFrameToCommit);
  DCHECK(
      !trees_in_viz_breakdown ||
      (stage_type == StageType::kEndActivateToSubmitUpdateDisplayTree ||
       stage_type ==
           StageType::kSubmitUpdateDisplayTreeToPresentationCompositorFrame));
  const int frame_sequence_tracker_type_index =
      static_cast<int>(frame_sequence_tracker_type);

  const int stage_type_index = GetSubstageIndex(
      stage_type, viz_breakdown, blink_breakdown, trees_in_viz_breakdown);

  const int histogram_index =
      (stage_type_index * kFrameSequenceTrackerTypeCount) +
      frame_sequence_tracker_type_index;

  CHECK_LT(stage_type_index, kStagesWithBreakdownCount);
  CHECK_GE(stage_type_index, 0);
  CHECK_LT(histogram_index, kMaxCompositorLatencyHistogramIndex);
  CHECK_GE(histogram_index, 0);

  auto histogram_name = GetCompositorLatencyHistogramName(
      frame_sequence_tracker_type, stage_type, viz_breakdown, blink_breakdown,
      trees_in_viz_breakdown);

  // Note: There's a 1:1 mapping between `histogram_index` and the name
  // returned by `GetCompositorLatencyHistogramName()` which allows the use
  // of `STATIC_HISTOGRAM_POINTER_GROUP()` to cache histogram objects.
  STATIC_HISTOGRAM_POINTER_GROUP(
      histogram_name, histogram_index, kMaxCompositorLatencyHistogramIndex,
      AddTimeMicrosecondsGranularity(time_delta),
      base::Histogram::FactoryMicrosecondsTimeGet(
          histogram_name, kCompositorLatencyHistogramMin,
          kCompositorLatencyHistogramMax,
          kCompositorLatencyHistogramBucketCount,
          base::HistogramBase::kUmaTargetedHistogramFlag));
}

void CompositorFrameReporter::DropEventMetricsWhichDidNotCauseFrameUpdate() {
  DCHECK(!dropped_non_damaging_events_metrics_);
  // First re-arrange `events_metrics_` so that:
  //   1. [`events_metrics_.begin()`, `first_to_erase`) only contains metrics
  //      which caused a frame update.
  //   2. [`first_to_erase`, `events_metrics_.end()`) contains metrics which
  //      didn't cause a frame update.
  auto first_to_erase =
      std::remove_if(events_metrics_.begin(), events_metrics_.end(),
                     [](const std::unique_ptr<EventMetrics>& metrics) {
                       return !metrics->caused_frame_update();
                     });
  // Then delete the metrics which didn't cause a frame update.
  events_metrics_.erase(first_to_erase, events_metrics_.end());
  dropped_non_damaging_events_metrics_ = true;
}

void CompositorFrameReporter::ReportEventLatencyMetrics() const {
  const StageData& total_latency_stage = stage_history_.back();
  DCHECK_EQ(StageType::kTotalLatency, total_latency_stage.stage_type);
  DCHECK(dropped_non_damaging_events_metrics_);

  if (global_trackers_.latency_ukm_reporter) {
    global_trackers_.latency_ukm_reporter->ReportEventLatencyUkm(
        events_metrics_, stage_history_, *processed_blink_breakdown_,
        *processed_viz_breakdown_);
  }

  std::vector<EventLatencyTracker::LatencyData> latencies;

  for (const auto& event_metrics : events_metrics_) {
    DCHECK(event_metrics);
    DCHECK(event_metrics->caused_frame_update());
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

#if BUILDFLAG(IS_ANDROID)
          ReportTopControlsMetric(histogram_base_name, top_controls_moved_,
                                  total_latency, event_metrics->type(),
                                  event_metrics->GetHistogramBucketing());
#endif  // BUILDFLAG(IS_ANDROID)
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
        args_, std::move(latencies));
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

  static constexpr char kTraceCategory[] =
      "cc,benchmark," TRACE_DISABLED_BY_DEFAULT("devtools.timeline.frame");
  bool enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(kTraceCategory, &enabled);
  if (!enabled) {
    return;
  }

  const auto trace_track =
      perfetto::Track(base::trace_event::GetNextGlobalTraceId());
  TRACE_EVENT_BEGIN(
      kTraceCategory, "PipelineReporter", trace_track, args_.frame_time,
      [&](perfetto::EventContext context) {
        using perfetto::protos::pbzero::ChromeFrameReporter2;
        ChromeFrameReporter2::State state;
        switch (info.final_state) {
          case FrameInfo::FrameFinalState::kPresentedAll:
            state = ChromeFrameReporter2::STATE_PRESENTED_ALL;
            break;
          case FrameInfo::FrameFinalState::kPresentedPartialNewMain:
          case FrameInfo::FrameFinalState::kPresentedPartialOldMain:
          case FrameInfo::FrameFinalState::kPresentedPartialWithoutWaiting:
            state = ChromeFrameReporter2::STATE_PRESENTED_PARTIAL;
            break;
          case FrameInfo::FrameFinalState::kNoUpdateDesired:
            state = ChromeFrameReporter2::STATE_NO_UPDATE_DESIRED;
            break;
          case FrameInfo::FrameFinalState::kDropped:
            state = ChromeFrameReporter2::STATE_DROPPED;
            break;
        }

        auto* reporter =
            context.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                ->set_frame_reporter();
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
          DCHECK(state == ChromeFrameReporter2::STATE_DROPPED ||
                 state == ChromeFrameReporter2::STATE_PRESENTED_PARTIAL);
        }
        reporter->set_affects_smoothness(info.IsDroppedAffectingSmoothness());
        ChromeFrameReporter2::ScrollState scroll_state;
        switch (info.scroll_thread) {
          case FrameInfo::SmoothEffectDrivingThread::kMain:
            scroll_state = ChromeFrameReporter2::SCROLL_MAIN_THREAD;
            break;
          case FrameInfo::SmoothEffectDrivingThread::kCompositor:
            scroll_state = ChromeFrameReporter2::SCROLL_COMPOSITOR_THREAD;
            break;
          case FrameInfo::SmoothEffectDrivingThread::kRaster:
            scroll_state = ChromeFrameReporter2::SCROLL_RASTER;
            break;
          case FrameInfo::SmoothEffectDrivingThread::kUnknown:
            scroll_state = ChromeFrameReporter2::SCROLL_NONE;
            break;
        }
        reporter->set_scroll_state(scroll_state);
        reporter->set_has_main_animation(
            HasMainThreadAnimation(active_trackers_));
        reporter->set_has_compositor_animation(
            HasCompositorThreadAnimation(active_trackers_));

        bool has_smooth_input_main = false;
        DCHECK(dropped_non_damaging_events_metrics_);
        for (const auto& event_metrics : events_metrics_) {
          DCHECK(event_metrics->caused_frame_update());
          has_smooth_input_main |= event_metrics->HasSmoothInputEvent();
        }
        reporter->set_has_smooth_input_main(has_smooth_input_main);
        reporter->set_has_high_latency(
            (frame_termination_time_ - args_.frame_time) > kHighLatencyMin);

        if (is_forked_) {
          reporter->set_frame_type(ChromeFrameReporter2::FORKED);
        } else if (is_backfill_) {
          reporter->set_frame_type(ChromeFrameReporter2::BACKFILL);
        }

        reporter->set_surface_frame_trace_id(args_.trace_id);
        const std::optional<int64_t>& display_trace_id =
            viz_breakdown_.presentation_feedback.display_trace_id;
        if (display_trace_id) {
          reporter->set_display_trace_id(*display_trace_id);
        }
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

    if (stage.stage_type ==
        StageType::kSubmitUpdateDisplayTreeToPresentationCompositorFrame) {
      DCHECK(processed_trees_in_viz_breakdown_);
      for (auto it = processed_trees_in_viz_breakdown_->CreateIterator();
           it.IsValid(); it.Advance()) {
        base::TimeTicks start_time = it.GetStartTime();
        base::TimeTicks end_time = it.GetEndTime();
        if (start_time >= end_time) {
          continue;
        }
        const char* breakdown_name =
            GetTreesInVizBreakdownName(it.GetBreakdown());
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

void CompositorFrameReporter::ReportScrollJankMetrics() {
  ReportScrollJankV1Metrics();
  ReportScrollJankV4Metrics();
}

void CompositorFrameReporter::ReportScrollJankV1Metrics() {
  int32_t fling_input_count = 0;
  int32_t normal_input_count = 0;
  float total_predicted_delta = 0;
  bool had_earliest_gesture_scroll = false;
  bool had_latest_gesture_scroll = false;
  bool is_scroll_start = false;

  // This handles cases when we have multiple scroll events. Events for dropped
  // frames are reported by the reporter for next presented frame which could
  // lead to having multiple scroll events.
  // TODO(crbug.com/402148798): Deprecate usage of latest_event.
  base::TimeTicks earliest_event_generation_ts = base::TimeTicks::Max();
  ScrollUpdateEventMetrics* latest_event = nullptr;
  base::TimeTicks latest_event_generation_ts = base::TimeTicks::Min();
  base::TimeTicks last_coalesced_ts = base::TimeTicks::Min();
  for (auto& event : events_metrics_) {
    EventMetrics::EventType event_type = event->type();
    bool caused_frame_update = event->caused_frame_update();
    base::TimeTicks generation_ts = event->GetDispatchStageTimestamp(
        EventMetrics::DispatchStage::kGenerated);
    TRACE_EVENT("input", "GestureType", "gesture", event_type, "generation_ts",
                generation_ts, "caused_frame_update", caused_frame_update);
    if (!caused_frame_update) {
      continue;
    }
    auto* scroll_update = event->AsScrollUpdate();
    if (!scroll_update) {
      continue;
    }
    total_predicted_delta += scroll_update->predicted_delta();
    // Earliest is always applied, event when the scroll update failed to
    // successfully produce a scroll.
    if (!had_earliest_gesture_scroll ||
        generation_ts < earliest_event_generation_ts) {
      earliest_event_generation_ts = generation_ts;
      had_earliest_gesture_scroll = true;
    }

    // We check the type first, as if this `is_scroll_start` we need to save the
    // `latest_event`. Otherwise UKMs will not be emitted.
    switch (event_type) {
      case EventMetrics::EventType::kFirstGestureScrollUpdate:
        is_scroll_start = true;
        [[fallthrough]];
      case EventMetrics::EventType::kGestureScrollUpdate:
        normal_input_count += scroll_update->coalesced_event_count();
        break;
      case EventMetrics::EventType::kInertialGestureScrollUpdate:
        fling_input_count += scroll_update->coalesced_event_count();
        break;
      default:
        NOTREACHED();
    }

    if ((!had_latest_gesture_scroll ||
         generation_ts > latest_event_generation_ts) &&
        (scroll_update->did_scroll() || is_scroll_start)) {
      latest_event = scroll_update;
      latest_event_generation_ts = generation_ts;
      had_latest_gesture_scroll = true;
    }
    last_coalesced_ts =
        std::max(last_coalesced_ts, scroll_update->last_timestamp());
  }

  if (!had_latest_gesture_scroll) {
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
          *latest_event);
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
        latest_event->trace_id());
  }
  if (global_trackers_.scroll_jank_dropped_frame_tracker) {
    global_trackers_.scroll_jank_dropped_frame_tracker
        ->ReportLatestPresentationData(*latest_event, last_coalesced_ts,
                                       end_timestamp, args_.interval);
  }
  if (global_trackers_.scroll_jank_ukm_reporter) {
    global_trackers_.scroll_jank_ukm_reporter
        ->UpdateLatestFrameAndEmitPredictorJank(end_timestamp);
  }
}

void CompositorFrameReporter::ReportScrollJankV4Metrics() {
  // In order for the fast scroll and fling continuity rules of the scroll jank
  // v4 metric to work correctly, they need to be informed about `EventMetrics`
  // that didn't cause frame updates.
  DCHECK(!dropped_non_damaging_events_metrics_);

  if (auto* scroll_jank_v4_processor =
          global_trackers_.scroll_jank_v4_processor) {
    scroll_jank_v4_processor->ProcessEventsMetricsForPresentedFrame(
        events_metrics_,
        /* presentation_ts= */ viz_breakdown_.presentation_feedback.timestamp,
        args_);
  }
}

void CompositorFrameReporter::ReportPaintMetric() const {
  CHECK(paint_metric_.has_value());
  constexpr static char kAverageInvalidatedArea[] =
      "Graphics.Paint.UI.NormalizedInvalidatedArea";

  // For optimal histogram bucketing, convert floating-point values into
  // integers while preserving the desired level of decimal precision.
  constexpr static int kConversionFactor = 100'000;

  // During layer animations (and other cases), many frames are generated but
  // without any repainting. Skipping such frames as reporting these frames will
  // create a bias towards zero when averaging buckets.
  if (paint_metric_ == 0) {
    return;
  }

  // The expected ranges is [0, 6].
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      kAverageInvalidatedArea, paint_metric_.value() * kConversionFactor,
      /*minimum=*/0,
      /*maximum=*/(6 * kConversionFactor) + 1, /*bucket_count=*/50);
}

// TODO(crbug.com/454006102): Report EventLatency for TreesInViz mode
void CompositorFrameReporter::ReportEventLatencyTraceEvents() const {
  DCHECK(dropped_non_damaging_events_metrics_);
  for (const auto& event_metrics : events_metrics_) {
    DCHECK(event_metrics->caused_frame_update());
    EventLatencyTracingRecorder::RecordEventLatencyTraceEvent(
        event_metrics.get(), frame_termination_time_, &args_, &stage_history_,
        processed_viz_breakdown_.get(),
        viz_breakdown_.presentation_feedback.display_trace_id);
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
  std::optional<FrameFinalState> final_state_v4;
  FrameFinalState final_state_raster_property =
      FrameFinalState::kNoUpdateDesired;
  FrameFinalState final_state_raster_scroll = FrameFinalState::kNoUpdateDesired;
  auto smooth_thread = smooth_thread_;
  auto scrolling_thread = scrolling_thread_;

  switch (frame_termination_status_) {
    case FrameTerminationStatus::kPresentedFrame:
      if (has_partial_update_) {
        final_state = is_accompanied_by_main_thread_update_
                          ? FrameFinalState::kPresentedPartialNewMain
                          : FrameFinalState::kPresentedPartialOldMain;
        // When the Main thread effect is not throttle, and we are not scheduled
        // to wait for it, then we do not expect its update to accompany the
        // Compositor thread effect. When throttling the Active Tree will become
        // stale compared to the Compositor thread effect. We allow one frame of
        // staleness. Larger periods than that denote that the Main thread is
        // failing to produce new content. So we allow those to be marked as
        // dropped.
        if (!will_throttle_main_ && !waiting_for_main_ &&
            active_tree_staleness_ <= 1) {
          final_state_v4 = FrameFinalState::kPresentedPartialWithoutWaiting;
        }
      } else {
        final_state = FrameFinalState::kPresentedAll;
      }

      final_state_raster_property = final_state;
      final_state_raster_scroll = final_state;
      if (want_new_tree_ && !created_new_tree_) {
        final_state_raster_property = FrameFinalState::kDropped;
      }
      if (scrolling_thread == FrameInfo::SmoothEffectDrivingThread::kRaster) {
        if (invalidate_raster_scroll_ && !created_new_tree_) {
          final_state_raster_scroll = FrameFinalState::kDropped;
        }
      }
      break;

    case FrameTerminationStatus::kDidNotPresentFrame:
    case FrameTerminationStatus::kReplacedByNewReporter:
      final_state = FrameFinalState::kDropped;
      final_state_raster_property = FrameFinalState::kDropped;
      final_state_raster_scroll = FrameFinalState::kDropped;
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

      final_state_raster_property = final_state;
      if (want_new_tree_ && !created_new_tree_) {
        final_state_raster_property = FrameFinalState::kDropped;
      }
      final_state_raster_scroll = final_state;
      if (scrolling_thread == FrameInfo::SmoothEffectDrivingThread::kRaster &&
          !invalidate_raster_scroll_) {
        final_state_raster_scroll = FrameFinalState::kDropped;
      }

      // TDOD(crbug.com/369633237): The following assumption is no longer
      // correct. The logic remains while V3 PercentFrameDropped metrics
      // continue to be exported. If the compositor-thread is running an
      // animation, and it ends with 'did not produce frame', then that implies
      // that the compositor animation did not cause any visual changes. So for
      // such cases, update the `smooth_thread` for the FrameInfo created to
      // exclude the compositor thread. However, it is important to keep
      // `final_state` unchanged, because the main-thread update (if any) did
      // get dropped.
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

  // It is possible that we may have been informed of `kDidNotPresentFrame` or
  // `kDidNotProduceFrame` depending on scheduling choices made during this
  // frame. For both cases WaitingOnMain is not accurate when we may be
  // throttling the Main thread. In such cases updated logic in
  // CompositorFrameReportingController will denote if we actually had no damage
  // expected.
  if (frame_skipped_reason_v4_.has_value() &&
      frame_skipped_reason_v4_.value() == FrameSkippedReason::kNoDamage) {
    final_state_v4 = FrameFinalState::kNoUpdateDesired;
  }

  FrameInfo info;

  // We separate final state and smooth thread fields while both V3 and V4
  // metrics are being reported. V3 and V3 metrics make different assumptions
  // about dropped frames, resulting in different final FrameInfo states.
  info.final_state = final_state;
  info.final_state_v4 = final_state_v4.value_or(final_state);
  info.final_state_raster_property = final_state_raster_property;
  info.final_state_raster_scroll = final_state_raster_scroll;
  info.smooth_thread = smooth_thread;
  info.smooth_thread_raster_property = smooth_thread_;
  info.scroll_thread = scrolling_thread;
  info.checkerboarded_needs_raster = checkerboarded_needs_raster_;
  info.checkerboarded_needs_record = checkerboarded_needs_record_;
  info.sequence_number = args_.frame_id.sequence_number;
  info.did_raster_inducing_scroll = invalidate_raster_scroll_;

  if (frame_skip_reason_.has_value() &&
      frame_skip_reason() == FrameSkippedReason::kNoDamage) {
    // If the frame was explicitly skipped because of 'no damage', then that
    // means this frame contains the response ('no damage') from the
    // main-thread.
    info.main_thread_response = FrameInfo::MainThreadResponse::kIncluded;
    // Main threaded work can often cause no damage. Such as when rAF is running
    // but changing content outside the viewport. Or being used by a site for
    // time based delayed work, and never actually drawing. Do not treat these
    // as drops. Since they do not prevent the Compositor thread from
    // submitting, and there is nothing visual for user to miss.
    if (info.smooth_thread == SmoothThread::kSmoothMain) {
      info.final_state_v4 = FrameFinalState::kNoUpdateDesired;
    }
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

}  // namespace cc
