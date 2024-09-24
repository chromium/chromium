// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/metrics/frame_sequence_metrics.h"

#include <memory>
#include <string>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "cc/metrics/frame_sequence_tracker.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace cc {

using SmoothEffectDrivingThread = FrameInfo::SmoothEffectDrivingThread;

bool ShouldReportForAnimation(FrameSequenceTrackerType sequence_type,
                              SmoothEffectDrivingThread thread_type) {
  // kSETMainThreadAnimation and kSETCompositorAnimation sequences are a subset
  // of kMainThreadAnimation and kCompositorAnimation sequences. So these are
  // excluded from the AllAnimation metric to avoid double counting.

  if (sequence_type == FrameSequenceTrackerType::kCompositorAnimation)
    return thread_type == SmoothEffectDrivingThread::kCompositor;

  if (sequence_type == FrameSequenceTrackerType::kMainThreadAnimation ||
      sequence_type == FrameSequenceTrackerType::kJSAnimation)
    return thread_type == SmoothEffectDrivingThread::kMain;

  return false;
}

bool ShouldReportForInteraction(
    FrameSequenceTrackerType sequence_type,
    SmoothEffectDrivingThread reporting_thread_type,
    SmoothEffectDrivingThread metrics_effective_thread_type) {
  // For scrollbar/touch/wheel scroll, the slower thread is the one we want to
  // report. For pinch-zoom, it's the compositor-thread.
  if (sequence_type == FrameSequenceTrackerType::kScrollbarScroll ||
      sequence_type == FrameSequenceTrackerType::kTouchScroll ||
      sequence_type == FrameSequenceTrackerType::kWheelScroll)
    return reporting_thread_type == metrics_effective_thread_type;

  if (sequence_type == FrameSequenceTrackerType::kPinchZoom)
    return reporting_thread_type == SmoothEffectDrivingThread::kCompositor;

  return false;
}

namespace {

constexpr uint32_t kMaxNoUpdateFrameCount = 100;

const char* GetThreadTypeName(SmoothEffectDrivingThread type) {
  switch (type) {
    case SmoothEffectDrivingThread::kCompositor:
      return "CompositorThread";
    case SmoothEffectDrivingThread::kMain:
      return "MainThread";
    default:
      NOTREACHED();
  }
}

// Avoid reporting any throughput metric for sequences that do not have a
// sufficient number of frames.
constexpr int kMinFramesForThroughputMetric = 100;

constexpr int kBuiltinSequenceNum =
    static_cast<int>(FrameSequenceTrackerType::kMaxType) + 1;
constexpr int kMaximumHistogramIndex = 3 * kBuiltinSequenceNum;
constexpr int kMaximumJankV3HistogramIndex = 2 * kBuiltinSequenceNum;

int GetIndexForMetric(SmoothEffectDrivingThread thread_type,
                      FrameSequenceTrackerType type) {
  if (thread_type == SmoothEffectDrivingThread::kMain)
    return static_cast<int>(type);
  if (thread_type == SmoothEffectDrivingThread::kCompositor)
    return static_cast<int>(type) + kBuiltinSequenceNum;
  return static_cast<int>(type) + 2 * kBuiltinSequenceNum;
}

int GetIndexForJankV3Metric(SmoothEffectDrivingThread thread_type,
                            FrameSequenceTrackerType type) {
  if (thread_type == SmoothEffectDrivingThread::kMain) {
    return static_cast<int>(type);
  }
  DCHECK_EQ(thread_type, SmoothEffectDrivingThread::kCompositor);
  return static_cast<int>(type) + kBuiltinSequenceNum;
}

std::string GetCheckerboardingV3HistogramName(FrameSequenceTrackerType type) {
  return base::StrCat(
      {"Graphics.Smoothness.Checkerboarding3.",
       FrameSequenceTracker::GetFrameSequenceTrackerTypeName(type)});
}

std::string GetCheckerboardingV4HistogramName(FrameSequenceTrackerType type) {
  return base::StrCat(
      {"Graphics.Smoothness.Checkerboarding4.",
       FrameSequenceTracker::GetFrameSequenceTrackerTypeName(type)});
}

std::string GetCheckerboardingV3ThreadedHistogramName(
    FrameSequenceTrackerType type,
    const char* thread_name) {
  return base::StrCat(
      {"Graphics.Smoothness.Checkerboarding3.", thread_name, ".",
       FrameSequenceTracker::GetFrameSequenceTrackerTypeName(type)});
}

std::string GetJankV3HistogramName(FrameSequenceTrackerType type,
                                   const char* thread_name) {
  return base::StrCat(
      {"Graphics.Smoothness.Jank3.", thread_name, ".",
       FrameSequenceTracker::GetFrameSequenceTrackerTypeName(type)});
}

std::string GetThroughputV3HistogramName(FrameSequenceTrackerType type,
                                         const char* thread_name) {
  return base::StrCat(
      {"Graphics.Smoothness.PercentDroppedFrames3.", thread_name, ".",
       FrameSequenceTracker::GetFrameSequenceTrackerTypeName(type)});
}

}  // namespace

