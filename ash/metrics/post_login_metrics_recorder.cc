// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/post_login_metrics_recorder.h"

#include <string>
#include <vector>

#include "ash/metrics/login_unlock_throughput_recorder.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/shell.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "chromeos/ash/components/metrics/login_event_recorder.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace ash {

namespace {

// Tracing ID and trace events row name.
// This must be a constexpr.
constexpr char kLoginThroughput[] = "LoginThroughput";

// Unit tests often miss initialization and thus we use different label.
constexpr char kLoginThroughputUnordered[] = "LoginThroughput-unordered";

std::string GetDeviceModeSuffix() {
  return display::Screen::GetScreen()->InTabletMode() ? "TabletMode"
                                                      : "ClamshellMode";
}

}  // namespace

PostLoginMetricsRecorder::PostLoginMetricsRecorder(
    LoginUnlockThroughputRecorder* login_unlock_throughput_recorder) {
  post_login_event_observation_.Observe(login_unlock_throughput_recorder);
}

PostLoginMetricsRecorder::~PostLoginMetricsRecorder() = default;

void PostLoginMetricsRecorder::AddLoginTimeMarker(
    const std::string& marker_name) {
  // Unit tests often miss the full initialization flow so we use a
  // different label in this case.
  if (markers_.empty() && marker_name != kLoginThroughput) {
    markers_.emplace_back(kLoginThroughputUnordered);

    const base::TimeTicks begin = markers_.front().time();
    const base::TimeTicks end = begin;

    TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
        "startup", kLoginThroughputUnordered, TRACE_ID_LOCAL(kLoginThroughput),
        begin);
    TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
        "startup", kLoginThroughputUnordered, TRACE_ID_LOCAL(kLoginThroughput),
        end);
  }

  markers_.emplace_back(marker_name);
  bool reported = false;

#define REPORT_LOGIN_THROUGHPUT_EVENT(metric)                        \
  if (marker_name == metric) {                                       \
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(                \
        "startup", metric, TRACE_ID_LOCAL(kLoginThroughput), begin); \
    TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(                  \
        "startup", metric, TRACE_ID_LOCAL(kLoginThroughput), end);   \
    reported = true;                                                 \
  }                                                                  \
  class __STUB__

  if (markers_.size() > 1) {
    const base::TimeTicks begin = markers_[markers_.size() - 2].time();
    const base::TimeTicks end = markers_[markers_.size() - 1].time();

    REPORT_LOGIN_THROUGHPUT_EVENT(
        "Ash.LoginSessionRestore.AllBrowserWindowsCreated");
    REPORT_LOGIN_THROUGHPUT_EVENT(
        "Ash.LoginSessionRestore.AllBrowserWindowsShown");
    REPORT_LOGIN_THROUGHPUT_EVENT(
        "Ash.LoginSessionRestore.AllShelfIconsLoaded");
    REPORT_LOGIN_THROUGHPUT_EVENT(
        "Ash.LoginSessionRestore.AllBrowserWindowsPresented");
    REPORT_LOGIN_THROUGHPUT_EVENT(
        "Ash.LoginSessionRestore.ShelfLoginAnimationEnd");
    REPORT_LOGIN_THROUGHPUT_EVENT("LoginAnimationEnd");
    REPORT_LOGIN_THROUGHPUT_EVENT("LoginFinished");
    REPORT_LOGIN_THROUGHPUT_EVENT(
        "Ash.LoginAnimation.Smoothness.ClamshellMode");
    REPORT_LOGIN_THROUGHPUT_EVENT("Ash.LoginAnimation.Smoothness.TabletMode");
    REPORT_LOGIN_THROUGHPUT_EVENT("Ash.LoginAnimation.Jank.ClamshellMode");
    REPORT_LOGIN_THROUGHPUT_EVENT("Ash.LoginAnimation.Jank.TabletMode");
    REPORT_LOGIN_THROUGHPUT_EVENT("Ash.LoginAnimation.Duration2.ClamshellMode");
    REPORT_LOGIN_THROUGHPUT_EVENT("Ash.LoginAnimation.Duration2.TabletMode");
    REPORT_LOGIN_THROUGHPUT_EVENT(
        "Ash.UnlockAnimation.Smoothness.ClamshellMode");
    REPORT_LOGIN_THROUGHPUT_EVENT("Ash.UnlockAnimation.Smoothness.TabletMode");
    REPORT_LOGIN_THROUGHPUT_EVENT("ArcUiAvailable");
    REPORT_LOGIN_THROUGHPUT_EVENT("OnAuthSuccess");
    REPORT_LOGIN_THROUGHPUT_EVENT("UserLoggedIn");
    if (!reported) {
      constexpr char kFailedEvent[] = "FailedToReportEvent";
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
          "startup", kFailedEvent, TRACE_ID_LOCAL(kLoginThroughput), begin);
      TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
          "startup", kFailedEvent, TRACE_ID_LOCAL(kLoginThroughput), end);
    }
  } else {
    // The first event will be used as a row name in the tracing UI.
    const base::TimeTicks begin = markers_.front().time();
    const base::TimeTicks end = begin;

    REPORT_LOGIN_THROUGHPUT_EVENT(kLoginThroughput);
  }
