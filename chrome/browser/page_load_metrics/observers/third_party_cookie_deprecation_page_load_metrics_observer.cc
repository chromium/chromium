// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/third_party_cookie_deprecation_page_load_metrics_observer.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/experiment/experiment_manager_impl.h"
#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

namespace {

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
  experiment_manager_ = tpcd::experiment::ExperimentManagerImpl::GetForProfile(
      Profile::FromBrowserContext(context));
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
    bool blocked_by_policy) {
  RecordCookieUseCounters(url, first_party_url, blocked_by_policy);
}

void ThirdPartyCookieDeprecationMetricsObserver::OnCookieChange(
    const GURL& url,
    const GURL& first_party_url,
    const net::CanonicalCookie& cookie,
    bool blocked_by_policy) {
  RecordCookieUseCounters(url, first_party_url, blocked_by_policy);
}

void ThirdPartyCookieDeprecationMetricsObserver::RecordCookieUseCounters(
    const GURL& url,
    const GURL& first_party_url,
    bool blocked_by_policy) {
  if (blocked_by_policy || !IsThirdParty(url, first_party_url)) {
    return;
  }

  // Record third party cookie metrics if the access is blocked by third
  // party cookies deprecation experiment.
  bool is_blocked_by_experiment = IsBlockedByThirdPartyDeprecationExperiment();
  UMA_HISTOGRAM_BOOLEAN(
      "PageLoad.Clients.ThirdPartyCookieAccessBlockedByExperiment",
      is_blocked_by_experiment);

  if (is_blocked_by_experiment) {
    page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(
        GetDelegate().GetWebContents()->GetPrimaryMainFrame(),
        std::vector<blink::mojom::WebFeature>{
            blink::mojom::WebFeature::
                kThirdPartyCookieAccessBlockByExperiment});
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

  return experiment_manager_ &&
         experiment_manager_->IsClientEligible() == true &&
         tpcd::experiment::kDisable3PCookies.Get();
}
