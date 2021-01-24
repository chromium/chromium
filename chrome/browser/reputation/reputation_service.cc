// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reputation/reputation_service.h"

#include <cstddef>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/singleton.h"
#include "chrome/browser/lookalikes/lookalike_url_blocking_page.h"
#include "chrome/browser/lookalikes/lookalike_url_navigation_throttle.h"
#include "chrome/browser/lookalikes/lookalike_url_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/reputation/local_heuristics.h"
#include "chrome/browser/safe_browsing/user_interaction_observer.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/lookalikes/core/lookalike_url_util.h"
#include "components/reputation/core/safety_tips_config.h"
#include "components/security_state/core/features.h"
#include "components/security_state/core/security_state.h"
#include "components/url_formatter/spoof_checks/top_domains/top500_domains.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/url_constants.h"

namespace {

using security_state::SafetyTipStatus;

// This factory helps construct and find the singleton ReputationService linked
// to a Profile.
class ReputationServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ReputationService* GetForProfile(Profile* profile) {
    return static_cast<ReputationService*>(
        GetInstance()->GetServiceForBrowserContext(profile,
                                                   /*create_service=*/true));
  }
  static ReputationServiceFactory* GetInstance() {
    return base::Singleton<ReputationServiceFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<ReputationServiceFactory>;

  ReputationServiceFactory()
      : BrowserContextKeyedServiceFactory(
            "ReputationServiceFactory",
            BrowserContextDependencyManager::GetInstance()) {}

  ~ReputationServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override {
    return new ReputationService(static_cast<Profile*>(profile));
  }

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override {
    return chrome::GetBrowserContextOwnInstanceInIncognito(context);
  }

  DISALLOW_COPY_AND_ASSIGN(ReputationServiceFactory);
};

// Returns whether or not the Safety Tip should be suppressed for the given URL.
// Checks SafeBrowsing-style permutations of |url| against the component updater
// allowlist, as well as any enterprise-set allowlisting of the hostname, and
// returns whether the URL is explicitly allowed. Fails closed, so that warnings
// are suppressed if the component is unavailable.
bool ShouldSuppressWarning(Profile* profile, const GURL& url) {
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
  return reputation::IsUrlAllowlistedBySafetyTipsComponent(proto, url);
}

// Gets the eTLD+1 of the provided hostname, including private registries (e.g.
// foo.blogspot.com returns blogspot.com.
std::string GetETLDPlusOneWithPrivateRegistries(const std::string& hostname) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      hostname, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

}  // namespace

ReputationService::ReputationService(Profile* profile)
    : profile_(profile),
      sensitive_keywords_(top500_domains::kTopKeywords),
      num_sensitive_keywords_(top500_domains::kNumTopKeywords) {}

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

void ReputationService::SetSensitiveKeywordsForTesting(
    const char* const* new_keywords,
    size_t num_new_keywords) {
  sensitive_keywords_ = new_keywords;
  num_sensitive_keywords_ = num_new_keywords;
}

void ReputationService::GetReputationStatusWithEngagedSites(
    const GURL& url,
    bool has_delayed_warning,
    ReputationCheckCallback callback,
    const std::vector<DomainInfo>& engaged_sites) {
  const DomainInfo navigated_domain = GetDomainInfo(url);

  ReputationCheckResult result;

  // We evaluate every heuristic for metrics, but only display the first result
  // for the UI. We use |done_checking_reputation_status| to track when we've
  // settled on the safety tip to show in the UI, so as to not overwrite this
  // decision with other heuristics that may trigger later.
  bool done_checking_reputation_status = false;

  // 0. Server-side warning suppression.
  // If the URL is on the allowlist list, do nothing else. This is only used to
  // mitigate false positives, so no further processing should be done.
  if (ShouldSuppressWarning(profile_, url)) {
    done_checking_reputation_status = true;
  }

  // 1. Engagement check
  // Ensure that this URL is not already engaged. We can't use the synchronous
  // SiteEngagementService::IsEngagementAtLeast as it has side effects.  This
  // check intentionally ignores the scheme.
  const auto already_engaged =
      std::find_if(engaged_sites.begin(), engaged_sites.end(),
                   [navigated_domain](const DomainInfo& engaged_domain) {
                     return (navigated_domain.domain_and_registry ==
                             engaged_domain.domain_and_registry);
                   });
  if (already_engaged != engaged_sites.end()) {
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

  // 3. Protect against bad false positives by allowing top domains.
  // Empty domain_and_registry happens on private domains.
  if (navigated_domain.domain_and_registry.empty() ||
      IsTopDomain(navigated_domain)) {
    done_checking_reputation_status = true;
  }

  // 4. Lookalike heuristics.
  GURL safe_url;
  if (already_engaged == engaged_sites.end() &&
      ShouldTriggerSafetyTipFromLookalike(url, navigated_domain, engaged_sites,
                                          &safe_url)) {
    if (!done_checking_reputation_status) {
      result.suggested_url = safe_url;
      result.safety_tip_status = SafetyTipStatus::kLookalike;
    }

    result.triggered_heuristics.lookalike_heuristic_triggered = true;
    done_checking_reputation_status = true;
  }

  // 5. Keyword heuristics.
  if (ShouldTriggerSafetyTipFromKeywordInURL(url, navigated_domain,
                                             sensitive_keywords_,
                                             num_sensitive_keywords_)) {
    if (!done_checking_reputation_status) {
      result.safety_tip_status = SafetyTipStatus::kBadKeyword;
    }

    result.triggered_heuristics.keywords_heuristic_triggered = true;
    done_checking_reputation_status = true;
  }

  // 6. This case is an experimental variation on Safe Browsing delayed warnings
  // (https://crbug.com/1057157) to measure the effect of simplified domain
  // display (https://crbug.com/1090393). In this experiment, Chrome delays Safe
  // Browsing warnings until user interaction to see if the simplified domain
  // display UI treatment affects how people interact with the page. In this
  // variation, Chrome shows a Safety Tip on such pages, to try to isolate the
  // effect of the UI treatment to when people's attention is drawn to the
  // omnibox.
  if (has_delayed_warning &&
      base::FeatureList::IsEnabled(
          security_state::features::kSafetyTipUIOnDelayedWarning)) {
    // Intentionally don't check |done_checking_reputation_status| here, as we
    // want this Safety Tip to take precedence. In this case, where there is a
    // delayed Safe Browsing warning, we know the page is actually suspicious.
    result.safety_tip_status = SafetyTipStatus::kBadReputation;
    result.triggered_heuristics.blocklist_heuristic_triggered = true;
    done_checking_reputation_status = true;
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
}