#undef REPORT_LOGIN_THROUGHPUT_EVENT
  DCHECK(reported) << "Failed to report " << marker_name
                   << ", markers_.size()=" << markers_.size();
}

void PostLoginMetricsRecorder::OnAuthSuccess(base::TimeTicks ts) {
  EnsureTracingSliceNamed(ts);
  AddLoginTimeMarker("OnAuthSuccess");
}

void PostLoginMetricsRecorder::OnUserLoggedIn(base::TimeTicks ts,
                                              bool is_ash_restarted,
                                              bool is_regular_user_or_owner) {
  std::optional<base::TimeTicks> timestamp_on_auth_success = timestamp_origin_;

  EnsureTracingSliceNamed(ts);
  AddLoginTimeMarker("UserLoggedIn");

  if (is_ash_restarted || !is_regular_user_or_owner) {
    return;
  }

  // Report UserLoggedIn histogram if we had OnAuthSuccess() event previously.
  if (timestamp_on_auth_success.has_value()) {
    const base::TimeDelta duration = ts - timestamp_on_auth_success.value();
    base::UmaHistogramTimes("Ash.Login.LoggedInStateChanged", duration);
  }
}

void PostLoginMetricsRecorder::OnAllExpectedShelfIconLoaded(
    base::TimeTicks ts) {
  if (timestamp_origin_.has_value()) {
    const base::TimeDelta duration_ms = ts - timestamp_origin_.value();
    constexpr char kAshLoginSessionRestoreAllShelfIconsLoaded[] =
        "Ash.LoginSessionRestore.AllShelfIconsLoaded";
    UMA_HISTOGRAM_CUSTOM_TIMES(kAshLoginSessionRestoreAllShelfIconsLoaded,
                               duration_ms, base::Milliseconds(1),
                               base::Seconds(100), 100);
    AddLoginTimeMarker(kAshLoginSessionRestoreAllShelfIconsLoaded);
  }
}

void PostLoginMetricsRecorder::OnAllBrowserWindowsCreated(base::TimeTicks ts) {
  if (timestamp_origin_.has_value()) {
    const base::TimeDelta duration_ms = ts - timestamp_origin_.value();
    constexpr char kAshLoginSessionRestoreAllBrowserWindowsCreated[] =
        "Ash.LoginSessionRestore.AllBrowserWindowsCreated";
    UMA_HISTOGRAM_CUSTOM_TIMES(kAshLoginSessionRestoreAllBrowserWindowsCreated,
                               duration_ms, base::Milliseconds(1),
                               base::Seconds(100), 100);
    AddLoginTimeMarker(kAshLoginSessionRestoreAllBrowserWindowsCreated);
  }
}

void PostLoginMetricsRecorder::OnAllBrowserWindowsShown(base::TimeTicks ts) {
  if (timestamp_origin_.has_value()) {
    const base::TimeDelta duration_ms = ts - timestamp_origin_.value();
    constexpr char kAshLoginSessionRestoreAllBrowserWindowsShown[] =
        "Ash.LoginSessionRestore.AllBrowserWindowsShown";
    UMA_HISTOGRAM_CUSTOM_TIMES("Ash.LoginSessionRestore.AllBrowserWindowsShown",
                               duration_ms, base::Milliseconds(1),
                               base::Seconds(100), 100);
    AddLoginTimeMarker(kAshLoginSessionRestoreAllBrowserWindowsShown);
  }
}

void PostLoginMetricsRecorder::OnAllBrowserWindowsPresented(
    base::TimeTicks ts) {
  if (timestamp_origin_.has_value()) {
    const base::TimeDelta duration_ms = ts - timestamp_origin_.value();
    constexpr char kAshLoginSessionRestoreAllBrowserWindowsPresented[] =
        "Ash.LoginSessionRestore.AllBrowserWindowsPresented";
    // Headless units do not report presentation time, so we only report
    // the histogram if primary display is functional.
    if (display::Screen::GetScreen()->GetPrimaryDisplay().detected()) {
      UMA_HISTOGRAM_CUSTOM_TIMES(
          kAshLoginSessionRestoreAllBrowserWindowsPresented, duration_ms,
          base::Milliseconds(1), base::Seconds(100), 100);
    }
    AddLoginTimeMarker(kAshLoginSessionRestoreAllBrowserWindowsPresented);
  }
}

