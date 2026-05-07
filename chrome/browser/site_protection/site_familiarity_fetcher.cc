// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_protection/site_familiarity_fetcher.h"

#include "base/functional/callback.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/site_protection/site_familiarity_process_selection_user_data.h"
#include "chrome/browser/site_protection/site_familiarity_utils.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/content/browser/web_ui/web_ui_content_info_singleton.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "extensions/common/constants.h"
#include "url/origin.h"

namespace site_protection {

// Enables skipping site familiarity calculations on navigations to DSE search
// result pages.
BASE_FEATURE(kSkipSiteFamiliarityDeferralForDefaultSearchEngine,
             base::FEATURE_ENABLED_BY_DEFAULT);

namespace {

// The minimum amount of time ago that a site must have been visited in order to
// be considered familiar.
const base::TimeDelta kMinAgeOfInitialVisitForFamiliarity = base::Hours(24);

std::set<GURL>& GetFamiliarUrlsForTesting() {
  static base::NoDestructor<std::set<GURL>> familiar_urls_for_testing;
  return *familiar_urls_for_testing;
}

bool IsUrlFamiliarForTesting(const GURL& url) {
  return GetFamiliarUrlsForTesting().contains(url);  // IN-TEST
}

}  // anonymous namespace

// This class logs site familiarity determinations using CRSBLOG.
// These logs can be viewed by first opening chrome://safe-browsing/#tab-log,
// then navigating to the URL of interest in a separate tab.
SiteFamiliarityFetcher::SiteFamiliarityFetcher(Profile* profile)
    : profile_(profile) {}

SiteFamiliarityFetcher::~SiteFamiliarityFetcher() = default;

void SiteFamiliarityFetcher::SetUrlFamiliarForTesting(const GURL& url) {
  GetFamiliarUrlsForTesting().insert(url);  // IN-TEST
}

void SiteFamiliarityFetcher::ResetFamiliarUrlsForTesting() {
  GetFamiliarUrlsForTesting().clear();  // IN-TEST
}

void SiteFamiliarityFetcher::Start(const GURL& url,
                                   SiteFamiliarityFetcher::Callback callback) {
  CRSBLOG << "SiteFamiliarityFetcher::Start [URL]: " << url;
  fetch_url_ = url;
  callback_ = std::move(callback);
  // Clear state in case there are in-progress requests.
  fetched_history_ = false;
  fetched_sb_list_ = false;
  weak_factory_.InvalidateWeakPtrs();

  if (IsUrlFamiliarForTesting(fetch_url_)) {
    CRSBLOG << "SiteFamiliarityFetcher::Start [URL]: " << fetch_url_
            << " is familiar for testing";
    OnComputedVerdictWithoutFetches(/*is_site_familiar=*/true);
    return;
  }

  if (fetch_url_.scheme() == url::kDataScheme) {
    // Data URLs normally stay in the process of their initiator, and in those
    // cases it won't currently matter how site familiarity is set here, due to
    // https://crbug.com/452135534. Disable v8 optimizers for the remaining
    // cases, such as browser-initiated top-level navigations to data: URLs.
    CRSBLOG << "SiteFamiliarityFetcher::Start [URL]: " << fetch_url_
            << " is data scheme";
    OnComputedVerdictWithoutFetches(/*is_site_familiar=*/false);
    return;
  }

  if (fetch_url_.scheme() == extensions::kExtensionScheme) {
    // chrome-extension:// URLs are not recorded in chrome://history.
    // Given that extensions were either explicitly installed by the user or
    // installed via enterprise policy, consider chrome://extension URLs to be
    // familiar.
    CRSBLOG << "SiteFamiliarityFetcher::Start [URL]: " << fetch_url_
            << " is extension scheme";
    OnComputedVerdictWithoutFetches(/*is_site_familiar=*/true);
    return;
  }

  if (!fetch_url_.SchemeIsHTTPOrHTTPS()) {
    // Disable v8-optimizers for all other web-safe non-http, non-https URLs.
    // Non web-safe schemes such as chrome:// have special handling in
    // ChromeContentBrowserClient::AreV8OptimizationsDisabledForSite().
    // Visits to most web-safe non-http, non-https schemes are not recorded in
    // chrome://history. See CanAddURLToHistory().
    CRSBLOG << "SiteFamiliarityFetcher::Start [URL]: " << fetch_url_
            << " is not HTTP/HTTPS";
    OnComputedVerdictWithoutFetches(/*is_site_familiar=*/false);
    return;
  }

  if (base::FeatureList::IsEnabled(
          kSkipSiteFamiliarityDeferralForDefaultSearchEngine) &&
      IsDefaultSearchEngineUrl(fetch_url_, profile_)) {
    // Assume the default search engine search results are familiar to the user.
    OnComputedVerdictWithoutFetches(/*is_site_familiar=*/true);
    return;
  }

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  history_service->GetLastVisitToOrigin(
      url::Origin::Create(fetch_url_), base::Time(),
      base::Time::Now() - kMinAgeOfInitialVisitForFamiliarity,
      history::VisitQuery404sPolicy::kInclude404s,
      base::BindOnce(&SiteFamiliarityFetcher::OnFetchedHistory,
                     weak_factory_.GetWeakPtr()),
      &task_tracker_);
  StartFetchingSafeBrowsingHighConfidenceAllowlist();

  // The SiteEngagementService provides a synchronous API, so just compute that
  // familiarity component now.
  site_engagement::SiteEngagementService* site_engagement_service =
      site_engagement::SiteEngagementService::Get(profile_);
  has_engagement_score_higher_than_threshold_ =
      site_engagement_service->GetScore(fetch_url_) >=
      kMinSiteEngagementScoreForFamiliarity;
}

void SiteFamiliarityFetcher::
    StartFetchingSafeBrowsingHighConfidenceAllowlist() {
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  if (g_browser_process->safe_browsing_service()) {
    if (auto database_manager =
            g_browser_process->safe_browsing_service()->database_manager()) {
      database_manager->CheckUrlForHighConfidenceAllowlist(
          fetch_url_,
          base::BindOnce(
              &SiteFamiliarityFetcher::OnGotHighConfidenceAllowlistResult,
              weak_factory_.GetWeakPtr()));
      return;
    }
  }
#endif

  OnGotHighConfidenceAllowlistResult(
      /*url_on_safe_browsing_high_confidence_allowlist=*/true,
      /*logging_details=*/std::nullopt);
}

void SiteFamiliarityFetcher::OnComputedVerdictWithoutFetches(
    bool is_site_familiar) {
  OnFetchedHistory(history::HistoryLastVisitResult());
  OnGotHighConfidenceAllowlistResult(
      /*url_on_safe_browsing_high_confidence_allowlist=*/
      is_site_familiar,
      /*logging_details=*/std::nullopt);
}

void SiteFamiliarityFetcher::OnFetchedHistory(
    history::HistoryLastVisitResult last_visit_result) {
  fetched_history_ = true;
  has_record_older_than_threshold_ =
      last_visit_result.success && !last_visit_result.last_visit.is_null() &&
      (last_visit_result.last_visit <
       (base::Time::Now() - kMinAgeOfInitialVisitForFamiliarity));
  RunCallbackIfFinished();
}

void SiteFamiliarityFetcher::OnGotHighConfidenceAllowlistResult(
    bool url_on_safe_browsing_high_confidence_allowlist,
    std::optional<safe_browsing::SafeBrowsingDatabaseManager::
                      HighConfidenceAllowlistCheckLoggingDetails>
        logging_details) {
  fetched_sb_list_ = true;
  is_on_sb_list_ = url_on_safe_browsing_high_confidence_allowlist;
  RunCallbackIfFinished();
}

void SiteFamiliarityFetcher::RunCallbackIfFinished() {
  if (!fetched_history_ || !fetched_sb_list_) {
    // Not finished.
    return;
  }

  if (!callback_) {
    // Callback was already run.
    return;
  }

  Verdict verdict = (has_engagement_score_higher_than_threshold_ ||
                     has_record_older_than_threshold_ || is_on_sb_list_)
                        ? Verdict::kFamiliar
                        : Verdict::kUnfamiliar;
  CRSBLOG << "SiteFamiliarityFetcher decision [URL]: " << fetch_url_
          << " [Verdict]: "
          << (verdict == Verdict::kFamiliar ? "Familiar" : "Unfamiliar")
          << " [Engagement>Threshold]: "
          << has_engagement_score_higher_than_threshold_
          << " [History>Threshold]: " << has_record_older_than_threshold_
          << " [OnSBAllowlist]: " << is_on_sb_list_;
  std::move(callback_).Run(verdict);
}

}  //  namespace site_protection