FrameSequenceMetrics::V3::V3() = default;
FrameSequenceMetrics::V3::~V3() = default;

FrameSequenceMetrics::CustomReportData::CustomReportData(
    uint32_t frames_expected,
    uint32_t frames_dropped,
    uint32_t jank_count,
    std::vector<Jank> janks)
    : frames_expected_v3(frames_expected),
      frames_dropped_v3(frames_dropped),
      jank_count_v3(jank_count),
      janks(std::move(janks)) {}
FrameSequenceMetrics::CustomReportData::CustomReportData() = default;

FrameSequenceMetrics::CustomReportData::CustomReportData(
    const CustomReportData&) = default;
FrameSequenceMetrics::CustomReportData&
FrameSequenceMetrics::CustomReportData::operator=(const CustomReportData&) =
    default;

FrameSequenceMetrics::CustomReportData::~CustomReportData() = default;

FrameSequenceMetrics::FrameSequenceMetrics(FrameSequenceTrackerType type)
    : type_(type) {}

FrameSequenceMetrics::~FrameSequenceMetrics() {
  // If we did not see sufficient frames to report we will be kept around to
  // have the data `Merge` into the next sequence of `type_`. We do not
  // terminate active traces immediately when we stop, as the sequence may
  // restart, leading to `AdoptTrace`.
  //
  // However we may not be merged before teardown, if so terminate the trace
  // now.
  if (trace_data_.trace_id) {
    trace_data_.Terminate(v3_, v4_, GetEffectiveThread());
  }
}

void FrameSequenceMetrics::ReportLeftoverData() {
  if (HasDataLeftForReporting() || type_ == FrameSequenceTrackerType::kCustom)
    ReportMetrics();
}

void FrameSequenceMetrics::SetScrollingThread(
    SmoothEffectDrivingThread scrolling_thread) {
  DCHECK(type_ == FrameSequenceTrackerType::kTouchScroll ||
         type_ == FrameSequenceTrackerType::kWheelScroll ||
         type_ == FrameSequenceTrackerType::kScrollbarScroll);
  DCHECK_EQ(scrolling_thread_, SmoothEffectDrivingThread::kUnknown);
  scrolling_thread_ = scrolling_thread;
}

void FrameSequenceMetrics::SetCustomReporter(CustomReporter custom_reporter) {
  DCHECK_EQ(FrameSequenceTrackerType::kCustom, type_);
  custom_reporter_ = std::move(custom_reporter);
}

