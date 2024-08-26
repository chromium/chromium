// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/visibility_metrics_logger.h"

#include "base/containers/contains.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_constants.h"

using content::BrowserThread;

namespace android_webview {

namespace {

const char* SchemeEnumToString(VisibilityMetricsLogger::Scheme scheme) {
  switch (scheme) {
    case VisibilityMetricsLogger::Scheme::kEmpty:
      return "empty";
    case VisibilityMetricsLogger::Scheme::kUnknown:
      return "unknown";
    case VisibilityMetricsLogger::Scheme::kHttp:
      return url::kHttpScheme;
    case VisibilityMetricsLogger::Scheme::kHttps:
      return url::kHttpsScheme;
    case VisibilityMetricsLogger::Scheme::kFile:
      return url::kFileScheme;
    case VisibilityMetricsLogger::Scheme::kFtp:
      return url::kFtpScheme;
    case VisibilityMetricsLogger::Scheme::kData:
      return url::kDataScheme;
    case VisibilityMetricsLogger::Scheme::kJavaScript:
      return url::kJavaScriptScheme;
    case VisibilityMetricsLogger::Scheme::kAbout:
      return url::kAboutScheme;
    case VisibilityMetricsLogger::Scheme::kChrome:
      return content::kChromeUIScheme;
    case VisibilityMetricsLogger::Scheme::kBlob:
      return url::kBlobScheme;
    case VisibilityMetricsLogger::Scheme::kContent:
      return url::kContentScheme;
    case VisibilityMetricsLogger::Scheme::kIntent:
      return "intent";
    default:
      NOTREACHED();
  }
}

// Have bypassed the usual macros here because they do not support a
// means to increment counters by more than 1 per call.
base::HistogramBase* GetOrCreateHistogramForDurationTracking(
    const std::string& name,
    int max_value) {
  return base::Histogram::FactoryGet(
      name, 1, max_value + 1, max_value + 2,
      base::HistogramBase::kUmaTargetedHistogramFlag);
}

// base::Histogram::FactoryGet would internally convert to std::string anyway,
// this overload is for convenience.
base::HistogramBase* GetOrCreateHistogramForDurationTracking(const char* name,
                                                             int max_value) {
  return GetOrCreateHistogramForDurationTracking(std::string(name), max_value);
}

base::HistogramBase* GetGlobalVisibilityHistogram() {
  static base::HistogramBase* histogram(GetOrCreateHistogramForDurationTracking(
      "Android.WebView.Visibility.Global",
      static_cast<int>(VisibilityMetricsLogger::Visibility::kMaxValue)));
  return histogram;
}

base::HistogramBase* GetPerWebViewVisibilityHistogram() {
  static base::HistogramBase* histogram(GetOrCreateHistogramForDurationTracking(
      "Android.WebView.Visibility.PerWebView",
      static_cast<int>(VisibilityMetricsLogger::Visibility::kMaxValue)));
  return histogram;
}

void LogGlobalVisibleScheme(VisibilityMetricsLogger::Scheme scheme,
                            int32_t seconds) {
  static base::HistogramBase* histogram(GetOrCreateHistogramForDurationTracking(
      "Android.WebView.VisibleScheme.Global",
      static_cast<int>(VisibilityMetricsLogger::Scheme::kMaxValue)));
  histogram->AddCount(static_cast<int32_t>(scheme), seconds);
}

void LogPerWebViewVisibleScheme(VisibilityMetricsLogger::Scheme scheme,
                                int32_t seconds) {
  static base::HistogramBase* histogram(GetOrCreateHistogramForDurationTracking(
      "Android.WebView.VisibleScheme.PerWebView",
      static_cast<int>(VisibilityMetricsLogger::Scheme::kMaxValue)));
  histogram->AddCount(static_cast<int32_t>(scheme), seconds);
}

void LogGlobalVisibleScreenCoverage(int percentage, int32_t seconds) {
  static base::HistogramBase* histogram(GetOrCreateHistogramForDurationTracking(
      "Android.WebView.VisibleScreenCoverage.Global", 100));
  histogram->AddCount(percentage, seconds);
}

void LogPerWebViewVisibleScreenCoverage(int percentage, int32_t seconds) {
  static base::HistogramBase* histogram(GetOrCreateHistogramForDurationTracking(
      "Android.WebView.VisibleScreenCoverage.PerWebView", 100));
  histogram->AddCount(percentage, seconds);
}

void LogPerSchemeVisibleScreenCoverage(VisibilityMetricsLogger::Scheme scheme,
                                       int percentage,
                                       int32_t seconds) {
  GetOrCreateHistogramForDurationTracking(
      std::string("Android.WebView.VisibleScreenCoverage.PerWebView.") +
          SchemeEnumToString(scheme),
      100)
      ->AddCount(percentage, seconds);
}

}  // anonymous namespace

// static
VisibilityMetricsLogger::Scheme VisibilityMetricsLogger::SchemeStringToEnum(
    const std::string& scheme) {
  if (scheme.empty())
    return Scheme::kEmpty;
  if (scheme == url::kHttpScheme)
    return Scheme::kHttp;
  if (scheme == url::kHttpsScheme)
    return Scheme::kHttps;
  if (scheme == url::kFileScheme)
    return Scheme::kFile;
  if (scheme == url::kFtpScheme)
    return Scheme::kFtp;
  if (scheme == url::kDataScheme)
    return Scheme::kData;
  if (scheme == url::kJavaScriptScheme)
    return Scheme::kJavaScript;
  if (scheme == url::kAboutScheme)
    return Scheme::kAbout;
  if (scheme == content::kChromeUIScheme)
    return Scheme::kChrome;
  if (scheme == url::kBlobScheme)
    return Scheme::kBlob;
  if (scheme == url::kContentScheme)
    return Scheme::kContent;
  if (scheme == "intent")
    return Scheme::kIntent;
  return Scheme::kUnknown;
}

VisibilityMetricsLogger::VisibilityMetricsLogger() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  last_update_time_ = base::TimeTicks::Now();
}