void PostLoginMetricsRecorder::OnShelfAnimationFinished(base::TimeTicks ts) {
  if (timestamp_origin_.has_value()) {
    const base::TimeDelta duration_ms = ts - timestamp_origin_.value();
    constexpr char kAshLoginSessionRestoreShelfLoginAnimationEnd[] =
        "Ash.LoginSessionRestore.ShelfLoginAnimationEnd";
    UMA_HISTOGRAM_CUSTOM_TIMES(kAshLoginSessionRestoreShelfLoginAnimationEnd,
                               duration_ms, base::Milliseconds(1),
                               base::Seconds(100), 100);
    AddLoginTimeMarker(kAshLoginSessionRestoreShelfLoginAnimationEnd);
  }
}

void PostLoginMetricsRecorder::OnCompositorAnimationFinished(
    base::TimeTicks ts,
    const cc::FrameSequenceMetrics::CustomReportData& data) {
  if (!data.frames_expected_v3) {
    LOG(WARNING) << "Zero frames expected in login animation throughput data";
    return;
  }

  LoginEventRecorder::Get()->AddLoginTimeMarker("LoginAnimationEnd",
                                                /*send_to_uma=*/false,
                                                /*write_to_file=*/false);
  AddLoginTimeMarker("LoginAnimationEnd");

  // Report could happen during Shell shutdown. Early out in that case.
  if (!Shell::HasInstance() || !Shell::Get()->tablet_mode_controller()) {
    return;
  }

  constexpr char smoothness_name[] = "Ash.LoginAnimation.Smoothness.";
  constexpr char jank_name[] = "Ash.LoginAnimation.Jank.";
  constexpr char duration_name[] = "Ash.LoginAnimation.Duration2.";
  std::string suffix = GetDeviceModeSuffix();

  int smoothness = metrics_util::CalculateSmoothnessV3(data);
  int jank = metrics_util::CalculateJankV3(data);

  DCHECK(timestamp_origin_.has_value());
  int duration_ms = (ts - timestamp_origin_.value()).InMilliseconds();

  base::UmaHistogramPercentage(smoothness_name + suffix, smoothness);
  AddLoginTimeMarker(smoothness_name + suffix);

  base::UmaHistogramPercentage(jank_name + suffix, jank);
  AddLoginTimeMarker(jank_name + suffix);

  base::UmaHistogramCustomTimes(
      duration_name + suffix, base::Milliseconds(duration_ms),
      base::Milliseconds(100), base::Seconds(30), 100);
  AddLoginTimeMarker(duration_name + suffix);
}

void PostLoginMetricsRecorder::OnArcUiReady(base::TimeTicks ts) {
  AddLoginTimeMarker("ArcUiAvailable");

  // It seems that neither `OnAuthSuccess` nor `LoggedInStateChanged` is called
  // on some ARC tests.
  if (!timestamp_origin_.has_value()) {
    return;
  }

  const base::TimeDelta duration = ts - timestamp_origin_.value();
  base::UmaHistogramCustomTimes("Ash.Login.ArcUiAvailableAfterLogin.Duration",
                                duration, base::Milliseconds(100),
                                base::Seconds(30), 100);
  LOCAL_HISTOGRAM_TIMES("Ash.Tast.ArcUiAvailableAfterLogin.Duration", duration);
}

void PostLoginMetricsRecorder::OnShelfIconsLoadedAndSessionRestoreDone(
    base::TimeTicks ts) {
  // Unblock deferred task now.
  // TODO(b/328339021, b/323098858): This is the mitigation against a bug
  // that animation observation has race condition.
  // Can be in a part of better architecture.
  base::UmaHistogramCustomTimes(
      "BootTime.Login4", ts - timestamp_origin_.value(),
      base::Milliseconds(100), base::Seconds(100), 100);
}

void PostLoginMetricsRecorder::OnShelfAnimationAndCompositorAnimationDone(
    base::TimeTicks ts) {
  AddLoginTimeMarker("LoginFinished");
  LoginEventRecorder::Get()->AddLoginTimeMarker("LoginFinished",
                                                /*send_to_uma=*/false,
                                                /*write_to_file=*/false);

  base::UmaHistogramCustomTimes(
      "BootTime.Login3", ts - timestamp_origin_.value(),
      base::Milliseconds(100), base::Seconds(100), 100);

  LoginEventRecorder::Get()->RunScheduledWriteLoginTimes();
}

PostLoginMetricsRecorder::TimeMarker::TimeMarker(const std::string& name)
    : name_(name) {}

void PostLoginMetricsRecorder::EnsureTracingSliceNamed(base::TimeTicks ts) {
  // EnsureTracingSliceNamed should be called only on expected events.
  // If login ThroughputRecording did not start with either OnAuthSuccess
  // or LoggedInStateChanged the tracing slice will have the "-unordered"
  // suffix.
  //
  // Depending on the login flow this function may get called multiple times.
  if (markers_.empty()) {
    // The first event will name the tracing row.
    AddLoginTimeMarker(kLoginThroughput);
    timestamp_origin_ = ts;
  }
}

}  // namespace ash
