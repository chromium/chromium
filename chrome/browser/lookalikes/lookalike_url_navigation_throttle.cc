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
#include "chrome/browser/reputation/reputation_service.h"
#include "chrome/browser/reputation/safety_tips_config.h"
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

}  // namespace

LookalikeUrlNavigationThrottle::LookalikeUrlNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle),
      profile_(Profile::FromBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext())) {}

LookalikeUrlNavigationThrottle::~LookalikeUrlNavigationThrottle() {}

ThrottleCheckResult LookalikeUrlNavigationThrottle::WillProcessResponse() {
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

  // Ignore errors, subframe and same document navigations.
  if (handle->GetNetErrorCode() != net::OK || !handle->IsInMainFrame() ||
      handle->IsSameDocument()) {
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

  // If this is a reload and if the current URL is the last URL of the stored
  // redirect chain, the interstitial was probably reloaded. Stop the reload and
  // navigate back to the original lookalike URL so that the whole throttle is
  // exercised again.
  if (handle->GetReloadType() != content::ReloadType::NONE &&
      IsInterstitialReload(handle->GetURL(),
                           interstitial_params.redirect_chain)) {
    CHECK(interstitial_params.url.SchemeIsHTTPOrHTTPS());
    // See
    // https://groups.google.com/a/chromium.org/forum/#!topic/chromium-dev/plIZV3Rkzok
    // for why this is OK. Assume interstitial reloads are always browser
    // initiated.
    handle->GetWebContents()->OpenURL(content::OpenURLParams(
        interstitial_params.url, interstitial_params.referrer,
        WindowOpenDisposition::CURRENT_TAB,
        ui::PageTransition::PAGE_TRANSITION_RELOAD,
        false /* is_renderer_initiated */));
    return content::NavigationThrottle::CANCEL_AND_IGNORE;
  }

  LookalikeUrlService* service = LookalikeUrlService::Get(profile_);
  if (!use_test_profile_ && service->EngagedSitesNeedUpdating()) {
    service->ForceUpdateEngagedSites(
        base::BindOnce(&LookalikeUrlNavigationThrottle::PerformChecksDeferred,
                       weak_factory_.GetWeakPtr()));
    return content::NavigationThrottle::DEFER;
  }

  return PerformChecks(service->GetLatestEngagedSites());
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
    const std::vector<DomainInfo>& engaged_sites) {
  ThrottleCheckResult result = PerformChecks(engaged_sites);

  if (result.action() == content::NavigationThrottle::PROCEED) {
    Resume();
    return;
  }

  CancelDeferredNavigation(result);
}

ThrottleCheckResult LookalikeUrlNavigationThrottle::PerformChecks(
    const std::vector<DomainInfo>& engaged_sites) {
  DCHECK_EQ(
      navigation_handle()
          ->GetRedirectChain()[navigation_handle()->GetRedirectChain().size() -
                               1],
      navigation_handle()->GetURL());

  // Check for two lookalikes -- at the beginning and end of the redirect chain.
  const GURL& first_url = navigation_handle()->GetRedirectChain()[0];
  LookalikeUrlMatchType first_match_type;
  GURL first_suggested_url;
  bool first_is_lookalike = IsLookalikeUrl(
      first_url, engaged_sites, &first_match_type, &first_suggested_url);

  const GURL& last_url = navigation_handle()->GetURL();
  LookalikeUrlMatchType last_match_type;
  GURL last_suggested_url;
  // If first_url == last_url, then don't check a second time. This saves time,
  // and avoids clouding metrics.
  bool last_is_lookalike =
      first_url != last_url &&
      IsLookalikeUrl(last_url, engaged_sites, &last_match_type,
                     &last_suggested_url);

  if (!first_is_lookalike && !last_is_lookalike) {
    return content::NavigationThrottle::PROCEED;
  }

  // If the first URL is a lookalike, but we ended up on the suggested site
  // anyway, don't warn.
  if (first_is_lookalike &&
      last_url.DomainIs(GetETLDPlusOne(first_suggested_url.host()))) {
    first_is_lookalike = false;
  }

  // source_id corresponds to last_url, even when first_url is what triggered.
  // TODO(crbug.com/1133598): disambiguate first_- vs. last_urls.
  ukm::SourceId source_id = ukm::ConvertToSourceId(
      navigation_handle()->GetNavigationId(), ukm::SourceIdType::NAVIGATION_ID);

  if (first_is_lookalike &&
      ShouldBlockLookalikeUrlNavigation(first_match_type)) {
    return ShowInterstitial(first_suggested_url, first_url, source_id,
                            first_match_type);
  }

  if (last_is_lookalike && ShouldBlockLookalikeUrlNavigation(last_match_type)) {
    return ShowInterstitial(last_suggested_url, last_url, source_id,
                            last_match_type);
  }

  // Interstitial normally records UKM, but still record when it's not shown.
  RecordUkmForLookalikeUrlBlockingPage(
      source_id, first_is_lookalike ? first_match_type : last_match_type,
      LookalikeUrlBlockingPageUserAction::kInterstitialNotShown);
  return content::NavigationThrottle::PROCEED;
}

bool LookalikeUrlNavigationThrottle::IsLookalikeUrl(
    const GURL& url,
    const std::vector<DomainInfo>& engaged_sites,
    LookalikeUrlMatchType* match_type,
    GURL* suggested_url) {
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }

  const DomainInfo navigated_domain = GetDomainInfo(url);
  // Empty domain_and_registry happens on private domains.
  if (navigated_domain.domain_and_registry.empty() ||
      IsTopDomain(navigated_domain)) {
    return content::NavigationThrottle::PROCEED;
  }

  // Fetch the component allowlist.
  const auto* proto = GetSafetyTipsRemoteConfigProto();

  // When there's no proto (like at browser start), fail-safe and don't block.
  if (!proto) {
    return false;
  }

  // If the URL is in the component allowlist, don't show any warning.
  if (IsUrlAllowlistedBySafetyTipsComponent(proto, url.GetWithEmptyPath())) {
    return false;
  }

  // If the URL is in the local temporary allowlist, don't show any warning.
  if (ReputationService::Get(profile_)->IsIgnored(url)) {
    return false;
  }

  // If the host is allowlisted by policy, don't show any warning.
  if (IsAllowedByEnterprisePolicy(profile_->GetPrefs(), url)) {
    return false;
  }

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
    return false;
  }

  const LookalikeTargetAllowlistChecker in_target_allowlist =
      base::BindRepeating(&IsTargetHostAllowlistedBySafetyTipsComponent, proto);
  std::string matched_domain;
  if (GetMatchingDomain(navigated_domain, engaged_sites, in_target_allowlist,
                        &matched_domain, match_type)) {
    DCHECK(!matched_domain.empty());

    RecordUMAFromMatchType(*match_type);

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
    std::string suggested_domain = GetETLDPlusOne(matched_domain);
    DCHECK(!suggested_domain.empty());
    // Drop everything but the parts of the origin.
    GURL::Replacements replace_host;
    replace_host.SetHostStr(suggested_domain);
    *suggested_url = url.ReplaceComponents(replace_host).GetWithEmptyPath();
    return true;
  }

  if (ShouldBlockBySpoofCheckResult(navigated_domain)) {
    *match_type = LookalikeUrlMatchType::kFailedSpoofChecks;
    *suggested_url = GURL();
    RecordUMAFromMatchType(*match_type);
    return true;
  }

  return false;
}
