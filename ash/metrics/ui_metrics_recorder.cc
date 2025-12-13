// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/ui_metrics_recorder.h"

#include <array>
#include <string_view>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "cc/metrics/event_metrics.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

using EventType = cc::EventMetrics::EventType;

namespace ash {

namespace {

using FpsBucket = UiMetricsRecorder::FpsBucket;
using CoreEventType = UiMetricsRecorder::CoreEventType;
constexpr int kMaxFpsBucketIndex = UiMetricsRecorder::kMaxFpsBucketIndex;
constexpr int kMaxCoreEventTypeIndex =
    UiMetricsRecorder::kMaxCoreEventTypeIndex;

// A fixed table of event latency histogram names. This is to avoid runtime
// string constructions and for better readability.
constexpr std::array<
    std::string_view,
    static_cast<size_t>(cc::EventMetrics::EventType::kMaxValue) + 1>
    kEventLatencyHistogramNames = {{
        "Ash.EventLatency.MousePressed.TotalLatency",
        "Ash.EventLatency.MouseReleased.TotalLatency",
        "Ash.EventLatency.MouseWheel.TotalLatency",
        "Ash.EventLatency.KeyPressed.TotalLatency",
        "Ash.EventLatency.KeyReleased.TotalLatency",
        "Ash.EventLatency.TouchPressed.TotalLatency",
        "Ash.EventLatency.TouchReleased.TotalLatency",
        "Ash.EventLatency.TouchMoved.TotalLatency",
        "Ash.EventLatency.GestureScrollBegin.TotalLatency",
        "Ash.EventLatency.GestureScrollUpdate.TotalLatency",
        "Ash.EventLatency.GestureScrollEnd.TotalLatency",
        "Ash.EventLatency.GestureDoubleTap.TotalLatency",
        "Ash.EventLatency.GestureLongPress.TotalLatency",
        "Ash.EventLatency.GestureLongTap.TotalLatency",
        "Ash.EventLatency.GestureShowPress.TotalLatency",
        "Ash.EventLatency.GestureTap.TotalLatency",
        "Ash.EventLatency.GestureTapCancel.TotalLatency",
        "Ash.EventLatency.GestureTapDown.TotalLatency",
        "Ash.EventLatency.GestureTapUnconfirmed.TotalLatency",
        "Ash.EventLatency.GestureTwoFingerTap.TotalLatency",
        "Ash.EventLatency.FirstGestureScrollUpdate.TotalLatency",
        "Ash.EventLatency.MouseDragged.TotalLatency",
        "Ash.EventLatency.GesturePinchBegin.TotalLatency",
        "Ash.EventLatency.GesturePinchEnd.TotalLatency",
        "Ash.EventLatency.GesturePinchUpdate.TotalLatency",
        "Ash.EventLatency.InertialGestureScrollUpdate.TotalLatency",
        "Ash.EventLatency.MouseMoved.TotalLatency",
        "Ash.EventLatency.InertialGestureScrollEnd.TotalLatency",
    }};

static_assert(kEventLatencyHistogramNames.size() ==
                  static_cast<size_t>(cc::EventMetrics::EventType::kMaxValue) +
                      1,
              "kEventLatencyHistogramNames needs to be updated to match "
              "cc::EventMetrics::EventType");

constexpr int kMaxEventTypeIndex =
    static_cast<int>(cc::EventMetrics::EventType::kMaxValue) + 1;

// A fixed table of core event latency + fps histogram names. This is to avoid
// runtime string constructions and for better readability.
constexpr std::array<std::string_view,
                     kMaxCoreEventTypeIndex * kMaxFpsBucketIndex>
    kCoreEventLatencyHistogramNames = {{
        // 30Fps
        "Ash.EventLatency.Core.KeyPressed.30Fps.TotalLatency",
        "Ash.EventLatency.Core.KeyReleased.30Fps.TotalLatency",
        "Ash.EventLatency.Core.MousePressed.30Fps.TotalLatency",
        "Ash.EventLatency.Core.MouseReleased.30Fps.TotalLatency",
        "Ash.EventLatency.Core.MouseDragged.30Fps.TotalLatency",
        // 60Fps
        "Ash.EventLatency.Core.KeyPressed.60Fps.TotalLatency",
        "Ash.EventLatency.Core.KeyReleased.60Fps.TotalLatency",
        "Ash.EventLatency.Core.MousePressed.60Fps.TotalLatency",
        "Ash.EventLatency.Core.MouseReleased.60Fps.TotalLatency",
        "Ash.EventLatency.Core.MouseDragged.60Fps.TotalLatency",
        // 120Fps
        "Ash.EventLatency.Core.KeyPressed.120Fps.TotalLatency",
        "Ash.EventLatency.Core.KeyReleased.120Fps.TotalLatency",
        "Ash.EventLatency.Core.MousePressed.120Fps.TotalLatency",
        "Ash.EventLatency.Core.MouseReleased.120Fps.TotalLatency",
        "Ash.EventLatency.Core.MouseDragged.120Fps.TotalLatency",
        // OtherFps
        "Ash.EventLatency.Core.KeyPressed.OtherFps.TotalLatency",
        "Ash.EventLatency.Core.KeyReleased.OtherFps.TotalLatency",
        "Ash.EventLatency.Core.MousePressed.OtherFps.TotalLatency",
        "Ash.EventLatency.Core.MouseReleased.OtherFps.TotalLatency",
        "Ash.EventLatency.Core.MouseDragged.OtherFps.TotalLatency",
        // UnsetFps
        "Ash.EventLatency.Core.KeyPressed.UnsetFps.TotalLatency",
        "Ash.EventLatency.Core.KeyReleased.UnsetFps.TotalLatency",
        "Ash.EventLatency.Core.MousePressed.UnsetFps.TotalLatency",
        "Ash.EventLatency.Core.MouseReleased.UnsetFps.TotalLatency",
        "Ash.EventLatency.Core.MouseDragged.UnsetFps.TotalLatency",
    }};

bool IsCoreMetric(EventType event_type) {
  return event_type == EventType::kMouseDragged ||
         event_type == EventType::kMousePressed ||
         event_type == EventType::kMouseReleased ||
         event_type == EventType::kKeyPressed ||
         event_type == EventType::kKeyReleased;
}

CoreEventType EventTypeToCoreEventType(cc::EventMetrics::EventType event_type) {
  switch (event_type) {
    case EventType::kKeyPressed:
      return CoreEventType::kKeyPressed;
    case EventType::kKeyReleased:
      return CoreEventType::kKeyReleased;
    case EventType::kMousePressed:
      return CoreEventType::kMousePressed;
    case EventType::kMouseReleased:
      return CoreEventType::kMouseReleased;
    case EventType::kMouseDragged:
      return CoreEventType::kMouseDragged;
    default:
      NOTREACHED();
  }
}

FpsBucket GetFpsBucket(base::TimeDelta interval) {
  if (interval.is_zero()) {
    return FpsBucket::kUnset;
  }

  constexpr base::TimeDelta k120FpsInterval = base::Hertz(120);
  constexpr base::TimeDelta k60FpsInterval = base::Hertz(60);
  constexpr base::TimeDelta k30FpsInterval = base::Hertz(30);

  // Use an epsilon to compare intervals to account for small variations.
  constexpr base::TimeDelta kEpsilonTimeDelta = base::Milliseconds(0.5);

  // Test 60fps first since it should be the more popular case.
  if ((interval - k60FpsInterval).magnitude() <= kEpsilonTimeDelta) {
    return FpsBucket::k60Fps;
  }

  if ((interval - k120FpsInterval).magnitude() <= kEpsilonTimeDelta) {
    return FpsBucket::k120Fps;
  }
  if ((interval - k30FpsInterval).magnitude() <= kEpsilonTimeDelta) {
    return FpsBucket::k30Fps;
  }
  return FpsBucket::kOtherFps;
}

int GetHistogramIndex(CoreEventType core_event_type, FpsBucket fps_bucket) {
  return static_cast<int>(fps_bucket) * kMaxCoreEventTypeIndex +
         static_cast<int>(core_event_type);
}

}  // namespace

UiMetricsRecorder::UiMetricsRecorder() = default;
UiMetricsRecorder::~UiMetricsRecorder() = default;

void UiMetricsRecorder::OnUserLoggedIn() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // OnUserLoggedIn could be called multiple times from any states.
  // e.g.
  //   from kBeforeLogin: sign-in from the login screen and on cryptohome mount
  //   from kDuringLogin: during user profile loading after checking ownership
  //   from kInSession: adding a new user to the existing session.
  // Only set kDuringLogin on first OnUserLoggedIn call from kBeforeLogin so
  // that kDuringLogin starts from cryptohome mount.
  if (state_ == State::kBeforeLogin) {
    state_ = State::kDuringLogin;
    user_logged_in_time_ = base::TimeTicks::Now();
  }
}

