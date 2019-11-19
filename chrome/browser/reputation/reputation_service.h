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
};

// Wrapper used to store the results of a reputation check. Specifically, this
// is passed to the callback given to GetReputationStatus.  |url| is the URL
// applicable for this result.
struct ReputationCheckResult {
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

  // Tells the service that the user has explicitly ignored the warning, and
  // records a histogram.
  // Exposed in subsequent results from GetReputationStatus.
  void SetUserIgnore(content::WebContents* web_contents,
                     const GURL& url,
                     SafetyTipInteraction interaction);

 private:
  // Returns whether the warning should be shown on the given URL. This is
  // mostly just a helper function to ensure that we always query the allowlist
  // by origin.
  bool IsIgnored(const GURL& url) const;

  // Callback once we have up-to-date |engaged_sites|. Performs checks on the
  // navigated |url|. Displays the warning when needed.
  void GetReputationStatusWithEngagedSites(
      ReputationCheckCallback callback,
      const GURL& url,
      const std::vector<DomainInfo>& engaged_sites);

  // Set of origins that we've warned about, and the user has explicitly
  // ignored.  Used to avoid re-warning the user.
  std::set<url::Origin> warning_dismissed_origins_;

  Profile* profile_;

  base::WeakPtrFactory<ReputationService> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ReputationService);
};

// Checks SafeBrowsing-style permutations of |url| against the component updater
// blocklist and returns the match type. kNone means the URL is not blocked.
security_state::SafetyTipStatus GetSafetyTipUrlBlockType(const GURL& url);

#endif  // CHROME_BROWSER_REPUTATION_REPUTATION_SERVICE_H_
