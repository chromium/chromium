// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_protection/site_protection_metrics_observer.h"

#include <math.h>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/site_protection/site_familiarity_heuristic_name.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/site_engagement/content/engagement_type.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/page.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace site_protection {
namespace {

// Returns rounded site engagement score to record in UKM. The score is rounded
// to limit granularity.
int RoundSiteEngagementScoreForUkm(double site_engagement_score) {
  return static_cast<int>(floor(site_engagement_score / 10) * 10);
}

}  // anonymous namespace

SiteProtectionMetricsObserver::MetricsData::MetricsData() = default;
SiteProtectionMetricsObserver::MetricsData::~MetricsData() = default;

SiteProtectionMetricsObserver::SiteProtectionMetricsObserver(
    content::WebContents* web_contents)
    : content::WebContentsUserData<SiteProtectionMetricsObserver>(
          *web_contents),
      content::WebContentsObserver(web_contents),
      site_engagement::SiteEngagementObserver(
          site_engagement::SiteEngagementServiceFactory::GetForProfile(
              web_contents->GetBrowserContext())),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      history_service_(HistoryServiceFactory::GetForProfile(
          profile_,
          ServiceAccessType::EXPLICIT_ACCESS)) {}

SiteProtectionMetricsObserver::~SiteProtectionMetricsObserver() = default;

void SiteProtectionMetricsObserver::OnEngagementEvent(
    content::WebContents* web_contents,
    const GURL& url,
    double score,
    double old_score,
    site_engagement::EngagementType type,
    const std::optional<webapps::AppId>& app_id) {
  if (type == site_engagement::EngagementType::kNavigation) {
    got_points_navigation_ = {/*url=*/url,
                              /* score_before_navigation=*/old_score};
  }
}

