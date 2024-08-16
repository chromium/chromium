// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/post_login_metrics_recorder.h"

#include <string>
#include <vector>

#include "ash/metrics/deferred_metrics_reporter.h"
#include "ash/metrics/login_unlock_throughput_recorder.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/shell.h"
#include "base/check_is_test.h"
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

constexpr char kAshLoginSessionRestoreAllShelfIconsLoaded[] =
    "Ash.LoginSessionRestore.AllShelfIconsLoaded";

constexpr char kAshLoginSessionRestoreAllBrowserWindowsCreated[] =
    "Ash.LoginSessionRestore.AllBrowserWindowsCreated";

constexpr char kAshLoginSessionRestoreAllBrowserWindowsShown[] =
    "Ash.LoginSessionRestore.AllBrowserWindowsShown";

constexpr char kAshLoginSessionRestoreAllBrowserWindowsPresented[] =
    "Ash.LoginSessionRestore.AllBrowserWindowsPresented";

constexpr char kAshLoginSessionRestoreShelfLoginAnimationEnd[] =
    "Ash.LoginSessionRestore.ShelfLoginAnimationEnd";

constexpr char kUmaMetricsPrefixAutoRestore[] = "Ash.LoginPerf.AutoRestore.";
constexpr char kUmaMetricsPrefixManualRestore[] =
    "Ash.LoginPerf.ManualRestore.";

std::string GetDeviceModeSuffix() {
  return display::Screen::GetScreen()->InTabletMode() ? "TabletMode"
                                                      : "ClamshellMode";
}

void ReportLoginThroughputEvent(const std::string& event_name,
                                base::TimeTicks begin,
                                base::TimeTicks end) {
  // NOTE: list all expected event names with string literals here because we
  // cannot use dynamic strings for event names. (I.e. event names are filtered
  // out for privacy reasons when reported with TRACE_EVENT_COPY_* macros.)

#define REPORT_IF_MATCH(metric)                                      \
  if (event_name == metric) {                                        \
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(                \
        "startup", metric, TRACE_ID_LOCAL(kLoginThroughput), begin); \
    TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(                  \
        "startup", metric, TRACE_ID_LOCAL(kLoginThroughput), end);   \
    return;                                                          \
  }
  REPORT_IF_MATCH(kLoginThroughput);
  REPORT_IF_MATCH(kLoginThroughputUnordered);
  REPORT_IF_MATCH("OnAuthSuccess");
  REPORT_IF_MATCH("UserLoggedIn");
  REPORT_IF_MATCH("LoginAnimationEnd");
  REPORT_IF_MATCH("LoginFinished");
  REPORT_IF_MATCH("ArcUiAvailable");
  REPORT_IF_MATCH("Ash.LoginSessionRestore.AllBrowserWindowsCreated");
  REPORT_IF_MATCH("Ash.LoginSessionRestore.AllBrowserWindowsShown");
  REPORT_IF_MATCH("Ash.LoginSessionRestore.AllShelfIconsLoaded");
  REPORT_IF_MATCH("Ash.LoginSessionRestore.AllBrowserWindowsPresented");
  REPORT_IF_MATCH("Ash.LoginSessionRestore.ShelfLoginAnimationEnd");
#undef REPORT_IF_MATCH

  LOG(ERROR) << "Failed to report " << event_name;
  DCHECK(false) << "Failed to report " << event_name;
  constexpr char kFailedEvent[] = "FailedToReportEvent";
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      "startup", kFailedEvent, TRACE_ID_LOCAL(kLoginThroughput), begin);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "startup", kFailedEvent, TRACE_ID_LOCAL(kLoginThroughput), end);
}

class MetricTime : public DeferredMetricsReporter::Metric {
 public:
  MetricTime(std::string name,
             base::TimeDelta duration,
             base::TimeDelta min = base::Milliseconds(100),
             base::TimeDelta max = base::Seconds(30),
             size_t buckets = 100)
      : name_(std::move(name)),
        duration_(duration),
        min_(min),
        max_(max),
        buckets_(buckets) {}
  ~MetricTime() override = default;

  void Report(const std::string& prefix) override {
    base::UmaHistogramCustomTimes(prefix + name_, duration_, min_, max_,
                                  buckets_);
  }

 private:
  const std::string name_;
  const base::TimeDelta duration_;
  const base::TimeDelta min_;
  const base::TimeDelta max_;
  const size_t buckets_;
};

class MetricPercentage : public DeferredMetricsReporter::Metric {
 public:
  MetricPercentage(std::string name, int percentage)
      : name_(std::move(name)), percentage_(percentage) {}
  ~MetricPercentage() override = default;

  void Report(const std::string& prefix) override {
    base::UmaHistogramPercentage(prefix + name_, percentage_);
  }

 private:
  const std::string name_;
  const int percentage_;
};

}  // namespace

