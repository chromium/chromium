// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/compositor_frame_reporter.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/rolling_time_delta_history.h"
#include "cc/metrics/dropped_frame_counter.h"
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

// Names for the viz breakdowns that are shown in trace as substages under
// PipelineReporter -> SubmitCompositorFrameToPresentationCompositorFrame or
// EventLatency -> SubmitCompositorFrameToPresentationCompositorFrame.
constexpr const char* GetVizBreakdownName(VizBreakdown stage) {
  switch (stage) {
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
      return "";
  }
}

// Names for CompositorFrameReporter::StageType, which should be updated in case
// of changes to the enum.
constexpr const char* GetStageName(int stage_type_index,
                                   bool impl_only = false) {
  switch (stage_type_index) {
    case static_cast<int>(StageType::kBeginImplFrameToSendBeginMainFrame):
      if (impl_only)
        return "BeginImplFrameToFinishImpl";
      return "BeginImplFrameToSendBeginMainFrame";
    case static_cast<int>(StageType::kSendBeginMainFrameToCommit):
      if (impl_only)
        return "SendBeginMainFrameToBeginMainAbort";
      return "SendBeginMainFrameToCommit";
    case static_cast<int>(StageType::kCommit):
      return "Commit";
    case static_cast<int>(StageType::kEndCommitToActivation):
      return "EndCommitToActivation";
    case static_cast<int>(StageType::kActivation):
      return "Activation";
    case static_cast<int>(StageType::kEndActivateToSubmitCompositorFrame):
      if (impl_only)
        return "ImplFrameDoneToSubmitCompositorFrame";
      return "EndActivateToSubmitCompositorFrame";
    case static_cast<int>(
        StageType::kSubmitCompositorFrameToPresentationCompositorFrame):
      return "SubmitCompositorFrameToPresentationCompositorFrame";
    case static_cast<int>(StageType::kTotalLatency):
      return "TotalLatency";
    case static_cast<int>(VizBreakdown::kSubmitToReceiveCompositorFrame) +
        kVizBreakdownInitialIndex:
      return "SubmitCompositorFrameToPresentationCompositorFrame."
             "SubmitToReceiveCompositorFrame";
    case static_cast<int>(VizBreakdown::kReceivedCompositorFrameToStartDraw) +
        kVizBreakdownInitialIndex:
      return "SubmitCompositorFrameToPresentationCompositorFrame."
             "ReceivedCompositorFrameToStartDraw";
    case static_cast<int>(VizBreakdown::kStartDrawToSwapStart) +
        kVizBreakdownInitialIndex:
      return "SubmitCompositorFrameToPresentationCompositorFrame."
             "StartDrawToSwapStart";
    case static_cast<int>(VizBreakdown::kSwapStartToSwapEnd) +
        kVizBreakdownInitialIndex:
      return "SubmitCompositorFrameToPresentationCompositorFrame."
             "SwapStartToSwapEnd";
    case static_cast<int>(VizBreakdown::kSwapEndToPresentationCompositorFrame) +
        kVizBreakdownInitialIndex:
      return "SubmitCompositorFrameToPresentationCompositorFrame."
             "SwapEndToPresentationCompositorFrame";
    case static_cast<int>(VizBreakdown::kSwapStartToBufferAvailable) +
        kVizBreakdownInitialIndex:
      return "SubmitCompositorFrameToPresentationCompositorFrame."
             "SwapStartToBufferAvailable";
    case static_cast<int>(VizBreakdown::kBufferAvailableToBufferReady) +
        kVizBreakdownInitialIndex:
      return "SubmitCompositorFrameToPresentationCompositorFrame."
             "BufferAvailableToBufferReady";
    case static_cast<int>(VizBreakdown::kBufferReadyToLatch) +
        kVizBreakdownInitialIndex:
      return "SubmitCompositorFrameToPresentationCompositorFrame."
             "BufferReadyToLatch";
    case static_cast<int>(VizBreakdown::kLatchToSwapEnd) +
        kVizBreakdownInitialIndex:
      return "SubmitCompositorFrameToPresentationCompositorFrame."
             "LatchToSwapEnd";
    case static_cast<int>(BlinkBreakdown::kHandleInputEvents) +
        kBlinkBreakdownInitialIndex:
      return "SendBeginMainFrameToCommit.HandleInputEvents";
    case static_cast<int>(BlinkBreakdown::kAnimate) +
        kBlinkBreakdownInitialIndex:
      return "SendBeginMainFrameToCommit.Animate";
    case static_cast<int>(BlinkBreakdown::kStyleUpdate) +
        kBlinkBreakdownInitialIndex:
      return "SendBeginMainFrameToCommit.StyleUpdate";
    case static_cast<int>(BlinkBreakdown::kLayoutUpdate) +
        kBlinkBreakdownInitialIndex:
      return "SendBeginMainFrameToCommit.LayoutUpdate";
    case static_cast<int>(BlinkBreakdown::kPrepaint) +
        kBlinkBreakdownInitialIndex:
      return "SendBeginMainFrameToCommit.Prepaint";
    case static_cast<int>(BlinkBreakdown::kCompositingInputs) +
        kBlinkBreakdownInitialIndex:
      return "SendBeginMainFrameToCommit.CompositingInputs";
    case static_cast<int>(BlinkBreakdown::kCompositingAssignments) +
        kBlinkBreakdownInitialIndex:
      return "SendBeginMainFrameToCommit.CompositingAssignments";
    case static_cast<int>(BlinkBreakdown::kPaint) + kBlinkBreakdownInitialIndex:
      return "SendBeginMainFrameToCommit.Paint";
    case static_cast<int>(BlinkBreakdown::kScrollingCoordinator) +
        kBlinkBreakdownInitialIndex:
      return "SendBeginMainFrameToCommit.ScrollingCoordinator";
    case static_cast<int>(BlinkBreakdown::kCompositeCommit) +
        kBlinkBreakdownInitialIndex:
      return "SendBeginMainFrameToCommit.CompositeCommit";
    case static_cast<int>(BlinkBreakdown::kUpdateLayers) +
        kBlinkBreakdownInitialIndex:
      return "SendBeginMainFrameToCommit.UpdateLayers";
    case static_cast<int>(BlinkBreakdown::kBeginMainSentToStarted) +
        kBlinkBreakdownInitialIndex:
      return "SendBeginMainFrameToCommit.BeginMainSentToStarted";
    default:
      NOTREACHED();
      return "";
  }
}