void SiteProtectionMetricsObserver::PrimaryPageChanged(content::Page& page) {
  std::optional<GotPointsNavigation> got_points_navigation =
      std::move(got_points_navigation_);
  got_points_navigation_ = std::nullopt;

  // HistoryService is null in tests.
  if (!history_service_) {
    return;
  }

  GURL last_committed_url = page.GetMainDocument().GetLastCommittedURL();
  if (!last_committed_url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  // Store any in-progress data in `metrics_data` so that we can still log the
  // matching heuristics even if the page navigates prior to the asynchronous
  // data fetches completing.
  auto metrics_data = std::make_unique<MetricsData>();
  metrics_data->ukm_source_id = page.GetMainDocument().GetPageUkmSourceId();
  metrics_data->last_committed_url = last_committed_url;
  metrics_data->last_committed_origin =
      page.GetMainDocument().GetLastCommittedOrigin();
  metrics_data->data_fetch_start_time = base::Time::Now();

  base::UmaHistogramBoolean(
      "SafeBrowsing.SiteProtection.FamiliarityMetricDataFetchStart", true);

  metrics_data->site_engagement_score =
      (got_points_navigation &&
       metrics_data->last_committed_url == got_points_navigation->url)
          ? got_points_navigation->score_before_navigation
          : site_engagement::SiteEngagementServiceFactory::GetForProfile(
                profile_)
                ->GetScore(metrics_data->last_committed_url);

  if (metrics_data->site_engagement_score >= 50) {
    metrics_data->matched_heuristics.push_back(
        SiteFamiliarityHeuristicName::kSiteEngagementScoreGte50);
  }
  if (metrics_data->site_engagement_score >= 25) {
    metrics_data->matched_heuristics.push_back(
        SiteFamiliarityHeuristicName::kSiteEngagementScoreGte25);
  }
  if (metrics_data->site_engagement_score >= 10) {
    metrics_data->matched_heuristics.push_back(
        SiteFamiliarityHeuristicName::kSiteEngagementScoreGte10);
  }
  if (metrics_data->site_engagement_score >= .01) {
    metrics_data->matched_heuristics.push_back(
        SiteFamiliarityHeuristicName::kSiteEngagementScoreExists);
  }

  url::Origin last_committed_origin = metrics_data->last_committed_origin;
  history_service_->GetLastVisitToOrigin(
      last_committed_origin, base::Time(), base::Time::Now() - base::Hours(4),
      base::BindOnce(
          &SiteProtectionMetricsObserver::OnGotVisitToOriginOlderThan4HoursAgo,
          weak_factory_.GetWeakPtr(), std::move(metrics_data)),
      &task_tracker_);
}

bool SiteProtectionMetricsObserver::HasPendingTasksForTesting() {
  return weak_factory_.HasWeakPtrs();
}

void SiteProtectionMetricsObserver::OnGotVisitToOriginOlderThan4HoursAgo(
    std::unique_ptr<MetricsData> metrics_data,
    history::HistoryLastVisitResult last_visit_result) {
  if (last_visit_result.success && !last_visit_result.last_visit.is_null()) {
    metrics_data->matched_heuristics.push_back(
        SiteFamiliarityHeuristicName::kVisitedMoreThanFourHoursAgo);
    metrics_data->most_strict_matched_history_heuristic =
        SiteFamiliarityHistoryHeuristicName::kVisitedMoreThanFourHoursAgo;
    metrics_data->last_visit_time = last_visit_result.last_visit;

    if (last_visit_result.last_visit < (base::Time::Now() - base::Days(1))) {
      OnGotVisitToOriginOlderThanADayAgo(std::move(metrics_data),
                                         std::move(last_visit_result));
      return;
    }
  }

  url::Origin last_committed_origin = metrics_data->last_committed_origin;
  history_service_->GetLastVisitToOrigin(
      last_committed_origin, base::Time(), base::Time::Now() - base::Days(1),
      base::BindOnce(
          &SiteProtectionMetricsObserver::OnGotVisitToOriginOlderThanADayAgo,
          weak_factory_.GetWeakPtr(), std::move(metrics_data)),
      &task_tracker_);
}

void SiteProtectionMetricsObserver::OnGotVisitToOriginOlderThanADayAgo(
    std::unique_ptr<MetricsData> metrics_data,
    history::HistoryLastVisitResult last_visit_result) {
  if (last_visit_result.success && !last_visit_result.last_visit.is_null()) {
    metrics_data->matched_heuristics.push_back(
        SiteFamiliarityHeuristicName::kVisitedMoreThanADayAgo);
    metrics_data->most_strict_matched_history_heuristic =
        SiteFamiliarityHistoryHeuristicName::kVisitedMoreThanADayAgo;
    metrics_data->last_visit_time = last_visit_result.last_visit;
    OnKnowIfAnyVisitOlderThanADayAgo(std::move(metrics_data),
                                     /*has_visit_older_than_a_day_ago=*/true);
    return;
  }

  history::QueryOptions history_query_options;
  history_query_options.end_time = base::Time::Now() - base::Days(1);
  history_query_options.max_count = 1;
  history_service_->QueryHistory(
      u"", std::move(history_query_options),
      base::BindOnce(&SiteProtectionMetricsObserver::OnGotVisitOlderThanADayAgo,
                     weak_factory_.GetWeakPtr(), std::move(metrics_data)),
      &task_tracker_);
}

void SiteProtectionMetricsObserver::OnGotVisitOlderThanADayAgo(
    std::unique_ptr<MetricsData> metrics_data,
    history::QueryResults query_results) {
  OnKnowIfAnyVisitOlderThanADayAgo(
      std::move(metrics_data),
      /*any_visits_more_than_a_day_ago*/ !query_results.empty());
}

void SiteProtectionMetricsObserver::OnKnowIfAnyVisitOlderThanADayAgo(
    std::unique_ptr<MetricsData> metrics_data,
    bool any_visit_older_than_a_day_ago) {
  if (!any_visit_older_than_a_day_ago) {
    metrics_data->matched_heuristics.push_back(
        SiteFamiliarityHeuristicName::kNoVisitsToAnySiteMoreThanADayAgo);
    metrics_data->most_strict_matched_history_heuristic =
        SiteFamiliarityHistoryHeuristicName::kNoVisitsToAnySiteMoreThanADayAgo;
  }

  if (g_browser_process->safe_browsing_service()) {
    if (auto database_manager =
            g_browser_process->safe_browsing_service()->database_manager()) {
      GURL last_committed_url = metrics_data->last_committed_url;
      database_manager->CheckUrlForHighConfidenceAllowlist(
          last_committed_url,
          base::BindOnce(&SiteProtectionMetricsObserver::
                             OnGotHighConfidenceAllowlistResult,
                         weak_factory_.GetWeakPtr(), std::move(metrics_data)));
      return;
    }
  }

  OnGotHighConfidenceAllowlistResult(
      std::move(metrics_data),
      /*url_on_safe_browsing_high_confidence_allowlist=*/false,
      /*logging_details=*/std::nullopt);
}

void SiteProtectionMetricsObserver::OnGotHighConfidenceAllowlistResult(
    std::unique_ptr<MetricsData> metrics_data,
    bool url_on_safe_browsing_high_confidence_allowlist,
    std::optional<safe_browsing::SafeBrowsingDatabaseManager::
                      HighConfidenceAllowlistCheckLoggingDetails>
        logging_details) {
  if (logging_details && (!logging_details->were_all_stores_available ||
                          logging_details->was_allowlist_size_too_small)) {
    metrics_data->matched_heuristics.push_back(
        SiteFamiliarityHeuristicName::kGlobalAllowlistNotReady);
    url_on_safe_browsing_high_confidence_allowlist = false;
  }
  if (url_on_safe_browsing_high_confidence_allowlist) {
    metrics_data->url_on_safe_browsing_high_confidence_allowlist = true;
    metrics_data->matched_heuristics.push_back(
        SiteFamiliarityHeuristicName::kGlobalAllowlistMatch);
  }

  // Guess as to whether the site was previously categorized as unfamiliar.
  //
  // For the purpose of
  // SiteFamiliarityHeuristicName::kFamiliarLikelyPreviouslyUnfamiliar an
  // unfamiliar site is a site which is:
  // - Not on the safe browsing high confidence allowlist
  // AND
  // - Wasn't visited more than 24 hours ago
  // AND
  // - Wasn't visited with a fresh profile which doesn't have any history
  //   older than 24 hours.
  //
  // Assume that high confidence allowlist is stable and that if origin is
  // currently on high confidence allowlist that it would have been previously
  // on high confidence allowlist. Ignore site engagement score. Ignoring
  // site engagement score is ok because the site engagement score is capped
  // for site engagement all on the same day.
  std::optional<base::Time> last_visit_time = metrics_data->last_visit_time;
  if (!url_on_safe_browsing_high_confidence_allowlist && last_visit_time &&
      *last_visit_time < (base::Time::Now() - base::Days(1))) {
    url::Origin last_committed_origin = metrics_data->last_committed_origin;
    history_service_->GetLastVisitToOrigin(
        last_committed_origin, base::Time(), *last_visit_time - base::Days(1),
        base::BindOnce(&SiteProtectionMetricsObserver::
                           OnGotVisitToOriginOlderThanADayPriorToPreviousVisit,
                       weak_factory_.GetWeakPtr(), std::move(metrics_data)),
        &task_tracker_);
    return;
  }

  LogMetrics(std::move(metrics_data));
}

void SiteProtectionMetricsObserver::
    OnGotVisitToOriginOlderThanADayPriorToPreviousVisit(
        std::unique_ptr<MetricsData> metrics_data,
        history::HistoryLastVisitResult last_visit_result) {
  if (!last_visit_result.success || last_visit_result.last_visit.is_null()) {
    // Check whether
    // SiteFamiliarityHistoryHeuristicName::kNoVisitsToAnySiteMoreThanADayAgo
    // heuristic would have matched the previous visit.
    history::QueryOptions history_query_options;
    history_query_options.end_time =
        *metrics_data->last_visit_time - base::Days(1);
    history_query_options.max_count = 1;
    history_service_->QueryHistory(
        u"", std::move(history_query_options),
        base::BindOnce(&SiteProtectionMetricsObserver::
                           OnGotVisitOlderThanADayPriorToPreviousVisit,
                       weak_factory_.GetWeakPtr(), std::move(metrics_data)),
        &task_tracker_);
    return;
  }

  OnKnowIfSiteWasLikelyPreviouslyFamiliar(
      std::move(metrics_data),
      /*was_site_likely_previously_familiar=*/true);
  return;
}

void SiteProtectionMetricsObserver::OnGotVisitOlderThanADayPriorToPreviousVisit(
    std::unique_ptr<MetricsData> metrics_data,
    history::QueryResults query_results) {
  OnKnowIfSiteWasLikelyPreviouslyFamiliar(
      std::move(metrics_data),
      /*was_site_likely_previously_familiar=*/query_results.empty());
}

void SiteProtectionMetricsObserver::OnKnowIfSiteWasLikelyPreviouslyFamiliar(
    std::unique_ptr<MetricsData> metrics_data,
    bool was_site_likely_previously_familiar) {
  if (!was_site_likely_previously_familiar) {
    metrics_data->matched_heuristics.push_back(
        SiteFamiliarityHeuristicName::kFamiliarLikelyPreviouslyUnfamiliar);
    metrics_data->most_strict_matched_history_heuristic =
        SiteFamiliarityHistoryHeuristicName::
            kVisitedMoreThanADayAgoPreviouslyUnfamiliar;
  }

  LogMetrics(std::move(metrics_data));
}

void SiteProtectionMetricsObserver::LogMetrics(
    std::unique_ptr<MetricsData> metrics_data) {
  bool no_heuristics_match = metrics_data->matched_heuristics.empty();
  if (no_heuristics_match) {
    metrics_data->matched_heuristics.push_back(
        SiteFamiliarityHeuristicName::kNoHeuristicMatch);
  }

  base::UmaHistogramTimes(
      profile_->IsOffTheRecord()
          ? "SafeBrowsing.SiteProtection.FamiliarityMetricDataFetchDuration."
            "OffTheRecord"
          : "SafeBrowsing.SiteProtection.FamiliarityMetricDataFetchDuration",
      (base::Time::Now() - metrics_data->data_fetch_start_time));

  for (SiteFamiliarityHeuristicName heuristic :
       metrics_data->matched_heuristics) {
    base::UmaHistogramEnumeration(
        profile_->IsOffTheRecord()
            ? "SafeBrowsing.SiteProtection.FamiliarityHeuristic.OffTheRecord"
            : "SafeBrowsing.SiteProtection.FamiliarityHeuristic",
        heuristic);
  }

  ukm::builders::SiteFamiliarityHeuristicResult(metrics_data->ukm_source_id)
      .SetAnyHeuristicsMatch(!no_heuristics_match)
      .SetOnHighConfidenceAllowlist(
          metrics_data->url_on_safe_browsing_high_confidence_allowlist)
      .SetSiteEngagementScore(
          RoundSiteEngagementScoreForUkm(metrics_data->site_engagement_score))
      .SetSiteFamiliarityHistoryHeuristic(
          static_cast<int>(metrics_data->most_strict_matched_history_heuristic))
      .Record(ukm::UkmRecorder::Get());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SiteProtectionMetricsObserver);

}  // namespace site_protection
