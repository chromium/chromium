// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REPUTATION_REPUTATION_SERVICE_H_
#define CHROME_BROWSER_REPUTATION_REPUTATION_SERVICE_H_

#include <set>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/reputation/safety_tip_ui.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/security_state/core/security_state.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"
#include "url/origin.h"

class Profile;
struct DomainInfo;

// All potential heuristics that can trigger. Used temporarily to keep track of
// which heuristics trigger during a reputation check, and later used to decide
// which metrics get recorded.
struct TriggeredHeuristics {
  bool blocklist_heuristic_triggered = false;
  bool lookalike_heuristic_triggered = false;
  bool keywords_heuristic_triggered = false;

  inline bool triggered_any() const {
    return blocklist_heuristic_triggered || lookalike_heuristic_triggered ||
           keywords_heuristic_triggered;
  }
};

// Wrapper used to store the results of a reputation check. Specifically, this
// is passed to the callback given to GetReputationStatus.  |url| is the URL
// applicable for this result.
struct ReputationCheckResult {
  ReputationCheckResult() = default;
  ReputationCheckResult(const ReputationCheckResult& other) = default;

  security_state::SafetyTipStatus safety_tip_status =
      security_state::SafetyTipStatus::kNone;
  GURL url;
  GURL suggested_url;
  TriggeredHeuristics triggered_heuristics;
};

// Callback type used for retrieving reputation status. The results of the
// reputation check are given in |result|.
using ReputationCheckCallback =
    base::OnceCallback<void(ReputationCheckResult result)>;

// Provides reputation information on URLs for Safety Tips.
class ReputationService : public KeyedService {
 public:
  explicit ReputationService(Profile* profile);
  ~ReputationService() override;

  static ReputationService* Get(Profile* profile);

  // Calculate the overall reputation status of the given URL, and
  // asynchronously call |callback| with the results. See
  // ReputationCheckCallback above for details on what's returned. |callback|
  // will be called regardless of whether |url| is flagged or
  // not. (Specifically, |callback| will be called with SafetyTipStatus::kNone
  // if the url is not flagged).
  void GetReputationStatus(const GURL& url, ReputationCheckCallback callback);

  // Returns whether the user has dismissed a similar warning, and thus no
  // warning should be shown for the provided url.
  bool IsIgnored(const GURL& url) const;

  // Tells the service that the user has explicitly ignored the warning (thus
  // adding to the profile-wide allowlist)..
  void SetUserIgnore(const GURL& url);

  // Tells the service that the user has the UI disabled, and thus the warning
  // should be ignored.  This ensures that subsequent loads of the page are not
  // seen as flagged in metrics. This only impacts metrics for control groups.
  void OnUIDisabledFirstVisit(const GURL& url);

  // Used to help mock the generated keywords for the sensitive keywords
  // heuristic for testing. The keywords passed to this function MUST be in
  // sorted order, and must have a lifetime at least as long as this service.
  void SetSensitiveKeywordsForTesting(const char* const* new_keywords,
                                      size_t num_new_keywords);

 private:
  // Callback once we have up-to-date |engaged_sites|. Performs checks on the
  // navigated |url|. Displays the warning when needed.
  void GetReputationStatusWithEngagedSites(
      const GURL& url,
      ReputationCheckCallback callback,
      const std::vector<DomainInfo>& engaged_sites);

  // Set of origins that we've warned about, and the user has explicitly
  // ignored.  Used to avoid re-warning the user.
  std::set<url::Origin> warning_dismissed_origins_;

  Profile* profile_;

  const char* const* sensitive_keywords_;
  size_t num_sensitive_keywords_;

  base::WeakPtrFactory<ReputationService> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ReputationService);
};

#endif  // CHROME_BROWSER_REPUTATION_REPUTATION_SERVICE_H_