// Names for CompositorFrameReporter::FrameReportType, which should be
// updated in case of changes to the enum.
constexpr const char* kReportTypeNames[]{
    "", "MissedDeadlineFrame.", "DroppedFrame.", "CompositorOnlyFrame."};

static_assert(base::size(kReportTypeNames) == kFrameReportTypeCount,
              "Compositor latency report types has changed.");

// This value should be recalculated in case of changes to the number of values
// in CompositorFrameReporter::DroppedFrameReportType or in
// CompositorFrameReporter::StageType
constexpr int kMaxCompositorLatencyHistogramIndex =
    kFrameReportTypeCount * kFrameSequenceTrackerTypeCount *
    (kStageTypeCount + kAllBreakdownCount);
constexpr base::TimeDelta kCompositorLatencyHistogramMin =
    base::TimeDelta::FromMicroseconds(1);
constexpr base::TimeDelta kCompositorLatencyHistogramMax =
    base::TimeDelta::FromMilliseconds(350);
constexpr int kCompositorLatencyHistogramBucketCount = 50;

constexpr int kEventLatencyEventTypeCount =
    static_cast<int>(EventMetrics::EventType::kMaxValue) + 1;
constexpr int kEventLatencyScrollTypeCount =
    static_cast<int>(EventMetrics::ScrollType::kMaxValue) + 1;
constexpr int kMaxEventLatencyHistogramBaseIndex =
    kEventLatencyEventTypeCount * kEventLatencyScrollTypeCount;
constexpr int kMaxEventLatencyHistogramIndex =
    kMaxEventLatencyHistogramBaseIndex * (kStageTypeCount + kAllBreakdownCount);
constexpr base::TimeDelta kEventLatencyHistogramMin =
    base::TimeDelta::FromMicroseconds(1);
constexpr base::TimeDelta kEventLatencyHistogramMax =
    base::TimeDelta::FromSeconds(5);
constexpr int kEventLatencyHistogramBucketCount = 100;

std::string GetCompositorLatencyHistogramName(
    const int report_type_index,
    FrameSequenceTrackerType frame_sequence_tracker_type,
    const int stage_type_index) {
  DCHECK_LE(frame_sequence_tracker_type, FrameSequenceTrackerType::kMaxType);
  const char* tracker_type_name =
      FrameSequenceTracker::GetFrameSequenceTrackerTypeName(
          frame_sequence_tracker_type);
  DCHECK(tracker_type_name);
  bool impl_only_frame =
      (report_type_index ==
       static_cast<int>(FrameReportType::kCompositorOnlyFrame));
  return base::StrCat({"CompositorLatency.",
                       kReportTypeNames[report_type_index], tracker_type_name,
                       *tracker_type_name ? "." : "",
                       GetStageName(stage_type_index, impl_only_frame)});
}

std::string GetEventLatencyHistogramBaseName(
    const EventMetrics& event_metrics) {
  const bool is_scroll = event_metrics.scroll_type().has_value();
  return base::StrCat({"EventLatency.", event_metrics.GetTypeName(),
                       is_scroll ? "." : "",
                       is_scroll ? event_metrics.GetScrollTypeName() : ""});
}

base::TimeTicks ComputeSafeDeadlineForFrame(const viz::BeginFrameArgs& args) {
  return args.frame_time + (args.interval * 1.5);
}

void ReportOffsetBetweenDeadlineAndPresentationTime(
    const viz::BeginFrameArgs& args,
    base::TimeTicks presentation_time) {
  const base::TimeTicks strict_deadline = args.frame_time + args.interval;
  const base::TimeDelta offset = presentation_time - strict_deadline;
  // |strict_deadline| and |presentation_time| should normally be pretty close.
  // Measure how close they are.
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "CompositorLatency.Diagnostic.PresentationTimeDeltaFromDeadline", offset,
      base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromMilliseconds(32), /*bucket_count=*/16);
}

