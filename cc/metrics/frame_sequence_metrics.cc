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
#include "cc/metrics/throughput_ukm_reporter.h"
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

// Avoid reporting any throughput metric for sequences that do not have a
// sufficient number of frames.
constexpr int kMinFramesForThroughputMetric = 100;

constexpr int kBuiltinSequenceNum =
    static_cast<int>(FrameSequenceTrackerType::kMaxType) + 1;
constexpr int kMaximumHistogramIndex = 3 * kBuiltinSequenceNum;

int GetIndexForMetric(SmoothEffectDrivingThread thread_type,
                      FrameSequenceTrackerType type) {
  if (thread_type == SmoothEffectDrivingThread::kMain)
    return static_cast<int>(type);
  if (thread_type == SmoothEffectDrivingThread::kCompositor)
    return static_cast<int>(type) + kBuiltinSequenceNum;
  return static_cast<int>(type) + 2 * kBuiltinSequenceNum;
}

std::string GetCheckerboardingHistogramName(FrameSequenceTrackerType type) {
  return base::StrCat(
      {"Graphics.Smoothness.Checkerboarding.",
       FrameSequenceTracker::GetFrameSequenceTrackerTypeName(type)});
}

std::string GetThroughputHistogramName(FrameSequenceTrackerType type,
                                       const char* thread_name) {
  return base::StrCat(
      {"Graphics.Smoothness.PercentDroppedFrames.", thread_name, ".",
       FrameSequenceTracker::GetFrameSequenceTrackerTypeName(type)});
}

std::string GetThroughputV2HistogramName(FrameSequenceTrackerType type,
                                         const char* thread_name) {
  return base::StrCat(
      {"Graphics.Smoothness.PercentDroppedFrames2.", thread_name, ".",
       FrameSequenceTracker::GetFrameSequenceTrackerTypeName(type)});
}

std::string GetThroughputV3HistogramName(FrameSequenceTrackerType type,
                                         const char* thread_name) {
  return base::StrCat(
      {"Graphics.Smoothness.PercentDroppedFrames3.", thread_name, ".",
       FrameSequenceTracker::GetFrameSequenceTrackerTypeName(type)});
}

std::string GetMissedDeadlineHistogramName(FrameSequenceTrackerType type,
                                           const char* thread_name) {
  return base::StrCat(
      {"Graphics.Smoothness.PercentMissedDeadlineFrames.", thread_name, ".",
       FrameSequenceTracker::GetFrameSequenceTrackerTypeName(type)});
}

bool IsInteractionType(FrameSequenceTrackerType sequence_type) {
  return sequence_type == FrameSequenceTrackerType::kScrollbarScroll ||
         sequence_type == FrameSequenceTrackerType::kTouchScroll ||
         sequence_type == FrameSequenceTrackerType::kWheelScroll ||
         sequence_type == FrameSequenceTrackerType::kPinchZoom;
}

}  // namespace

FrameSequenceMetrics::FrameSequenceMetrics(FrameSequenceTrackerType type,
                                           ThroughputUkmReporter* ukm_reporter)
    : type_(type), throughput_ukm_reporter_(ukm_reporter) {
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

  v2_.frames_expected += metrics->v2_.frames_expected;
  v2_.frames_dropped += metrics->v2_.frames_dropped;

  v3_.frames_expected += metrics->v3_.frames_expected;
  v3_.frames_dropped += metrics->v3_.frames_dropped;

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
         v2_.frames_expected >= kMinFramesForThroughputMetric ||
         v3_.frames_expected >= kMinFramesForThroughputMetric;
}

bool FrameSequenceMetrics::HasDataLeftForReporting() const {
  return impl_throughput_.frames_expected > 0 || v2_.frames_expected > 0 ||
         main_throughput_.frames_expected > 0 || v3_.frames_expected > 0;
}

void FrameSequenceMetrics::AdoptTrace(FrameSequenceMetrics* adopt_from) {
  DCHECK(!trace_data_.trace_id);
  trace_data_.trace_id = adopt_from->trace_data_.trace_id;
  adopt_from->trace_data_.trace_id = nullptr;
}

void FrameSequenceMetrics::AdvanceTrace(base::TimeTicks timestamp) {
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
  trace_data_.Advance(timestamp, expected, dropped);
}

