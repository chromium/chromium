// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SITE_PROTECTION_SITE_FAMILIARITY_FETCHER_H_
#define CHROME_BROWSER_SITE_PROTECTION_SITE_FAMILIARITY_FETCHER_H_

#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/history/core/browser/history_types.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "content/public/browser/process_selection_deferring_condition.h"
#include "url/gurl.h"
#include "url/origin.h"

class Profile;

namespace site_protection {

BASE_DECLARE_FEATURE(kSkipSiteFamiliarityDeferralForDefaultSearchEngine);

// The minimum site engagement score that a site must have in order to be
// considered familiar.
inline constexpr int kMinSiteEngagementScoreForFamiliarity = 10;

// Calculates the site familiarity based on information from the
// SiteEngagementService, chrome://history and the
// safe-browsing-high-confidence-allowlist.
//
// To optimize performance, the fetcher will return a "familiar" verdict as
// soon as any component of the familiarity heuristic (Site Engagement,
// History, or Safe Browsing Allowlist) determines the site is familiar,
// returning early and cancelling any pending asynchronous lookups.
class SiteFamiliarityFetcher {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(SiteFamiliarity)
  enum class Verdict {
    kFamiliar = 0,
    kUnfamiliar = 1,
    kMaxValue = kUnfamiliar,
  };
  // LINT.ThenChange(//tools/metrics/histograms/enums.xml:SiteFamiliarity)

  typedef base::OnceCallback<void(Verdict)> Callback;

  explicit SiteFamiliarityFetcher(Profile* profile);
  ~SiteFamiliarityFetcher();

  // Starts fetching data to determine whether passed-in URL is familiar.
  // Cancels any in-progress fetches.
  void Start(const GURL& url, Callback callback);

  // The GURL for which site familiarity is being computed.
  const GURL& fetch_url() { return fetch_url_; }

  // Set a URL as familiar for testing purposes.
  static void SetUrlFamiliarForTesting(const GURL& url);

  // Clears the set of URLs explicitly marked as familiar for testing.
  // Should be called during test TearDown to prevent cross-test pollution.
  // TODO: crbug.com/493200120 - Rewrite this using an RAII scoped helper.
  static void ResetFamiliarUrlsForTesting();

 private:
  // Initiates safe-browsing-high-confidence-allowlist request.
  void StartFetchingSafeBrowsingHighConfidenceAllowlist();

  // Called when the site-familiarity verdict has been computed.
  // If `log_verdict` is true, logs the detailed verdict using CRSBLOG.
  void OnComputedVerdict(Verdict verdict, bool log_verdict = false);

  // Called when the history request completes.
  void OnFetchedHistory(history::HistoryLastVisitResult last_visit_result);

  // Called when safe-browsing-high-confidence-allowlist has been fetched.
  void OnGotHighConfidenceAllowlistResult(
      bool url_on_safe_browsing_high_confidence_allowlist,
      std::optional<safe_browsing::SafeBrowsingDatabaseManager::
                        HighConfidenceAllowlistCheckLoggingDetails>
          logging_details);

  // Runs callback if site-familiarity verdict has been computed.
  void RunCallbackIfFinished();

  // As SiteFamiliarityFetcher is owned by the NavigationRequest, assume that
  // SiteFamiliarityFetcher is destroyed prior to `profile_` getting destroyed.
  raw_ptr<Profile> profile_;

  // The GURL for which history service and
  // safe-browsing-high-confidence-allowlist queries are being performed.
  GURL fetch_url_;

  // Whether the history service query is complete.
  bool fetched_history_ = false;

  // Whether the history service has an entry for `fetch_url_`'s origin from
  // more than `kMinAgeOfInitialVisitForFamiliarity`.
  bool has_record_older_than_threshold_ = false;

  // Whether the safe-browsing-high-confidence-allowlist has been fetched.
  bool fetched_sb_list_ = false;

  // Whether `fetch_url_` is on the safe-browsing high-confidence-allowlist.
  bool is_on_sb_list_ = false;

  // Whether the site engagement score for `fetch_url_` is higher than
  // `kMinSiteEngagementScoreForFamiliarity`.
  bool has_engagement_score_higher_than_threshold_ = false;

  // Callback passed to Start().
  Callback callback_;

  base::CancelableTaskTracker task_tracker_;
  base::WeakPtrFactory<SiteFamiliarityFetcher> weak_factory_{this};
};

}  // namespace site_protection

#endif  // CHROME_BROWSER_SITE_PROTECTION_SITE_FAMILIARITY_FETCHER_H_