SmoothEffectDrivingThread FrameSequenceMetrics::GetEffectiveThread() const {
  switch (type_) {
    case FrameSequenceTrackerType::kCompositorAnimation:
    case FrameSequenceTrackerType::kSETCompositorAnimation:
    case FrameSequenceTrackerType::kPinchZoom:
    case FrameSequenceTrackerType::kVideo:
      return SmoothEffectDrivingThread::kCompositor;

    case FrameSequenceTrackerType::kMainThreadAnimation:
    case FrameSequenceTrackerType::kSETMainThreadAnimation:
    case FrameSequenceTrackerType::kRAF:
    case FrameSequenceTrackerType::kCanvasAnimation:
    case FrameSequenceTrackerType::kJSAnimation:
      return SmoothEffectDrivingThread::kMain;

    case FrameSequenceTrackerType::kTouchScroll:
    case FrameSequenceTrackerType::kScrollbarScroll:
    case FrameSequenceTrackerType::kWheelScroll:
      return scrolling_thread_;

    case FrameSequenceTrackerType::kCustom:
      return SmoothEffectDrivingThread::kMain;

    case FrameSequenceTrackerType::kMaxType:
      NOTREACHED();
  }
  return SmoothEffectDrivingThread::kUnknown;
}

void FrameSequenceMetrics::Merge(
    std::unique_ptr<FrameSequenceMetrics> metrics) {
  // Merging custom trackers are not supported.
  DCHECK_NE(type_, FrameSequenceTrackerType::kCustom);
  DCHECK_EQ(type_, metrics->type_);
  DCHECK_EQ(GetEffectiveThread(), metrics->GetEffectiveThread());

  v3_.frames_expected += metrics->v3_.frames_expected;
  v3_.frames_dropped += metrics->v3_.frames_dropped;
  v3_.frames_missing_content += metrics->v3_.frames_missing_content;
  v3_.jank_count += metrics->v3_.jank_count;
  for (const auto& jank : metrics->v3_.janks) {
    v3_.janks.emplace_back(jank);
  }
  v3_.no_update_count += metrics->v3_.no_update_count;
  if (v3_.last_begin_frame_args.frame_time <
      metrics->v3_.last_begin_frame_args.frame_time) {
    v3_.last_begin_frame_args = metrics->v3_.last_begin_frame_args;
    v3_.last_frame = metrics->v3_.last_frame;
    v3_.last_presented_frame = metrics->v3_.last_presented_frame;
    v3_.last_frame_delta = metrics->v3_.last_frame_delta;
    v3_.no_update_duration = metrics->v3_.no_update_duration;
  }
  v4_.frames_checkerboarded += metrics->v4_.frames_checkerboarded;
  v4_.frames_checkerboarded_need_raster +=
      metrics->v4_.frames_checkerboarded_need_raster;
  v4_.frames_checkerboarded_need_record +=
      metrics->v4_.frames_checkerboarded_need_record;
  DCHECK_EQ(v3_.frames_missing_content, v4_.frames_checkerboarded_need_raster);
}

bool FrameSequenceMetrics::HasEnoughDataForReporting() const {
  return v3_.frames_expected >= kMinFramesForThroughputMetric;
}

bool FrameSequenceMetrics::HasDataLeftForReporting() const {
  return v3_.frames_expected > 0;
}

void FrameSequenceMetrics::AdoptTrace(FrameSequenceMetrics* adopt_from) {
  DCHECK(!trace_data_.trace_id);
  trace_data_.trace_id = adopt_from->trace_data_.trace_id;
  trace_data_.last_presented_sequence_number =
      adopt_from->trace_data_.last_presented_sequence_number;
  trace_data_.last_timestamp = adopt_from->trace_data_.last_timestamp;
  trace_data_.frame_count = adopt_from->trace_data_.frame_count;
  adopt_from->trace_data_.trace_id = 0u;
}

