// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/lookalike_url_navigation_throttle.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/lookalikes/lookalike_url_blocking_page.h"
#include "chrome/browser/lookalikes/lookalike_url_controller_client.h"
#include "chrome/browser/lookalikes/lookalike_url_service.h"
#include "chrome/browser/lookalikes/lookalike_url_tab_storage.h"
#include "chrome/browser/prerender/chrome_prerender_contents_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/reputation/safety_tips_config.h"
#include "chrome/common/chrome_features.h"
#include "components/lookalikes/core/features.h"
#include "components/lookalikes/core/lookalike_url_ui_util.h"
#include "components/lookalikes/core/lookalike_url_util.h"
#include "components/prerender/browser/prerender_contents.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/url_formatter/spoof_checks/top_domains/top500_domains.h"
#include "components/url_formatter/spoof_checks/top_domains/top_domain_util.h"
#include "content/public/browser/navigation_handle.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"

namespace {

typedef content::NavigationThrottle::ThrottleCheckResult ThrottleCheckResult;

// Returns true if |current_url| is at the end of the redirect chain
// stored in |stored_redirect_chain|.
bool IsInterstitialReload(const GURL& current_url,
                          const std::vector<GURL>& stored_redirect_chain) {
  return stored_redirect_chain.size() > 1 &&
         stored_redirect_chain[stored_redirect_chain.size() - 1] == current_url;
}

// Returns the index of the first URL in the redirect chain which has a
// different eTLD+1 than the initial URL. If all URLs have the same eTLD+1,
// returns 0.
size_t FindFirstCrossSiteURL(const std::vector<GURL>& redirect_chain) {
  DCHECK_GE(redirect_chain.size(), 2u);
  const GURL initial_url = redirect_chain[0];
  const std::string initial_etld_plus_one = GetETLDPlusOne(initial_url.host());
  for (size_t i = 1; i < redirect_chain.size(); i++) {
    if (initial_etld_plus_one != GetETLDPlusOne(redirect_chain[i].host())) {
      return i;
    }
  }
  return 0;
}

}  // namespace

bool IsSafeRedirect(const std::string& matching_domain,
                    const std::vector<GURL>& redirect_chain) {
  if (redirect_chain.size() < 2) {
    return false;
  }
  const size_t first_cross_site_redirect =
      FindFirstCrossSiteURL(redirect_chain);
  DCHECK_GE(first_cross_site_redirect, 0u);
  DCHECK_LE(first_cross_site_redirect, redirect_chain.size() - 1);
  if (first_cross_site_redirect == 0) {
    // All URLs in the redirect chain belong to the same eTLD+1.
    return false;
  }
  // There is a redirect from the initial eTLD+1 to another site. In order to be
  // a safe redirect, it should be to the root of |matching_domain|. This
  // ignores any further redirects after |matching_domain|.
  const GURL redirect_target = redirect_chain[first_cross_site_redirect];
  return matching_domain == GetETLDPlusOne(redirect_target.host()) &&
         redirect_target == redirect_target.GetWithEmptyPath();
}

LookalikeUrlNavigationThrottle::LookalikeUrlNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle),
      profile_(Profile::FromBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext())) {}

LookalikeUrlNavigationThrottle::~LookalikeUrlNavigationThrottle() {}