PostLoginMetricsRecorder::PostLoginMetricsRecorder(
    LoginUnlockThroughputRecorder* login_unlock_throughput_recorder) {
  if (login_unlock_throughput_recorder) {
    post_login_event_observation_.Observe(login_unlock_throughput_recorder);
  } else {
    // Unit tests call this without providing a
    // login_unlock_throughput_recorder.
    CHECK_IS_TEST();
  }
}

PostLoginMetricsRecorder::~PostLoginMetricsRecorder() = default;

void PostLoginMetricsRecorder::OnAuthSuccess(base::TimeTicks ts) {
  EnsureTracingSliceNamed(ts);
  AddLoginTimeMarker("OnAuthSuccess", ts);
}

void PostLoginMetricsRecorder::OnUserLoggedIn(base::TimeTicks ts,
                                              bool is_ash_restarted,
                                              bool is_regular_user_or_owner) {
  std::optional<base::TimeTicks> timestamp_on_auth_success = timestamp_origin_;

  EnsureTracingSliceNamed(ts);
  AddLoginTimeMarker("UserLoggedIn", ts);

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
    const base::TimeDelta duration = ts - timestamp_origin_.value();
    UMA_HISTOGRAM_CUSTOM_TIMES(kAshLoginSessionRestoreAllShelfIconsLoaded,
                               duration, base::Milliseconds(1),
                               base::Seconds(100), 100);
    AddLoginTimeMarker(kAshLoginSessionRestoreAllShelfIconsLoaded, ts);

    uma_login_perf_.ReportOrSchedule(
        std::make_unique<MetricTime>("AllShelfIconsLoaded", duration));
  }
}

void PostLoginMetricsRecorder::OnSessionRestoreDataLoaded(
    base::TimeTicks ts,
    bool restore_automatically) {
  if (restore_automatically) {
    uma_login_perf_.SetPrefix(kUmaMetricsPrefixAutoRestore);
  } else {
    uma_login_perf_.SetPrefix(kUmaMetricsPrefixManualRestore);
  }
  uma_login_perf_.MarkReadyToReport();
}

void PostLoginMetricsRecorder::OnAllBrowserWindowsCreated(base::TimeTicks ts) {
  if (timestamp_origin_.has_value()) {
    const base::TimeDelta duration = ts - timestamp_origin_.value();
    UMA_HISTOGRAM_CUSTOM_TIMES(kAshLoginSessionRestoreAllBrowserWindowsCreated,
                               duration, base::Milliseconds(1),
                               base::Seconds(100), 100);
    AddLoginTimeMarker(kAshLoginSessionRestoreAllBrowserWindowsCreated, ts);

    uma_login_perf_.ReportOrSchedule(
        std::make_unique<MetricTime>("AllBrowserWindowsCreated", duration));
  }
}

void PostLoginMetricsRecorder::OnAllBrowserWindowsShown(base::TimeTicks ts) {
  if (timestamp_origin_.has_value()) {
    const base::TimeDelta duration = ts - timestamp_origin_.value();
    UMA_HISTOGRAM_CUSTOM_TIMES(kAshLoginSessionRestoreAllBrowserWindowsShown,
                               duration, base::Milliseconds(1),
                               base::Seconds(100), 100);
    AddLoginTimeMarker(kAshLoginSessionRestoreAllBrowserWindowsShown, ts);

    uma_login_perf_.ReportOrSchedule(
        std::make_unique<MetricTime>("AllBrowserWindowsShown", duration));
  }
}

void PostLoginMetricsRecorder::OnAllBrowserWindowsPresented(
    base::TimeTicks ts) {
  if (timestamp_origin_.has_value()) {
    const base::TimeDelta duration = ts - timestamp_origin_.value();
    // Headless units do not report presentation time, so we only report
    // the histogram if primary display is functional.
    if (display::Screen::GetScreen()->GetPrimaryDisplay().detected()) {
      UMA_HISTOGRAM_CUSTOM_TIMES(
          kAshLoginSessionRestoreAllBrowserWindowsPresented, duration,
          base::Milliseconds(1), base::Seconds(100), 100);

      uma_login_perf_.ReportOrSchedule(
          std::make_unique<MetricTime>("AllBrowserWindowsPresented", duration));
    }
    AddLoginTimeMarker(kAshLoginSessionRestoreAllBrowserWindowsPresented, ts);
  }
}

