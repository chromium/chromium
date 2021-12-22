// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/visibility_metrics_logger.h"

#include "android_webview/common/aw_features.h"
#include "base/cxx17_backports.h"
#include "base/feature_list.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_constants.h"

using content::BrowserThread;

namespace android_webview {

namespace {

// Have bypassed the usual macros here because they do not support a
// means to increment counters by more than 1 per call.
base::HistogramBase* CreateHistogramForDurationTracking(const char* name,
                                                        int max_value) {
  return base::Histogram::FactoryGet(
      name, 1, max_value + 1, max_value + 2,
      base::HistogramBase::kUmaTargetedHistogramFlag);
}

base::HistogramBase* GetGlobalVisibilityHistogram() {
  static base::HistogramBase* histogram(CreateHistogramForDurationTracking(
      "Android.WebView.Visibility.Global",
      static_cast<int>(VisibilityMetricsLogger::Visibility::kMaxValue)));
  return histogram;
}

base::HistogramBase* GetPerWebViewVisibilityHistogram() {
  static base::HistogramBase* histogram(CreateHistogramForDurationTracking(
      "Android.WebView.Visibility.PerWebView",
      static_cast<int>(VisibilityMetricsLogger::Visibility::kMaxValue)));
  return histogram;
}

void LogGlobalVisibleScheme(VisibilityMetricsLogger::Scheme scheme,
                            int32_t seconds) {
  static base::HistogramBase* histogram(CreateHistogramForDurationTracking(
      "Android.WebView.VisibleScheme.Global",
      static_cast<int>(VisibilityMetricsLogger::Scheme::kMaxValue)));
  histogram->AddCount(static_cast<int32_t>(scheme), seconds);
}

void LogPerWebViewVisibleScheme(VisibilityMetricsLogger::Scheme scheme,
                                int32_t seconds) {
  static base::HistogramBase* histogram(CreateHistogramForDurationTracking(
      "Android.WebView.VisibleScheme.PerWebView",
      static_cast<int>(VisibilityMetricsLogger::Scheme::kMaxValue)));
  histogram->AddCount(static_cast<int32_t>(scheme), seconds);
}

base::HistogramBase* GetOpenWebVisibileScreenPortionHistogram() {
  static base::HistogramBase* histogram(CreateHistogramForDurationTracking(
      "Android.WebView.WebViewOpenWebVisible.ScreenPortion2",
      static_cast<int>(
          VisibilityMetricsLogger::WebViewOpenWebScreenPortion::kMaxValue)));
  return histogram;
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
  DCHECK(client_visibility_.find(client) == client_visibility_.end());

  UpdateDurations();

  client_visibility_[client] = VisibilityInfo();
  ProcessClientUpdate(client, client->GetVisibilityInfo());
}

void VisibilityMetricsLogger::RemoveClient(Client* client) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(client_visibility_.find(client) != client_visibility_.end());

  UpdateDurations();

  ProcessClientUpdate(client, VisibilityInfo());
  client_visibility_.erase(client);
}

void VisibilityMetricsLogger::ClientVisibilityChanged(Client* client) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(client_visibility_.find(client) != client_visibility_.end());

  UpdateDurations();

  ProcessClientUpdate(client, client->GetVisibilityInfo());
}

void VisibilityMetricsLogger::UpdateOpenWebScreenArea(int pixels,
                                                      int percentage) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  UpdateDurations();

  DCHECK(percentage >= 0);
  DCHECK(percentage <= 100);

  if (pixels == 0) {
    current_open_web_screen_portion_ = VisibilityMetricsLogger::
        WebViewOpenWebScreenPortion::kExactlyZeroPercent;
  } else {
    current_open_web_screen_portion_ =
        static_cast<VisibilityMetricsLogger::WebViewOpenWebScreenPortion>(
            percentage / 10);
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

  for (size_t i = 0; i < base::size(per_scheme_visible_counts_); i++) {
    if (!per_scheme_visible_counts_[i])
      continue;
    per_scheme_trackers_[i].any_webview_tracked_duration_ += delta;
    per_scheme_trackers_[i].per_webview_duration_ +=
        delta * per_scheme_visible_counts_[i];
  }

  if (per_scheme_visible_counts_[static_cast<size_t>(Scheme::kHttp)] > 0 ||
      per_scheme_visible_counts_[static_cast<size_t>(Scheme::kHttps)] > 0) {
    open_web_screen_portion_tracked_duration_[static_cast<int>(
        current_open_web_screen_portion_)] += delta;
  }

  last_update_time_ = update_time;
}

bool VisibilityMetricsLogger::VisibilityInfo::IsVisible() const {
  return view_attached && view_visible && window_visible;
}

bool VisibilityMetricsLogger::VisibilityInfo::IsDisplayingOpenWebContent()
    const {
  return IsVisible() && (scheme == Scheme::kHttp || scheme == Scheme::kHttps);
}

void VisibilityMetricsLogger::ProcessClientUpdate(Client* client,
                                                  const VisibilityInfo& info) {
  VisibilityInfo curr_info = client_visibility_[client];
  bool was_visible = curr_info.IsVisible();
  bool is_visible = info.IsVisible();
  Scheme old_scheme = curr_info.scheme;
  Scheme new_scheme = info.scheme;
  client_visibility_[client] = info;
  DCHECK(!was_visible || all_clients_visible_count_ > 0);

  bool any_client_was_visible = all_clients_visible_count_ > 0;

  if (!was_visible && is_visible) {
    ++all_clients_visible_count_;
  } else if (was_visible && !is_visible) {
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
  RecordScreenPortionMetrics();
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
  for (size_t i = 0; i < base::size(per_scheme_trackers_); i++) {
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

void VisibilityMetricsLogger::RecordScreenPortionMetrics() {
  if (!base::FeatureList::IsEnabled(features::kWebViewMeasureScreenCoverage))
    return;
  for (size_t i = 0; i < base::size(open_web_screen_portion_tracked_duration_);
       i++) {
    int32_t elapsed_seconds =
        open_web_screen_portion_tracked_duration_[i].InSeconds();
    if (elapsed_seconds == 0)
      continue;

    open_web_screen_portion_tracked_duration_[i] -=
        base::Seconds(elapsed_seconds);
    GetOpenWebVisibileScreenPortionHistogram()->AddCount(i, elapsed_seconds);
  }
}

}  // namespace android_webview