#define REPORT_VIZ_TRACE_EVENT(start_time, end_time, index, trace_func) \
  if (start_time <= end_time) {                                         \
    const char* substage_name =                                         \
        GetVizBreakdownName(static_cast<VizBreakdown>(index));          \
    trace_func(start_time, end_time, substage_name);                    \
  }

#define REPORT_VIZ_BREAKDOWN_TRACES(trace_func)                                \
  size_t start_to_buffer_available =                                           \
      static_cast<size_t>(VizBreakdown::kSwapStartToBufferAvailable);          \
  bool has_ready_timings = !!viz_breakdown_list_[start_to_buffer_available];   \
  for (size_t i = 0; i < start_to_buffer_available; i++) {                     \
    if (!viz_breakdown_list_[i])                                               \
      break;                                                                   \
                                                                               \
    if (i == static_cast<size_t>(VizBreakdown::kSwapStartToSwapEnd) &&         \
        has_ready_timings) {                                                   \
      size_t latch_to_swap_end =                                               \
          static_cast<size_t>(VizBreakdown::kLatchToSwapEnd);                  \
      for (size_t j = start_to_buffer_available; j <= latch_to_swap_end;       \
           j++) {                                                              \
        REPORT_VIZ_TRACE_EVENT(viz_breakdown_list_[j]->first,                  \
                               viz_breakdown_list_[j]->second, j, trace_func); \
      }                                                                        \
    } else {                                                                   \
      REPORT_VIZ_TRACE_EVENT(viz_breakdown_list_[i]->first,                    \
                             viz_breakdown_list_[i]->second, i, trace_func);   \
    }                                                                          \
  }

}  // namespace

CompositorFrameReporter::CompositorFrameReporter(
    const ActiveTrackers& active_trackers,
    const viz::BeginFrameArgs& args,
    LatencyUkmReporter* latency_ukm_reporter,
    bool should_report_metrics,
    SmoothThread smooth_thread,
    int layer_tree_host_id)
    : should_report_metrics_(should_report_metrics),
      args_(args),
      active_trackers_(active_trackers),
      latency_ukm_reporter_(latency_ukm_reporter),
      smooth_thread_(smooth_thread),
      layer_tree_host_id_(layer_tree_host_id) {}

