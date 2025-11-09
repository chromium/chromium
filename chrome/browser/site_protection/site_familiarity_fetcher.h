// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SITE_PROTECTION_SITE_FAMILIARITY_FETCHER_H_
#define CHROME_BROWSER_SITE_PROTECTION_SITE_FAMILIARITY_FETCHER_H_

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

// Calculates the site familiarity based on information in chrome://history and
// the safe-browsing-high-confidence-allowlist.
class SiteFamiliarityFetcher {
 public:
  enum class Verdict {
    kFamiliar,
    kUnfamiliar,
  };

  typedef base::OnceCallback<void(Verdict)> Callback;

  explicit SiteFamiliarityFetcher(Profile* profile);
  ~SiteFamiliarityFetcher();

  // Starts fetching data to determine whether passed-in URL is familiar.
  // Cancels any in-progress fetches.
  void Start(const GURL& url, Callback callback);

  // The GURL for which site familiarity is being computed.
  const GURL& fetch_url() { return fetch_url_; }

 private:
  // Initiates safe-browsing-high-confidence-allowlist request.
  void StartFetchingSafeBrowsingHighConfidenceAllowlist();

  // Called when the site-familiarity verdict has been computed without querying
  // history or the safe-browsing-high-confidence-allowlist.
  void OnComputedVerdictWithoutFetches(bool is_site_familiar);

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

  // Callback passed to Start().
  Callback callback_;

  base::CancelableTaskTracker task_tracker_;
  base::WeakPtrFactory<SiteFamiliarityFetcher> weak_factory_{this};
};

}  // namespace site_protection

#endif  // CHROME_BROWSER_SITE_PROTECTION_SITE_FAMILIARITY_FETCHER_H_