VisibilityMetricsLogger::~VisibilityMetricsLogger() = default;

void VisibilityMetricsLogger::AddClient(Client* client) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!base::Contains(client_visibility_, client));

  UpdateDurations();

  client_visibility_[client] = VisibilityInfo();
  ProcessClientUpdate(client, client->GetVisibilityInfo(),
                      ClientAction::kAdded);
}

void VisibilityMetricsLogger::RemoveClient(Client* client) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(base::Contains(client_visibility_, client));

  UpdateDurations();

  ProcessClientUpdate(client, VisibilityInfo(), ClientAction::kRemoved);
  client_visibility_.erase(client);
}

void VisibilityMetricsLogger::ClientVisibilityChanged(Client* client) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(base::Contains(client_visibility_, client));

  UpdateDurations();

  ProcessClientUpdate(client, client->GetVisibilityInfo(),
                      ClientAction::kVisibilityChanged);
}

void VisibilityMetricsLogger::UpdateScreenCoverage(
    int global_percentage,
    const std::vector<Scheme>& schemes,
    const std::vector<int>& scheme_percentages) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(schemes.size() == scheme_percentages.size());

  UpdateDurations();

  DCHECK(global_percentage >= 0);
  DCHECK(global_percentage <= 100);
  global_coverage_percentage_ = global_percentage;

  schemes_to_coverage_percentages_.clear();
  for (size_t i = 0; i < schemes.size(); i++) {
    DCHECK(scheme_percentages[i] >= 0);
    DCHECK(scheme_percentages[i] <= 100);
    schemes_to_coverage_percentages_.emplace(schemes[i], scheme_percentages[i]);
  }
}