void PostLoginMetricsRecorder::OnShelfAnimationFinished(base::TimeTicks ts) {
  if (timestamp_origin_.has_value()) {
    const base::TimeDelta duration = ts - timestamp_origin_.value();
    UMA_HISTOGRAM_CUSTOM_TIMES(kAshLoginSessionRestoreShelfLoginAnimationEnd,
                               duration, base::Milliseconds(1),
                               base::Seconds(100), 100);
    AddLoginTimeMarker(kAshLoginSessionRestoreShelfLoginAnimationEnd, ts);

    uma_login_perf_.ReportOrSchedule(
        std::make_unique<MetricTime>("ShelfLoginAnimationEnd", duration));
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
  AddLoginTimeMarker("LoginAnimationEnd", ts);

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
  base::TimeDelta duration = ts - timestamp_origin_.value();

  base::UmaHistogramPercentage(smoothness_name + suffix, smoothness);
  base::UmaHistogramPercentage(jank_name + suffix, jank);
  base::UmaHistogramCustomTimes(duration_name + suffix, duration,
                                base::Milliseconds(100), base::Seconds(30),
                                100);

  uma_login_perf_.ReportOrSchedule(std::make_unique<MetricPercentage>(
      "PostLoginAnimation.Smoothness." + suffix, smoothness));
  uma_login_perf_.ReportOrSchedule(std::make_unique<MetricPercentage>(
      "PostLoginAnimation.Jank." + suffix, jank));
  uma_login_perf_.ReportOrSchedule(std::make_unique<MetricTime>(
      "PostLoginAnimation.Duration." + suffix, duration));
}

void PostLoginMetricsRecorder::OnArcUiReady(base::TimeTicks ts) {
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

  uma_login_perf_.ReportOrSchedule(std::make_unique<MetricTime>(
      "ArcUiAvailableAfterLogin", duration, base::Milliseconds(100),
      base::Seconds(100), 100));

  // Note that this event is only reported when OnArcUiReady is called
  // before OnShelfAnimationAndCompositorAnimationDone.
  AddLoginTimeMarker("ArcUiAvailable", ts);
}

void PostLoginMetricsRecorder::OnShelfIconsLoadedAndSessionRestoreDone(
    base::TimeTicks ts) {
  // TODO(b/328339021, b/323098858): This is the mitigation against a bug that
  // animation observation has race condition. Can be in a part of better
  // architecture.
  base::UmaHistogramCustomTimes(
      "BootTime.Login4", ts - timestamp_origin_.value(),
      base::Milliseconds(100), base::Seconds(100), 100);
}

void PostLoginMetricsRecorder::OnShelfAnimationAndCompositorAnimationDone(
    base::TimeTicks ts) {
  AddLoginTimeMarker("LoginFinished", ts);
  // This is the last event we expect. We can report all the events now.
  ReportTraceEvents();

  LoginEventRecorder::Get()->AddLoginTimeMarker("LoginFinished",
                                                /*send_to_uma=*/false,
                                                /*write_to_file=*/false);

  auto total_duration =
      LoginEventRecorder::Get()->GetDuration("LoginStarted", "LoginFinished");
  // For unit tests, "LoginStarted" may not be recorded.
  if (total_duration.has_value()) {
    uma_login_perf_.ReportOrSchedule(
        std::make_unique<MetricTime>("TotalDuration", total_duration.value()));
  }

  base::UmaHistogramCustomTimes(
      "BootTime.Login3", ts - timestamp_origin_.value(),
      base::Milliseconds(100), base::Seconds(100), 100);

  LoginEventRecorder::Get()->RunScheduledWriteLoginTimes();
}

PostLoginMetricsRecorder::TimeMarker::TimeMarker(const std::string& name,
                                                 base::TimeTicks time)
    : name_(name), time_(time) {}

void PostLoginMetricsRecorder::AddLoginTimeMarker(
    const std::string& marker_name,
    base::TimeTicks marker_timestamp) {
  // Unit tests often miss the full initialization flow so we use a
  // different label in this case.
  if (markers_.empty() && marker_name != kLoginThroughput) {
    markers_.emplace_back(kLoginThroughputUnordered,
                          marker_timestamp - base::Microseconds(1));
  }

  markers_.emplace_back(marker_name, marker_timestamp);
}

void PostLoginMetricsRecorder::EnsureTracingSliceNamed(base::TimeTicks ts) {
  // EnsureTracingSliceNamed should be called only on expected events.
  // If login ThroughputRecording did not start with either OnAuthSuccess
  // or LoggedInStateChanged the tracing slice will have the "-unordered"
  // suffix.
  //
  // Depending on the login flow this function may get called multiple times.
  if (markers_.empty()) {
    // The first event will name the tracing row.
    AddLoginTimeMarker(kLoginThroughput, ts - base::Microseconds(1));
    timestamp_origin_ = ts;
  }
}

void PostLoginMetricsRecorder::ReportTraceEvents() {
  CHECK(!markers_.empty());

  std::sort(markers_.begin(), markers_.end());

  // First marker has the name of tracing track.
  ReportLoginThroughputEvent(markers_[0].name(), markers_[0].time(),
                             markers_[0].time());
  for (size_t i = 1; i < markers_.size(); ++i) {
    ReportLoginThroughputEvent(markers_[i].name(), markers_[i - 1].time(),
                               markers_[i].time());
  }

  markers_.clear();
}

}  // namespace ash
