// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_METRICS_VISIBILITY_METRICS_LOGGER_H_
#define ANDROID_WEBVIEW_BROWSER_METRICS_VISIBILITY_METRICS_LOGGER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/time/time.h"

namespace android_webview {

class VisibilityMetricsLogger {
 public:
  // These values are persisted to logs and must match the WebViewUrlScheme enum
  // defined in enums.xml. Entries should not be renumbered and numeric values
  // should never be reused.
  enum class Scheme {
    kEmpty = 0,
    kUnknown = 1,
    kHttp = 2,
    kHttps = 3,
    kFile = 4,
    kFtp = 5,
    kData = 6,
    kJavaScript = 7,
    kAbout = 8,
    kChrome = 9,
    kBlob = 10,
    kContent = 11,
    kIntent = 12,
    kMaxValue = kIntent,
  };

  static Scheme SchemeStringToEnum(const std::string& scheme);

  struct VisibilityInfo {
    bool view_attached = false;
    bool view_visible = false;
    bool window_visible = false;
    Scheme scheme = Scheme::kEmpty;

    bool IsVisible() const;
    bool IsDisplayingOpenWebContent() const;
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
  enum class WebViewOpenWebScreenPortion {
    kZeroPercent = 0,
    kTenPercent = 1,
    kTwentyPercent = 2,
    kThirtyPercent = 3,
    kFortyPercent = 4,
    kFiftyPercent = 5,
    kSixtyPercent = 6,
    kSeventyPercent = 7,
    kEightyPercent = 8,
    kNinetyPercent = 9,
    kOneHundredPercent = 10,
    kExactlyZeroPercent = 11,
    kMaxValue = kExactlyZeroPercent,
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
  void UpdateOpenWebScreenArea(int pixels, int percentage);

  void RecordMetrics();

  // Set a callback that is executed when global visibility changes, i.e. when:
  //  - false => true: no client was visible and one becomes visible.
  //  - true => false: >=1 clients were visible and all became hidden.
  using OnVisibilityChangedCallback =
      base::RepeatingCallback<void(bool /*visible*/)>;
  void SetOnVisibilityChangedCallback(OnVisibilityChangedCallback);

 private:
  void UpdateDurations();
  void ProcessClientUpdate(Client* client, const VisibilityInfo& info);
  void RecordVisibilityMetrics();
  void RecordVisibleSchemeMetrics();
  void RecordScreenPortionMetrics();

  // Counts the number of visible clients.
  size_t all_clients_visible_count_ = 0;
  // Counts the number of visible clients per scheme.
  size_t per_scheme_visible_counts_[static_cast<size_t>(Scheme::kMaxValue) +
                                    1] = {};

  struct WebViewDurationTracker {
    // Duration any WebView meets the tracking criteria
    base::TimeDelta any_webview_tracked_duration_ = base::Seconds(0);
    // Duration no WebViews meet the tracking criteria
    base::TimeDelta no_webview_tracked_duration_ = base::Seconds(0);
    // Total duration that WebViews meet the tracking criteria (i.e. if
    // 2x WebViews meet the criteria for 1 second then increment by 2 seconds)
    base::TimeDelta per_webview_duration_ = base::Seconds(0);
    // Total duration that WebViews exist but do not meet the tracking criteria
    base::TimeDelta per_webview_untracked_duration_ = base::Seconds(0);
  };

  WebViewDurationTracker all_clients_tracker_;
  WebViewDurationTracker
      per_scheme_trackers_[static_cast<size_t>(Scheme::kMaxValue) + 1] = {};

  base::TimeTicks last_update_time_;
  std::map<Client*, VisibilityInfo> client_visibility_;

  WebViewOpenWebScreenPortion current_open_web_screen_portion_ =
      WebViewOpenWebScreenPortion::kZeroPercent;
  base::TimeDelta open_web_screen_portion_tracked_duration_
      [static_cast<size_t>(WebViewOpenWebScreenPortion::kMaxValue) + 1] = {};

  OnVisibilityChangedCallback on_visibility_changed_callback_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_METRICS_VISIBILITY_METRICS_LOGGER_H_
