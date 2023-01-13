// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/safety_tip_service.h"

#include <cstddef>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/lookalikes/lookalike_url_blocking_page.h"
#include "chrome/browser/lookalikes/lookalike_url_navigation_throttle.h"
#include "chrome/browser/lookalikes/lookalike_url_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/safe_browsing/user_interaction_observer.h"
#include "chrome/common/channel_info.h"
#include "components/lookalikes/core/lookalike_url_util.h"
#include "components/lookalikes/core/safety_tips_config.h"
#include "components/security_state/core/security_state.h"
#include "components/url_formatter/spoof_checks/top_domains/top500_domains.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/url_constants.h"

using lookalikes::DomainInfo;
using lookalikes::LookalikeUrlMatchType;

namespace {

using security_state::SafetyTipStatus;

// This factory helps construct and find the singleton SafetyTipService linked
// to a Profile.
class SafetyTipServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static SafetyTipService* GetForProfile(Profile* profile) {
    return static_cast<SafetyTipService*>(
        GetInstance()->GetServiceForBrowserContext(profile,
                                                   /*create_service=*/true));
  }
  static SafetyTipServiceFactory* GetInstance() {
    return base::Singleton<SafetyTipServiceFactory>::get();
  }

  SafetyTipServiceFactory(const SafetyTipServiceFactory&) = delete;
  SafetyTipServiceFactory& operator=(const SafetyTipServiceFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<SafetyTipServiceFactory>;

  SafetyTipServiceFactory()
      : ProfileKeyedServiceFactory(
            "SafetyTipServiceFactory",
            ProfileSelections::BuildForRegularAndIncognito()) {}

  ~SafetyTipServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override {
    return new SafetyTipService(static_cast<Profile*>(profile));
  }
};

// Returns whether or not the Safety Tip should be suppressed on the given URL,
// if it's accused of spoofing |victim_url|. Checks both against the component
// updater allowlist, as well as any enterprise-set allowlist.  Fails closed, so
// that warnings are suppressed if the component is unavailable.
bool ShouldSuppressWarning(Profile* profile,
                           const GURL& url,
                           const GURL& victim_url) {
  // Check any policy-set allowlist.
  if (lookalikes::IsAllowedByEnterprisePolicy(profile->GetPrefs(), url)) {
    return true;
  }

  auto* proto = lookalikes::GetSafetyTipsRemoteConfigProto();
  if (!proto) {
    // This happens when the component hasn't downloaded yet. This should only
    // happen for a short time after initial upgrade to M79.
    //
    // Disable all Safety Tips during that time. Otherwise, we would continue to
    // flag on any known false positives until the client received the update.
    return true;
  }
  return lookalikes::IsUrlAllowlistedBySafetyTipsComponent(
      proto, url.GetWithEmptyPath(), victim_url.GetWithEmptyPath());
}

// Gets the eTLD+1 of the provided hostname, including private registries (e.g.
// foo.blogspot.com returns blogspot.com.
std::string GetETLDPlusOneWithPrivateRegistries(const std::string& hostname) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      hostname, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

bool ShouldTriggerSafetyTipFromLookalike(
    const GURL& url,
    const DomainInfo& navigated_domain,
    const std::vector<DomainInfo>& engaged_sites,
    GURL* safe_url,
    LookalikeUrlMatchType* match_type) {
  if (navigated_domain.domain_and_registry.empty()) {
    return false;
  }

  std::string matched_domain;
  auto* config = lookalikes::GetSafetyTipsRemoteConfigProto();
  const lookalikes::LookalikeTargetAllowlistChecker in_target_allowlist =
      base::BindRepeating(
          &lookalikes::IsTargetHostAllowlistedBySafetyTipsComponent, config);
  if (!GetMatchingDomain(navigated_domain, engaged_sites, in_target_allowlist,
                         config, &matched_domain, match_type)) {
    return false;
  }

  if (GetActionForMatchType(
          config, chrome::GetChannel(), navigated_domain.domain_and_registry,
          *match_type) == lookalikes::LookalikeActionType::kShowSafetyTip) {
    *safe_url = GetSuggestedURL(*match_type, url, matched_domain);
    return true;
  }
  return false;
}

}  // namespace

SafetyTipService::SafetyTipService(Profile* profile) : profile_(profile) {}

SafetyTipService::~SafetyTipService() = default;

// static
SafetyTipService* SafetyTipService::Get(Profile* profile) {
  return SafetyTipServiceFactory::GetForProfile(profile);
}

