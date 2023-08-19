// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/frame_sequence_metrics.h"

#include <memory>
#include <string>
#include <utility>

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "cc/metrics/frame_sequence_tracker.h"
#include "cc/metrics/jank_metrics.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"

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

const char* GetJankThreadTypeName(FrameInfo::SmoothEffectDrivingThread type) {
  switch (type) {
    case FrameInfo::SmoothEffectDrivingThread::kCompositor:
      return "Compositor";
    case FrameInfo::SmoothEffectDrivingThread::kMain:
      return "Main";
    default:
      NOTREACHED();
      return "";
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

std::string GetCheckerboardingHistogramName(FrameSequenceTrackerType type) {
  return base::StrCat(
      {"Graphics.Smoothness.Checkerboarding.",
       FrameSequenceTracker::GetFrameSequenceTrackerTypeName(type)});
}

std::string GetCheckerboardingV3HistogramName(FrameSequenceTrackerType type) {
  return base::StrCat(
      {"Graphics.Smoothness.Checkerboarding3.",
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

FrameSequenceMetrics::FrameSequenceMetrics(FrameSequenceTrackerType type)
    : type_(type) {
  SmoothEffectDrivingThread thread_type = GetEffectiveThread();

  // Only construct |jank_reporter_| if it has a valid tracker and thread type.
  // For scrolling tracker types, |jank_reporter_| may be constructed later in
  // SetScrollingThread().
  if (thread_type == SmoothEffectDrivingThread::kCompositor ||
      thread_type == SmoothEffectDrivingThread::kMain) {
    jank_reporter_ = std::make_unique<JankMetrics>(type, thread_type);
  }
}

FrameSequenceMetrics::~FrameSequenceMetrics() = default;

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

  DCHECK(!jank_reporter_);
  DCHECK_NE(scrolling_thread, SmoothEffectDrivingThread::kUnknown);
  jank_reporter_ = std::make_unique<JankMetrics>(type_, scrolling_thread);
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
  impl_throughput_.Merge(metrics->impl_throughput_);
  main_throughput_.Merge(metrics->main_throughput_);
  frames_checkerboarded_ += metrics->frames_checkerboarded_;

  v3_.frames_expected += metrics->v3_.frames_expected;
  v3_.frames_dropped += metrics->v3_.frames_dropped;
  v3_.frames_missing_content += metrics->v3_.frames_missing_content;
  v3_.jank_count += metrics->v3_.jank_count;
  v3_.no_update_count += metrics->v3_.no_update_count;
  if (v3_.last_begin_frame_args.frame_time <
      metrics->v3_.last_begin_frame_args.frame_time) {
    v3_.last_begin_frame_args = metrics->v3_.last_begin_frame_args;
    v3_.last_frame = metrics->v3_.last_frame;
    v3_.last_presented_frame = metrics->v3_.last_presented_frame;
    v3_.last_frame_delta = metrics->v3_.last_frame_delta;
    v3_.no_update_duration = metrics->v3_.no_update_duration;
  }

  if (jank_reporter_)
    jank_reporter_->Merge(std::move(metrics->jank_reporter_));

  // Reset the state of |metrics| before destroying it, so that it doesn't end
  // up reporting the metrics.
  metrics->impl_throughput_ = {};
  metrics->main_throughput_ = {};
  metrics->frames_checkerboarded_ = 0;
}

bool FrameSequenceMetrics::HasEnoughDataForReporting() const {
  return impl_throughput_.frames_expected >= kMinFramesForThroughputMetric ||
         main_throughput_.frames_expected >= kMinFramesForThroughputMetric ||
         v3_.frames_expected >= kMinFramesForThroughputMetric;
}

bool FrameSequenceMetrics::HasDataLeftForReporting() const {
  return impl_throughput_.frames_expected > 0 ||
         main_throughput_.frames_expected > 0 || v3_.frames_expected > 0;
}

void FrameSequenceMetrics::AdoptTrace(FrameSequenceMetrics* adopt_from) {
  DCHECK(!trace_data_.trace_id);
  trace_data_.trace_id = adopt_from->trace_data_.trace_id;
  trace_data_v3_.trace_id = adopt_from->trace_data_v3_.trace_id;
  adopt_from->trace_data_.trace_id = nullptr;
  adopt_from->trace_data_v3_.trace_id = nullptr;
}

void FrameSequenceMetrics::AdvanceTrace(base::TimeTicks timestamp,
                                        uint64_t sequence_number) {
  uint32_t expected = 0, dropped = 0;
  switch (GetEffectiveThread()) {
    case SmoothEffectDrivingThread::kCompositor:
      expected = impl_throughput_.frames_expected;
      dropped =
          impl_throughput_.frames_expected - impl_throughput_.frames_produced;
      break;

    case SmoothEffectDrivingThread::kMain:
      expected = main_throughput_.frames_expected;
      dropped =
          main_throughput_.frames_expected - main_throughput_.frames_produced;
      break;

    case SmoothEffectDrivingThread::kUnknown:
      NOTREACHED();
  }
  trace_data_.Advance(trace_data_.last_timestamp, timestamp, expected, dropped,
                      sequence_number, "FrameSequenceTracker");
}

void FrameSequenceMetrics::ReportMetrics() {
  DCHECK_LE(impl_throughput_.frames_produced, impl_throughput_.frames_expected);
  DCHECK_LE(main_throughput_.frames_produced, main_throughput_.frames_expected);

  // Terminates |trace_data_| for all types of FrameSequenceTracker.
  trace_data_.Terminate();
  trace_data_v3_.TerminateV3(v3_);

  if (type_ == FrameSequenceTrackerType::kCustom) {
    DCHECK(!custom_reporter_.is_null());
    std::move(custom_reporter_)
        .Run({
            main_throughput_.frames_expected,
            main_throughput_.frames_produced,
            jank_reporter_->jank_count(),
        });

    main_throughput_ = {};
    impl_throughput_ = {};
    jank_reporter_->Reset();
    frames_checkerboarded_ = 0;
    return;
  }

  const auto thread_type = GetEffectiveThread();
  const bool is_animation = ShouldReportForAnimation(type(), thread_type);
  const bool is_interaction =
      ShouldReportForInteraction(type(), thread_type, thread_type);

  if (v3_.frames_expected >= kMinFramesForThroughputMetric) {
    const int percent_missing_content =
        std::ceil(100. * v3_.frames_missing_content /
                  static_cast<double>(v3_.frames_expected));
    const int percent =
        v3_.frames_expected == 0
            ? 0
            : std::ceil(100. * v3_.frames_dropped /
                        static_cast<double>(v3_.frames_expected));
    const int percent_jank = std::ceil(
        100. * v3_.jank_count / static_cast<double>(v3_.frames_expected));

    if (is_animation) {
      UMA_HISTOGRAM_PERCENTAGE(
          "Graphics.Smoothness.Checkerboarding3.AllAnimations",
          percent_missing_content);
      UMA_HISTOGRAM_PERCENTAGE("Graphics.Smoothness.Jank3.AllAnimations",
                               percent_jank);
      UMA_HISTOGRAM_PERCENTAGE(
          "Graphics.Smoothness.PercentDroppedFrames3.AllAnimations", percent);
    }
    if (is_interaction) {
      UMA_HISTOGRAM_PERCENTAGE(
          "Graphics.Smoothness.Checkerboarding3.AllInteractions",
          percent_missing_content);
      UMA_HISTOGRAM_PERCENTAGE("Graphics.Smoothness.Jank3.AllInteractions",
                               percent_jank);
      UMA_HISTOGRAM_PERCENTAGE(
          "Graphics.Smoothness.PercentDroppedFrames3.AllInteractions", percent);
    }
    if (is_animation || is_interaction) {
      UMA_HISTOGRAM_PERCENTAGE(
          "Graphics.Smoothness.Checkerboarding3.AllSequences",
          percent_missing_content);
      UMA_HISTOGRAM_PERCENTAGE("Graphics.Smoothness.Jank3.AllSequences",
                               percent_jank);
      UMA_HISTOGRAM_PERCENTAGE(
          "Graphics.Smoothness.PercentDroppedFrames3.AllSequences", percent);
    }

    const char* thread_name =
        thread_type == SmoothEffectDrivingThread::kCompositor
            ? "CompositorThread"
            : "MainThread";

    STATIC_HISTOGRAM_POINTER_GROUP(
        GetThroughputV3HistogramName(type(), thread_name),
        GetIndexForMetric(thread_type, type_), kMaximumHistogramIndex,
        Add(percent),
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

    const char* jank_thread_name = GetJankThreadTypeName(GetEffectiveThread());
    STATIC_HISTOGRAM_POINTER_GROUP(
        GetJankV3HistogramName(type_, jank_thread_name),
        GetIndexForJankV3Metric(GetEffectiveThread(), type_),
        kMaximumJankV3HistogramIndex, Add(percent_jank),
        base::LinearHistogram::FactoryGet(
            GetJankV3HistogramName(type_, jank_thread_name), 1, 100, 101,
            base::HistogramBase::kUmaTargetedHistogramFlag));
    v3_.frames_expected = 0u;
    v3_.frames_dropped = 0u;
    v3_.frames_missing_content = 0u;
    v3_.no_update_count = 0u;
    v3_.jank_count = 0u;
  }

  // Report the checkerboarding metrics.
  if (impl_throughput_.frames_expected >= kMinFramesForThroughputMetric) {
    const int checkerboarding_percent = static_cast<int>(
        100 * frames_checkerboarded_ / impl_throughput_.frames_expected);
    STATIC_HISTOGRAM_POINTER_GROUP(
        GetCheckerboardingHistogramName(type_), static_cast<int>(type_),
        static_cast<int>(FrameSequenceTrackerType::kMaxType),
        Add(checkerboarding_percent),
        base::LinearHistogram::FactoryGet(
            GetCheckerboardingHistogramName(type_), 1, 100, 101,
            base::HistogramBase::kUmaTargetedHistogramFlag));
    ThroughputData::ReportCheckerboardingHistogram(
        this, SmoothEffectDrivingThread::kCompositor, checkerboarding_percent);
    frames_checkerboarded_ = 0;
  }

  // Report the jank metrics
  if (jank_reporter_) {
    if (jank_reporter_->thread_type() ==
            SmoothEffectDrivingThread::kCompositor &&
        impl_throughput_.frames_expected >= kMinFramesForThroughputMetric) {
      DCHECK_EQ(jank_reporter_->thread_type(), this->GetEffectiveThread());
      jank_reporter_->ReportJankMetrics(impl_throughput_.frames_expected);
    } else if (jank_reporter_->thread_type() ==
                   SmoothEffectDrivingThread::kMain &&
               main_throughput_.frames_expected >=
                   kMinFramesForThroughputMetric) {
      DCHECK_EQ(jank_reporter_->thread_type(), this->GetEffectiveThread());
      jank_reporter_->ReportJankMetrics(main_throughput_.frames_expected);
    }
  }

  // Reset the metrics that reach reporting threshold.
  if (impl_throughput_.frames_expected >= kMinFramesForThroughputMetric)
    impl_throughput_ = {};
  if (main_throughput_.frames_expected >= kMinFramesForThroughputMetric)
    main_throughput_ = {};
}

void FrameSequenceMetrics::ComputeJank(SmoothEffectDrivingThread thread_type,
                                       uint32_t frame_token,
                                       base::TimeTicks presentation_time,
                                       base::TimeDelta frame_interval) {
  if (!jank_reporter_)
    return;

  if (thread_type == jank_reporter_->thread_type())
    jank_reporter_->AddPresentedFrame(frame_token, presentation_time,
                                      frame_interval);
}

void FrameSequenceMetrics::NotifySubmitForJankReporter(
    SmoothEffectDrivingThread thread_type,
    uint32_t frame_token,
    uint32_t sequence_number) {
  if (!jank_reporter_)
    return;

  if (thread_type == jank_reporter_->thread_type())
    jank_reporter_->AddSubmitFrame(frame_token, sequence_number);
}

void FrameSequenceMetrics::NotifyNoUpdateForJankReporter(
    SmoothEffectDrivingThread thread_type,
    uint32_t sequence_number,
    base::TimeDelta frame_interval) {
  if (!jank_reporter_)
    return;

  if (thread_type == jank_reporter_->thread_type())
    jank_reporter_->AddFrameWithNoUpdate(sequence_number, frame_interval);
}

void FrameSequenceMetrics::ThroughputData::ReportCheckerboardingHistogram(
    FrameSequenceMetrics* metrics,
    SmoothEffectDrivingThread thread_type,
    int percent) {
  const auto sequence_type = metrics->type();
  DCHECK_LT(sequence_type, FrameSequenceTrackerType::kMaxType);

  const bool is_animation =
      ShouldReportForAnimation(sequence_type, thread_type);
  const bool is_interaction = ShouldReportForInteraction(
      metrics->type(), thread_type, metrics->GetEffectiveThread());

  if (is_animation) {
    UMA_HISTOGRAM_PERCENTAGE(
        "Graphics.Smoothness.Checkerboarding.AllAnimations", percent);
  }

  if (is_interaction) {
    UMA_HISTOGRAM_PERCENTAGE(
        "Graphics.Smoothness.Checkerboarding.AllInteractions", percent);
  }

  if (is_animation || is_interaction) {
    UMA_HISTOGRAM_PERCENTAGE("Graphics.Smoothness.Checkerboarding.AllSequences",
                             percent);
  }
}

std::unique_ptr<base::trace_event::TracedValue>
FrameSequenceMetrics::ThroughputData::ToTracedValue(
    const ThroughputData& impl,
    const ThroughputData& main,
    SmoothEffectDrivingThread effective_thread) {
  auto dict = std::make_unique<base::trace_event::TracedValue>();
  if (effective_thread == SmoothEffectDrivingThread::kMain) {
    dict->SetInteger("main-frames-produced", main.frames_produced);
    dict->SetInteger("main-frames-expected", main.frames_expected);
  } else {
    dict->SetInteger("impl-frames-produced", impl.frames_produced);
    dict->SetInteger("impl-frames-expected", impl.frames_expected);
  }
  return dict;
}

FrameSequenceMetrics::TraceData::TraceData(FrameSequenceMetrics* m)
    : metrics(m) {
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("cc,benchmark", &enabled);
}

FrameSequenceMetrics::TraceData::~TraceData() = default;

void FrameSequenceMetrics::TraceData::Terminate() {
  if (!enabled || !trace_id)
    return;
  TRACE_EVENT_NESTABLE_ASYNC_END2(
      "cc,benchmark", "FrameSequenceTracker", TRACE_ID_LOCAL(trace_id), "args",
      ThroughputData::ToTracedValue(metrics->impl_throughput(),
                                    metrics->main_throughput(),
                                    metrics->GetEffectiveThread()),
      "checkerboard", metrics->frames_checkerboarded());
  trace_id = nullptr;
}

void FrameSequenceMetrics::TraceData::TerminateV3(const V3& v3) {
  if (!enabled || !trace_id) {
    return;
  }
  auto dict = std::make_unique<base::trace_event::TracedValue>();
  dict->BeginDictionary("data");
  dict->SetInteger("expected", v3.frames_expected);
  dict->SetInteger("dropped", v3.frames_dropped);
  dict->SetInteger("missing_content", v3.frames_missing_content);
  dict->EndDictionary();
  TRACE_EVENT_NESTABLE_ASYNC_END1("cc,benchmark", "FrameSequenceTrackerV3",
                                  TRACE_ID_LOCAL(trace_id), "args",
                                  std::move(dict));
  trace_id = nullptr;
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
    trace_id = this;
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
  dict->SetInteger("dopped", dropped);
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
  switch (effective_thread) {
    case SmoothEffectDrivingThread::kCompositor:
      if (frame_info.WasSmoothCompositorUpdateDropped()) {
        ++v3_.frames_dropped;
      }
      ++v3_.frames_expected;
      CalculateCheckerboardingAndJankV3(
          args, frame_info, frame_info.GetFinalStateForThread(effective_thread),
          last_presented_termination_time, termination_time);
      break;
    case SmoothEffectDrivingThread::kMain:
      if (frame_info.WasSmoothMainUpdateExpected()) {
        if (frame_info.WasSmoothMainUpdateDropped()) {
          ++v3_.frames_dropped;
        }
        ++v3_.frames_expected;
        CalculateCheckerboardingAndJankV3(
            args, frame_info,
            frame_info.GetFinalStateForThread(effective_thread),
            last_presented_termination_time, termination_time);
      } else {
        IncrementJankIdleTimeV3(last_presented_termination_time,
                                termination_time);
      }
      break;
    case SmoothEffectDrivingThread::kUnknown:
      NOTREACHED();
      break;
  }
  v3_.last_begin_frame_args = args;
  v3_.last_frame = frame_info;
}

void FrameSequenceMetrics::CalculateCheckerboardingAndJankV3(
    const viz::BeginFrameArgs& args,
    const FrameInfo& frame_info,
    FrameInfo::FrameFinalState final_state,
    base::TimeTicks last_presented_termination_time,
    base::TimeTicks termination_time) {
  switch (final_state) {
    case FrameInfo::FrameFinalState::kNoUpdateDesired:
      IncrementJankIdleTimeV3(last_presented_termination_time,
                              termination_time);
      ABSL_FALLTHROUGH_INTENDED;
    case FrameInfo::FrameFinalState::kDropped:
      if (v3_.last_presented_frame.has_missing_content) {
        ++v3_.frames_missing_content;
      }
      break;
    case FrameInfo::FrameFinalState::kPresentedAll:
    case FrameInfo::FrameFinalState::kPresentedPartialOldMain:
    case FrameInfo::FrameFinalState::kPresentedPartialNewMain:
      if (frame_info.has_missing_content) {
        ++v3_.frames_missing_content;
      }

      // The first frame of a sequence will have no previous timestamp. We don't
      // calculate it for jank. However we start the tracing from when the
      // sequence was started.
      bool will_ignore_current_frame =
          v3_.no_update_count >= kMaxNoUpdateFrameCount;
      if (last_presented_termination_time.is_null()) {
        last_presented_termination_time = trace_data_v3_.last_timestamp;
        will_ignore_current_frame = true;
      }

      // TODO(crbug.com/1450940): A new FrameSequenceTracker, that has yet to
      // process its first frame uses its creation time as starting point of
      // nested traces. FrameSorter processes a FrameInfo when both threads are
      // complete. It's possible for the smoothness thread component to have
      // completed before this tracker started. We do not include them in the
      // traces.
      if (!last_presented_termination_time.is_null() &&
          termination_time > last_presented_termination_time) {
        trace_data_v3_.Advance(last_presented_termination_time,
                               termination_time, v3_.frames_expected,
                               v3_.frames_dropped, frame_info.sequence_number,
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
        TraceJankV3(frame_info.sequence_number, last_presented_termination_time,
                    termination_time);
      }

      v3_.last_frame_delta = current_frame_delta;
      v3_.no_update_duration = base::TimeDelta();
      v3_.no_update_count = 0;
      v3_.last_presented_frame = frame_info;
      break;
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
  if (!trace_data_v3_.enabled) {
    return;
  }
  auto dict = std::make_unique<base::trace_event::TracedValue>();
  dict->BeginDictionary("data");
  dict->SetInteger("frame_sequence_number", sequence_number);
  dict->SetInteger("last_presented_frame_sequence_number",
                   v3_.last_presented_frame.sequence_number);
  dict->SetString("thread-type", GetJankThreadTypeName(GetEffectiveThread()));
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