void VisibilityMetricsLogger::UpdateDurations() {
  base::TimeTicks update_time = base::TimeTicks::Now();
  base::TimeDelta delta = update_time - last_update_time_;
  if (all_clients_visible_count_ > 0) {
    all_clients_tracker_.any_webview_tracked_duration_ += delta;
  } else {
    all_clients_tracker_.no_webview_tracked_duration_ += delta;
  }
  all_clients_tracker_.per_webview_duration_ +=
      delta * all_clients_visible_count_;
  all_clients_tracker_.per_webview_untracked_duration_ +=
      delta * (client_visibility_.size() - all_clients_visible_count_);

  for (size_t i = 0; i < std::size(per_scheme_visible_counts_); i++) {
    if (!per_scheme_visible_counts_[i])
      continue;
    per_scheme_trackers_[i].any_webview_tracked_duration_ += delta;
    per_scheme_trackers_[i].per_webview_duration_ +=
        delta * per_scheme_visible_counts_[i];
  }

  if (all_clients_visible_count_ > 0) {
    global_coverage_percentage_durations_[global_coverage_percentage_] += delta;

    for (auto& scheme_and_percentage : schemes_to_coverage_percentages_) {
      schemes_to_percentages_to_durations_[scheme_and_percentage.first]
                                          [scheme_and_percentage.second] +=
          delta;
    }
  }

  last_update_time_ = update_time;
}

bool VisibilityMetricsLogger::VisibilityInfo::IsVisible() const {
  return view_attached && view_visible && window_visible;
}

void VisibilityMetricsLogger::ProcessClientUpdate(Client* client,
                                                  const VisibilityInfo& info,
                                                  ClientAction action) {
  VisibilityInfo curr_info = client_visibility_[client];
  bool was_visible = curr_info.IsVisible();
  bool is_visible = info.IsVisible();
  Scheme old_scheme = curr_info.scheme;
  Scheme new_scheme = info.scheme;
  client_visibility_[client] = info;
  DCHECK(!was_visible || all_clients_visible_count_ > 0);

  bool any_client_was_visible = all_clients_visible_count_ > 0;

  if (action == ClientAction::kAdded) {
    // Only emit the event if the WebView is visible so that the track gets the
    // appropriate name.
    // TODO(b/280334022): set the track name explicitly after the Perfetto SDK
    // migration is finished (crbug/1006541).
    if (is_visible) {
      TRACE_EVENT_BEGIN("android_webview.timeline", "WebViewVisible",
                        perfetto::Track::FromPointer(client));
    }
  }

  // If visibility changes or the client is removed, close the event
  // corresponding to the previous visibility state.
  if (action == ClientAction::kRemoved || was_visible != is_visible) {
    TRACE_EVENT_END("android_webview.timeline",
                    perfetto::Track::FromPointer(client));
  }

  if (!was_visible && is_visible) {
    if (action != ClientAction::kRemoved) {
      TRACE_EVENT_BEGIN("android_webview.timeline", "WebViewVisible",
                        perfetto::Track::FromPointer(client));
    }
    ++all_clients_visible_count_;
  } else if (was_visible && !is_visible) {
    if (action != ClientAction::kRemoved) {
      TRACE_EVENT_BEGIN("android_webview.timeline", "WebViewInvisible",
                        perfetto::Track::FromPointer(client));
    }
    --all_clients_visible_count_;
  }

  if (was_visible)
    per_scheme_visible_counts_[static_cast<size_t>(old_scheme)]--;
  if (is_visible)
    per_scheme_visible_counts_[static_cast<size_t>(new_scheme)]++;

  bool any_client_is_visible = all_clients_visible_count_ > 0;
  if (on_visibility_changed_callback_ &&
      any_client_was_visible != any_client_is_visible) {
    on_visibility_changed_callback_.Run(any_client_is_visible);
  }
}

void VisibilityMetricsLogger::SetOnVisibilityChangedCallback(
    OnVisibilityChangedCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  on_visibility_changed_callback_ = std::move(callback);
}

void VisibilityMetricsLogger::RecordMetrics() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  UpdateDurations();
  RecordVisibilityMetrics();
  RecordVisibleSchemeMetrics();
  RecordScreenCoverageMetrics();
}