void SafetyTipService::GetSafetyTipStatus(const GURL& url,
                                          content::WebContents* web_contents,
                                          SafetyTipCheckCallback callback) {
  DCHECK(url.SchemeIsHTTPOrHTTPS());
  LookalikeUrlService* service = LookalikeUrlService::Get(profile_);
  if (service->EngagedSitesNeedUpdating()) {
    service->ForceUpdateEngagedSites(
        base::BindOnce(&SafetyTipService::GetSafetyTipStatusWithEngagedSites,
                       weak_factory_.GetWeakPtr(), url, std::move(callback)));
    // If the engaged sites need updating, there's nothing to do until callback.
    return;
  }

  GetSafetyTipStatusWithEngagedSites(url, std::move(callback),
                                     service->GetLatestEngagedSites());
}

bool SafetyTipService::IsIgnored(const GURL& url) const {
  return warning_dismissed_etld1s_.count(
             GetETLDPlusOneWithPrivateRegistries(url.host())) > 0;
}

void SafetyTipService::SetUserIgnore(const GURL& url) {
  warning_dismissed_etld1s_.insert(
      GetETLDPlusOneWithPrivateRegistries(url.host()));
}

void SafetyTipService::OnUIDisabledFirstVisit(const GURL& url) {
  warning_dismissed_etld1s_.insert(
      GetETLDPlusOneWithPrivateRegistries(url.host()));
}

void SafetyTipService::ResetWarningDismissedETLDPlusOnesForTesting() {
  warning_dismissed_etld1s_.clear();
}

void SafetyTipService::GetSafetyTipStatusWithEngagedSites(
    const GURL& url,
    SafetyTipCheckCallback callback,
    const std::vector<DomainInfo>& engaged_sites) {
  base::TimeTicks start = base::TimeTicks::Now();

  const DomainInfo navigated_domain = lookalikes::GetDomainInfo(url);

  UMA_HISTOGRAM_TIMES("Security.SafetyTips.GetDomainInfoTime",
                      base::TimeTicks::Now() - start);

  SafetyTipCheckResult result;

  // We evaluate every heuristic for metrics, but only display the first result
  // for the UI. We use |done_checking_safety_tip_status| to track when we've
  // settled on the safety tip to show in the UI, so as to not overwrite this
  // decision with other heuristics that may trigger later.
  bool done_checking_safety_tip_status = false;

  // 1. Engagement check
  // Ensure that this URL is not already engaged. We can't use the synchronous
  // SiteEngagementService::IsEngagementAtLeast as it has side effects.  This
  // check intentionally ignores the scheme.
  const bool already_engaged =
      base::Contains(engaged_sites, navigated_domain.domain_and_registry,
                     &DomainInfo::domain_and_registry);
  if (already_engaged) {
    done_checking_safety_tip_status = true;
  }

  // 2. Protect against bad false positives by allowing top domains and safe
  // TLDs. Empty domain_and_registry happens on private domains.
  if (navigated_domain.domain_and_registry.empty() ||
      lookalikes::IsTopDomain(navigated_domain) ||
      lookalikes::IsSafeTLD(navigated_domain.domain_and_registry)) {
    done_checking_safety_tip_status = true;
  }

  // 3. Lookalike heuristics.
  GURL safe_url;
  LookalikeUrlMatchType match_type;
  if (!already_engaged &&
      ShouldTriggerSafetyTipFromLookalike(url, navigated_domain, engaged_sites,
                                          &safe_url, &match_type)) {
    if (!done_checking_safety_tip_status) {
      result.suggested_url = safe_url;
      result.safety_tip_status = SafetyTipStatus::kLookalike;
    }
    result.lookalike_heuristic_triggered = true;
    done_checking_safety_tip_status = true;
  }
  DCHECK(result.safety_tip_status != SafetyTipStatus::kBadKeyword);
  DCHECK(result.safety_tip_status != SafetyTipStatus::kBadReputation);

  // If we found a SafetyTipStatus, possibly clear it if the URL is on the
  // allowlist.
  if (result.safety_tip_status != SafetyTipStatus::kUnknown &&
      result.safety_tip_status != SafetyTipStatus::kNone &&
      ShouldSuppressWarning(profile_, url, result.suggested_url)) {
    result.safety_tip_status = SafetyTipStatus::kNone;
    result.suggested_url = GURL();
  }

  if (IsIgnored(url)) {
    if (result.safety_tip_status == SafetyTipStatus::kLookalike) {
      result.safety_tip_status = SafetyTipStatus::kLookalikeIgnored;
    }
    // The local allowlist is used by both the interstitial and safety tips, so
    // it's possible to hit this case even when we're not in the conditions
    // above. It's also possible to get kNone here when a domain is added to
    // the server-side allowlist after it has been ignored. In these cases,
    // there's no additional action required.
  }
  result.url = url;

  DCHECK(done_checking_safety_tip_status ||
         !result.lookalike_heuristic_triggered);
  std::move(callback).Run(result);

  UMA_HISTOGRAM_TIMES(
      "Security.SafetyTips.GetReputationStatusWithEngagedSitesTime",
      base::TimeTicks::Now() - start);
}
