// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reputation/reputation_service.h"

#include <string>
#include <utility>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/singleton.h"
#include "chrome/browser/lookalikes/lookalike_url_interstitial_page.h"
#include "chrome/browser/lookalikes/lookalike_url_navigation_throttle.h"
#include "chrome/browser/lookalikes/lookalike_url_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/reputation/local_heuristics.h"
#include "chrome/browser/reputation/safety_tip_ui_helper.h"
#include "chrome/browser/reputation/safety_tips_config.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"
#include "components/security_state/core/security_state.h"
#include "components/url_formatter/spoof_checks/top_domains/top500_domains.h"
#include "url/url_constants.h"

namespace {

using chrome_browser_safety_tips::FlaggedPage;
using chrome_browser_safety_tips::UrlPattern;
using safe_browsing::V4ProtocolManagerUtil;
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

// Given a URL, generates all possible variant URLs to check the blocklist for.
// This is conceptually almost identical to safe_browsing::UrlToFullHashes, but
// without the hashing step.
//
// Note: Blocking "a.b/c/" does NOT block http://a.b/c without the trailing /.
void UrlToPatterns(const GURL& url, std::vector<std::string>* patterns) {
  std::string canon_host;
  std::string canon_path;
  std::string canon_query;
  V4ProtocolManagerUtil::CanonicalizeUrl(url, &canon_host, &canon_path,
                                         &canon_query);

  std::vector<std::string> hosts;
  if (url.HostIsIPAddress()) {
    hosts.push_back(url.host());
  } else {
    V4ProtocolManagerUtil::GenerateHostVariantsToCheck(canon_host, &hosts);
  }

  std::vector<std::string> paths;
  V4ProtocolManagerUtil::GeneratePathVariantsToCheck(canon_path, canon_query,
                                                     &paths);

  for (const std::string& host : hosts) {
    for (const std::string& path : paths) {
      DCHECK(path.length() == 0 || path[0] == '/');
      patterns->push_back(host + path);
    }
  }
}

security_state::SafetyTipStatus FlagTypeToSafetyTipStatus(
    FlaggedPage::FlagType type) {
  switch (type) {
    case FlaggedPage::FlagType::FlaggedPage_FlagType_UNKNOWN:
    case FlaggedPage::FlagType::FlaggedPage_FlagType_YOUNG_DOMAIN:
      // Reached if component includes these flags, which might happen to
      // support newer Chrome releases.
      return security_state::SafetyTipStatus::kNone;
    case FlaggedPage::FlagType::FlaggedPage_FlagType_BAD_REP:
      return security_state::SafetyTipStatus::kBadReputation;
  }
  NOTREACHED();
  return security_state::SafetyTipStatus::kNone;
}

// Returns whether or not the Safety Tip should be suppressed for the given URL.
// Checks SafeBrowsing-style permutations of |url| against the component updater
// allowlist and returns whether the URL is explicitly allowed. Fails closed, so
// that warnings are suppressed if the component is unavailable.
bool ShouldSuppressWarning(const GURL& url) {
  std::vector<std::string> patterns;
  UrlToPatterns(url, &patterns);

  auto* proto = GetSafetyTipsRemoteConfigProto();
  if (!proto) {
    // This happens when the component hasn't downloaded yet. This should only
    // happen for a short time after initial upgrade to M79.
    //
    // Disable all Safety Tips during that time. Otherwise, we would continue to
    // flag on any known false positives until the client received the update.
    return true;
  }

  auto allowed_pages = proto->allowed_pattern();
  for (const auto& pattern : patterns) {
    UrlPattern search_target;
    search_target.set_pattern(pattern);

    auto lower = std::lower_bound(
        allowed_pages.begin(), allowed_pages.end(), search_target,
        [](const UrlPattern& a, const UrlPattern& b) -> bool {
          return a.pattern() < b.pattern();
        });

    if (lower != allowed_pages.end() && pattern == lower->pattern()) {
      return true;
    }
  }

  return false;
}

}  // namespace

ReputationService::ReputationService(Profile* profile) : profile_(profile) {}

ReputationService::~ReputationService() = default;

// static
ReputationService* ReputationService::Get(Profile* profile) {
  return ReputationServiceFactory::GetForProfile(profile);
}

