// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/third_party_cookie_deprecation_page_load_metrics_observer.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_onboarding_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/experiment/experiment_manager_impl.h"
#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

namespace {

using ThirdPartyCookieAllowMechanism =
    content_settings::CookieSettingsBase::ThirdPartyCookieAllowMechanism;
using OnboardingStatus =
    privacy_sandbox::TrackingProtectionOnboarding::OnboardingStatus;

bool IsSameSite(const GURL& url1, const GURL& url2) {
  return url1.SchemeIs(url2.scheme()) &&
         net::registry_controlled_domains::SameDomainOrHost(
             url1, url2,
             net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

}  // namespace

ThirdPartyCookieDeprecationMetricsObserver::
    ThirdPartyCookieDeprecationMetricsObserver(
        content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  experiment_manager_ =
      tpcd::experiment::ExperimentManagerImpl::GetForProfile(profile);
  cookie_settings_ = CookieSettingsFactory::GetForProfile(profile);
  tracking_protection_onboarding_ =
      TrackingProtectionOnboardingFactory::GetForProfile(profile);
}

ThirdPartyCookieDeprecationMetricsObserver::
    ~ThirdPartyCookieDeprecationMetricsObserver() = default;

const char* ThirdPartyCookieDeprecationMetricsObserver::GetObserverName()
    const {
  static const char kName[] = "ThirdPartyCookieDeprecationMetricsObserver";
  return kName;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ThirdPartyCookieDeprecationMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // TODO(victortan): confirm whether we need to collect metrics on Prerendering
  // cases.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ThirdPartyCookieDeprecationMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // OnCookies{Read|Change} need the observer-side forwarding.
  return FORWARD_OBSERVING;
}

void ThirdPartyCookieDeprecationMetricsObserver::OnCookiesRead(
    const GURL& url,
    const GURL& first_party_url,
    bool blocked_by_policy,
    bool is_ad_tagged,
    const net::CookieSettingOverrides& cookie_setting_overrides) {
  RecordCookieUseCounters(url, first_party_url, blocked_by_policy,
                          cookie_setting_overrides);
  RecordCookieReadUseCounters(url, first_party_url, blocked_by_policy,
                              is_ad_tagged);
}

void ThirdPartyCookieDeprecationMetricsObserver::OnCookieChange(
    const GURL& url,
    const GURL& first_party_url,
    const net::CanonicalCookie& cookie,
    bool blocked_by_policy,
    bool is_ad_tagged,
    const net::CookieSettingOverrides& cookie_setting_overrides) {
  RecordCookieUseCounters(url, first_party_url, blocked_by_policy,
                          cookie_setting_overrides);
}

void ThirdPartyCookieDeprecationMetricsObserver::RecordCookieUseCounters(
    const GURL& url,
    const GURL& first_party_url,
    bool blocked_by_policy,
    const net::CookieSettingOverrides& cookie_setting_overrides) {
  if (blocked_by_policy || !IsThirdParty(url, first_party_url)) {
    return;
  }

  // Record third party cookie metrics if the access is blocked by third
  // party cookies deprecation experiment when some mechanism re-enable the
  // third party cookie access.
  bool is_blocked_by_experiment = IsBlockedByThirdPartyDeprecationExperiment();
  UMA_HISTOGRAM_BOOLEAN(
      "PageLoad.Clients.TPCD.ThirdPartyCookieAccessBlockedByExperiment2",
      is_blocked_by_experiment);

  const ThirdPartyCookieAllowMechanism allow_mechanism =
      cookie_settings_->GetThirdPartyCookieAllowMechanism(
          url, first_party_url, cookie_setting_overrides);
  if (allow_mechanism != ThirdPartyCookieAllowMechanism::kNone) {
    UMA_HISTOGRAM_ENUMERATION(
        "PageLoad.Clients.TPCD.CookieAccess.ThirdPartyCookieAllowMechanism",
        allow_mechanism);
  }

  if (!is_blocked_by_experiment) {
    return;
  }

  // Record the following blink feature usage cookie metrics when the 3PCD
  // experiment is actual block third party cookies, which means tracking
  // protection is onboard.
  std::vector<blink::mojom::WebFeature> third_party_cookie_features;
  third_party_cookie_features.push_back(
      blink::mojom::WebFeature::kThirdPartyCookieAccessBlockByExperiment);

  switch (allow_mechanism) {
    case ThirdPartyCookieAllowMechanism::kAllowByExplicitSetting:
      third_party_cookie_features.push_back(
          blink::mojom::WebFeature::
              kThirdPartyCookieDeprecation_AllowByExplicitSetting);
      break;
    case ThirdPartyCookieAllowMechanism::kAllowByGlobalSetting:
      third_party_cookie_features.push_back(
          blink::mojom::WebFeature::
              kThirdPartyCookieDeprecation_AllowByGlobalSetting);
      break;
    case ThirdPartyCookieAllowMechanism::kAllowBy3PCDMetadata:
      third_party_cookie_features.push_back(
          blink::mojom::WebFeature::
              kThirdPartyCookieDeprecation_AllowBy3PCDMetadata);
      break;
    case ThirdPartyCookieAllowMechanism::kAllowBy3PCD:
      third_party_cookie_features.push_back(
          blink::mojom::WebFeature::kThirdPartyCookieDeprecation_AllowBy3PCD);
      break;
    case ThirdPartyCookieAllowMechanism::kAllowBy3PCDHeuristics:
      third_party_cookie_features.push_back(
          blink::mojom::WebFeature::
              kThirdPartyCookieDeprecation_AllowBy3PCDHeuristics);
      break;
    case ThirdPartyCookieAllowMechanism::kAllowByStorageAccess:
      third_party_cookie_features.push_back(
          blink::mojom::WebFeature::
              kThirdPartyCookieDeprecation_AllowByStorageAccess);
      break;
    case ThirdPartyCookieAllowMechanism::kAllowByTopLevelStorageAccess:
      third_party_cookie_features.push_back(
          blink::mojom::WebFeature::
              kThirdPartyCookieDeprecation_AllowByTopLevelStorageAccess);
      break;
    default:
      // No feature usage recorded for unknow mechanism values.
      break;
  }

  // Report the feature usage if there's anything to report.
  if (third_party_cookie_features.size() > 0) {
    page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(
        GetDelegate().GetWebContents()->GetPrimaryMainFrame(),
        std::move(third_party_cookie_features));
  }
}

void ThirdPartyCookieDeprecationMetricsObserver::RecordCookieReadUseCounters(
    const GURL& url,
    const GURL& first_party_url,
    bool blocked_by_policy,
    bool is_ad_tagged) {
  if (blocked_by_policy || !IsThirdParty(url, first_party_url)) {
    return;
  }

  bool is_blocked_by_experiment = IsBlockedByThirdPartyDeprecationExperiment();
  if (is_ad_tagged) {
    UMA_HISTOGRAM_BOOLEAN(
        "PageLoad.Clients.TPCD.AdTPCAccess.BlockedByExperiment2",
        is_blocked_by_experiment);
  }
  if (!is_blocked_by_experiment) {
    return;
  }
  UMA_HISTOGRAM_BOOLEAN(
      "PageLoad.Clients.TPCD.TPCAccess.BlockedByExperiment.IsAdOrNonAd2",
      is_ad_tagged);

  if (is_ad_tagged) {
    page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(
        GetDelegate().GetWebContents()->GetPrimaryMainFrame(),
        std::vector<blink::mojom::WebFeature>{
            blink::mojom::WebFeature::
                kThirdPartyCookieAdAccessBlockByExperiment});
  }
}

bool ThirdPartyCookieDeprecationMetricsObserver::IsThirdParty(
    const GURL& url,
    const GURL& first_party_url) {
  // TODO(victortan): Optimize the domain lookup.
  // See comments for GetThirdPartyInfo() in //components layer
  // third_party_metrics_observer.h.
  if (!url.is_valid() || IsSameSite(url, first_party_url)) {
    return false;
  }

  std::string registrable_domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  if (registrable_domain.empty() && !url.has_host()) {
    return false;
  }
  return true;
}

bool ThirdPartyCookieDeprecationMetricsObserver::
    IsBlockedByThirdPartyDeprecationExperiment() {
  if (!base::FeatureList::IsEnabled(
          features::kCookieDeprecationFacilitatedTesting)) {
    return false;
  }

  // Only record the metric when cookie deprecation label onboarding since third
  // party cookie is not really disabled before onboarding.
  return experiment_manager_ &&
         experiment_manager_->IsClientEligible() == true &&
         tpcd::experiment::kDisable3PCookies.Get() &&
         tracking_protection_onboarding_ &&
         tracking_protection_onboarding_->GetOnboardingStatus() ==
             OnboardingStatus::kOnboarded;
  ;
}
