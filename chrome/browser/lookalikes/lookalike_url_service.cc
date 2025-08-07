// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/lookalike_url_service.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_clock.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/safe_browsing/user_interaction_observer.h"
#include "chrome/common/channel_info.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/lookalikes/core/lookalike_url_util.h"
#include "components/lookalikes/core/safety_tips_config.h"
#include "components/security_state/core/security_state.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/url_formatter/spoof_checks/top_domains/top_domain_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/url_constants.h"

using lookalikes::DomainInfo;
using lookalikes::LookalikeActionType;
using lookalikes::LookalikeUrlMatchType;
using security_state::SafetyTipStatus;

namespace {

constexpr base::TimeDelta kEngagedSiteUpdateInterval = base::Seconds(60);

// static
std::vector<DomainInfo> UpdateEngagedSitesOnWorkerThread(
    base::Time now,
    scoped_refptr<HostContentSettingsMap> map) {
  TRACE_EVENT0("navigation",
               "LookalikeUrlService UpdateEngagedSitesOnWorkerThread");
  std::vector<DomainInfo> new_engaged_sites;

  auto details =
      site_engagement::SiteEngagementService::GetAllDetailsInBackground(now,
                                                                        map);
  TRACE_EVENT1("navigation", "LookalikeUrlService SiteEngagementService",
               "site_count", details.size());
  for (const site_engagement::mojom::SiteEngagementDetails& detail : details) {
    if (!detail.origin.SchemeIsHTTPOrHTTPS()) {
      continue;
    }
    // Ignore sites with an engagement score below threshold.
    if (!site_engagement::SiteEngagementService::IsEngagementAtLeast(
            detail.total_score, blink::mojom::EngagementLevel::MEDIUM)) {
      continue;
    }
    const DomainInfo domain_info = lookalikes::GetDomainInfo(detail.origin);
    if (domain_info.domain_and_registry.empty()) {
      continue;
    }
    new_engaged_sites.push_back(domain_info);
  }

  return new_engaged_sites;
}

// Gets the eTLD+1 of the provided hostname, including private registries (e.g.
// foo.blogspot.com returns blogspot.com.
std::string GetETLDPlusOneWithPrivateRegistries(const std::string& hostname) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      hostname, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

void RecordReputationStatusWithEngagedSitesTime(base::TimeTicks start) {
  UMA_HISTOGRAM_TIMES(
      "Security.SafetyTips.GetReputationStatusWithEngagedSitesTime",
      base::TimeTicks::Now() - start);
}

}  // namespace

LookalikeUrlService::LookalikeUrlService(
    PrefService* pref_service,
    HostContentSettingsMap* host_content_settings_map)
    : pref_service_(pref_service),
      host_content_settings_map_(host_content_settings_map),
      clock_(base::DefaultClock::GetInstance()) {}

LookalikeUrlService::~LookalikeUrlService() = default;

bool LookalikeUrlService::EngagedSitesNeedUpdating() const {
  if (last_engagement_fetch_time_.is_null())
    return true;
  const base::TimeDelta elapsed = clock_->Now() - last_engagement_fetch_time_;
  return elapsed >= kEngagedSiteUpdateInterval;
}

void LookalikeUrlService::ForceUpdateEngagedSites(
    EngagedSitesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("navigation", "LookalikeUrlService::ForceUpdateEngagedSites");

  // Queue an update on a worker thread if necessary.
  if (!update_in_progress_) {
    update_in_progress_ = true;

    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::TaskPriority::USER_BLOCKING,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&UpdateEngagedSitesOnWorkerThread, clock_->Now(),
                       base::WrapRefCounted<HostContentSettingsMap>(
                           host_content_settings_map_)),
        base::BindOnce(&LookalikeUrlService::OnUpdateEngagedSitesCompleted,
                       weak_factory_.GetWeakPtr()));
  }

  // Postpone the execution of the callback after the update is completed.
  pending_update_complete_callbacks_.push_back(std::move(callback));
}