std::unique_ptr<CompositorFrameReporter>
CompositorFrameReporter::CopyReporterAtBeginImplStage() {
  if (stage_history_.empty() ||
      stage_history_.front().stage_type !=
          StageType::kBeginImplFrameToSendBeginMainFrame ||
      (!did_finish_impl_frame() && !did_not_produce_frame_time_.has_value())) {
    return nullptr;
  }
  auto new_reporter = std::make_unique<CompositorFrameReporter>(
      active_trackers_, args_, latency_ukm_reporter_, should_report_metrics_,
      smooth_thread_, layer_tree_host_id_);
  new_reporter->did_finish_impl_frame_ = did_finish_impl_frame_;
  new_reporter->impl_frame_finish_time_ = impl_frame_finish_time_;
  new_reporter->main_frame_abort_time_ = main_frame_abort_time_;
  new_reporter->current_stage_.stage_type =
      StageType::kBeginImplFrameToSendBeginMainFrame;
  new_reporter->current_stage_.start_time = stage_history_.front().start_time;
  new_reporter->set_tick_clock(tick_clock_);
  new_reporter->SetDroppedFrameCounter(dropped_frame_counter_);
  new_reporter->cloned_from_ = weak_factory_.GetWeakPtr();

  // TODO(https://crbug.com/1127872) Check |cloned_to_| is null before replacing
  // it.
  cloned_to_ = new_reporter->GetWeakPtr();
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
  // stage
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

void CompositorFrameReporter::SetEventsMetrics(
    std::vector<EventMetrics> events_metrics) {
  DCHECK_EQ(0u, events_metrics_.size());
  events_metrics_ = std::move(events_metrics);
}

void CompositorFrameReporter::TerminateReporter() {
  if (frame_termination_status_ == FrameTerminationStatus::kUnknown)
    TerminateFrame(FrameTerminationStatus::kUnknown, Now());

  PopulateBlinkBreakdownList();
  PopulateVizBreakdownList();

  DCHECK_EQ(current_stage_.start_time, base::TimeTicks());
  switch (frame_termination_status_) {
    case FrameTerminationStatus::kPresentedFrame:
      EnableReportType(FrameReportType::kNonDroppedFrame);
      ReportOffsetBetweenDeadlineAndPresentationTime(args_,
                                                     frame_termination_time_);
      if (ComputeSafeDeadlineForFrame(args_) < frame_termination_time_)
        EnableReportType(FrameReportType::kMissedDeadlineFrame);
      break;
    case FrameTerminationStatus::kDidNotPresentFrame:
      EnableReportType(FrameReportType::kDroppedFrame);
      break;
    case FrameTerminationStatus::kReplacedByNewReporter:
      EnableReportType(FrameReportType::kDroppedFrame);
      break;
    case FrameTerminationStatus::kDidNotProduceFrame:
      if (frame_skip_reason_.has_value() &&
          frame_skip_reason() == FrameSkippedReason::kNoDamage) {
        // If this reporter was cloned, and the cloned repoter was marked as
        // containing 'partial update' (i.e. missing desired updates from the
        // main-thread), but this reporter terminated with 'no damage', then
        // reset the 'partial update' flag from the cloned reporter.
        if (cloned_to_ && cloned_to_->has_partial_update())
          cloned_to_->set_has_partial_update(false);
      } else {
        // If no frames were produced, it was not due to no-damage, then it is a
        // dropped frame.
        EnableReportType(FrameReportType::kDroppedFrame);
      }
      break;
    case FrameTerminationStatus::kUnknown:
      break;
  }

  ReportCompositorLatencyTraceEvents();
  if (TestReportType(FrameReportType::kNonDroppedFrame))
    ReportEventLatencyTraceEvents();

  // Only report compositor latency histograms if the frame was produced.
  if (should_report_metrics_ && report_types_.any()) {
    DCHECK(stage_history_.size());
    DCHECK_EQ(SumOfStageHistory(), stage_history_.back().end_time -
                                       stage_history_.front().start_time);
    stage_history_.emplace_back(StageType::kTotalLatency,
                                stage_history_.front().start_time,
                                stage_history_.back().end_time);

    ReportCompositorLatencyHistograms();
    // Only report event latency histograms if the frame was presented.
    if (TestReportType(FrameReportType::kNonDroppedFrame))
      ReportEventLatencyHistograms();
  }

  if (dropped_frame_counter_) {
    if (TestReportType(FrameReportType::kDroppedFrame)) {
      dropped_frame_counter_->AddDroppedFrame();
    } else {
      if (has_partial_update_)
        dropped_frame_counter_->AddPartialFrame();
      else
        dropped_frame_counter_->AddGoodFrame();
    }

    if (IsDroppedFrameAffectingSmoothness())
      dropped_frame_counter_->AddDroppedFrameAffectingSmoothness();
  }
}

void CompositorFrameReporter::EndCurrentStage(base::TimeTicks end_time) {
  if (current_stage_.start_time == base::TimeTicks())
    return;
  current_stage_.end_time = end_time;
  stage_history_.push_back(current_stage_);
  current_stage_.start_time = base::TimeTicks();
}

void CompositorFrameReporter::ReportCompositorLatencyHistograms() const {
  for (const StageData& stage : stage_history_) {
    ReportStageHistogramWithBreakdown(stage);
    for (size_t type = 0; type < active_trackers_.size(); ++type) {
      if (active_trackers_.test(type)) {
        // Report stage breakdowns.
        ReportStageHistogramWithBreakdown(
            stage, static_cast<FrameSequenceTrackerType>(type));
      }
    }
  }
  for (size_t type = 0; type < report_types_.size(); ++type) {
    if (!report_types_.test(type))
      continue;
    FrameReportType report_type = static_cast<FrameReportType>(type);
    UMA_HISTOGRAM_ENUMERATION("CompositorLatency.Type", report_type);
    if (latency_ukm_reporter_) {
      latency_ukm_reporter_->ReportCompositorLatencyUkm(
          report_type, stage_history_, active_trackers_, viz_breakdown_);
    }
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
        case FrameSequenceTrackerType::kCanvas:
          UMA_HISTOGRAM_ENUMERATION("CompositorLatency.Type.Canvas",
                                    report_type);
          break;
        case FrameSequenceTrackerType::kJSAnimation:
          UMA_HISTOGRAM_ENUMERATION("CompositorLatency.Type.JSAnimation",
                                    report_type);
          break;
        case FrameSequenceTrackerType::kCustom:
        case FrameSequenceTrackerType::kMaxType:
          NOTREACHED();
          break;
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
}

void CompositorFrameReporter::ReportStageHistogramWithBreakdown(
    const CompositorFrameReporter::StageData& stage,
    FrameSequenceTrackerType frame_sequence_tracker_type) const {
  base::TimeDelta stage_delta = stage.end_time - stage.start_time;
  ReportCompositorLatencyHistogram(frame_sequence_tracker_type,
                                   static_cast<int>(stage.stage_type),
                                   stage_delta);
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
  for (size_t i = 0; i < base::size(blink_breakdown_list_); i++) {
    ReportCompositorLatencyHistogram(frame_sequence_tracker_type,
                                     kBlinkBreakdownInitialIndex + i,
                                     blink_breakdown_list_[i]);
  }
}

void CompositorFrameReporter::ReportCompositorLatencyVizBreakdowns(
    FrameSequenceTrackerType frame_sequence_tracker_type) const {
  for (size_t i = 0; i < base::size(viz_breakdown_list_); i++) {
    if (!viz_breakdown_list_[i]) {
#if DCHECK_IS_ON()
      // Remaining breakdowns should be unset.
      for (; i < base::size(viz_breakdown_list_); i++)
        DCHECK(!viz_breakdown_list_[i]);
#endif
      break;
    }
    const base::TimeTicks start_time = viz_breakdown_list_[i]->first;
    const base::TimeTicks end_time = viz_breakdown_list_[i]->second;
    ReportCompositorLatencyHistogram(frame_sequence_tracker_type,
                                     kVizBreakdownInitialIndex + i,
                                     end_time - start_time);
  }
}

void CompositorFrameReporter::ReportCompositorLatencyHistogram(
    FrameSequenceTrackerType frame_sequence_tracker_type,
    const int stage_type_index,
    base::TimeDelta time_delta) const {
  for (size_t type = 0; type < report_types_.size(); ++type) {
    if (!report_types_.test(type))
      continue;
    FrameReportType report_type = static_cast<FrameReportType>(type);
    const int report_type_index = static_cast<int>(report_type);
    const int frame_sequence_tracker_type_index =
        static_cast<int>(frame_sequence_tracker_type);
    const int histogram_index =
        (stage_type_index * kFrameSequenceTrackerTypeCount +
         frame_sequence_tracker_type_index) *
            kFrameReportTypeCount +
        report_type_index;

    CHECK_LT(stage_type_index, kStageTypeCount + kAllBreakdownCount);
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
            report_type_index, frame_sequence_tracker_type, stage_type_index),
        histogram_index, kMaxCompositorLatencyHistogramIndex,
        AddTimeMicrosecondsGranularity(time_delta),
        base::Histogram::FactoryMicrosecondsTimeGet(
            GetCompositorLatencyHistogramName(report_type_index,
                                              frame_sequence_tracker_type,
                                              stage_type_index),
            kCompositorLatencyHistogramMin, kCompositorLatencyHistogramMax,
            kCompositorLatencyHistogramBucketCount,
            base::HistogramBase::kUmaTargetedHistogramFlag));
  }
}

