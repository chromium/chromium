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
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/lookalikes/lookalike_url_controller_client.h"
#include "chrome/browser/lookalikes/lookalike_url_interstitial_page.h"
#include "chrome/browser/lookalikes/lookalike_url_service.h"
#include "chrome/browser/lookalikes/lookalike_url_tab_storage.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/url_formatter/spoof_checks/top_domains/top500_domains.h"
#include "components/url_formatter/spoof_checks/top_domains/top_domain_util.h"
#include "content/public/browser/navigation_handle.h"
#include "third_party/blink/public/mojom/referrer.mojom.h"

namespace {

const base::FeatureParam<bool> kEnableInterstitialForTopSites{
    &features::kLookalikeUrlNavigationSuggestionsUI, "topsites", false};

using MatchType = LookalikeUrlInterstitialPage::MatchType;
using UserAction = LookalikeUrlInterstitialPage::UserAction;
using url_formatter::TopDomainEntry;

typedef content::NavigationThrottle::ThrottleCheckResult ThrottleCheckResult;

void RecordEvent(
    LookalikeUrlNavigationThrottle::NavigationSuggestionEvent event) {
  UMA_HISTOGRAM_ENUMERATION(LookalikeUrlNavigationThrottle::kHistogramName,
                            event);
}

bool SkeletonsMatch(const url_formatter::Skeletons& skeletons1,
                    const url_formatter::Skeletons& skeletons2) {
  DCHECK(!skeletons1.empty());
  DCHECK(!skeletons2.empty());
  for (const std::string& skeleton1 : skeletons1) {
    if (base::Contains(skeletons2, skeleton1)) {
      return true;
    }
  }
  return false;
}

// Returns a site that the user has used before that the eTLD+1 in
// |domain_and_registry| may be attempting to spoof, based on skeleton
// comparison.
std::string GetMatchingSiteEngagementDomain(
    const std::vector<DomainInfo>& engaged_sites,
    const DomainInfo& navigated_domain) {
  DCHECK(!navigated_domain.domain_and_registry.empty());
  for (const DomainInfo& engaged_site : engaged_sites) {
    DCHECK(!engaged_site.domain_and_registry.empty());
    DCHECK_NE(navigated_domain.domain_and_registry,
              engaged_site.domain_and_registry);
    if (SkeletonsMatch(navigated_domain.skeletons, engaged_site.skeletons)) {
      return engaged_site.domain_and_registry;
    }
  }
  return std::string();
}

// Returns the first matching top domain with an edit distance of at most one
// to |domain_and_registry|. This search is done in lexicographic order on the
// top 500 suitable domains, instead of in order by popularity. This means that
// the resulting "similar" domain may not be the most popular domain that
// matches.
std::string GetSimilarDomainFromTop500(const DomainInfo& navigated_domain) {
  for (const std::string& navigated_skeleton : navigated_domain.skeletons) {
    for (const char* const top_domain_skeleton :
         top500_domains::kTop500EditDistanceSkeletons) {
      if (IsEditDistanceAtMostOne(base::UTF8ToUTF16(navigated_skeleton),
                                  base::UTF8ToUTF16(top_domain_skeleton))) {
        const std::string top_domain =
            url_formatter::LookupSkeletonInTopDomains(top_domain_skeleton)
                .domain;
        DCHECK(!top_domain.empty());
        // If the only difference between the navigated and top
        // domains is the registry part, this is unlikely to be a spoofing
        // attempt. Ignore this match and continue. E.g. If the navigated domain
        // is google.com.tw and the top domain is google.com.tr, this won't
        // produce a match.
        const std::string top_domain_without_registry =
            url_formatter::top_domains::HostnameWithoutRegistry(top_domain);
        DCHECK(url_formatter::top_domains::IsEditDistanceCandidate(
            top_domain_without_registry));
        if (navigated_domain.domain_without_registry !=
            top_domain_without_registry) {
          return top_domain;
        }
      }
    }
  }
  return std::string();
}

// Returns the first matching engaged domain with an edit distance of at most
// one to |domain_and_registry|.
std::string GetSimilarDomainFromEngagedSites(
    const DomainInfo& navigated_domain,
    const std::vector<DomainInfo>& engaged_sites) {
  for (const std::string& navigated_skeleton : navigated_domain.skeletons) {
    for (const DomainInfo& engaged_site : engaged_sites) {
      if (!url_formatter::top_domains::IsEditDistanceCandidate(
              engaged_site.domain_and_registry)) {
        continue;
      }
      for (const std::string& engaged_skeleton : engaged_site.skeletons) {
        if (IsEditDistanceAtMostOne(base::UTF8ToUTF16(navigated_skeleton),
                                    base::UTF8ToUTF16(engaged_skeleton))) {
          // If the only difference between the navigated and engaged
          // domain is the registry part, this is unlikely to be a spoofing
          // attempt. Ignore this match and continue. E.g. If the navigated
          // domain is google.com.tw and the top domain is google.com.tr, this
          // won't produce a match.
          if (navigated_domain.domain_without_registry !=
              engaged_site.domain_without_registry) {
            return engaged_site.domain_and_registry;
          }
        }
      }
    }
  }
  return std::string();
}

// Returns true if |current_url| is at the end of the redirect chain
// stored in |stored_redirect_chain|.
bool IsInterstitialReload(const GURL& current_url,
                          const std::vector<GURL>& stored_redirect_chain) {
  return stored_redirect_chain.size() > 1 &&
         stored_redirect_chain[stored_redirect_chain.size() - 1] == current_url;
}

void RecordUMAFromMatchType(MatchType match_type) {
  switch (match_type) {
    case MatchType::kTopSite:
      RecordEvent(LookalikeUrlNavigationThrottle::NavigationSuggestionEvent::
                      kMatchTopSite);
      break;
    case MatchType::kSiteEngagement:
      RecordEvent(LookalikeUrlNavigationThrottle::NavigationSuggestionEvent::
                      kMatchSiteEngagement);
      break;
    case MatchType::kEditDistance:
      RecordEvent(LookalikeUrlNavigationThrottle::NavigationSuggestionEvent::
                      kMatchEditDistance);
      break;
    case MatchType::kEditDistanceSiteEngagement:
      RecordEvent(LookalikeUrlNavigationThrottle::NavigationSuggestionEvent::
                      kMatchEditDistanceSiteEngagement);
      break;
    case MatchType::kNone:
      break;
  }
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

// static
const char LookalikeUrlNavigationThrottle::kHistogramName[] =
    "NavigationSuggestion.Event";

bool IsTopDomain(const DomainInfo& domain_info) {
  // Top domains are only accessible through their skeletons, so query the top
  // domains trie for each skeleton of this domain.
  for (const std::string& skeleton : domain_info.skeletons) {
    const TopDomainEntry top_domain =
        url_formatter::LookupSkeletonInTopDomains(skeleton);
    if (domain_info.domain_and_registry == top_domain.domain) {
      return true;
    }
  }
  return false;
}

bool IsEditDistanceAtMostOne(const base::string16& str1,
                             const base::string16& str2) {
  if (str1.size() > str2.size() + 1 || str2.size() > str1.size() + 1) {
    return false;
  }
  base::string16::const_iterator i = str1.begin();
  base::string16::const_iterator j = str2.begin();
  size_t edit_count = 0;
  while (i != str1.end() && j != str2.end()) {
    if (*i == *j) {
      i++;
      j++;
    } else {
      edit_count++;
      if (edit_count > 1) {
        return false;
      }

      if (str1.size() > str2.size()) {
        // First string is longer than the second. This can only happen if the
        // first string has an extra character.
        i++;
      } else if (str2.size() > str1.size()) {
        // Second string is longer than the first. This can only happen if the
        // second string has an extra character.
        j++;
      } else {
        // Both strings are the same length. This can only happen if the two
        // strings differ by a single character.
        i++;
        j++;
      }
    }
  }
  if (i != str1.end() || j != str2.end()) {
    // A character at the end did not match.
    edit_count++;
  }
  return edit_count <= 1;
}

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
    return true;
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
      interstitials_enabled_(base::FeatureList::IsEnabled(
          features::kLookalikeUrlNavigationSuggestionsUI)),
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
  if (profile_->AsTestingProfile()) {
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

  // If the URL is in the allowlist, don't show any warning.
  if (tab_storage->IsDomainAllowed(url.host())) {
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
  if (service->EngagedSitesNeedUpdating()) {
    service->ForceUpdateEngagedSites(
        base::BindOnce(&LookalikeUrlNavigationThrottle::PerformChecksDeferred,
                       weak_factory_.GetWeakPtr(), url, navigated_domain,
                       check_safe_redirect));
    // If we're not going to show an interstitial, there's no reason to delay
    // the navigation any further.
    if (!interstitials_enabled_) {
      return content::NavigationThrottle::PROCEED;
    }
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
    MatchType match_type) {
  content::NavigationHandle* handle = navigation_handle();
  content::WebContents* web_contents = handle->GetWebContents();

  auto controller = std::make_unique<LookalikeUrlControllerClient>(
      web_contents, url, safe_url);

  std::unique_ptr<LookalikeUrlInterstitialPage> blocking_page(
      new LookalikeUrlInterstitialPage(web_contents, safe_url, source_id,
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
  if (prerender::PrerenderContents::FromWebContents(web_contents)) {
    return nullptr;
  }

  // Otherwise, always insert the throttle for metrics recording.
  return std::make_unique<LookalikeUrlNavigationThrottle>(navigation_handle);
}

// static
bool LookalikeUrlNavigationThrottle::ShouldDisplayInterstitial(
    MatchType match_type,
    const DomainInfo& navigated_domain) {
  if (!base::FeatureList::IsEnabled(
          features::kLookalikeUrlNavigationSuggestionsUI)) {
    return false;
  }
  if (match_type == MatchType::kSiteEngagement) {
    return true;
  }
  return match_type == MatchType::kTopSite &&
         kEnableInterstitialForTopSites.Get() &&
         navigated_domain.idn_result.matching_top_domain.is_top_500;
}

bool LookalikeUrlNavigationThrottle::GetMatchingDomain(
    const DomainInfo& navigated_domain,
    const std::vector<DomainInfo>& engaged_sites,
    std::string* matched_domain,
    MatchType* match_type) {
  DCHECK(!navigated_domain.domain_and_registry.empty());
  DCHECK(matched_domain);
  DCHECK(match_type);

  if (navigated_domain.idn_result.has_idn_component) {
    // If the navigated domain is IDN, check its skeleton against engaged sites
    // and top domains.
    const std::string matched_engaged_domain =
        GetMatchingSiteEngagementDomain(engaged_sites, navigated_domain);
    if (!matched_engaged_domain.empty()) {
      *matched_domain = matched_engaged_domain;
      *match_type = MatchType::kSiteEngagement;
      return true;
    }

    if (!navigated_domain.idn_result.matching_top_domain.domain.empty()) {
      // In practice, this is not possible since the top domain list does not
      // contain IDNs, so domain_and_registry can't both have IDN and be a top
      // domain. Still, sanity check in case the top domain list changes in the
      // future.
      // At this point, navigated domain should not be a top domain.
      DCHECK_NE(navigated_domain.domain_and_registry,
                navigated_domain.idn_result.matching_top_domain.domain);
      *matched_domain = navigated_domain.idn_result.matching_top_domain.domain;
      *match_type = MatchType::kTopSite;
      return true;
    }
  }

  if (!url_formatter::top_domains::IsEditDistanceCandidate(
          navigated_domain.domain_and_registry)) {
    return false;
  }

  // If we can't find an exact top domain or an engaged site, try to find an
  // engaged domain within an edit distance of one.
  const std::string similar_engaged_domain =
      GetSimilarDomainFromEngagedSites(navigated_domain, engaged_sites);
  if (!similar_engaged_domain.empty() &&
      navigated_domain.domain_and_registry != similar_engaged_domain) {
    *matched_domain = similar_engaged_domain;
    *match_type = MatchType::kEditDistanceSiteEngagement;
    return true;
  }

  // Finally, try to find a top domain within an edit distance of one.
  const std::string similar_top_domain =
      GetSimilarDomainFromTop500(navigated_domain);
  if (!similar_top_domain.empty() &&
      navigated_domain.domain_and_registry != similar_top_domain) {
    *matched_domain = similar_top_domain;
    *match_type = MatchType::kEditDistance;
    return true;
  }
  return false;
}

void LookalikeUrlNavigationThrottle::PerformChecksDeferred(
    const GURL& url,
    const DomainInfo& navigated_domain,
    bool check_safe_redirect,
    const std::vector<DomainInfo>& engaged_sites) {
  ThrottleCheckResult result =
      PerformChecks(url, navigated_domain, check_safe_redirect, engaged_sites);

  if (!interstitials_enabled_) {
    return;
  }

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
  MatchType match_type;

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

  if (!GetMatchingDomain(navigated_domain, engaged_sites, &matched_domain,
                         &match_type)) {
    return content::NavigationThrottle::PROCEED;
  }
  DCHECK(!matched_domain.empty());

  RecordUMAFromMatchType(match_type);

  if (check_safe_redirect &&
      IsSafeRedirect(matched_domain, navigation_handle()->GetRedirectChain())) {
    return content::NavigationThrottle::PROCEED;
  }

  if (ShouldDisplayInterstitial(match_type, navigated_domain)) {
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
  LookalikeUrlInterstitialPage::RecordUkmEvent(
      source_id, match_type, UserAction::kInterstitialNotShown);

  return content::NavigationThrottle::PROCEED;
}