void LookalikeUrlService::OnUpdateEngagedSitesCompleted(
    std::vector<DomainInfo> new_engaged_sites) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(update_in_progress_);
  TRACE_EVENT0("navigation",
               "LookalikeUrlService::OnUpdateEngagedSitesCompleted");
  engaged_sites_.swap(new_engaged_sites);
  last_engagement_fetch_time_ = clock_->Now();
  update_in_progress_ = false;

  // Call pending callbacks.
  std::vector<EngagedSitesCallback> callbacks;
  callbacks.swap(pending_update_complete_callbacks_);
  for (auto&& callback : callbacks) {
    std::move(callback).Run(engaged_sites_);
  }
}

const std::vector<DomainInfo> LookalikeUrlService::GetLatestEngagedSites()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return engaged_sites_;
}

void LookalikeUrlService::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

LookalikeUrlService::LookalikeUrlCheckResult
LookalikeUrlService::CheckUrlForLookalikes(
    const GURL& url,
    const std::vector<DomainInfo>& engaged_sites,
    bool stop_checking_on_allowlist_or_ignore) const {
  LookalikeUrlCheckResult result;

  // Don't warn on non-HTTP(s) sites or non-public domains.
  if (!url.SchemeIsHTTPOrHTTPS() || net::HostStringIsLocalhost(url.host()) ||
      net::IsHostnameNonUnique(url.host()) ||
      lookalikes::GetETLDPlusOne(url.host()).empty() ||
      lookalikes::IsSafeTLD(url.host())) {
    return result;
  }

  if (IsIgnored(url)) {
    result.is_warning_previously_dismissed = true;
    if (stop_checking_on_allowlist_or_ignore) {
      return result;
    }
  }

  // When there's no proto (like at browser start), fail-safe and don't block.
  const auto* proto = lookalikes::GetSafetyTipsRemoteConfigProto();
  if (!proto) {
    return result;
  }

  // If the host is allowlisted by policy, don't show any warning.
  if (lookalikes::IsAllowedByEnterprisePolicy(pref_service_, url)) {
    result.is_allowlisted = true;
    if (stop_checking_on_allowlist_or_ignore) {
      return result;
    }
  }

  // GetDomainInfo() is expensive, so do possible early-abort checks first.
  base::TimeTicks get_domain_info_start = base::TimeTicks::Now();
  const DomainInfo navigated_domain = lookalikes::GetDomainInfo(url);
  result.get_domain_info_duration =
      base::TimeTicks::Now() - get_domain_info_start;

  if (IsTopDomain(navigated_domain)) {
    return result;
  }

  // Ensure that this URL is not already engaged. We can't use the synchronous
  // SiteEngagementService::IsEngagementAtLeast as it has side effects. We check
  // in PerformChecks to ensure we have up-to-date engaged_sites. This check
  // ignores the scheme which is okay since it's more conservative: If the user
  // is engaged with http://domain.test, not showing the warning on
  // https://domain.test is acceptable.
  if (base::Contains(engaged_sites, navigated_domain.domain_and_registry,
                     &DomainInfo::domain_and_registry)) {
    return result;
  }

  const lookalikes::LookalikeTargetAllowlistChecker in_target_allowlist =
      base::BindRepeating(
          &lookalikes::IsTargetHostAllowlistedBySafetyTipsComponent, proto);
  std::string matched_domain;
  if (GetMatchingDomain(navigated_domain, engaged_sites, in_target_allowlist,
                        proto, &matched_domain, &result.match_type)) {
    DCHECK(!matched_domain.empty());
    result.suggested_url =
        GetSuggestedURL(result.match_type, url, matched_domain);

    if (lookalikes::IsUrlAllowlistedBySafetyTipsComponent(
            proto, url.GetWithEmptyPath(), result.suggested_url)) {
      result.is_allowlisted = true;
      if (stop_checking_on_allowlist_or_ignore) {
        return result;
      }
    }
    result.action_type = GetActionForMatchType(
        proto, chrome::GetChannel(), navigated_domain.domain_and_registry,
        result.match_type);
  } else if (ShouldBlockBySpoofCheckResult(navigated_domain)) {
    result.match_type = LookalikeUrlMatchType::kFailedSpoofChecks;
    result.suggested_url = GURL();

    // Domains that trigger spoof checking are allowlisted as if they were
    // spoofing themselves, so pass in the spoofing URL as the canonical.
    if (lookalikes::IsUrlAllowlistedBySafetyTipsComponent(
            proto, url.GetWithEmptyPath(), url)) {
      result.is_allowlisted = true;
      if (stop_checking_on_allowlist_or_ignore) {
        return result;
      }
    }
    result.action_type = GetActionForMatchType(
        proto, chrome::GetChannel(), navigated_domain.domain_and_registry,
        result.match_type);
  }

  return result;
}