void ReputationService::GetReputationStatus(const GURL& url,
                                            ReputationCheckCallback callback) {
  DCHECK(url.SchemeIsHTTPOrHTTPS());

  LookalikeUrlService* service = LookalikeUrlService::Get(profile_);
  if (service->EngagedSitesNeedUpdating()) {
    service->ForceUpdateEngagedSites(
        base::BindOnce(&ReputationService::GetReputationStatusWithEngagedSites,
                       weak_factory_.GetWeakPtr(), std::move(callback), url));
    // If the engaged sites need updating, there's nothing to do until callback.
    return;
  }

  GetReputationStatusWithEngagedSites(std::move(callback), url,
                                      service->GetLatestEngagedSites());
}

void ReputationService::SetUserIgnore(content::WebContents* web_contents,
                                      const GURL& url,
                                      SafetyTipInteraction interaction) {
  // Record that the user dismissed the safety tip. kDismiss is the base case,
  // which makes it easier to track overall dismissal metrics without having
  // to re-constitute from separate histograms that record specifically how the
  // user dismissed the safety tip. The way the user dismissed the dialog is
  // also recorded to this interaction histogram, but with a more specific value
  // (e.g. kDismissWithEsc) that is passed into this method.
  RecordSafetyTipInteractionHistogram(web_contents,
                                      SafetyTipInteraction::kDismiss);
  // Record a histogram indicating how the user dismissed the safety tip
  // (i.e. esc key, close button, or ignore button).
  RecordSafetyTipInteractionHistogram(web_contents, interaction);
  warning_dismissed_origins_.insert(url::Origin::Create(url));
}

bool ReputationService::IsIgnored(const GURL& url) const {
  return warning_dismissed_origins_.count(url::Origin::Create(url)) > 0;
}

void ReputationService::GetReputationStatusWithEngagedSites(
    ReputationCheckCallback callback,
    const GURL& url,
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
  if (ShouldSuppressWarning(url)) {
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
  security_state::SafetyTipStatus status = GetSafetyTipUrlBlockType(url);
  if (status != security_state::SafetyTipStatus::kNone) {
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
      result.safety_tip_status = security_state::SafetyTipStatus::kLookalike;
    }

    result.triggered_heuristics.lookalike_heuristic_triggered = true;
    done_checking_reputation_status = true;
  }

  // 5. Keyword heuristics.
  if (ShouldTriggerSafetyTipFromKeywordInURL(
          url, navigated_domain, top500_domains::kTop500Keywords, 500)) {
    if (!done_checking_reputation_status) {
      result.safety_tip_status = security_state::SafetyTipStatus::kBadKeyword;
    }

    result.triggered_heuristics.keywords_heuristic_triggered = true;
    done_checking_reputation_status = true;
  }

  if (IsIgnored(url)) {
    if (result.safety_tip_status == SafetyTipStatus::kBadReputation) {
      result.safety_tip_status = SafetyTipStatus::kBadReputationIgnored;
    } else if (result.safety_tip_status == SafetyTipStatus::kLookalike) {
      result.safety_tip_status = SafetyTipStatus::kLookalikeIgnored;
    } else {
      // Only reputation or lookalikes should show bubbles.
      NOTREACHED();
    }
  }
  result.url = url;
  std::move(callback).Run(result);
}

security_state::SafetyTipStatus GetSafetyTipUrlBlockType(const GURL& url) {
  std::vector<std::string> patterns;
  UrlToPatterns(url, &patterns);

  auto* proto = GetSafetyTipsRemoteConfigProto();
  if (!proto) {
    return security_state::SafetyTipStatus::kNone;
  }

  auto flagged_pages = proto->flagged_page();
  for (const auto& pattern : patterns) {
    FlaggedPage search_target;
    search_target.set_pattern(pattern);

    auto lower = std::lower_bound(
        flagged_pages.begin(), flagged_pages.end(), search_target,
        [](const FlaggedPage& a, const FlaggedPage& b) -> bool {
          return a.pattern() < b.pattern();
        });

    while (lower != flagged_pages.end() && pattern == lower->pattern()) {
      // Skip over sites with unexpected flag types and keep looking for other
      // matches. This allows components to include flag types not handled by
      // this release.
      auto type = FlagTypeToSafetyTipStatus(lower->type());
      if (type != security_state::SafetyTipStatus::kNone) {
        return type;
      }
      ++lower;
    }
  }

  return security_state::SafetyTipStatus::kNone;
}