void FrameSequenceMetrics::ReportMetrics() {
  // Terminates |trace_data_| for all types of FrameSequenceTracker.
  trace_data_.Terminate(v3_, v4_, GetEffectiveThread());

  if (type_ == FrameSequenceTrackerType::kCustom) {
    DCHECK(!custom_reporter_.is_null());
    std::move(custom_reporter_)
        .Run(CustomReportData(v3_.frames_expected, v3_.frames_dropped,
                              v3_.jank_count, std::move(v3_.janks)));

    v3_.frames_expected = 0u;
    v3_.frames_dropped = 0u;
    v3_.frames_missing_content = 0u;
    v3_.no_update_count = 0u;
    v3_.jank_count = 0u;
    v3_.janks.clear();
    v4_.frames_checkerboarded = 0u;
    v4_.frames_checkerboarded_need_raster = 0u;
    v4_.frames_checkerboarded_need_record = 0u;
    return;
  }

  const auto thread_type = GetEffectiveThread();
  const bool is_animation = ShouldReportForAnimation(type(), thread_type);
  const bool is_interaction =
      ShouldReportForInteraction(type(), thread_type, thread_type);

  if (v3_.frames_expected >= kMinFramesForThroughputMetric) {
    auto get_percent = [this](uint32_t frames) -> int {
      if (v3_.frames_expected == 0) {
        return 0;
      }
      return std::ceil(100. * frames /
                       static_cast<double>(v3_.frames_expected));
    };

    const int percent_missing_content = get_percent(v3_.frames_missing_content);
    const int percent_dropped = get_percent(v3_.frames_dropped);
    const int percent_jank = get_percent(v3_.jank_count);

    // v4.
    const int percent_checkerboarded = get_percent(v4_.frames_checkerboarded);
    const int percent_checkerboarded_need_raster =
        get_percent(v4_.frames_checkerboarded_need_raster);
    const int percent_checkerboarded_need_record =
        get_percent(v4_.frames_checkerboarded_need_record);

    if (is_animation) {
      UMA_HISTOGRAM_PERCENTAGE(
          "Graphics.Smoothness.Checkerboarding3.AllAnimations",
          percent_missing_content);
      UMA_HISTOGRAM_PERCENTAGE(
          "Graphics.Smoothness.Checkerboarding4.AllAnimations",
          percent_checkerboarded);
      UMA_HISTOGRAM_PERCENTAGE("Graphics.Smoothness.Jank3.AllAnimations",
                               percent_jank);
      UMA_HISTOGRAM_PERCENTAGE(
          "Graphics.Smoothness.PercentDroppedFrames3.AllAnimations",
          percent_dropped);
    }
    if (is_interaction) {
      UMA_HISTOGRAM_PERCENTAGE(
          "Graphics.Smoothness.Checkerboarding3.AllInteractions",
          percent_missing_content);
      UMA_HISTOGRAM_PERCENTAGE(
          "Graphics.Smoothness.Checkerboarding4.AllInteractions",
          percent_checkerboarded);
      UMA_HISTOGRAM_PERCENTAGE("Graphics.Smoothness.Jank3.AllInteractions",
                               percent_jank);
      UMA_HISTOGRAM_PERCENTAGE(
          "Graphics.Smoothness.PercentDroppedFrames3.AllInteractions",
          percent_dropped);
    }
    if (is_animation || is_interaction) {
      UMA_HISTOGRAM_PERCENTAGE(
          "Graphics.Smoothness.Checkerboarding3.AllSequences",
          percent_missing_content);
      UMA_HISTOGRAM_PERCENTAGE(
          "Graphics.Smoothness.Checkerboarding4.AllSequences",
          percent_checkerboarded);
      UMA_HISTOGRAM_PERCENTAGE(
          "Graphics.Smoothness.CheckerboardingNeedRaster4.AllSequences",
          percent_checkerboarded_need_raster);
      UMA_HISTOGRAM_PERCENTAGE(
          "Graphics.Smoothness.CheckerboardingNeedRecord4.AllSequences",
          percent_checkerboarded_need_record);
      UMA_HISTOGRAM_PERCENTAGE("Graphics.Smoothness.Jank3.AllSequences",
                               percent_jank);
      UMA_HISTOGRAM_PERCENTAGE(
          "Graphics.Smoothness.PercentDroppedFrames3.AllSequences",
          percent_dropped);
    }

    const char* thread_name = GetThreadTypeName(thread_type);

    STATIC_HISTOGRAM_POINTER_GROUP(
        GetThroughputV3HistogramName(type(), thread_name),
        GetIndexForMetric(thread_type, type_), kMaximumHistogramIndex,
        Add(percent_dropped),
        base::LinearHistogram::FactoryGet(
            GetThroughputV3HistogramName(type(), thread_name), 1, 100, 101,
            base::HistogramBase::kUmaTargetedHistogramFlag));

    STATIC_HISTOGRAM_POINTER_GROUP(
        GetCheckerboardingV3HistogramName(type_), static_cast<int>(type_),
        static_cast<int>(FrameSequenceTrackerType::kMaxType),
        Add(percent_missing_content),
        base::LinearHistogram::FactoryGet(
            GetCheckerboardingV3HistogramName(type_), 1, 100, 101,
            base::HistogramBase::kUmaTargetedHistogramFlag));
    STATIC_HISTOGRAM_POINTER_GROUP(
        GetCheckerboardingV4HistogramName(type_), static_cast<int>(type_),
        static_cast<int>(FrameSequenceTrackerType::kMaxType),
        Add(percent_checkerboarded),
        base::LinearHistogram::FactoryGet(
            GetCheckerboardingV4HistogramName(type_), 1, 100, 101,
            base::HistogramBase::kUmaTargetedHistogramFlag));

    if (scrolling_thread_ != SmoothEffectDrivingThread::kUnknown) {
      STATIC_HISTOGRAM_POINTER_GROUP(
          GetCheckerboardingV3ThreadedHistogramName(type_, thread_name),
          GetIndexForMetric(thread_type, type_), kMaximumHistogramIndex,
          Add(percent_missing_content),
          base::LinearHistogram::FactoryGet(
              GetCheckerboardingV3ThreadedHistogramName(type_, thread_name), 1,
              100, 101, base::HistogramBase::kUmaTargetedHistogramFlag));
    }

    STATIC_HISTOGRAM_POINTER_GROUP(
        GetJankV3HistogramName(type_, thread_name),
        GetIndexForJankV3Metric(thread_type, type_),
        kMaximumJankV3HistogramIndex, Add(percent_jank),
        base::LinearHistogram::FactoryGet(
            GetJankV3HistogramName(type_, thread_name), 1, 100, 101,
            base::HistogramBase::kUmaTargetedHistogramFlag));
    v3_.frames_expected = 0u;
    v3_.frames_dropped = 0u;
    v3_.frames_missing_content = 0u;
    v3_.no_update_count = 0u;
    v3_.jank_count = 0u;
    v4_.frames_checkerboarded = 0u;
    v4_.frames_checkerboarded_need_raster = 0u;
    v4_.frames_checkerboarded_need_record = 0u;
  }
}

