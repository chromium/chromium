// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_METRICS_LOGGER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_METRICS_LOGGER_H_

#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/resource_coordinator/tab_metrics_event.pb.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/base/page_transition_types.h"

class Browser;

namespace base {
class TimeDelta;
}  // namespace base

namespace content {
class WebContents;
}  // namespace content

namespace tab_ranker {
struct TabFeatures;
struct WindowFeatures;
}  // namespace tab_ranker

// Logs metrics for a tab and its WebContents when requested.
// Must be used on the UI thread.
class TabMetricsLogger {
 public:
  // The state of the page loaded in a tab's main frame, starting since the last
  // navigation.
  struct PageMetrics {
    // Number of key events.
    int key_event_count = 0;
    // Number of mouse events.
    int mouse_event_count = 0;
    // Number of touch events.
    int touch_event_count = 0;
    // Number of times this tab has been reactivated.
    int num_reactivations = 0;
    // Source of the last committed navigation.
    ui::PageTransition page_transition = ui::PAGE_TRANSITION_FIRST;
  };

  // A struct that contains metrics to be logged in ForegroundedOrClosed event.
  struct ForegroundedOrClosedMetrics {
    bool is_foregrounded = false;
    bool is_discarded = false;
    int64_t time_from_backgrounded = 0;
    int64_t label_id = 0;
  };

  TabMetricsLogger();
  ~TabMetricsLogger();

  // Logs metrics for the tab with the given |tab_features|. Does nothing if
  // |ukm_source_id| is zero.
  void LogTabMetrics(ukm::SourceId ukm_source_id,
                     const tab_ranker::TabFeatures& tab_features,
                     content::WebContents* web_contents,
                     int64_t label_id);

  // Logs TabManager.Background.ForegroundedOrClosed UKM for a tab that was
  // shown or closed after being inactive.
  void LogForegroundedOrClosedMetrics(
      ukm::SourceId ukm_source_id,
      const ForegroundedOrClosedMetrics& metrics);

  // Logs TabManager.TabLifetime UKM for a closed tab.
  void LogTabLifetime(ukm::SourceId ukm_source_id,
                      base::TimeDelta time_since_navigation);

  // Returns the site engagement score for the WebContents, rounded down to 10s
  // to limit granularity. Returns -1 if site engagement service is disabled.
  static int GetSiteEngagementScore(content::WebContents* web_contents);

  // Creates TabFeatures for logging or scoring tabs.
  // A common function for populating these features ensures that the same
  // values are used for logging training examples to UKM and for locally
  // scoring tabs.
  static base::Optional<tab_ranker::TabFeatures> GetTabFeatures(
      const PageMetrics& page_metrics,
      content::WebContents* web_contents);

  // Returns a populated WindowFeatures for the browser.
  static tab_ranker::WindowFeatures CreateWindowFeatures(
      const Browser* browser);

  void set_query_id(int64_t query_id) { query_id_ = query_id; }

 private:
  // query_id should be set whenever a new tabRanker query happens, so all logs
  // that happened within the same query will have same query_id_.
  int64_t query_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TabMetricsLogger);
};

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_METRICS_LOGGER_H_
