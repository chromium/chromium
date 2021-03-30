// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_NAVIGATION_THROTTLE_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "chrome/browser/lookalikes/digital_asset_links_cross_validator.h"
#include "chrome/browser/lookalikes/lookalike_url_blocking_page.h"
#include "components/digital_asset_links/digital_asset_links_handler.h"
#include "components/site_engagement/core/mojom/site_engagement_details.mojom.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/navigation_throttle.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace content {
class NavigationHandle;
}  // namespace content

class Profile;

struct DomainInfo;

// Returns true if the redirect is deemed to be safe. These are generally
// defensive registrations where the domain owner redirects the IDN to the ASCII
// domain. See the unit tests for examples.
// In short, |url| must redirect to the root of |safe_url_host| or one
// of its subdomains.
bool IsSafeRedirect(const std::string& safe_url_host,
                    const std::vector<GURL>& redirect_chain);

// Observes navigations and shows an interstitial if the navigated domain name
// is visually similar to a top domain or a domain with a site engagement score.
//
// Remember to update //docs/idn.md with the appropriate information if you
// modify the lookalike heuristics.
//
// This throttle assumes that two navigations never share the same throttle.
class LookalikeUrlNavigationThrottle : public content::NavigationThrottle {
 public:
  explicit LookalikeUrlNavigationThrottle(content::NavigationHandle* handle);
  ~LookalikeUrlNavigationThrottle() override;

  // content::NavigationThrottle:
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;

  static std::unique_ptr<LookalikeUrlNavigationThrottle>
  MaybeCreateNavigationThrottle(content::NavigationHandle* navigation_handle);

  // The throttle normally ignores testing profiles and returns PROCEED. This
  // function forces unit tests to not ignore them .
  void SetUseTestProfileForTesting() { use_test_profile_ = true; }

 private:
  // Performs synchronous top domain and engaged site checks on the navigated
  // and redirected urls. Uses |engaged_sites| for the engaged site checks.
  // This function can also defer the check and schedule a
  // cancellation/resumption if additional checks need to be done such as
  // validating Digital Asset Links manifests.
  ThrottleCheckResult PerformChecks(
      const std::vector<DomainInfo>& engaged_sites);

  // A void-returning variant, only used with deferred throttle results (e.g.
  // when we need to fetch engaged sites list or digital asset link manifests).
  void PerformChecksDeferred(const std::vector<DomainInfo>& engaged_sites);

  // Returns whether |url| is a lookalike, setting |match_type| and
  // |suggested_url| appropriately. Used in PerformChecks() on a per-URL basis.
  bool IsLookalikeUrl(const GURL& url,
                      const std::vector<DomainInfo>& engaged_sites,
                      LookalikeUrlMatchType* match_type,
                      GURL* suggested_url);

  // Shows a full page interstitial. |safe_domain| is the domain suggested as
  // safe by the interstitial. |lookalike_domain| is the domain that triggered
  // the warning.
  // This function can display two types of interstitials depending on the
  // value of |safe_domain|:
  // - If |safe_domain| is a valid URL, it displays a lookalike interstitial
  // that suggests the user to go to |safe_domain| instead.
  // - Otherwise, it displays the punycode interstitial which doesn't suggest a
  // safe URL.
  ThrottleCheckResult ShowInterstitial(const GURL& safe_domain,
                                       const GURL& lookalike_domain,
                                       ukm::SourceId source_id,
                                       LookalikeUrlMatchType match_type,
                                       bool triggered_by_initial_url);

  // Checks digital asset links of |lookalike_domain| and |safe_domain| and
  // shows a full page interstitial if either manifest validation fails.
  ThrottleCheckResult CheckManifestsAndMaybeShowInterstitial(
      const GURL& safe_domain,
      const GURL& lookalike_domain,
      ukm::SourceId source_id,
      LookalikeUrlMatchType match_type,
      bool triggered_by_initial_url);

  // Callback for digital asset link manifest validations.
  void OnManifestValidationResult(const GURL& safe_domain,
                                  const GURL& lookalike_domain,
                                  ukm::SourceId source_id,
                                  LookalikeUrlMatchType match_type,
                                  bool triggered_by_initial_url,
                                  bool validation_success);

  Profile* profile_;
  bool use_test_profile_ = false;

  std::unique_ptr<DigitalAssetLinkCrossValidator> digital_asset_link_validator_;

  base::WeakPtrFactory<LookalikeUrlNavigationThrottle> weak_factory_{this};
};

#endif  // CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_NAVIGATION_THROTTLE_H_