ThrottleCheckResult LookalikeUrlNavigationThrottle::HandleThrottleRequest(
    const GURL& url,
    bool check_safe_redirect) {
  // Ignore if running unit tests. Some tests use
  // TestMockTimeTaskRunner::ScopedContext and call CreateTestWebContents()
  // which navigates and waits for throttles to complete using a RunLoop.
  // However, TestMockTimeTaskRunner::ScopedContext disallows RunLoop so those
  // tests crash. We should only do this with a real profile anyways.
  // use_test_profile is set by unit tests to true so that the rest of the
  // throttle is exercised.
  // In other words, this condition is false in production code, browser tests
  // and only lookalike unit tests. It's true in all non-lookalike unit tests.
  if (!use_test_profile_ && profile_->AsTestingProfile()) {
    return content::NavigationThrottle::PROCEED;
  }

  content::NavigationHandle* handle = navigation_handle();

  // Ignore subframe and same document navigations.
  if (!handle->IsInMainFrame() || handle->IsSameDocument()) {
    return content::NavigationThrottle::PROCEED;
  }

  // Get stored interstitial parameters early. By doing so, we ensure that a
  // navigation to an irrelevant (for this interstitial's purposes) URL such as
  // chrome://settings while the lookalike interstitial is being shown clears
  // the stored state:
  // 1. User navigates to lookalike.tld which redirects to site.tld.
  // 2. Interstitial shown.
  // 3. User navigates to chrome://settings.
  // If, after this, the user somehow ends up on site.tld with a reload (e.g.
  // with ReloadType::ORIGINAL_REQUEST_URL), this will correctly not show an
  // interstitial.
  LookalikeUrlTabStorage* tab_storage =
      LookalikeUrlTabStorage::GetOrCreate(handle->GetWebContents());
  const LookalikeUrlTabStorage::InterstitialParams interstitial_params =
      tab_storage->GetInterstitialParams();
  tab_storage->ClearInterstitialParams();

  if (!url.SchemeIsHTTPOrHTTPS()) {
    return content::NavigationThrottle::PROCEED;
  }

  // Fetch the component allowlist.
  const auto* proto = GetSafetyTipsRemoteConfigProto();

  // When there's no proto (like at browser start), fail-safe and don't block.
  if (!proto) {
    return content::NavigationThrottle::PROCEED;
  }

  // If the URL is in the component allowlist, don't show any warning.
  if (IsUrlAllowlistedBySafetyTipsComponent(proto, url.GetWithEmptyPath())) {
    return content::NavigationThrottle::PROCEED;
  }

  // If the URL is in the local temporary allowlist, don't show any warning.
  if (tab_storage->IsDomainAllowed(url.host())) {
    return content::NavigationThrottle::PROCEED;
  }

  // If the host is allowlisted by policy, don't show any warning.
  if (IsAllowedByEnterprisePolicy(profile_->GetPrefs(), url)) {
    return content::NavigationThrottle::PROCEED;
  }

  // If this is a reload and if the current URL is the last URL of the stored
  // redirect chain, the interstitial was probably reloaded. Stop the reload and
  // navigate back to the original lookalike URL so that the whole throttle is
  // exercised again.
  if (handle->GetReloadType() != content::ReloadType::NONE &&
      IsInterstitialReload(url, interstitial_params.redirect_chain)) {
    CHECK(interstitial_params.url.SchemeIsHTTPOrHTTPS());
    // See
    // https://groups.google.com/a/chromium.org/forum/#!topic/chromium-dev/plIZV3Rkzok
    // for why this is OK. Assume interstitial reloads are always browser
    // initiated.
    navigation_handle()->GetWebContents()->OpenURL(content::OpenURLParams(
        interstitial_params.url, interstitial_params.referrer,
        WindowOpenDisposition::CURRENT_TAB,
        ui::PageTransition::PAGE_TRANSITION_RELOAD,
        false /* is_renderer_initiated */));
    return content::NavigationThrottle::CANCEL_AND_IGNORE;
  }

  const DomainInfo navigated_domain = GetDomainInfo(url);
  // Empty domain_and_registry happens on private domains.
  if (navigated_domain.domain_and_registry.empty() ||
      IsTopDomain(navigated_domain)) {
    return content::NavigationThrottle::PROCEED;
  }

  LookalikeUrlService* service = LookalikeUrlService::Get(profile_);
  if (!use_test_profile_ && service->EngagedSitesNeedUpdating()) {
    service->ForceUpdateEngagedSites(
        base::BindOnce(&LookalikeUrlNavigationThrottle::PerformChecksDeferred,
                       weak_factory_.GetWeakPtr(), url, navigated_domain,
                       check_safe_redirect));
    return content::NavigationThrottle::DEFER;
  }

  return PerformChecks(url, navigated_domain, check_safe_redirect,
                       service->GetLatestEngagedSites());
}