FrameSequenceMetrics::TraceData::TraceData(FrameSequenceMetrics* m)
    : metrics(m) {
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("cc,benchmark", &enabled);
}

FrameSequenceMetrics::TraceData::~TraceData() = default;

void FrameSequenceMetrics::TraceData::Terminate(
    const V3& v3,
    const V4& v4,
    FrameInfo::SmoothEffectDrivingThread effective_thread) {
  if (!enabled || !trace_id) {
    return;
  }
  auto dict = std::make_unique<base::trace_event::TracedValue>();
  dict->BeginDictionary("data");
  dict->SetInteger("expected", v3.frames_expected);
  dict->SetInteger("dropped", v3.frames_dropped);
  dict->SetInteger("missing_content", v3.frames_missing_content);
  // v4.
  dict->SetInteger("checkerboarded", v4.frames_checkerboarded);
  DCHECK_EQ(v3.frames_missing_content, v4.frames_checkerboarded_need_raster);
  dict->SetInteger("checkerboarded_need_raster",
                   v4.frames_checkerboarded_need_raster);
  dict->SetInteger("checkerboarded_need_record",
                   v4.frames_checkerboarded_need_record);
  dict->EndDictionary();
  base::TimeTicks termination_time =
      v3.last_presented_frame.GetTerminationTimeForThread(effective_thread);
  // FrameSequenceTracker termination is based on FrameSorter. This way it can
  // reflect delays in gfx::PresentationFeedback arriving, while also not
  // terminating due to out-of-order dropped frames. The default termination is
  // when the final frame produced for the sequence has been presented.
  //
  // Otherwise we will terminate once a frame, newer than the final one of the
  // sequence has been produced, not dropped, and sorted. This can be a
  // FrameInfo::FrameFinalState::NoUpdateDesired, which has no termination time,
  // due to never being presented. When this occurs after a series of dropped
  // frames, we can potentially have an entire sequence that was dropped.
  // Leading to `v3.last_presented_frame` having no valid timestamp.
  //
  // Here we check for alternative timestamps, so as not to provide a null
  // timestamp to the trace.
  if (termination_time.is_null()) {
    termination_time =
        v3.last_frame.GetTerminationTimeForThread(effective_thread);
    if (termination_time.is_null()) {
      termination_time = base::TimeTicks::Now();
    }
  }
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP1(
      "cc,benchmark", "FrameSequenceTrackerV3", TRACE_ID_LOCAL(trace_id),
      termination_time, "args", std::move(dict));
  trace_id = 0u;
}

