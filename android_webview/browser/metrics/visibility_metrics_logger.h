// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_METRICS_VISIBILITY_METRICS_LOGGER_H_
#define ANDROID_WEBVIEW_BROWSER_METRICS_VISIBILITY_METRICS_LOGGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"

namespace android_webview {

// Records how much of the screen is covered by WebViews. This helps us
// determine what WebView is being used for.
//
// Lifetime: Singleton
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
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Visibility {
    kVisible = 0,
    kNotVisible = 1,
    kMaxValue = kNotVisible
  };

  class Client {
   public:
    virtual VisibilityInfo GetVisibilityInfo() = 0;
  };

  enum class ClientAction {
    kAdded = 0,
    kRemoved = 1,
    kVisibilityChanged = 2,
    kMaxValue = kVisibilityChanged,
  };

  VisibilityMetricsLogger();
  virtual ~VisibilityMetricsLogger();

  VisibilityMetricsLogger(const VisibilityMetricsLogger&) = delete;
  VisibilityMetricsLogger& operator=(const VisibilityMetricsLogger&) = delete;

  void AddClient(Client* client);
  void RemoveClient(Client* client);
  void ClientVisibilityChanged(Client* client);
  void UpdateScreenCoverage(int global_percentage,
                            const std::vector<Scheme>& schemes,
                            const std::vector<int>& scheme_percentages);

  void RecordMetrics();

  // Set a callback that is executed when global visibility changes, i.e. when:
  //  - false => true: no client was visible and one becomes visible.
  //  - true => false: >=1 clients were visible and all became hidden.
  using OnVisibilityChangedCallback =
      base::RepeatingCallback<void(bool /*visible*/)>;
  void SetOnVisibilityChangedCallback(OnVisibilityChangedCallback);

 private:
  void UpdateDurations();
  void ProcessClientUpdate(Client* client,
                           const VisibilityInfo& info,
                           ClientAction action);
  void RecordVisibilityMetrics();
  void RecordVisibleSchemeMetrics();
  void RecordScreenCoverageMetrics();

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

  // The screen coverage percentage for all visible AwContents merged together.
  int global_coverage_percentage_ = 0;

  // The durations by screen coverage percentage for all visible AwContents
  // merged together.
  base::TimeDelta global_coverage_percentage_durations_[101] = {};

  // The currently visible schemes and their screen coverage percentages. A
  // scheme can occur more than once at a time so this uses a multimap.
  std::multimap<Scheme, int> schemes_to_coverage_percentages_;

  // The durations by screen coverage percentage and visible scheme.
  std::map<Scheme, std::map<int, base::TimeDelta>>
      schemes_to_percentages_to_durations_;

  OnVisibilityChangedCallback on_visibility_changed_callback_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_METRICS_VISIBILITY_METRICS_LOGGER_H_