void UiMetricsRecorder::OnPostLoginAnimationFinish() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This happens when adding a user to the existing session. Ignore it to
  // treat secondary user login as in session since multiple profile feature is
  // deprecating.
  if (state_ == State::kInSession)
    return;

  DCHECK_EQ(State::kDuringLogin, state_);
  state_ = State::kInSession;
  user_session_start_time_ = base::TimeTicks::Now();
}

void UiMetricsRecorder::ReportPercentDroppedFramesInOneSecondWindow2(
    double percent) {
  UMA_HISTOGRAM_PERCENTAGE("Ash.Smoothness.PercentDroppedFrames_1sWindow2",
                           percent);

  // Time to exclude from user session to be reported under "InSession" metric.
  constexpr base::TimeDelta chopped_user_session_time = base::Minutes(1);
  if (user_session_start_time_ &&
      base::TimeTicks::Now() - user_session_start_time_.value() >=
          chopped_user_session_time) {
    UMA_HISTOGRAM_PERCENTAGE(
        "Ash.Smoothness.PercentDroppedFrames_1sWindow2.InSession", percent);
  }
}

void UiMetricsRecorder::ReportEventLatency(
    const viz::BeginFrameArgs& args,
    std::vector<cc::EventLatencyTracker::LatencyData> latencies) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  constexpr base::TimeDelta kMaxLatency = base::Seconds(5);
  constexpr base::TimeDelta kLongLatency = base::Milliseconds(500);
  const FpsBucket fps_bucket = GetFpsBucket(args.interval);

  for (auto& latency : latencies) {
    const char* event_type = cc::EventMetrics::GetTypeName(latency.event_type);

    const int event_type_histogram_index = static_cast<int>(latency.event_type);
    const std::string_view event_type_histogram_name =
        kEventLatencyHistogramNames[event_type_histogram_index];
    STATIC_HISTOGRAM_POINTER_GROUP(
        event_type_histogram_name, event_type_histogram_index,
        kMaxEventTypeIndex,
        AddTimeMicrosecondsGranularity(latency.total_latency),
        base::Histogram::FactoryMicrosecondsTimeGet(
            event_type_histogram_name, base::Milliseconds(1), kMaxLatency, 100,
            base::HistogramBase::kUmaTargetedHistogramFlag));

    UMA_HISTOGRAM_CUSTOM_TIMES("Ash.EventLatency.TotalLatency",
                               latency.total_latency, base::Milliseconds(1),
                               kMaxLatency, 100);

    if (IsCoreMetric(latency.event_type)) {
      UMA_HISTOGRAM_CUSTOM_TIMES("Ash.EventLatency.Core.TotalLatency",
                                 latency.total_latency, base::Milliseconds(1),
                                 kMaxLatency, 100);

      // Breakdown by event type and fps.
      const CoreEventType core_event_type =
          EventTypeToCoreEventType(latency.event_type);
      const int core_event_type_fps_histogram_index =
          GetHistogramIndex(core_event_type, fps_bucket);
      const std::string_view core_event_type_fps_histogram_name =
          kCoreEventLatencyHistogramNames[core_event_type_fps_histogram_index];
      STATIC_HISTOGRAM_POINTER_GROUP(
          core_event_type_fps_histogram_name,
          core_event_type_fps_histogram_index,
          kMaxCoreEventTypeIndex * kMaxFpsBucketIndex,
          AddTimeMicrosecondsGranularity(latency.total_latency),
          base::Histogram::FactoryMicrosecondsTimeGet(
              core_event_type_fps_histogram_name, base::Milliseconds(1),
              kMaxLatency, 100,
              base::HistogramBase::kUmaTargetedHistogramFlag));
    }

    if (latency.event_type != EventType::kGestureLongPress &&
        latency.event_type != EventType::kGestureLongTap &&
        latency.total_latency > kLongLatency) {
      VLOG(1) << "Ash event latency is longer than usual"
              << ", type=" << event_type
              << ", latency= " << latency.total_latency.InMilliseconds()
              << " ms";
    }
  }
}

// static
base::span<const std::string_view>
UiMetricsRecorder::GetEventLatencyHistogramNamesForTest() {
  return kEventLatencyHistogramNames;
}

// static
base::span<const std::string_view>
UiMetricsRecorder::GetCoreEventLatencyHistogramNamesForTest() {
  return kCoreEventLatencyHistogramNames;
}

}  // namespace ash