void FrameSequenceMetrics::TraceData::Advance(base::TimeTicks start_timestamp,
                                              base::TimeTicks new_timestamp,
                                              uint32_t expected,
                                              uint32_t dropped,
                                              uint64_t sequence_number,
                                              const char* histogram_name) {
  if (!enabled)
    return;
  if (!trace_id) {
    // The underlying usage of TRACE_ID_LOCAL is mapping the raw uint64_t from
    // the point into either `trace_event_internal::TraceID::LocalId` or
    // `perfetto::internal::LegacyTraceId`. However the trace macros don't
    // support just providing that object directly. Here we do the cast
    // ourselves ahead, and save the resulting value. This value will be used to
    // nest other traces, as well as close the async trace at a later time. The
    // value can also be merged into future sequences. This avoids holding
    // dangling ptrs.
    trace_id = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(this));
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP1(
        "cc,benchmark", histogram_name, TRACE_ID_LOCAL(trace_id),
        start_timestamp, "name",
        FrameSequenceTracker::GetFrameSequenceTrackerTypeName(metrics->type()));
  }

  auto dict = std::make_unique<base::trace_event::TracedValue>();
  dict->BeginDictionary("values");
  dict->SetInteger("sequence_number", sequence_number);
  dict->SetInteger("last_sequence", last_presented_sequence_number);
  dict->SetInteger("expected", expected);
  dict->SetInteger("dropped", dropped);
  dict->EndDictionary();

  // Use different names, because otherwise the trace-viewer shows the slices in
  // the same color, and that makes it difficult to tell the traces apart from
  // each other.
  const char* trace_names[] = {"Frame", "Frame ", "Frame   "};
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      "cc,benchmark", trace_names[++this->frame_count % 3],
      TRACE_ID_LOCAL(trace_id), start_timestamp);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP1(
      "cc,benchmark", trace_names[this->frame_count % 3],
      TRACE_ID_LOCAL(trace_id), new_timestamp, "data", std::move(dict));
  this->last_presented_sequence_number = sequence_number;
  this->last_timestamp = new_timestamp;
}