ThrottleCheckResult LookalikeUrlNavigationThrottle::WillProcessResponse() {
  if (navigation_handle()->GetNetErrorCode() != net::OK) {
    return content::NavigationThrottle::PROCEED;
  }
  // Do not check for if the redirect was safe. That should only be done when
  // the navigation is still being redirected.
  return HandleThrottleRequest(navigation_handle()->GetURL(), false);
}

ThrottleCheckResult LookalikeUrlNavigationThrottle::WillRedirectRequest() {
  const std::vector<GURL>& chain = navigation_handle()->GetRedirectChain();

  // WillRedirectRequest is called after a redirect occurs, so the end of the
  // chain is the URL that was redirected to. We need to check the preceding URL
  // that caused the redirection. The final URL in the chain is checked either:
  //  - after the next redirection (when there is a longer chain), or
  //  - by WillProcessResponse (before content is rendered).
  if (chain.size() < 2) {
    return content::NavigationThrottle::PROCEED;
  }
  return HandleThrottleRequest(chain[chain.size() - 2], true);
}

const char* LookalikeUrlNavigationThrottle::GetNameForLogging() {
  return "LookalikeUrlNavigationThrottle";
}

ThrottleCheckResult LookalikeUrlNavigationThrottle::ShowInterstitial(
    const GURL& safe_url,
    const GURL& url,
    ukm::SourceId source_id,
    LookalikeUrlMatchType match_type) {
  content::NavigationHandle* handle = navigation_handle();
  content::WebContents* web_contents = handle->GetWebContents();

  auto controller = std::make_unique<LookalikeUrlControllerClient>(
      web_contents, url, safe_url);

  std::unique_ptr<LookalikeUrlBlockingPage> blocking_page(
      new LookalikeUrlBlockingPage(web_contents, safe_url, url, source_id,
                                   match_type, std::move(controller)));

  base::Optional<std::string> error_page_contents =
      blocking_page->GetHTMLContents();

  security_interstitials::SecurityInterstitialTabHelper::AssociateBlockingPage(
      web_contents, handle->GetNavigationId(), std::move(blocking_page));

  // Store interstitial parameters in per-tab storage. Reloading the
  // interstitial once it's shown navigates to the final URL in the original
  // redirect chain. It also loses the original redirect chain. By storing these
  // parameters, we can check if the next navigation is a reload and act
  // accordingly.
  content::Referrer referrer(handle->GetReferrer().url,
                             handle->GetReferrer().policy);
  LookalikeUrlTabStorage::GetOrCreate(handle->GetWebContents())
      ->OnLookalikeInterstitialShown(url, referrer, handle->GetRedirectChain());

  return ThrottleCheckResult(content::NavigationThrottle::CANCEL,
                             net::ERR_BLOCKED_BY_CLIENT, error_page_contents);
}

std::unique_ptr<LookalikeUrlNavigationThrottle>
LookalikeUrlNavigationThrottle::MaybeCreateNavigationThrottle(
    content::NavigationHandle* navigation_handle) {
  // If the tab is being prerendered, stop here before it breaks metrics
  content::WebContents* web_contents = navigation_handle->GetWebContents();
  if (prerender::ChromePrerenderContentsDelegate::FromWebContents(
          web_contents)) {
    return nullptr;
  }

  // Otherwise, always insert the throttle for metrics recording.
  return std::make_unique<LookalikeUrlNavigationThrottle>(navigation_handle);
}

void LookalikeUrlNavigationThrottle::PerformChecksDeferred(
    const GURL& url,
    const DomainInfo& navigated_domain,
    bool check_safe_redirect,
    const std::vector<DomainInfo>& engaged_sites) {
  ThrottleCheckResult result =
      PerformChecks(url, navigated_domain, check_safe_redirect, engaged_sites);

  if (result.action() == content::NavigationThrottle::PROCEED) {
    Resume();
    return;
  }

  CancelDeferredNavigation(result);
}