void CompositorFrameReporter::ReportEventLatencyHistograms() const {
  for (const EventMetrics& event_metrics : events_metrics_) {
    const std::string histogram_base_name =
        GetEventLatencyHistogramBaseName(event_metrics);
    const int event_type_index = static_cast<int>(event_metrics.type());
    const int scroll_type_index =
        event_metrics.scroll_type()
            ? static_cast<int>(*event_metrics.scroll_type())
            : 0;
    const int histogram_base_index =
        event_type_index * kEventLatencyScrollTypeCount + scroll_type_index;

    // For scroll events, report total latency up to gpu-swap-begin. This is
    // useful in comparing new EventLatency metrics with LatencyInfo-based
    // scroll event latency metrics.
    if (event_metrics.scroll_type() && !viz_breakdown_.swap_timings.is_null()) {
      const base::TimeDelta swap_begin_latency =
          viz_breakdown_.swap_timings.swap_start - event_metrics.time_stamp();
      const std::string swap_begin_histogram_name =
          histogram_base_name + ".TotalLatencyToSwapBegin";
      // Note: There's a 1:1 mapping between `histogram_base_index` and
      // `swap_begin_histogram_name` which allows the use of
      // `STATIC_HISTOGRAM_POINTER_GROUP()` to cache histogram objects.
      STATIC_HISTOGRAM_POINTER_GROUP(
          swap_begin_histogram_name, histogram_base_index,
          kMaxEventLatencyHistogramBaseIndex,
          AddTimeMicrosecondsGranularity(swap_begin_latency),
          base::Histogram::FactoryMicrosecondsTimeGet(
              swap_begin_histogram_name, kEventLatencyHistogramMin,
              kEventLatencyHistogramMax, kEventLatencyHistogramBucketCount,
              base::HistogramBase::kUmaTargetedHistogramFlag));
    }

    // It is possible for an event to arrive in the compositor in the middle of
    // a frame (e.g. the browser received the event *after* renderer received a
    // begin-impl, and the event reached the compositor before that frame
    // ended). To handle such cases, find the first stage that happens after the
    // event's arrival in the browser.
    auto stage_it =
        std::find_if(stage_history_.begin(), stage_history_.end(),
                     [&event_metrics](const StageData& stage) {
                       return stage.start_time > event_metrics.time_stamp();
                     });
    // TODO(crbug.com/1079116): Ideally, at least the start time of
    // SubmitCompositorFrameToPresentationCompositorFrame stage should be
    // greater than the event time stamp, but apparently, this is not always the
    // case (see crbug.com/1093698). For now, skip to the next event in such
    // cases. Hopefully, the work to reduce discrepancies between the new
    // EventLatency and the old Event.Latency metrics would fix this issue. If
    // not, we need to reconsider investigating this issue.
    if (stage_it == stage_history_.end())
      continue;

    const base::TimeDelta b2r_latency =
        stage_it->start_time - event_metrics.time_stamp();
    const std::string b2r_histogram_name =
        histogram_base_name + ".BrowserToRendererCompositor";
    // Note: There's a 1:1 mapping between `histogram_base_index` and
    // `b2r_histogram_name` which allows the use of
    // `STATIC_HISTOGRAM_POINTER_GROUP()` to cache histogram objects.
    STATIC_HISTOGRAM_POINTER_GROUP(
        b2r_histogram_name, histogram_base_index,
        kMaxEventLatencyHistogramBaseIndex,
        AddTimeMicrosecondsGranularity(b2r_latency),
        base::Histogram::FactoryMicrosecondsTimeGet(
            b2r_histogram_name, kEventLatencyHistogramMin,
            kEventLatencyHistogramMax, kEventLatencyHistogramBucketCount,
            base::HistogramBase::kUmaTargetedHistogramFlag));

    for (; stage_it != stage_history_.end(); ++stage_it) {
      // Total latency is calculated since the event timestamp.
      const base::TimeTicks start_time =
          stage_it->stage_type == StageType::kTotalLatency
              ? event_metrics.time_stamp()
              : stage_it->start_time;
      const base::TimeDelta latency = stage_it->end_time - start_time;
      const int stage_type_index = static_cast<int>(stage_it->stage_type);
      ReportEventLatencyHistogram(histogram_base_index, histogram_base_name,
                                  stage_type_index, latency);

      switch (stage_it->stage_type) {
        case StageType::kSendBeginMainFrameToCommit:
          ReportEventLatencyBlinkBreakdowns(histogram_base_index,
                                            histogram_base_name);
          break;
        case StageType::kSubmitCompositorFrameToPresentationCompositorFrame:
          ReportEventLatencyVizBreakdowns(histogram_base_index,
                                          histogram_base_name);
          break;
        case StageType::kTotalLatency:
          UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
              "EventLatency.TotalLatency", latency, kEventLatencyHistogramMin,
              kEventLatencyHistogramMax, kEventLatencyHistogramBucketCount);
          break;
        default:
          break;
      }
    }
  }

  if (latency_ukm_reporter_) {
    latency_ukm_reporter_->ReportEventLatencyUkm(
        events_metrics_, stage_history_, viz_breakdown_);
  }
}