void FrameSequenceMetrics::ReportMetrics() {
  DCHECK_LE(impl_throughput_.frames_produced, impl_throughput_.frames_expected);
  DCHECK_LE(main_throughput_.frames_produced, main_throughput_.frames_expected);
  DCHECK_LE(impl_throughput_.frames_ontime, impl_throughput_.frames_expected);
  DCHECK_LE(main_throughput_.frames_ontime, main_throughput_.frames_expected);

  // Terminates |trace_data_| for all types of FrameSequenceTracker.
  trace_data_.Terminate();

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

  const bool main_report = ThroughputData::CanReportHistogram(
      this, SmoothEffectDrivingThread::kMain, main_throughput_);
  const bool compositor_report = ThroughputData::CanReportHistogram(
      this, SmoothEffectDrivingThread::kCompositor, impl_throughput_);

  const auto thread_type = GetEffectiveThread();
  const bool is_animation = ShouldReportForAnimation(type(), thread_type);
  const bool is_interaction =
      ShouldReportForInteraction(type(), thread_type, thread_type);

  if (v2_.frames_expected >= kMinFramesForThroughputMetric) {
    int percent = v2_.frames_expected == 0
                      ? 0
                      : std::ceil(100. * v2_.frames_dropped /
                                  static_cast<double>(v2_.frames_expected));

    if (is_animation) {
      UMA_HISTOGRAM_PERCENTAGE(
          "Graphics.Smoothness.PercentDroppedFrames2.AllAnimations", percent);
    }
    if (is_interaction) {
      UMA_HISTOGRAM_PERCENTAGE(
          "Graphics.Smoothness.PercentDroppedFrames2.AllInteractions", percent);
    }
    if (is_animation || is_interaction) {
      UMA_HISTOGRAM_PERCENTAGE(
          "Graphics.Smoothness.PercentDroppedFrames2.AllSequences", percent);
    }

    const char* thread_name =
        thread_type == SmoothEffectDrivingThread::kCompositor
            ? "CompositorThread"
            : "MainThread";

    STATIC_HISTOGRAM_POINTER_GROUP(
        GetThroughputV2HistogramName(type(), thread_name),
        GetIndexForMetric(thread_type, type_), kMaximumHistogramIndex,
        Add(percent),
        base::LinearHistogram::FactoryGet(
            GetThroughputV2HistogramName(type(), thread_name), 1, 100, 101,
            base::HistogramBase::kUmaTargetedHistogramFlag));
    v2_ = {};
  }

  if (v3_.frames_expected >= kMinFramesForThroughputMetric) {
    int percent = v3_.frames_expected == 0
                      ? 0
                      : std::ceil(100. * v3_.frames_dropped /
                                  static_cast<double>(v3_.frames_expected));

    if (is_animation) {
      UMA_HISTOGRAM_PERCENTAGE(
          "Graphics.Smoothness.PercentDroppedFrames3.AllAnimations", percent);
    }
    if (is_interaction) {
      UMA_HISTOGRAM_PERCENTAGE(
          "Graphics.Smoothness.PercentDroppedFrames3.AllInteractions", percent);
    }
    if (is_animation || is_interaction) {
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

    v3_ = {};
  }

  absl::optional<int> impl_throughput_percent_dropped;
  absl::optional<int> impl_throughput_percent_missed;
  absl::optional<int> main_throughput_percent_dropped;
  absl::optional<int> main_throughput_percent_missed;

  // Report the throughput metrics.
  if (compositor_report) {
    impl_throughput_percent_dropped =
        ThroughputData::ReportDroppedFramePercentHistogram(
            this, SmoothEffectDrivingThread::kCompositor,
            GetIndexForMetric(SmoothEffectDrivingThread::kCompositor, type_),
            impl_throughput_);
    impl_throughput_percent_missed =
        ThroughputData::ReportMissedDeadlineFramePercentHistogram(
            this, SmoothEffectDrivingThread::kCompositor,
            GetIndexForMetric(SmoothEffectDrivingThread::kCompositor, type_),
            impl_throughput_);
  }
  if (main_report) {
    main_throughput_percent_dropped =
        ThroughputData::ReportDroppedFramePercentHistogram(
            this, SmoothEffectDrivingThread::kMain,
            GetIndexForMetric(SmoothEffectDrivingThread::kMain, type_),
            main_throughput_);
    main_throughput_percent_missed =
        ThroughputData::ReportMissedDeadlineFramePercentHistogram(
            this, SmoothEffectDrivingThread::kMain,
            GetIndexForMetric(SmoothEffectDrivingThread::kMain, type_),
            main_throughput_);
  }

  // Report for the 'scrolling thread' for the scrolling interactions.
  if (scrolling_thread_ != SmoothEffectDrivingThread::kUnknown) {
    absl::optional<int> scrolling_thread_throughput_dropped;
    absl::optional<int> scrolling_thread_throughput_missed;
    switch (scrolling_thread_) {
      case SmoothEffectDrivingThread::kCompositor:
        scrolling_thread_throughput_dropped = impl_throughput_percent_dropped;
        scrolling_thread_throughput_missed = impl_throughput_percent_missed;
        break;
      case SmoothEffectDrivingThread::kMain:
        scrolling_thread_throughput_dropped = main_throughput_percent_dropped;
        scrolling_thread_throughput_missed = main_throughput_percent_missed;
        break;
      case SmoothEffectDrivingThread::kUnknown:
        NOTREACHED();
        break;
    }
    // It's OK to use the UMA histogram in the following code while still
    // using |GetThroughputHistogramName()| to get the name of the metric,
    // since the input-params to the function never change at runtime.
    if (scrolling_thread_throughput_dropped.has_value() &&
        scrolling_thread_throughput_missed.has_value()) {
      if (type_ == FrameSequenceTrackerType::kWheelScroll) {
        UMA_HISTOGRAM_PERCENTAGE(
            GetThroughputHistogramName(FrameSequenceTrackerType::kWheelScroll,
                                       "ScrollingThread"),
            scrolling_thread_throughput_dropped.value());
        UMA_HISTOGRAM_PERCENTAGE(
            GetMissedDeadlineHistogramName(
                FrameSequenceTrackerType::kWheelScroll, "ScrollingThread"),
            scrolling_thread_throughput_missed.value());
      } else if (type_ == FrameSequenceTrackerType::kTouchScroll) {
        UMA_HISTOGRAM_PERCENTAGE(
            GetThroughputHistogramName(FrameSequenceTrackerType::kTouchScroll,
                                       "ScrollingThread"),
            scrolling_thread_throughput_dropped.value());
        UMA_HISTOGRAM_PERCENTAGE(
            GetMissedDeadlineHistogramName(
                FrameSequenceTrackerType::kTouchScroll, "ScrollingThread"),
            scrolling_thread_throughput_missed.value());
      } else {
        DCHECK_EQ(type_, FrameSequenceTrackerType::kScrollbarScroll);
        UMA_HISTOGRAM_PERCENTAGE(
            GetThroughputHistogramName(
                FrameSequenceTrackerType::kScrollbarScroll, "ScrollingThread"),
            scrolling_thread_throughput_dropped.value());
        UMA_HISTOGRAM_PERCENTAGE(
            GetMissedDeadlineHistogramName(
                FrameSequenceTrackerType::kScrollbarScroll, "ScrollingThread"),
            scrolling_thread_throughput_missed.value());
      }
    }
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

bool FrameSequenceMetrics::ThroughputData::CanReportHistogram(
    FrameSequenceMetrics* metrics,
    SmoothEffectDrivingThread thread_type,
    const ThroughputData& data) {
  const auto sequence_type = metrics->type();
  DCHECK_LT(sequence_type, FrameSequenceTrackerType::kMaxType);

  // All video frames and SET compositor animations are compositor thread only.
  if ((sequence_type == FrameSequenceTrackerType::kVideo ||
       sequence_type == FrameSequenceTrackerType::kSETCompositorAnimation) &&
      thread_type == SmoothEffectDrivingThread::kMain)
    return false;

  // RAF, CanvasAnimation and SET main thread animations are main thread only.
  if ((sequence_type == FrameSequenceTrackerType::kRAF ||
       sequence_type == FrameSequenceTrackerType::kCanvasAnimation ||
       sequence_type == FrameSequenceTrackerType::kSETMainThreadAnimation) &&
      thread_type == SmoothEffectDrivingThread::kCompositor)
    return false;

  if (data.frames_expected < kMinFramesForThroughputMetric)
    return false;

  const bool is_animation =
      ShouldReportForAnimation(sequence_type, thread_type);

  return is_animation || IsInteractionType(sequence_type) ||
         sequence_type == FrameSequenceTrackerType::kVideo ||
         sequence_type == FrameSequenceTrackerType::kRAF ||
         sequence_type == FrameSequenceTrackerType::kCanvasAnimation ||
         sequence_type == FrameSequenceTrackerType::kSETMainThreadAnimation ||
         sequence_type == FrameSequenceTrackerType::kSETCompositorAnimation;
}

int FrameSequenceMetrics::ThroughputData::ReportDroppedFramePercentHistogram(
    FrameSequenceMetrics* metrics,
    SmoothEffectDrivingThread thread_type,
    int metric_index,
    const ThroughputData& data) {
  const auto sequence_type = metrics->type();
  DCHECK_LT(sequence_type, FrameSequenceTrackerType::kMaxType);
  DCHECK(CanReportHistogram(metrics, thread_type, data));

  // Throughput means the percent of frames that was expected to show on the
  // screen but didn't. In other words, the lower the throughput is, the
  // smoother user experience.
  const int percent = data.DroppedFramePercent();

  const bool is_animation =
      ShouldReportForAnimation(sequence_type, thread_type);
  const bool is_interaction = ShouldReportForInteraction(
      metrics->type(), thread_type, metrics->GetEffectiveThread());

  ThroughputUkmReporter* const ukm_reporter = metrics->ukm_reporter();

  if (is_animation) {
    TRACE_EVENT_INSTANT2("cc,benchmark", "PercentDroppedFrames.AllAnimations",
                         TRACE_EVENT_SCOPE_THREAD, "frames_expected",
                         data.frames_expected, "frames_produced",
                         data.frames_produced);

    UMA_HISTOGRAM_PERCENTAGE(
        "Graphics.Smoothness.PercentDroppedFrames.AllAnimations", percent);
    if (ukm_reporter) {
      ukm_reporter->ReportAggregateThroughput(AggregationType::kAllAnimations,
                                              percent);
    }
  }

  if (is_interaction) {
    TRACE_EVENT_INSTANT2("cc,benchmark", "PercentDroppedFrames.AllInteractions",
                         TRACE_EVENT_SCOPE_THREAD, "frames_expected",
                         data.frames_expected, "frames_produced",
                         data.frames_produced);
    UMA_HISTOGRAM_PERCENTAGE(
        "Graphics.Smoothness.PercentDroppedFrames.AllInteractions", percent);
    if (ukm_reporter) {
      ukm_reporter->ReportAggregateThroughput(AggregationType::kAllInteractions,
                                              percent);
    }
  }

  if (is_animation || is_interaction) {
    TRACE_EVENT_INSTANT2("cc,benchmark", "PercentDroppedFrames.AllSequences",
                         TRACE_EVENT_SCOPE_THREAD, "frames_expected",
                         data.frames_expected, "frames_produced",
                         data.frames_produced);
    UMA_HISTOGRAM_PERCENTAGE(
        "Graphics.Smoothness.PercentDroppedFrames.AllSequences", percent);
    if (ukm_reporter) {
      ukm_reporter->ReportAggregateThroughput(AggregationType::kAllSequences,
                                              percent);
    }
  }

  const char* thread_name =
      thread_type == SmoothEffectDrivingThread::kCompositor ? "CompositorThread"
                                                            : "MainThread";
  STATIC_HISTOGRAM_POINTER_GROUP(
      GetThroughputHistogramName(sequence_type, thread_name), metric_index,
      kMaximumHistogramIndex, Add(percent),
      base::LinearHistogram::FactoryGet(
          GetThroughputHistogramName(sequence_type, thread_name), 1, 100, 101,
          base::HistogramBase::kUmaTargetedHistogramFlag));
  return percent;
}

int FrameSequenceMetrics::ThroughputData::
    ReportMissedDeadlineFramePercentHistogram(
        FrameSequenceMetrics* metrics,
        SmoothEffectDrivingThread thread_type,
        int metric_index,
        const ThroughputData& data) {
  const auto sequence_type = metrics->type();
  DCHECK_LT(sequence_type, FrameSequenceTrackerType::kMaxType);

  // Throughput means the percent of frames that was expected to show on the
  // screen but didn't. In other words, the lower the throughput is, the
  // smoother user experience.
  const int percent = data.MissedDeadlineFramePercent();

  const bool is_animation =
      ShouldReportForAnimation(sequence_type, thread_type);
  const bool is_interaction = ShouldReportForInteraction(
      metrics->type(), thread_type, metrics->GetEffectiveThread());

  if (is_animation) {
    TRACE_EVENT_INSTANT2(
        "cc,benchmark", "PercentMissedDeadlineFrames.AllAnimations",
        TRACE_EVENT_SCOPE_THREAD, "frames_expected", data.frames_expected,
        "frames_ontime", data.frames_ontime);

    UMA_HISTOGRAM_PERCENTAGE(
        "Graphics.Smoothness.PercentMissedDeadlineFrames.AllAnimations",
        percent);
  }

  if (is_interaction) {
    TRACE_EVENT_INSTANT2(
        "cc,benchmark", "PercentMissedDeadlineFrames.AllInteractions",
        TRACE_EVENT_SCOPE_THREAD, "frames_expected", data.frames_expected,
        "frames_ontime", data.frames_ontime);
    UMA_HISTOGRAM_PERCENTAGE(
        "Graphics.Smoothness.PercentMissedDeadlineFrames.AllInteractions",
        percent);
  }

  if (is_animation || is_interaction) {
    TRACE_EVENT_INSTANT2(
        "cc,benchmark", "PercentMissedDeadlineFrames.AllSequences",
        TRACE_EVENT_SCOPE_THREAD, "frames_expected", data.frames_expected,
        "frames_ontime", data.frames_ontime);
    UMA_HISTOGRAM_PERCENTAGE(
        "Graphics.Smoothness.PercentMissedDeadlineFrames.AllSequences",
        percent);
  }

  const char* thread_name =
      thread_type == SmoothEffectDrivingThread::kCompositor ? "CompositorThread"
                                                            : "MainThread";
  STATIC_HISTOGRAM_POINTER_GROUP(
      GetMissedDeadlineHistogramName(sequence_type, thread_name), metric_index,
      kMaximumHistogramIndex, Add(percent),
      base::LinearHistogram::FactoryGet(
          GetMissedDeadlineHistogramName(sequence_type, thread_name), 1, 100,
          101, base::HistogramBase::kUmaTargetedHistogramFlag));
  return percent;
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
    dict->SetInteger("main-frames-ontime", main.frames_ontime);
  } else {
    dict->SetInteger("impl-frames-produced", impl.frames_produced);
    dict->SetInteger("impl-frames-expected", impl.frames_expected);
    dict->SetInteger("impl-frames-ontime", impl.frames_ontime);
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

void FrameSequenceMetrics::TraceData::Advance(base::TimeTicks new_timestamp,
                                              uint32_t expected,
                                              uint32_t dropped) {
  if (!enabled)
    return;
  if (!trace_id) {
    trace_id = this;
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP1(
        "cc,benchmark", "FrameSequenceTracker", TRACE_ID_LOCAL(trace_id),
        this->last_timestamp, "name",
        FrameSequenceTracker::GetFrameSequenceTrackerTypeName(metrics->type()));
  }
  // Use different names, because otherwise the trace-viewer shows the slices in
  // the same color, and that makes it difficult to tell the traces apart from
  // each other.
  const char* trace_names[] = {"Frame", "Frame ", "Frame   "};
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      "cc,benchmark", trace_names[++this->frame_count % 3],
      TRACE_ID_LOCAL(trace_id), this->last_timestamp);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP2(
      "cc,benchmark", trace_names[this->frame_count % 3],
      TRACE_ID_LOCAL(trace_id), new_timestamp, "expected", expected, "dropped",
      dropped);
  this->last_timestamp = new_timestamp;
}

void FrameSequenceMetrics::AddSortedFrame(const viz::BeginFrameArgs& args,
                                          const FrameInfo& frame_info) {
  switch (GetEffectiveThread()) {
    case SmoothEffectDrivingThread::kCompositor:
      if (frame_info.WasSmoothCompositorUpdateDropped()) {
        ++v2_.frames_dropped;
        ++v3_.frames_dropped;
      }
      ++v2_.frames_expected;
      ++v3_.frames_expected;
      break;
    case SmoothEffectDrivingThread::kMain:
      if (frame_info.WasSmoothMainUpdateExpected()) {
        if (frame_info.WasSmoothMainUpdateDropped()) {
          ++v3_.frames_dropped;
        }
        ++v3_.frames_expected;
      }
      if (frame_info.WasSmoothMainUpdateDropped()) {
        ++v2_.frames_dropped;
      }
      ++v2_.frames_expected;
      break;
    case SmoothEffectDrivingThread::kUnknown:
      NOTREACHED();
      break;
  }
}

}  // namespace cc