void LookalikeUrlService::CheckSafetyTipStatus(
    const GURL& url,
    content::WebContents* web_contents,
    SafetyTipCheckCallback callback) {
  DCHECK(url.SchemeIsHTTPOrHTTPS());
  if (EngagedSitesNeedUpdating()) {
    ForceUpdateEngagedSites(base::BindOnce(
        &LookalikeUrlService::CheckSafetyTipStatusWithEngagedSites,
        weak_factory_.GetWeakPtr(), url, std::move(callback)));
    // If the engaged sites need updating, there's nothing to do until callback.
    return;
  }

  CheckSafetyTipStatusWithEngagedSites(url, std::move(callback),
                                       GetLatestEngagedSites());
}

void LookalikeUrlService::CheckSafetyTipStatusWithEngagedSites(
    const GURL& url,
    SafetyTipCheckCallback callback,
    const std::vector<DomainInfo>& engaged_sites) {
  base::TimeTicks start = base::TimeTicks::Now();

  LookalikeUrlCheckResult lookalike_result =
      CheckUrlForLookalikes(url, engaged_sites,
                            /*stop_checking_on_allowlist_or_ignore=*/false);

  SafetyTipCheckResult result;
  result.url = url;

  if (lookalike_result.action_type != LookalikeActionType::kShowSafetyTip) {
    std::move(callback).Run(result);
    RecordReputationStatusWithEngagedSitesTime(start);
    return;
  }

  result.safety_tip_status = SafetyTipStatus::kNone;
  result.suggested_url = lookalike_result.suggested_url;
  result.safety_tip_status = SafetyTipStatus::kLookalike;
  result.lookalike_heuristic_triggered = true;

  if (lookalike_result.is_allowlisted) {
    // This will record a UKM but it won't show a warning.
    result.safety_tip_status = SafetyTipStatus::kNone;
    std::move(callback).Run(result);
    RecordReputationStatusWithEngagedSitesTime(start);
    return;
  }

  if (lookalike_result.is_warning_previously_dismissed) {
    result.safety_tip_status = SafetyTipStatus::kLookalikeIgnored;
    // The local allowlist is used by both the interstitial and safety tips, so
    // it's possible to hit this case even when we're not in the conditions
    // above. It's also possible to get kNone here when a domain is added to
    // the server-side allowlist after it has been ignored. In these cases,
    // there's no additional action required.
  }
  std::move(callback).Run(result);
  RecordReputationStatusWithEngagedSitesTime(start);
}

bool LookalikeUrlService::IsIgnored(const GURL& url) const {
  return warning_dismissed_etld1s_.count(
             GetETLDPlusOneWithPrivateRegistries(url.host())) > 0;
}

void LookalikeUrlService::SetUserIgnore(const GURL& url) {
  warning_dismissed_etld1s_.insert(
      GetETLDPlusOneWithPrivateRegistries(url.host()));
}

void LookalikeUrlService::OnUIDisabledFirstVisit(const GURL& url) {
  warning_dismissed_etld1s_.insert(
      GetETLDPlusOneWithPrivateRegistries(url.host()));
}

void LookalikeUrlService::ResetWarningDismissedETLDPlusOnesForTesting() {
  warning_dismissed_etld1s_.clear();
}