void CompositorFrameReporter::ReportEventLatencyBlinkBreakdowns(
    int histogram_base_index,
    const std::string& histogram_base_name) const {
  for (size_t i = 0; i < base::size(blink_breakdown_list_); i++) {
    ReportEventLatencyHistogram(histogram_base_index, histogram_base_name,
                                kBlinkBreakdownInitialIndex + i,
                                blink_breakdown_list_[i]);
  }
}

void CompositorFrameReporter::ReportEventLatencyVizBreakdowns(
    int histogram_base_index,
    const std::string& histogram_base_name) const {
  for (size_t i = 0; i < base::size(viz_breakdown_list_); i++) {
    if (!viz_breakdown_list_[i]) {
#if DCHECK_IS_ON()
      // Remaining breakdowns should be unset.
      for (; i < base::size(viz_breakdown_list_); i++)
        DCHECK(!viz_breakdown_list_[i]);
#endif
      break;
    }
    const base::TimeTicks start_time = viz_breakdown_list_[i]->first;
    const base::TimeTicks end_time = viz_breakdown_list_[i]->second;
    ReportEventLatencyHistogram(histogram_base_index, histogram_base_name,
                                kVizBreakdownInitialIndex + i,
                                end_time - start_time);
  }
}

void CompositorFrameReporter::ReportEventLatencyHistogram(
    int histogram_base_index,
    const std::string& histogram_base_name,
    int stage_type_index,
    base::TimeDelta latency) const {
  const std::string histogram_name =
      base::StrCat({histogram_base_name, ".", GetStageName(stage_type_index)});
  const int histogram_index =
      histogram_base_index * (kStageTypeCount + kAllBreakdownCount) +
      stage_type_index;
  // Note: There's a 1:1 mapping between `histogram_index` and `histogram_name`
  // which allows the use of `STATIC_HISTOGRAM_POINTER_GROUP()` to cache
  // histogram objects.
  STATIC_HISTOGRAM_POINTER_GROUP(
      histogram_name, histogram_index, kMaxEventLatencyHistogramIndex,
      AddTimeMicrosecondsGranularity(latency),
      base::Histogram::FactoryMicrosecondsTimeGet(
          histogram_name, kEventLatencyHistogramMin, kEventLatencyHistogramMax,
          kEventLatencyHistogramBucketCount,
          base::HistogramBase::kUmaTargetedHistogramFlag));
}