ThrottleCheckResult LookalikeUrlNavigationThrottle::PerformChecks(
    const GURL& url,
    const DomainInfo& navigated_domain,
    bool check_safe_redirect,
    const std::vector<DomainInfo>& engaged_sites) {
  std::string matched_domain;
  LookalikeUrlMatchType match_type;

  // Ensure that this URL is not already engaged. We can't use the synchronous
  // SiteEngagementService::IsEngagementAtLeast as it has side effects. We check
  // in PerformChecks to ensure we have up-to-date engaged_sites.
  // This check ignores the scheme which is okay since it's more conservative:
  // If the user is engaged with http://domain.test, not showing the warning on
  // https://domain.test is acceptable.
  const auto already_engaged =
      std::find_if(engaged_sites.begin(), engaged_sites.end(),
                   [navigated_domain](const DomainInfo& engaged_domain) {
                     return (navigated_domain.domain_and_registry ==
                             engaged_domain.domain_and_registry);
                   });
  if (already_engaged != engaged_sites.end()) {
    return content::NavigationThrottle::PROCEED;
  }

  ukm::SourceId source_id = ukm::ConvertToSourceId(
      navigation_handle()->GetNavigationId(), ukm::SourceIdType::NAVIGATION_ID);

  auto* config = GetSafetyTipsRemoteConfigProto();
  const LookalikeTargetAllowlistChecker in_target_allowlist =
      base::BindRepeating(&IsTargetHostAllowlistedBySafetyTipsComponent,
                          config);
  if (GetMatchingDomain(navigated_domain, engaged_sites, in_target_allowlist,
                        &matched_domain, &match_type)) {
    DCHECK(!matched_domain.empty());

    RecordUMAFromMatchType(match_type);

    if (check_safe_redirect &&
        IsSafeRedirect(matched_domain,
                       navigation_handle()->GetRedirectChain())) {
      return content::NavigationThrottle::PROCEED;
    }

    if (ShouldBlockLookalikeUrlNavigation(match_type, navigated_domain)) {
      // matched_domain can be a top domain or an engaged domain. Simply use its
      // eTLD+1 as the suggested domain.
      // 1. If matched_domain is a top domain: Top domain list already contains
      // eTLD+1s only so this works well.
      // 2. If matched_domain is an engaged domain and is not an eTLD+1, don't
      // suggest it. Otherwise, navigating to googlé.com and having engaged with
      // docs.google.com would suggest docs.google.com.
      //
      // When the navigated and matched domains are not eTLD+1s (e.g.
      // docs.googlé.com and docs.google.com), this will suggest google.com
      // instead of docs.google.com. This is less than ideal, but has two
      // benefits:
      // - Simpler code
      // - Fewer suggestions to non-existent domains. E.g. When the navigated
      // domain is nonexistent.googlé.com and the matched domain is
      // docs.google.com, we will suggest google.com instead of
      // nonexistent.google.com.
      const std::string suggested_domain = GetETLDPlusOne(matched_domain);
      DCHECK(!suggested_domain.empty());
      // Drop everything but the parts of the origin.
      GURL::Replacements replace_host;
      replace_host.SetHostStr(suggested_domain);
      const GURL suggested_url =
          url.ReplaceComponents(replace_host).GetWithEmptyPath();
      return ShowInterstitial(suggested_url, url, source_id, match_type);
    }
    // Interstitial normally records UKM, but still record when it's not shown.
    RecordUkmForLookalikeUrlBlockingPage(
        source_id, match_type,
        LookalikeUrlBlockingPageUserAction::kInterstitialNotShown);
    return content::NavigationThrottle::PROCEED;
  }

  if (base::FeatureList::IsEnabled(
          lookalikes::features::kLookalikeInterstitialForPunycode) &&
      ShouldBlockBySpoofCheckResult(navigated_domain)) {
    match_type = LookalikeUrlMatchType::kFailedSpoofChecks;
    RecordUMAFromMatchType(match_type);
    return ShowInterstitial(GURL(), url, source_id, match_type);
  }

  return content::NavigationThrottle::PROCEED;
}
