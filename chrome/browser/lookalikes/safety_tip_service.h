// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_SAFETY_TIP_SERVICE_H_
#define CHROME_BROWSER_LOOKALIKES_SAFETY_TIP_SERVICE_H_

#include <set>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/lookalikes/safety_tip_ui.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/security_state/core/security_state.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"
#include "url/origin.h"

class Profile;

namespace lookalikes {
struct DomainInfo;
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
  // True if a lookalike heuristic was triggered. Used temporarily to keep track
  // of whether a heuristic triggers during a safety tip check, and later used
  // to decide whether metrics get recorded.
  bool lookalike_heuristic_triggered = false;
};

// Callback type used for retrieving safety tip status. The results of the
// check are given in |result|.
using SafetyTipCheckCallback =
    base::OnceCallback<void(SafetyTipCheckResult result)>;

// Provides lookalike information on URLs for Safety Tips.
class SafetyTipService : public KeyedService {
 public:
  explicit SafetyTipService(Profile* profile);

  SafetyTipService(const SafetyTipService&) = delete;
  SafetyTipService& operator=(const SafetyTipService&) = delete;

  ~SafetyTipService() override;

  static SafetyTipService* Get(Profile* profile);

  // Calculate the safety tip status of the given URL, and
  // asynchronously call |callback| with the results. See
  // SafetyTipCheckCallback above for details on what's returned. |callback|
  // will be called regardless of whether |url| is flagged or
  // not. (Specifically, |callback| will be called with SafetyTipStatus::kNone
  // if the url is not flagged).
  void GetSafetyTipStatus(const GURL& url,
                          content::WebContents* web_contents,
                          SafetyTipCheckCallback callback);

  // Returns whether the user has dismissed a similar warning, and thus no
  // warning should be shown for the provided url.
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
  // Callback once we have up-to-date |engaged_sites|. Performs checks on the
  // navigated |url|. Displays the Safety Tip warning when needed.
  void GetSafetyTipStatusWithEngagedSites(
      const GURL& url,
      SafetyTipCheckCallback callback,
      const std::vector<lookalikes::DomainInfo>& engaged_sites);

  // Set of eTLD+1s that we've warned about, and the user has explicitly
  // ignored.  Used to avoid re-warning the user.
  std::set<std::string> warning_dismissed_etld1s_;

  raw_ptr<Profile, DanglingUntriaged> profile_;

  base::WeakPtrFactory<SafetyTipService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_LOOKALIKES_SAFETY_TIP_SERVICE_H_