void CompositorFrameReporter::ReportCompositorLatencyTraceEvents() const {
  if (stage_history_.empty())
    return;

  if (IsDroppedFrameAffectingSmoothness()) {
    devtools_instrumentation::DidDropSmoothnessFrame(layer_tree_host_id_,
                                                     args_.frame_time);
  }

  const auto trace_track = perfetto::Track(reinterpret_cast<uint64_t>(this));
  TRACE_EVENT_BEGIN(
      "cc,benchmark", "PipelineReporter", trace_track,
      stage_history_.front().start_time, [&](perfetto::EventContext context) {
        using perfetto::protos::pbzero::ChromeFrameReporter;
        bool frame_dropped = TestReportType(FrameReportType::kDroppedFrame);
        ChromeFrameReporter::State state;
        if (frame_dropped) {
          state = ChromeFrameReporter::STATE_DROPPED;
        } else if (frame_termination_status_ ==
                   FrameTerminationStatus::kDidNotProduceFrame) {
          state = ChromeFrameReporter::STATE_NO_UPDATE_DESIRED;
        } else {
          state = has_partial_update()
                      ? ChromeFrameReporter::STATE_PRESENTED_PARTIAL
                      : ChromeFrameReporter::STATE_PRESENTED_ALL;
        }
        auto* reporter = context.event()->set_chrome_frame_reporter();
        reporter->set_state(state);
        reporter->set_frame_source(args_.frame_id.source_id);
        reporter->set_frame_sequence(args_.frame_id.sequence_number);
        if (IsDroppedFrameAffectingSmoothness()) {
          DCHECK(state == ChromeFrameReporter::STATE_DROPPED ||
                 state == ChromeFrameReporter::STATE_PRESENTED_PARTIAL);
          reporter->set_affects_smoothness(true);
        }
        // TODO(crbug.com/1086974): Set 'drop reason' if applicable.
      });

  // The trace-viewer cannot seem to handle a single child-event that has the
  // same start/end timestamps as the parent-event. So avoid adding the
  // child-events if there's only one.
  if (stage_history_.size() > 1) {
    for (const auto& stage : stage_history_) {
      const int stage_type_index = static_cast<int>(stage.stage_type);
      CHECK_LT(stage_type_index, static_cast<int>(StageType::kStageTypeCount));
      CHECK_GE(stage_type_index, 0);
      if (stage.start_time >= frame_termination_time_)
        break;
      DCHECK_GE(stage.end_time, stage.start_time);
      if (stage.start_time == stage.end_time)
        continue;
      const char* stage_name = GetStageName(stage_type_index);
      TRACE_EVENT_BEGIN("cc,benchmark", stage_name, trace_track,
                        stage.start_time);
      if (stage.stage_type ==
          StageType::kSubmitCompositorFrameToPresentationCompositorFrame) {
        REPORT_VIZ_BREAKDOWN_TRACES([&](base::TimeTicks start_time,
                                        base::TimeTicks end_time,
                                        const char* substage_name) {
          TRACE_EVENT_BEGIN("cc,benchmark", substage_name, trace_track,
                            start_time);
          TRACE_EVENT_END("cc,benchmark", trace_track, end_time);
        });
      }
      TRACE_EVENT_END("cc,benchmark", trace_track, stage.end_time);
    }
  }

  TRACE_EVENT_END("cc,benchmark", trace_track, frame_termination_time_);
}

void CompositorFrameReporter::ReportEventLatencyTraceEvents() const {
  for (const EventMetrics& event_metrics : events_metrics_) {
    const auto trace_id = TRACE_ID_LOCAL(&event_metrics);
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP1(
        "cc,input", "EventLatency", trace_id, event_metrics.time_stamp(),
        "event", event_metrics.GetTypeName());

    // Find the first stage that happens after the event's arrival in the
    // browser.
    auto stage_it =
        std::find_if(stage_history_.begin(), stage_history_.end(),
                     [&event_metrics](const StageData& stage) {
                       return stage.start_time > event_metrics.time_stamp();
                     });
    // TODO(crbug.com/1079116): Ideally, at least the start time of
    // SubmitCompositorFrameToPresentationCompositorFrame stage should be
    // greater than the event time stamp, but apparently, this is not always the
    // case (see crbug.com/1093698). For now, skip to the next event in such
    // cases. Hopefully, the work to reduce discrepancies between the new
    // EventLatency and the old Event.Latency metrics would fix this issue. If
    // not, we need to reconsider investigating this issue.
    if (stage_it == stage_history_.end())
      continue;

    TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
        "cc,input", "BrowserToRendererCompositor", trace_id,
        event_metrics.time_stamp());
    TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
        "cc,input", "BrowserToRendererCompositor", trace_id,
        stage_it->start_time);

    for (; stage_it != stage_history_.end(); ++stage_it) {
      const int stage_type_index = static_cast<int>(stage_it->stage_type);
      CHECK_LT(stage_type_index, static_cast<int>(StageType::kStageTypeCount));
      CHECK_GE(stage_type_index, 0);
      if (stage_it->start_time >= frame_termination_time_)
        break;
      DCHECK_GE(stage_it->end_time, stage_it->start_time);
      if (stage_it->start_time == stage_it->end_time)
        continue;
      const char* stage_name = GetStageName(stage_type_index);

      TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
          "cc,input", stage_name, trace_id, stage_it->start_time);

      if (stage_it->stage_type ==
          StageType::kSubmitCompositorFrameToPresentationCompositorFrame) {
        REPORT_VIZ_BREAKDOWN_TRACES([&](base::TimeTicks start_time,
                                        base::TimeTicks end_time,
                                        const char* substage_name) {
          TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
              "cc,input", substage_name, trace_id, start_time);
          TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
              "cc,input", substage_name, trace_id, end_time);
        });
      }

      TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
          "cc,input", stage_name, trace_id, stage_it->end_time);
    }
    TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
        "cc,input", "EventLatency", trace_id, frame_termination_time_);
  }
}