void FrameSequenceMetrics::AddSortedFrame(const viz::BeginFrameArgs& args,
                                          const FrameInfo& frame_info) {
  const auto effective_thread = GetEffectiveThread();
  const auto last_presented_termination_time =
      v3_.last_presented_frame.GetTerminationTimeForThread(effective_thread);
  const auto termination_time =
      frame_info.GetTerminationTimeForThread(effective_thread);
  bool should_calculate_jank_and_checkerboarding = false;
  switch (effective_thread) {
    case SmoothEffectDrivingThread::kCompositor:
      if (frame_info.WasSmoothCompositorUpdateDropped()) {
        ++v3_.frames_dropped;
      }
      ++v3_.frames_expected;
      should_calculate_jank_and_checkerboarding = true;
      break;
    case SmoothEffectDrivingThread::kMain:
      if (frame_info.WasSmoothMainUpdateExpected()) {
        if (frame_info.WasSmoothMainUpdateDropped()) {
          ++v3_.frames_dropped;
        }
        ++v3_.frames_expected;
        should_calculate_jank_and_checkerboarding = true;
      } else {
        IncrementJankIdleTimeV3(last_presented_termination_time,
                                termination_time);
      }
      break;
    case SmoothEffectDrivingThread::kUnknown:
      NOTREACHED();
  }
  if (should_calculate_jank_and_checkerboarding) {
    auto final_state = frame_info.GetFinalStateForThread(effective_thread);
    CalculateJankV3(args, frame_info, final_state,
                    last_presented_termination_time, termination_time);
    CalculateCheckerboarding(frame_info, final_state);
  }
  v3_.last_begin_frame_args = args;
  v3_.last_frame = frame_info;
}

void FrameSequenceMetrics::CalculateJankV3(
    const viz::BeginFrameArgs& args,
    const FrameInfo& frame_info,
    FrameInfo::FrameFinalState final_state,
    base::TimeTicks last_presented_termination_time,
    base::TimeTicks termination_time) {
  switch (final_state) {
    case FrameInfo::FrameFinalState::kNoUpdateDesired:
      IncrementJankIdleTimeV3(last_presented_termination_time,
                              termination_time);
      break;
    case FrameInfo::FrameFinalState::kDropped:
      break;
    case FrameInfo::FrameFinalState::kPresentedAll:
    case FrameInfo::FrameFinalState::kPresentedPartialOldMain:
    case FrameInfo::FrameFinalState::kPresentedPartialNewMain:
      // The first frame of a sequence will have no previous timestamp. We don't
      // calculate it for jank. However we start the tracing from when the
      // sequence was started.
      bool will_ignore_current_frame =
          v3_.no_update_count >= kMaxNoUpdateFrameCount;
      if (last_presented_termination_time.is_null()) {
        last_presented_termination_time = trace_data_.last_timestamp;
        will_ignore_current_frame = true;
      }

      // TODO(crbug.com/40270377): A new FrameSequenceTracker, that has yet to
      // process its first frame uses its creation time as starting point of
      // nested traces. FrameSorter processes a FrameInfo when both threads are
      // complete. It's possible for the smoothness thread component to have
      // completed before this tracker started. We do not include them in the
      // traces.
      if (!last_presented_termination_time.is_null() &&
          termination_time > last_presented_termination_time) {
        trace_data_.Advance(last_presented_termination_time, termination_time,
                            v3_.frames_expected, v3_.frames_dropped,
                            frame_info.sequence_number,
                            "FrameSequenceTrackerV3");
      }

      const base::TimeDelta zero_delta = base::Milliseconds(0);
      base::TimeDelta current_frame_delta =
          will_ignore_current_frame
              ? zero_delta
              : termination_time - last_presented_termination_time -
                    v3_.no_update_duration;
      // Guard against the situation when the physical presentation interval is
      // shorter than |no_update_duration|. For example, consider two
      // BeginFrames A and B separated by 5 vsync cycles of no-updates (i.e.
      // |no_update_duration| = 5 vsync cycles); the Presentation of A occurs 2
      // vsync cycles after BeginFrame A, whereas Presentation B occurs in the
      // same vsync cycle as BeginFrame B. In this situation, the physical
      // presentation interval is shorter than 5 vsync cycles and will result
      // in a negative |current_frame_delta|.
      if (current_frame_delta < zero_delta) {
        current_frame_delta = zero_delta;
      }

      // The presentation interval is typically a multiple of VSync intervals
      // (i.e. 16.67ms, 33.33ms, 50ms ... on a 60Hz display) with small
      // fluctuations. The 0.5 * |frame_interval| criterion is chosen so that
      // the jank detection is robust to those fluctuations.
      if (!v3_.last_frame_delta.is_zero() &&
          current_frame_delta > v3_.last_frame_delta + 0.5 * args.interval) {
        ++v3_.jank_count;
        if (type_ == FrameSequenceTrackerType::kCustom) {
          // Record `last_presented_termination_time` and `current_frame_delta`
          // as the timestamp and duration of the current jank.
          v3_.janks.push_back(
              Jank(last_presented_termination_time, current_frame_delta));
        }
        TraceJankV3(frame_info.sequence_number, last_presented_termination_time,
                    termination_time);
      }

      v3_.last_frame_delta = current_frame_delta;
      v3_.no_update_duration = base::TimeDelta();
      v3_.no_update_count = 0;
      // It is possible for `frame_info` to have been terminated without
      // presentation before `last_presented_frame` was presented. We do not
      // update `last_presented_frame` in these cases so that the nested frames
      // in the trace are all aligned on presentations.
      if (v3_.last_presented_frame.GetTerminationTimeForThread(
              GetEffectiveThread()) >
          frame_info.GetTerminationTimeForThread(GetEffectiveThread())) {
      } else {
        v3_.last_presented_frame = frame_info;
      }
      break;
  }
}

