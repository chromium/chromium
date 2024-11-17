// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SITE_PROTECTION_SITE_PROTECTION_METRICS_OBSERVER_H_
#define CHROME_BROWSER_SITE_PROTECTION_SITE_PROTECTION_METRICS_OBSERVER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/clock.h"
#include "chrome/browser/site_protection/site_familiarity_heuristic_name.h"
#include "components/history/core/browser/history_types.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/site_engagement/content/engagement_type.h"
#include "components/site_engagement/content/site_engagement_observer.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace history {
struct HistoryLastVisitResult;
class HistoryService;
}  // namespace history

namespace site_protection {

// A class to log metrics related to different heuristics for assessing the
// site's familiarity to the user. These metrics will be used to create
// heuristics for whether Chromium should enable extra protections.
class SiteProtectionMetricsObserver
    : public content::WebContentsUserData<SiteProtectionMetricsObserver>,
      public content::WebContentsObserver,
      public site_engagement::SiteEngagementObserver {
 public:
  explicit SiteProtectionMetricsObserver(content::WebContents* web_contents);
  ~SiteProtectionMetricsObserver() override;

  SiteProtectionMetricsObserver(const SiteProtectionMetricsObserver&) = delete;
  SiteProtectionMetricsObserver& operator=(
      const SiteProtectionMetricsObserver&) = delete;

  // site_engagement::SiteEngagementObserver:
  void OnEngagementEvent(content::WebContents* web_contents,
                         const GURL& url,
                         double score,
                         double old_score,
                         site_engagement::EngagementType type,
                         const std::optional<webapps::AppId>& app_id) override;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

  // Returns whether there are any pending asynchronous tasks.
  bool HasPendingTasksForTesting();

 private:
  friend class content::WebContentsUserData<SiteProtectionMetricsObserver>;

  struct GotPointsNavigation {
    GURL url;
    double score_before_navigation = 0;
  };

  struct MetricsData {
    MetricsData();
    ~MetricsData();

    ukm::SourceId ukm_source_id = ukm::kInvalidSourceId;
    double site_engagement_score = 0;
    bool url_on_safe_browsing_high_confidence_allowlist = false;
    GURL last_committed_url;
    url::Origin last_committed_origin;
    base::Time data_fetch_start_time;
    std::optional<base::Time> last_visit_time;
    std::vector<SiteFamiliarityHeuristicName> matched_heuristics;
    SiteFamiliarityHistoryHeuristicName most_strict_matched_history_heuristic =
        SiteFamiliarityHistoryHeuristicName::kNoHeuristicMatch;
  };

  // Called with the most recent history visit to the origin in `metrics_data`
  // which occurred more than 4 hours ago.
  void OnGotVisitToOriginOlderThan4HoursAgo(
      std::unique_ptr<MetricsData> metrics_data,
      history::HistoryLastVisitResult last_visit_result);

  // Called with the most recent history visit to the origin in `metrics_data`
  // which occurred more than a day ago.
  void OnGotVisitToOriginOlderThanADayAgo(
      std::unique_ptr<MetricsData> metrics_data,
      history::HistoryLastVisitResult last_visit_result);

  // Called with the most recent history visit to any site which occurred more
  // than a day ago.
  void OnGotVisitOlderThanADayAgo(std::unique_ptr<MetricsData> metrics_data,
                                  history::QueryResults query_results);

  // Called with whether there was a history visit to any site more than a day
  // ago.
  void OnKnowIfAnyVisitOlderThanADayAgo(
      std::unique_ptr<MetricsData> metrics_data,
      bool has_visit_older_than_a_day_ago);

  // Called with whether the site is on the high confidence allowlist.
  void OnGotHighConfidenceAllowlistResult(
      std::unique_ptr<MetricsData> metrics_data,
      bool url_on_safe_browsing_high_confidence_allowlist,
      std::optional<safe_browsing::SafeBrowsingDatabaseManager::
                        HighConfidenceAllowlistCheckLoggingDetails>
          logging_details);

  // Called with the history visit to the origin in `metrics_data` which
  // occurred more than a day prior to the most recent visit to the origin.
  void OnGotVisitToOriginOlderThanADayPriorToPreviousVisit(
      std::unique_ptr<MetricsData> metrics_data,
      history::HistoryLastVisitResult last_visit_result);

  // Called with the history visit to any site which occurred more than a day
  // prior to the visit to the origin in `metrics_data`.
  void OnGotVisitOlderThanADayPriorToPreviousVisit(
      std::unique_ptr<MetricsData> metrics_data,
      history::QueryResults query_results);

  // Called with whether there is a history visit to any site more than a day
  // prior to the visit to the origin in `metrics_data`.
  void OnKnowIfSiteWasLikelyPreviouslyFamiliar(
      std::unique_ptr<MetricsData> metrics_data,
      bool was_site_likely_previously_familiar);

  void LogMetrics(std::unique_ptr<MetricsData> metrics_data);

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  raw_ptr<Profile> profile_;
  raw_ptr<history::HistoryService> history_service_;

  // Manages HistoryService tasks.
  base::CancelableTaskTracker task_tracker_;

  // Data about the last navigation-site-engagement event.
  std::optional<GotPointsNavigation> got_points_navigation_;

  base::WeakPtrFactory<SiteProtectionMetricsObserver> weak_factory_{this};
};

}  // namespace site_protection
#endif  // CHROME_BROWSER_SITE_PROTECTION_SITE_PROTECTION_METRICS_OBSERVER_H_