void CompositorFrameReporter::PopulateBlinkBreakdownList() {
  if (blink_start_time_.is_null())
    return;

  blink_breakdown_list_[static_cast<int>(BlinkBreakdown::kHandleInputEvents)] =
      blink_breakdown_.handle_input_events;
  blink_breakdown_list_[static_cast<int>(BlinkBreakdown::kAnimate)] =
      blink_breakdown_.animate;
  blink_breakdown_list_[static_cast<int>(BlinkBreakdown::kStyleUpdate)] =
      blink_breakdown_.style_update;
  blink_breakdown_list_[static_cast<int>(BlinkBreakdown::kLayoutUpdate)] =
      blink_breakdown_.layout_update;
  blink_breakdown_list_[static_cast<int>(BlinkBreakdown::kPrepaint)] =
      blink_breakdown_.prepaint;
  blink_breakdown_list_[static_cast<int>(BlinkBreakdown::kCompositingInputs)] =
      blink_breakdown_.compositing_inputs;
  blink_breakdown_list_[static_cast<int>(
      BlinkBreakdown::kCompositingAssignments)] =
      blink_breakdown_.compositing_assignments;
  blink_breakdown_list_[static_cast<int>(BlinkBreakdown::kPaint)] =
      blink_breakdown_.paint;
  blink_breakdown_list_[static_cast<int>(
      BlinkBreakdown::kScrollingCoordinator)] =
      blink_breakdown_.scrolling_coordinator;
  blink_breakdown_list_[static_cast<int>(BlinkBreakdown::kCompositeCommit)] =
      blink_breakdown_.composite_commit;
  blink_breakdown_list_[static_cast<int>(BlinkBreakdown::kUpdateLayers)] =
      blink_breakdown_.update_layers;
  blink_breakdown_list_[static_cast<int>(
      BlinkBreakdown::kBeginMainSentToStarted)] =
      begin_main_frame_start_ - blink_start_time_;
}

void CompositorFrameReporter::PopulateVizBreakdownList() {
  if (viz_start_time_.is_null())
    return;

  // Check if viz_breakdown is set. Testing indicates that sometimes the
  // received_compositor_frame_timestamp can be earlier than the given
  // |start_time|. Avoid reporting negative times.
  if (viz_breakdown_.received_compositor_frame_timestamp.is_null() ||
      viz_breakdown_.received_compositor_frame_timestamp < viz_start_time_) {
    return;
  }
  viz_breakdown_list_[static_cast<int>(
      VizBreakdown::kSubmitToReceiveCompositorFrame)] =
      std::make_pair(viz_start_time_,
                     viz_breakdown_.received_compositor_frame_timestamp);

  if (viz_breakdown_.draw_start_timestamp.is_null())
    return;
  viz_breakdown_list_[static_cast<int>(
      VizBreakdown::kReceivedCompositorFrameToStartDraw)] =
      std::make_pair(viz_breakdown_.received_compositor_frame_timestamp,
                     viz_breakdown_.draw_start_timestamp);

  if (viz_breakdown_.swap_timings.is_null())
    return;
  viz_breakdown_list_[static_cast<int>(VizBreakdown::kStartDrawToSwapStart)] =
      std::make_pair(viz_breakdown_.draw_start_timestamp,
                     viz_breakdown_.swap_timings.swap_start);

  viz_breakdown_list_[static_cast<int>(VizBreakdown::kSwapStartToSwapEnd)] =
      std::make_pair(viz_breakdown_.swap_timings.swap_start,
                     viz_breakdown_.swap_timings.swap_end);

  viz_breakdown_list_[static_cast<int>(
      VizBreakdown::kSwapEndToPresentationCompositorFrame)] =
      std::make_pair(viz_breakdown_.swap_timings.swap_end,
                     viz_breakdown_.presentation_feedback.timestamp);

  if (viz_breakdown_.presentation_feedback.ready_timestamp.is_null())
    return;
  viz_breakdown_list_[static_cast<int>(
      VizBreakdown::kSwapStartToBufferAvailable)] =
      std::make_pair(viz_breakdown_.swap_timings.swap_start,
                     viz_breakdown_.presentation_feedback.available_timestamp);
  viz_breakdown_list_[static_cast<int>(
      VizBreakdown::kBufferAvailableToBufferReady)] =
      std::make_pair(viz_breakdown_.presentation_feedback.available_timestamp,
                     viz_breakdown_.presentation_feedback.ready_timestamp);
  viz_breakdown_list_[static_cast<int>(VizBreakdown::kBufferReadyToLatch)] =
      std::make_pair(viz_breakdown_.presentation_feedback.ready_timestamp,
                     viz_breakdown_.presentation_feedback.latch_timestamp);
  viz_breakdown_list_[static_cast<int>(VizBreakdown::kLatchToSwapEnd)] =
      std::make_pair(viz_breakdown_.presentation_feedback.latch_timestamp,
                     viz_breakdown_.swap_timings.swap_end);
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

bool CompositorFrameReporter::IsDroppedFrameAffectingSmoothness() const {
  // If the frame was not shown, then it hurt smoothness only if either of the
  // threads is affecting smoothness (e.g. running an animation, scroll, pinch,
  // etc.).
  if (TestReportType(FrameReportType::kDroppedFrame)) {
    return smooth_thread_ != SmoothThread::kSmoothNone;
  }

  // If the frame was shown, but included only partial updates, then it hurt
  // smoothness only if the main-thread is affecting smoothness (e.g. running an
  // animation, or scroll etc.).
  if (has_partial_update_) {
    return smooth_thread_ == SmoothThread::kSmoothMain ||
           smooth_thread_ == SmoothThread::kSmoothBoth;
  }

  // If the frame was shown, and did not include partial updates, then this
  // frame did not hurt smoothness.
  return false;
}

base::WeakPtr<CompositorFrameReporter> CompositorFrameReporter::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void CompositorFrameReporter::AdoptReporter(
    std::unique_ptr<CompositorFrameReporter> reporter) {
  DCHECK_EQ(cloned_to_.get(), reporter.get());
  own_cloned_to_ = std::move(reporter);
}

}  // namespace cc