void FrameSequenceMetrics::CalculateCheckerboarding(
    const FrameInfo& frame_info,
    FrameInfo::FrameFinalState final_state) {
  const FrameInfo& used_frame_info =
      final_state == FrameInfo::FrameFinalState::kDropped
          ? v3_.last_presented_frame
          : frame_info;
  if (used_frame_info.checkerboarded_needs_raster) {
    ++v3_.frames_missing_content;
    ++v4_.frames_checkerboarded_need_raster;
  }
  if (used_frame_info.checkerboarded_needs_record) {
    ++v4_.frames_checkerboarded_need_record;
  }
  if (used_frame_info.checkerboarded_needs_raster ||
      used_frame_info.checkerboarded_needs_record) {
    ++v4_.frames_checkerboarded;
  }
}

void FrameSequenceMetrics::IncrementJankIdleTimeV3(
    base::TimeTicks last_presented_termination_time,
    base::TimeTicks termination_time) {
  // If `frame_info.sequence_number` of N takes a long time to present, it can
  // present after N-1 was either Dropped or NoUpdateDesired. We don't offset
  // jank calculation for these frames.
  if (last_presented_termination_time.is_null() ||
      termination_time < last_presented_termination_time) {
    return;
  }

  v3_.no_update_duration +=
      termination_time -
      v3_.last_frame.GetTerminationTimeForThread(GetEffectiveThread());
  ++v3_.no_update_count;
}

void FrameSequenceMetrics::TraceJankV3(uint64_t sequence_number,
                                       base::TimeTicks last_termination_time,
                                       base::TimeTicks termination_time) {
  if (!trace_data_.enabled) {
    return;
  }
  auto dict = std::make_unique<base::trace_event::TracedValue>();
  dict->BeginDictionary("data");
  dict->SetInteger("frame_sequence_number", sequence_number);
  dict->SetInteger("last_presented_frame_sequence_number",
                   v3_.last_presented_frame.sequence_number);
  dict->SetString("thread-type", GetThreadTypeName(GetEffectiveThread()));
  dict->SetString("tracker-type",
                  FrameSequenceTracker::GetFrameSequenceTrackerTypeName(type_));
  dict->EndDictionary();
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP1(
      "cc,benchmark", "JankV3", TRACE_ID_LOCAL(this), last_termination_time,
      "data", std::move(dict));
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "cc,benchmark", "JankV3", TRACE_ID_LOCAL(this), termination_time);
}

}  // namespace cc
