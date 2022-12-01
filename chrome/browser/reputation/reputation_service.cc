// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reputation/reputation_service.h"

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
#include "chrome/browser/reputation/local_heuristics.h"
#include "chrome/browser/safe_browsing/user_interaction_observer.h"
#include "components/lookalikes/core/lookalike_url_util.h"
#include "components/reputation/core/safety_tips_config.h"
#include "components/security_state/core/security_state.h"
#include "components/url_formatter/spoof_checks/top_domains/top500_domains.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/url_constants.h"

namespace {

using security_state::SafetyTipStatus;

// This factory helps construct and find the singleton ReputationService linked
// to a Profile.
class ReputationServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ReputationService* GetForProfile(Profile* profile) {
    return static_cast<ReputationService*>(
        GetInstance()->GetServiceForBrowserContext(profile,
                                                   /*create_service=*/true));
  }
  static ReputationServiceFactory* GetInstance() {
    return base::Singleton<ReputationServiceFactory>::get();
  }

  ReputationServiceFactory(const ReputationServiceFactory&) = delete;
  ReputationServiceFactory& operator=(const ReputationServiceFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<ReputationServiceFactory>;

  ReputationServiceFactory()
      : ProfileKeyedServiceFactory(
            "ReputationServiceFactory",
            ProfileSelections::BuildForRegularAndIncognito()) {}

  ~ReputationServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override {
    return new ReputationService(static_cast<Profile*>(profile));
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
  if (IsAllowedByEnterprisePolicy(profile->GetPrefs(), url)) {
    return true;
  }

  auto* proto = reputation::GetSafetyTipsRemoteConfigProto();
  if (!proto) {
    // This happens when the component hasn't downloaded yet. This should only
    // happen for a short time after initial upgrade to M79.
    //
    // Disable all Safety Tips during that time. Otherwise, we would continue to
    // flag on any known false positives until the client received the update.
    return true;
  }
  return reputation::IsUrlAllowlistedBySafetyTipsComponent(
      proto, url.GetWithEmptyPath(), victim_url.GetWithEmptyPath());
}

// Gets the eTLD+1 of the provided hostname, including private registries (e.g.
// foo.blogspot.com returns blogspot.com.
std::string GetETLDPlusOneWithPrivateRegistries(const std::string& hostname) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      hostname, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

}  // namespace

ReputationService::ReputationService(Profile* profile) : profile_(profile) {}

ReputationService::~ReputationService() = default;

// static
ReputationService* ReputationService::Get(Profile* profile) {
  return ReputationServiceFactory::GetForProfile(profile);
}

void ReputationService::GetReputationStatus(const GURL& url,
                                            content::WebContents* web_contents,
                                            ReputationCheckCallback callback) {
  DCHECK(url.SchemeIsHTTPOrHTTPS());

  bool has_delayed_warning =
      !!safe_browsing::SafeBrowsingUserInteractionObserver::FromWebContents(
          web_contents);

  LookalikeUrlService* service = LookalikeUrlService::Get(profile_);
  if (service->EngagedSitesNeedUpdating()) {
    service->ForceUpdateEngagedSites(
        base::BindOnce(&ReputationService::GetReputationStatusWithEngagedSites,
                       weak_factory_.GetWeakPtr(), url, has_delayed_warning,
                       std::move(callback)));
    // If the engaged sites need updating, there's nothing to do until callback.
    return;
  }

  GetReputationStatusWithEngagedSites(url, has_delayed_warning,
                                      std::move(callback),
                                      service->GetLatestEngagedSites());
}

bool ReputationService::IsIgnored(const GURL& url) const {
  return warning_dismissed_etld1s_.count(
             GetETLDPlusOneWithPrivateRegistries(url.host())) > 0;
}

void ReputationService::SetUserIgnore(const GURL& url) {
  warning_dismissed_etld1s_.insert(
      GetETLDPlusOneWithPrivateRegistries(url.host()));
}

void ReputationService::OnUIDisabledFirstVisit(const GURL& url) {
  warning_dismissed_etld1s_.insert(
      GetETLDPlusOneWithPrivateRegistries(url.host()));
}

void ReputationService::ResetWarningDismissedETLDPlusOnesForTesting() {
  warning_dismissed_etld1s_.clear();
}

void ReputationService::GetReputationStatusWithEngagedSites(
    const GURL& url,
    bool has_delayed_warning,
    ReputationCheckCallback callback,
    const std::vector<DomainInfo>& engaged_sites) {
  base::TimeTicks start = base::TimeTicks::Now();

  const DomainInfo navigated_domain = GetDomainInfo(url);

  UMA_HISTOGRAM_TIMES("Security.SafetyTips.GetDomainInfoTime",
                      base::TimeTicks::Now() - start);

  ReputationCheckResult result;

  // We evaluate every heuristic for metrics, but only display the first result
  // for the UI. We use |done_checking_reputation_status| to track when we've
  // settled on the safety tip to show in the UI, so as to not overwrite this
  // decision with other heuristics that may trigger later.
  bool done_checking_reputation_status = false;

  // 1. Engagement check
  // Ensure that this URL is not already engaged. We can't use the synchronous
  // SiteEngagementService::IsEngagementAtLeast as it has side effects.  This
  // check intentionally ignores the scheme.
  const bool already_engaged =
      base::Contains(engaged_sites, navigated_domain.domain_and_registry,
                     &DomainInfo::domain_and_registry);
  if (already_engaged) {
    done_checking_reputation_status = true;
  }

  // 2. Server-side blocklist check.
  SafetyTipStatus status = reputation::GetSafetyTipUrlBlockType(url);
  if (status != SafetyTipStatus::kNone) {
    if (!done_checking_reputation_status) {
      result.safety_tip_status = status;
    }

    result.triggered_heuristics.blocklist_heuristic_triggered = true;
    done_checking_reputation_status = true;
  }

  // 3. Protect against bad false positives by allowing top domains and safe
  // TLDs. Empty domain_and_registry happens on private domains.
  if (navigated_domain.domain_and_registry.empty() ||
      IsTopDomain(navigated_domain) ||
      IsSafeTLD(navigated_domain.domain_and_registry)) {
    done_checking_reputation_status = true;
  }

  // 4. Lookalike heuristics.
  GURL safe_url;
  if (!already_engaged &&
      ShouldTriggerSafetyTipFromLookalike(url, navigated_domain, engaged_sites,
                                          &safe_url)) {
    if (!done_checking_reputation_status) {
      result.suggested_url = safe_url;
      result.safety_tip_status = SafetyTipStatus::kLookalike;
    }

    result.triggered_heuristics.lookalike_heuristic_triggered = true;
    done_checking_reputation_status = true;
  }
  DCHECK(result.safety_tip_status != SafetyTipStatus::kBadKeyword);

  // If we found a SafetyTipStatus, possibly clear it if the URL is on the
  // allowlist.
  if (result.safety_tip_status != SafetyTipStatus::kUnknown &&
      result.safety_tip_status != SafetyTipStatus::kNone &&
      ShouldSuppressWarning(profile_, url, result.suggested_url)) {
    result.safety_tip_status = SafetyTipStatus::kNone;
    result.suggested_url = GURL();
  }

  if (IsIgnored(url)) {
    if (result.safety_tip_status == SafetyTipStatus::kBadReputation) {
      result.safety_tip_status = SafetyTipStatus::kBadReputationIgnored;
    } else if (result.safety_tip_status == SafetyTipStatus::kLookalike) {
      result.safety_tip_status = SafetyTipStatus::kLookalikeIgnored;
    }
    // The local allowlist is used by both the interstitial and safety tips, so
    // it's possible to hit this case even when we're not in the conditions
    // above. It's also possible to get kNone here when a domain is added to
    // the server-side allowlist after it has been ignored. In these cases,
    // there's no additional action required.
  }
  result.url = url;

  DCHECK(done_checking_reputation_status ||
         !result.triggered_heuristics.triggered_any());
  std::move(callback).Run(result);

  UMA_HISTOGRAM_TIMES(
      "Security.SafetyTips.GetReputationStatusWithEngagedSitesTime",
      base::TimeTicks::Now() - start);
}
