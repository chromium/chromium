// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_SERVICE_H_
#define CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_SERVICE_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/sequence_checker.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/lookalikes/core/lookalike_url_util.h"
#include "components/security_state/core/security_state.h"
#include "url/gurl.h"

class PrefService;

namespace base {
class Clock;
}

namespace lookalikes {
struct DomainInfo;
}

namespace content {
class WebContents;
}

// Wrapper used to store the results of a safety tip check. Specifically, this
// is passed to the callback given to GetSafetyTipStatus.  |url| is the URL
// applicable for this result.
struct SafetyTipCheckResult {
  SafetyTipCheckResult() = default;
  SafetyTipCheckResult(const SafetyTipCheckResult& other) = default;

  security_state::SafetyTipStatus safety_tip_status =
      security_state::SafetyTipStatus::kNone;
  GURL url;
  GURL suggested_url;
  // True if a lookalike heuristic was triggered. Used to track whether a
  // heuristic triggered for the given URL.
  bool lookalike_heuristic_triggered = false;
};

// Callback type used for retrieving safety tip status. The results of the
// check are given in |result|.
using SafetyTipCheckCallback =
    base::OnceCallback<void(SafetyTipCheckResult result)>;

// A service that handles operations on lookalike URLs. It can fetch the list of
// engaged sites in a background thread and cache the results until the next
// update. This is more efficient than fetching the list on each navigation for
// each tab separately.
class LookalikeUrlService : public KeyedService {
 public:
  // DO NOT pass a Profile here, pass keyed services dependencies explicitly
  // (crbug.com/368297674).
  LookalikeUrlService(PrefService* pref_service,
                      HostContentSettingsMap* host_content_settings_map);

  LookalikeUrlService(const LookalikeUrlService&) = delete;
  LookalikeUrlService& operator=(const LookalikeUrlService&) = delete;

  ~LookalikeUrlService() override;

  using EngagedSitesCallback =
      base::OnceCallback<void(const std::vector<lookalikes::DomainInfo>&)>;

  // Returns whether the engaged site list is recently updated. Returns true
  // even when an update has already been queued or is in progress.
  bool EngagedSitesNeedUpdating() const;

  // Triggers an update to the engaged site list if one is not already inflight,
  // then schedules |callback| to be called with the new list once available.
  void ForceUpdateEngagedSites(EngagedSitesCallback callback);

  // Returns the _current_ list of engaged sites, without updating them if
  // they're out of date.
  const std::vector<lookalikes::DomainInfo> GetLatestEngagedSites() const;

  void SetClockForTesting(base::Clock* clock);
  base::Clock* clock() const { return clock_; }

  // Stores the result of a lookalike URL check.
  struct LookalikeUrlCheckResult {
    // Action to take. kNone means the navigated URL wasn't a lookalike.
    lookalikes::LookalikeActionType action_type =
        lookalikes::LookalikeActionType::kNone;
    // Heuristic match type. kNone means the navigated URL wasn't a lookalike.
    lookalikes::LookalikeUrlMatchType match_type =
        lookalikes::LookalikeUrlMatchType::kNone;
    // Good URL suggested by the matching heuristic. Can be empty if there is no
    // suggestion in which case the UI will show a different string.
    GURL suggested_url;
    // If true, the URL was previously flagged as a lookalike and the warning
    // was dismissed by the user. Clears on browser restart for a given URL.
    bool is_warning_previously_dismissed = false;
    // If true, the URL was allowlisted by the component updater or enterprise
    // policies.
    bool is_allowlisted = false;
    // Elapsed time for the GetDomainInfo() call. Zero if the function wasn't
    // called.
    base::TimeDelta get_domain_info_duration;
  };

  // This is the main function to call to check if a url is a lookalike URL. It
  // runs all of the necessary checks on `url` before returning a result.
  // If `url` is a lookalike, returns a result with the `action_type` and
  // `match_type` populated.
  // If stop_checking_on_allowlist_or_ignore is true, stops checking if the
  // URL is allowlisted or a warning for it was previously ignored.
  LookalikeUrlCheckResult CheckUrlForLookalikes(
      const GURL& url,
      const std::vector<lookalikes::DomainInfo>& engaged_sites,
      bool stop_checking_on_allowlist_or_ignore) const;

  // Check the safety tip status of the given URL, and
  // asynchronously call |callback| with the results. See
  // SafetyTipCheckCallback above for details on what's returned. |callback|
  // will be called regardless of whether |url| is flagged or
  // not. (Specifically, |callback| will be called with SafetyTipStatus::kNone
  // if the url is not flagged).
  void CheckSafetyTipStatus(const GURL& url,
                            content::WebContents* web_contents,
                            SafetyTipCheckCallback callback);

  // Returns whether the user has dismissed a similar warning (interstitial or
  // safety tip), and thus no warning should be shown for the provided url.
  bool IsIgnored(const GURL& url) const;

  // Tells the service that the user has explicitly ignored the warning (thus
  // adding to the profile-wide allowlist).
  void SetUserIgnore(const GURL& url);

  // Tells the service that the user has the UI disabled, and thus the warning
  // should be ignored.  This ensures that subsequent loads of the page are not
  // seen as flagged in metrics. This only impacts metrics for control groups.
  void OnUIDisabledFirstVisit(const GURL& url);

  // Reset set of eTLD+1s to forget the user action that ignores warning. Only
  // for testing.
  void ResetWarningDismissedETLDPlusOnesForTesting();

 private:
  // Called when an async engaged site computation is finished.
  void OnUpdateEngagedSitesCompleted(
      std::vector<lookalikes::DomainInfo> new_engaged_sites);

  // Callback once we have up-to-date |engaged_sites|. Performs checks on the
  // navigated |url|. Caller will display the Safety Tip warning when needed.
  void CheckSafetyTipStatusWithEngagedSites(
      const GURL& url,
      SafetyTipCheckCallback callback,
      const std::vector<lookalikes::DomainInfo>& engaged_sites);

  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<HostContentSettingsMap> host_content_settings_map_;
  raw_ptr<base::Clock> clock_;
  base::Time last_engagement_fetch_time_;
  std::vector<lookalikes::DomainInfo> engaged_sites_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Indicates that an update to the engaged sites list has been queued. Serves
  // to prevent enqueuing excessive updates.
  bool update_in_progress_ = false;
  std::vector<EngagedSitesCallback> pending_update_complete_callbacks_;

  // Set of eTLD+1s that we've warned about, and the user has explicitly
  // ignored.  Used to avoid re-warning the user.
  std::set<std::string> warning_dismissed_etld1s_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<LookalikeUrlService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_SERVICE_H_
