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
#include "net/cookies/site_for_cookies.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "url/gurl.h"

namespace {
using CookieSettingsBase = content_settings::CookieSettingsBase;
using ThirdPartyCookieAllowMechanism =
    CookieSettingsBase::ThirdPartyCookieAllowMechanism;
using OnboardingStatus =
    privacy_sandbox::TrackingProtectionOnboarding::OnboardingStatus;

bool IsSameSite(const GURL& url1, const GURL& url2) {
  return url1.SchemeIs(url2.scheme()) &&
         net::registry_controlled_domains::SameDomainOrHost(
             url1, url2,
             net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum CookieReadStatus {
  kNone = 0,
  kNotOnboardedAllowed = 1,
  kNotOnboardedBlocked = 2,
  kNotOnboardedAllowedAd = 3,
  kNotOnboardedBlockedAd = 4,
  kAllowed = 5,
  kAllowedAd = 6,
  kAllowedOther = 7,
  kAllowedOtherAd = 8,
  kAllowedHeuristics = 9,
  kAllowedHeuristicsAd = 10,
  kAllowedMetadataGrant = 11,
  kAllowedMetadataGrantAd = 12,
  kAllowedTrial = 13,
  kAllowedTrialAd = 14,
  kAllowedTopLevelTrial = 15,
  kAllowedTopLevelTrialAd = 16,
  kBlocked = 17,
  kBlockedAd = 18,
  kBlockedOtherAd = 19,
  kBlockedSkippedHeuristicsAd = 20,
  kBlockedSkippedMetadataGrantAd = 21,
  kBlockedSkippedTrialAd = 22,
  kBlockedSkippedTopLevelTrialAd = 23,
  kMaxValue = kBlockedSkippedTopLevelTrialAd
};

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
    const net::CookieSettingOverrides& cookie_setting_overrides,
    bool is_partitioned_access) {
  const ThirdPartyCookieAllowMechanism allow_mechanism =
      cookie_settings_->GetThirdPartyCookieAllowMechanism(
          url, net::SiteForCookies::FromUrl(first_party_url), first_party_url,
          cookie_setting_overrides);
  RecordCookieUseCounters(url, first_party_url, blocked_by_policy,
                          allow_mechanism);
  RecordCookieReadUseCounters(url, first_party_url, blocked_by_policy,
                              is_ad_tagged, allow_mechanism,
                              cookie_setting_overrides, is_partitioned_access);
}

void ThirdPartyCookieDeprecationMetricsObserver::OnCookieChange(
    const GURL& url,
    const GURL& first_party_url,
    const net::CanonicalCookie& cookie,
    bool blocked_by_policy,
    bool is_ad_tagged,
    const net::CookieSettingOverrides& cookie_setting_overrides,
    bool is_partitioned_access) {
  const ThirdPartyCookieAllowMechanism allow_mechanism =
      cookie_settings_->GetThirdPartyCookieAllowMechanism(
          url, net::SiteForCookies::FromUrl(first_party_url), first_party_url,
          cookie_setting_overrides);
  RecordCookieUseCounters(url, first_party_url, blocked_by_policy,
                          allow_mechanism);
}

void ThirdPartyCookieDeprecationMetricsObserver::RecordCookieUseCounters(
    const GURL& url,
    const GURL& first_party_url,
    bool blocked_by_policy,
    ThirdPartyCookieAllowMechanism allow_mechanism) {
  if (!IsThirdParty(url, first_party_url)) {
    return;
  }
  if (blocked_by_policy) {
    page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(
        GetDelegate().GetWebContents()->GetPrimaryMainFrame(),
        blink::mojom::WebFeature::kThirdPartyCookieBlocked);
    return;
  }

  // Record third party cookie metrics if the access is blocked by third
  // party cookies deprecation experiment when some mechanism re-enable the
  // third party cookie access.
  bool is_blocked_by_experiment = IsBlockedByThirdPartyDeprecationExperiment();
  UMA_HISTOGRAM_BOOLEAN(
      "PageLoad.Clients.TPCD.ThirdPartyCookieAccessBlockedByExperiment2",
      is_blocked_by_experiment);

  if (allow_mechanism != ThirdPartyCookieAllowMechanism::kNone) {
    UMA_HISTOGRAM_ENUMERATION(
        "PageLoad.Clients.TPCD.CookieAccess.ThirdPartyCookieAllowMechanism3",
        allow_mechanism);
  }

  if (CookieSettingsBase::Is1PDtRelatedAllowMechanism(allow_mechanism)) {
    ukm::builders::Tpcd_Mitigations_Dt_FirstParty_Deployment2(
        GetDelegate()
            .GetWebContents()
            ->GetPrimaryMainFrame()
            ->GetPageUkmSourceId())
        .SetDeployed(allow_mechanism ==
                     ThirdPartyCookieAllowMechanism::kAllowByTopLevel3PCD)
        .Record(ukm::UkmRecorder::Get());
  }

  // Record the following blink feature usage cookie metrics.
  std::vector<blink::mojom::WebFeature> third_party_cookie_features;
  if (is_blocked_by_experiment) {
    third_party_cookie_features.push_back(
        blink::mojom::WebFeature::kThirdPartyCookieAccessBlockByExperiment);
  }

  switch (allow_mechanism) {
    case ThirdPartyCookieAllowMechanism::kAllowByExplicitSetting:
    case ThirdPartyCookieAllowMechanism::kAllowByTrackingProtectionException:
      third_party_cookie_features.push_back(
          blink::mojom::WebFeature::
              kThirdPartyCookieDeprecation_AllowByExplicitSetting);
      break;
    case ThirdPartyCookieAllowMechanism::kAllowByGlobalSetting:
      third_party_cookie_features.push_back(
          blink::mojom::WebFeature::
              kThirdPartyCookieDeprecation_AllowByGlobalSetting);
      break;
    case ThirdPartyCookieAllowMechanism::kAllowBy3PCDMetadataSource1pDt:
    case ThirdPartyCookieAllowMechanism::kAllowBy3PCDMetadataSource3pDt:
    case ThirdPartyCookieAllowMechanism::kAllowBy3PCDMetadataSourceUnspecified:
    case ThirdPartyCookieAllowMechanism::kAllowBy3PCDMetadataSourceTest:
    case ThirdPartyCookieAllowMechanism::kAllowBy3PCDMetadataSourceDogFood:
    case ThirdPartyCookieAllowMechanism::
        kAllowBy3PCDMetadataSourceCriticalSector:
    case ThirdPartyCookieAllowMechanism::kAllowBy3PCDMetadataSourceCuj:
    case ThirdPartyCookieAllowMechanism::kAllowBy3PCDMetadataSourceGovEduTld:
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
    case ThirdPartyCookieAllowMechanism::
        kAllowByEnterprisePolicyCookieAllowedForUrls:
      third_party_cookie_features.push_back(
          blink::mojom::WebFeature::
              kThirdPartyCookieDeprecation_AllowByEnterprisePolicyCookieAllowedForUrls);
      break;
    // No feature usage recorded for the following mechanism values.
    case ThirdPartyCookieAllowMechanism::kNone:
    case ThirdPartyCookieAllowMechanism::kAllowByTopLevel3PCD:
    case ThirdPartyCookieAllowMechanism::kAllowByScheme:
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
    bool is_ad_tagged,
    ThirdPartyCookieAllowMechanism allow_mechanism,
    const net::CookieSettingOverrides& cookie_setting_overrides,
    bool is_partitioned_access) {
  if (!IsThirdParty(url, first_party_url)) {
    return;
  }

  bool is_blocked_by_experiment = IsBlockedByThirdPartyDeprecationExperiment();

  if (!is_partitioned_access) {
    CookieReadStatus status = CookieReadStatus::kNone;
    if (!is_blocked_by_experiment) {
      if (is_ad_tagged) {
        status = blocked_by_policy ? CookieReadStatus::kNotOnboardedBlockedAd
                                   : CookieReadStatus::kNotOnboardedAllowedAd;
      } else {
        status = blocked_by_policy ? CookieReadStatus::kNotOnboardedBlocked
                                   : CookieReadStatus::kNotOnboardedAllowed;
      }
    } else {
      // If the cookie access was allowed, record the mitigation that allowed it
      // if any.
      if (!blocked_by_policy) {
        if (CookieSettingsBase::IsAnyTpcdMetadataAllowMechanism(
                allow_mechanism)) {
          status = is_ad_tagged ? CookieReadStatus::kAllowedMetadataGrantAd
                                : CookieReadStatus::kAllowedMetadataGrant;
        } else if (allow_mechanism ==
                   ThirdPartyCookieAllowMechanism::kAllowBy3PCD) {
          status = is_ad_tagged ? CookieReadStatus::kAllowedTrialAd
                                : CookieReadStatus::kAllowedTrial;
        } else if (allow_mechanism ==
                   ThirdPartyCookieAllowMechanism::kAllowBy3PCDHeuristics) {
          status = is_ad_tagged ? CookieReadStatus::kAllowedHeuristicsAd
                                : CookieReadStatus::kAllowedHeuristics;
        } else if (allow_mechanism ==
                   ThirdPartyCookieAllowMechanism::kAllowByTopLevel3PCD) {
          status = is_ad_tagged ? CookieReadStatus::kAllowedTopLevelTrialAd
                                : CookieReadStatus::kAllowedTopLevelTrial;
        } else if (allow_mechanism == ThirdPartyCookieAllowMechanism::kNone) {
          status = is_ad_tagged ? CookieReadStatus::kAllowedAd : kAllowed;
        } else {
          status =
              is_ad_tagged ? CookieReadStatus::kAllowedOtherAd : kAllowedOther;
        }
      } else {
        if (!is_ad_tagged) {
          status = CookieReadStatus::kBlocked;
        } else if (allow_mechanism == ThirdPartyCookieAllowMechanism::kNone) {
          // For cookies which are blocked by the experiment, we want to
          // understand explicitly how many cookies are blocked due to the ads
          // heuristics which skip 3PCD re-enablement rules.
          net::CookieSettingOverrides overrides = cookie_setting_overrides;
          overrides.RemoveAll(net::CookieSettingOverrides(
              {net::CookieSettingOverride::kSkipTPCDHeuristicsGrant,
               net::CookieSettingOverride::kSkipTPCDMetadataGrant,
               net::CookieSettingOverride::kSkipTPCDTrial,
               net::CookieSettingOverride::kSkipTopLevelTPCDTrial}));
          const ThirdPartyCookieAllowMechanism
              allow_mechanism_without_heuristics =
                  cookie_settings_->GetThirdPartyCookieAllowMechanism(
                      url, net::SiteForCookies::FromUrl(first_party_url),
                      first_party_url, overrides);

          // Check if this cookie was blocked because we explicitly skipped 3PCD
          // enablement mitigations.
          if (allow_mechanism_without_heuristics ==
              ThirdPartyCookieAllowMechanism::kNone) {
            status = CookieReadStatus::kBlockedAd;
          } else if (CookieSettingsBase::IsAnyTpcdMetadataAllowMechanism(
                         allow_mechanism_without_heuristics)) {
            status = CookieReadStatus::kBlockedSkippedMetadataGrantAd;
          } else if (allow_mechanism_without_heuristics ==
                     ThirdPartyCookieAllowMechanism::kAllowBy3PCD) {
            status = CookieReadStatus::kBlockedSkippedTrialAd;
          } else if (allow_mechanism_without_heuristics ==
                     ThirdPartyCookieAllowMechanism::kAllowBy3PCDHeuristics) {
            status = CookieReadStatus::kBlockedSkippedHeuristicsAd;
          } else if (allow_mechanism_without_heuristics ==
                     ThirdPartyCookieAllowMechanism::kAllowByTopLevel3PCD) {
            status = CookieReadStatus::kBlockedSkippedTopLevelTrialAd;
          } else {
            status = CookieReadStatus::kBlockedOtherAd;
          }
        } else {
          status = CookieReadStatus::kBlockedOtherAd;
        }
      }
    }

    base::UmaHistogramEnumeration(
        "PageLoad.Clients.TPCD.TPCAccess.CookieReadStatus2", status);

    if (status == CookieReadStatus::kBlockedSkippedMetadataGrantAd ||
        status == CookieReadStatus::kBlockedSkippedTrialAd ||
        status == CookieReadStatus::kBlockedSkippedHeuristicsAd ||
        status == CookieReadStatus::kBlockedSkippedTopLevelTrialAd) {
      page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(
          GetDelegate().GetWebContents()->GetPrimaryMainFrame(),
          std::vector<blink::mojom::WebFeature>{
              blink::mojom::WebFeature::kTpcdCookieReadBlockedByAdHeuristics});
    }
  }

  if (blocked_by_policy) {
    return;
  }

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
