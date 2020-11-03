// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_METRICS_VISIBILITY_METRICS_LOGGER_H_
#define ANDROID_WEBVIEW_BROWSER_METRICS_VISIBILITY_METRICS_LOGGER_H_

#include <map>

#include "base/callback.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"

namespace base {

class HistogramBase;

}  // namespace base

namespace android_webview {

class VisibilityMetricsLogger {
 public:
  struct VisibilityInfo {
    bool view_attached = false;
    bool view_visible = false;
    bool window_visible = false;
    bool scheme_http_or_https = false;
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Visibility {
    kVisible = 0,
    kNotVisible = 1,
    kMaxValue = kNotVisible
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class WebViewOpenWebVisibility {
    kDisplayOpenWebContent = 0,
    kNotDisplayOpenWebContent = 1,
    kMaxValue = kNotDisplayOpenWebContent
  };

  class Client {
   public:
    virtual VisibilityInfo GetVisibilityInfo() = 0;
  };

  VisibilityMetricsLogger();
  virtual ~VisibilityMetricsLogger();

  VisibilityMetricsLogger(const VisibilityMetricsLogger&) = delete;
  VisibilityMetricsLogger& operator=(const VisibilityMetricsLogger&) = delete;

  void AddClient(Client* client);
  void RemoveClient(Client* client);
  void ClientVisibilityChanged(Client* client);

  void RecordMetrics();

  // Set a callback that is executed when global visibility changes, i.e. when:
  //  - false => true: no client was visible and one becomes visible.
  //  - true => false: >=1 clients were visible and all became hidden.
  using OnVisibilityChangedCallback =
      base::RepeatingCallback<void(bool /*visible*/)>;
  void SetOnVisibilityChangedCallback(OnVisibilityChangedCallback);

 private:
  static base::HistogramBase* GetGlobalVisibilityHistogram();
  static base::HistogramBase* GetPerWebViewVisibilityHistogram();
  static base::HistogramBase* GetGlobalOpenWebVisibilityHistogram();
  static base::HistogramBase* GetPerWebViewOpenWebVisibilityHistogram();
  static base::HistogramBase* CreateHistogramForDurationTracking(
      const char* name,
      int max_value);

  void UpdateDurations(base::TimeTicks update_time);
  void ProcessClientUpdate(Client* client, const VisibilityInfo& info);
  bool IsVisible(const VisibilityInfo& info);
  bool IsDisplayingOpenWebContent(const VisibilityInfo& info);
  void RecordVisibilityMetrics();
  void RecordOpenWebDisplayMetrics();

  // Counter for visible clients
  size_t visible_client_count_ = 0;
  // Counter for visible web clients
  size_t visible_webcontent_client_count_ = 0;

  struct WebViewDurationTracker {
    // Duration any WebView meets the tracking criteria
    base::TimeDelta any_webview_tracked_duration_ =
        base::TimeDelta::FromSeconds(0);
    // Duration no WebViews meet the tracking criteria
    base::TimeDelta no_webview_tracked_duration_ =
        base::TimeDelta::FromSeconds(0);
    // Total duration that WebViews meet the tracking criteria (i.e. if
    // 2x WebViews meet the criteria for 1 second then increment by 2 sections)
    base::TimeDelta per_webview_duration_ = base::TimeDelta::FromSeconds(0);
    // Total duration that WebViews exist but do not meet the tracking criteria
    base::TimeDelta per_webview_untracked_duration_ =
        base::TimeDelta::FromSeconds(0);
  };

  WebViewDurationTracker visible_duration_tracker_;
  WebViewDurationTracker webcontent_visible_tracker_;

  base::TimeTicks last_update_time_;
  std::map<Client*, VisibilityInfo> client_visibility_;

  OnVisibilityChangedCallback on_visibility_changed_callback_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_METRICS_VISIBILITY_METRICS_LOGGER_H_
