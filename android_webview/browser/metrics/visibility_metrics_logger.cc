// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/visibility_metrics_logger.h"

#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using base::NoDestructor;
using content::BrowserThread;

namespace android_webview {

// Have bypassed the usual macros here because they do not support a
// means to increment counters by more than 1 per call.
base::HistogramBase*
VisibilityMetricsLogger::CreateHistogramForDurationTracking(const char* name,
                                                            int max_value) {
  return base::Histogram::FactoryGet(
      name, 1, max_value + 1, max_value + 2,
      base::HistogramBase::kUmaTargetedHistogramFlag);
}

base::HistogramBase* VisibilityMetricsLogger::GetGlobalVisibilityHistogram() {
  static NoDestructor<base::HistogramBase*> histogram(
      CreateHistogramForDurationTracking(
          "Android.WebView.Visibility.Global",
          static_cast<int>(VisibilityMetricsLogger::Visibility::kMaxValue)));
  return *histogram;
}

base::HistogramBase*
VisibilityMetricsLogger::GetPerWebViewVisibilityHistogram() {
  static NoDestructor<base::HistogramBase*> histogram(
      CreateHistogramForDurationTracking(
          "Android.WebView.Visibility.PerWebView",
          static_cast<int>(VisibilityMetricsLogger::Visibility::kMaxValue)));
  return *histogram;
}

base::HistogramBase*
VisibilityMetricsLogger::GetGlobalOpenWebVisibilityHistogram() {
  static NoDestructor<base::HistogramBase*> histogram(
      CreateHistogramForDurationTracking(
          "Android.WebView.WebViewOpenWebVisible.Global",
          static_cast<int>(
              VisibilityMetricsLogger::WebViewOpenWebVisibility::kMaxValue)));
  return *histogram;
}

base::HistogramBase*
VisibilityMetricsLogger::GetPerWebViewOpenWebVisibilityHistogram() {
  static NoDestructor<base::HistogramBase*> histogram(
      CreateHistogramForDurationTracking(
          "Android.WebView.WebViewOpenWebVisible.PerWebView",
          static_cast<int>(
              VisibilityMetricsLogger::WebViewOpenWebVisibility::kMaxValue)));
  return *histogram;
}

VisibilityMetricsLogger::VisibilityMetricsLogger() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  last_update_time_ = base::TimeTicks::Now();
}

VisibilityMetricsLogger::~VisibilityMetricsLogger() = default;

void VisibilityMetricsLogger::AddClient(Client* client) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(client_visibility_.find(client) == client_visibility_.end());

  UpdateDurations(base::TimeTicks::Now());

  client_visibility_[client] = VisibilityInfo();
  ProcessClientUpdate(client, client->GetVisibilityInfo());
}

void VisibilityMetricsLogger::RemoveClient(Client* client) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(client_visibility_.find(client) != client_visibility_.end());

  UpdateDurations(base::TimeTicks::Now());

  ProcessClientUpdate(client, VisibilityInfo());
  client_visibility_.erase(client);
}

void VisibilityMetricsLogger::ClientVisibilityChanged(Client* client) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(client_visibility_.find(client) != client_visibility_.end());

  UpdateDurations(base::TimeTicks::Now());

  ProcessClientUpdate(client, client->GetVisibilityInfo());
}

void VisibilityMetricsLogger::UpdateDurations(base::TimeTicks update_time) {
  base::TimeDelta delta = update_time - last_update_time_;
  if (visible_client_count_ > 0) {
    visible_duration_tracker_.any_webview_tracked_duration_ += delta;
  } else {
    visible_duration_tracker_.no_webview_tracked_duration_ += delta;
  }

  if (visible_webcontent_client_count_ > 0) {
    webcontent_visible_tracker_.any_webview_tracked_duration_ += delta;
  } else {
    webcontent_visible_tracker_.no_webview_tracked_duration_ += delta;
  }

  visible_duration_tracker_.per_webview_duration_ +=
      delta * visible_client_count_;
  visible_duration_tracker_.per_webview_untracked_duration_ +=
      delta * (client_visibility_.size() - visible_client_count_);

  webcontent_visible_tracker_.per_webview_duration_ +=
      delta * visible_webcontent_client_count_;
  webcontent_visible_tracker_.per_webview_untracked_duration_ +=
      delta * (client_visibility_.size() - visible_webcontent_client_count_);

  last_update_time_ = update_time;
}

bool VisibilityMetricsLogger::IsVisible(const VisibilityInfo& info) {
  return info.view_attached && info.view_visible && info.window_visible;
}

bool VisibilityMetricsLogger::IsDisplayingOpenWebContent(
    const VisibilityInfo& info) {
  return info.scheme_http_or_https;
}

