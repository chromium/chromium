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
#include "chrome/browser/engagement/site_engagement_details.mojom.h"
#include "chrome/browser/lookalikes/lookalike_url_blocking_page.h"
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
  ThrottleCheckResult PerformChecks(
      const std::vector<DomainInfo>& engaged_sites);

  // A void-returning variant, only used with deferred throttle results.
  void PerformChecksDeferred(const std::vector<DomainInfo>& engaged_sites);

  // Returns whether |url| is a lookalike, setting |match_type| and
  // |suggested_url| appropriately. Used in PerformChecks() on a per-URL basis.
  bool IsLookalikeUrl(const GURL& url,
                      const std::vector<DomainInfo>& engaged_sites,
                      LookalikeUrlMatchType* match_type,
                      GURL* suggested_url);

  ThrottleCheckResult ShowInterstitial(const GURL& safe_domain,
                                       const GURL& url,
                                       ukm::SourceId source_id,
                                       LookalikeUrlMatchType match_type);

  Profile* profile_;
  bool use_test_profile_ = false;
  base::WeakPtrFactory<LookalikeUrlNavigationThrottle> weak_factory_{this};
};

#endif  // CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_NAVIGATION_THROTTLE_H_