void VisibilityMetricsLogger::RecordVisibilityMetrics() {
  int32_t any_webview_visible_seconds;
  int32_t no_webview_visible_seconds;
  int32_t total_webview_visible_seconds;
  int32_t total_no_webview_visible_seconds;

  any_webview_visible_seconds =
      all_clients_tracker_.any_webview_tracked_duration_.InSeconds();
  all_clients_tracker_.any_webview_tracked_duration_ -=
      base::Seconds(any_webview_visible_seconds);
  no_webview_visible_seconds =
      all_clients_tracker_.no_webview_tracked_duration_.InSeconds();
  all_clients_tracker_.no_webview_tracked_duration_ -=
      base::Seconds(no_webview_visible_seconds);

  total_webview_visible_seconds =
      all_clients_tracker_.per_webview_duration_.InSeconds();
  all_clients_tracker_.per_webview_duration_ -=
      base::Seconds(total_webview_visible_seconds);
  total_no_webview_visible_seconds =
      all_clients_tracker_.per_webview_untracked_duration_.InSeconds();
  all_clients_tracker_.per_webview_untracked_duration_ -=
      base::Seconds(total_no_webview_visible_seconds);

  if (any_webview_visible_seconds) {
    GetGlobalVisibilityHistogram()->AddCount(
        static_cast<int>(Visibility::kVisible), any_webview_visible_seconds);
  }
  if (no_webview_visible_seconds) {
    GetGlobalVisibilityHistogram()->AddCount(
        static_cast<int>(Visibility::kNotVisible), no_webview_visible_seconds);
  }

  if (total_webview_visible_seconds) {
    GetPerWebViewVisibilityHistogram()->AddCount(
        static_cast<int>(Visibility::kVisible), total_webview_visible_seconds);
  }
  if (total_no_webview_visible_seconds) {
    GetPerWebViewVisibilityHistogram()->AddCount(
        static_cast<int>(Visibility::kNotVisible),
        total_no_webview_visible_seconds);
  }
}

void VisibilityMetricsLogger::RecordVisibleSchemeMetrics() {
  for (size_t i = 0; i < std::size(per_scheme_trackers_); i++) {
    Scheme scheme = static_cast<Scheme>(i);
    auto& tracker = per_scheme_trackers_[i];

    int32_t any_webview_seconds =
        tracker.any_webview_tracked_duration_.InSeconds();
    if (any_webview_seconds) {
      tracker.any_webview_tracked_duration_ -=
          base::Seconds(any_webview_seconds);
      LogGlobalVisibleScheme(scheme, any_webview_seconds);
    }

    int32_t per_webview_seconds = tracker.per_webview_duration_.InSeconds();
    if (per_webview_seconds) {
      tracker.per_webview_duration_ -= base::Seconds(per_webview_seconds);
      LogPerWebViewVisibleScheme(scheme, per_webview_seconds);
    }
  }
}

void VisibilityMetricsLogger::RecordScreenCoverageMetrics() {
  for (size_t i = 0; i < std::size(global_coverage_percentage_durations_);
       i++) {
    int32_t seconds = global_coverage_percentage_durations_[i].InSeconds();
    if (seconds == 0)
      continue;

    global_coverage_percentage_durations_[i] -= base::Seconds(seconds);
    LogGlobalVisibleScreenCoverage(i, seconds);
  }

  for (auto& scheme_and_map : schemes_to_percentages_to_durations_) {
    for (auto& percentage_and_duration : scheme_and_map.second) {
      int32_t seconds = percentage_and_duration.second.InSeconds();
      if (seconds == 0)
        continue;

      percentage_and_duration.second -= base::Seconds(seconds);
      LogPerWebViewVisibleScreenCoverage(percentage_and_duration.first,
                                         seconds);
      LogPerSchemeVisibleScreenCoverage(scheme_and_map.first,
                                        percentage_and_duration.first, seconds);
    }
  }
}

}  // namespace android_webview