void VisibilityMetricsLogger::ProcessClientUpdate(Client* client,
                                                  const VisibilityInfo& info) {
  VisibilityInfo curr_info = client_visibility_[client];
  bool was_visible = IsVisible(curr_info);
  bool is_visible = IsVisible(info);
  bool was_visible_web =
      IsVisible(curr_info) && IsDisplayingOpenWebContent(curr_info);
  bool is_visible_web = IsVisible(info) && IsDisplayingOpenWebContent(info);
  client_visibility_[client] = info;
  DCHECK(!was_visible || visible_client_count_ > 0);

  bool any_client_was_visible = visible_client_count_ > 0;

  if (!was_visible && is_visible) {
    ++visible_client_count_;
  } else if (was_visible && !is_visible) {
    --visible_client_count_;
  }

  if (!was_visible_web && is_visible_web) {
    ++visible_webcontent_client_count_;
  } else if (was_visible_web && !is_visible_web) {
    --visible_webcontent_client_count_;
  }

  bool any_client_is_visible = visible_client_count_ > 0;
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
  UpdateDurations(base::TimeTicks::Now());
  RecordVisibilityMetrics();
  RecordOpenWebDisplayMetrics();
}

void VisibilityMetricsLogger::RecordVisibilityMetrics() {
  int32_t any_webview_visible_seconds;
  int32_t no_webview_visible_seconds;
  int32_t total_webview_visible_seconds;
  int32_t total_no_webview_visible_seconds;

  any_webview_visible_seconds =
      visible_duration_tracker_.any_webview_tracked_duration_.InSeconds();
  visible_duration_tracker_.any_webview_tracked_duration_ -=
      base::TimeDelta::FromSeconds(any_webview_visible_seconds);
  no_webview_visible_seconds =
      visible_duration_tracker_.no_webview_tracked_duration_.InSeconds();
  visible_duration_tracker_.no_webview_tracked_duration_ -=
      base::TimeDelta::FromSeconds(no_webview_visible_seconds);

  total_webview_visible_seconds =
      visible_duration_tracker_.per_webview_duration_.InSeconds();
  visible_duration_tracker_.per_webview_duration_ -=
      base::TimeDelta::FromSeconds(total_webview_visible_seconds);
  total_no_webview_visible_seconds =
      visible_duration_tracker_.per_webview_untracked_duration_.InSeconds();
  visible_duration_tracker_.per_webview_untracked_duration_ -=
      base::TimeDelta::FromSeconds(total_no_webview_visible_seconds);

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

void VisibilityMetricsLogger::RecordOpenWebDisplayMetrics() {
  int32_t any_webcontent_visible_seconds;
  int32_t no_webcontent_visible_seconds;
  int32_t total_webcontent_isible_seconds;
  int32_t total_not_webcontent_or_not_visible_seconds;

  any_webcontent_visible_seconds =
      webcontent_visible_tracker_.any_webview_tracked_duration_.InSeconds();
  webcontent_visible_tracker_.any_webview_tracked_duration_ -=
      base::TimeDelta::FromSeconds(any_webcontent_visible_seconds);
  no_webcontent_visible_seconds =
      webcontent_visible_tracker_.no_webview_tracked_duration_.InSeconds();
  webcontent_visible_tracker_.no_webview_tracked_duration_ -=
      base::TimeDelta::FromSeconds(no_webcontent_visible_seconds);

  total_webcontent_isible_seconds =
      webcontent_visible_tracker_.per_webview_duration_.InSeconds();
  webcontent_visible_tracker_.per_webview_duration_ -=
      base::TimeDelta::FromSeconds(total_webcontent_isible_seconds);
  total_not_webcontent_or_not_visible_seconds =
      webcontent_visible_tracker_.per_webview_untracked_duration_.InSeconds();
  webcontent_visible_tracker_.per_webview_untracked_duration_ -=
      base::TimeDelta::FromSeconds(total_not_webcontent_or_not_visible_seconds);

  if (any_webcontent_visible_seconds) {
    GetGlobalOpenWebVisibilityHistogram()->AddCount(
        static_cast<int>(WebViewOpenWebVisibility::kDisplayOpenWebContent),
        any_webcontent_visible_seconds);
  }
  if (no_webcontent_visible_seconds) {
    GetGlobalOpenWebVisibilityHistogram()->AddCount(
        static_cast<int>(WebViewOpenWebVisibility::kNotDisplayOpenWebContent),
        no_webcontent_visible_seconds);
  }

  if (total_webcontent_isible_seconds) {
    GetPerWebViewOpenWebVisibilityHistogram()->AddCount(
        static_cast<int>(WebViewOpenWebVisibility::kDisplayOpenWebContent),
        total_webcontent_isible_seconds);
  }
  if (total_not_webcontent_or_not_visible_seconds) {
    GetPerWebViewOpenWebVisibilityHistogram()->AddCount(
        static_cast<int>(WebViewOpenWebVisibility::kNotDisplayOpenWebContent),
        total_not_webcontent_or_not_visible_seconds);
  }
}

}  // namespace android_webview