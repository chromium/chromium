// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/isolated/isolated_prerender_tab_helper.h"

#include <string>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_features.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_network_context_client.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_params.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_prefetch_metrics_collector.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_proxy_configurator.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_service.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_service_factory.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_subresource_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/google/core/common/google_util.h"
#include "components/language/core/browser/pref_names.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/prefs/pref_service.h"
#include "components/prerender/browser/prerender_manager.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/cookies/cookie_store.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "url/origin.h"

namespace {

const void* const kPrefetchingLikelyEventKey = 0;

base::Optional<base::TimeDelta> GetTotalPrefetchTime(
    network::mojom::URLResponseHead* head) {
  DCHECK(head);

  base::Time start = head->request_time;
  base::Time end = head->response_time;

  if (start.is_null() || end.is_null())
    return base::nullopt;

  return end - start;
}

base::Optional<base::TimeDelta> GetPrefetchConnectTime(
    network::mojom::URLResponseHead* head) {
  DCHECK(head);

  base::TimeTicks start = head->load_timing.connect_timing.connect_start;
  base::TimeTicks end = head->load_timing.connect_timing.connect_end;

  if (start.is_null() || end.is_null())
    return base::nullopt;

  return end - start;
}

void InformPLMOfLikelyPrefetching(content::WebContents* web_contents) {
  page_load_metrics::MetricsWebContentsObserver* metrics_web_contents_observer =
      page_load_metrics::MetricsWebContentsObserver::FromWebContents(
          web_contents);
  if (!metrics_web_contents_observer)
    return;

  metrics_web_contents_observer->BroadcastEventToObservers(
      IsolatedPrerenderTabHelper::PrefetchingLikelyEventKey());
}

void OnGotCookieList(
    const GURL& url,
    IsolatedPrerenderTabHelper::OnEligibilityResultCallback result_callback,
    const net::CookieAccessResultList& cookie_list,
    const net::CookieAccessResultList& excluded_cookies) {
  if (!cookie_list.empty()) {
    std::move(result_callback)
        .Run(url, false,
             IsolatedPrerenderPrefetchStatus::
                 kPrefetchNotEligibleUserHasCookies);
    return;
  }

  // Cookies are tricky because cookies for different paths or a higher level
  // domain (e.g.: m.foo.com and foo.com) may not show up in |cookie_list|, but
  // they will show up in |excluded_cookies|. To check for any cookies for a
  // domain, compare the domains of the prefetched |url| and the domains of all
  // the returned cookies.
  bool excluded_cookie_has_tld = false;
  for (const auto& cookie_result : excluded_cookies) {
    if (cookie_result.cookie.IsExpired(base::Time::Now())) {
      // Expired cookies don't count.
      continue;
    }

    if (url.DomainIs(cookie_result.cookie.DomainWithoutDot())) {
      excluded_cookie_has_tld = true;
      break;
    }
  }

  if (excluded_cookie_has_tld) {
    std::move(result_callback)
        .Run(url, false,
             IsolatedPrerenderPrefetchStatus::
                 kPrefetchNotEligibleUserHasCookies);
    return;
  }

  std::move(result_callback).Run(url, true, base::nullopt);
}

void CookieSetHelper(base::RepeatingClosure run_me,
                     net::CookieAccessResult access_result) {
  run_me.Run();
}

bool ShouldStartSpareRenderer() {
  if (!IsolatedPrerenderStartsSpareRenderer()) {
    return false;
  }

  for (content::RenderProcessHost::iterator iter(
           content::RenderProcessHost::AllHostsIterator());
       !iter.IsAtEnd(); iter.Advance()) {
    if (iter.GetCurrentValue()->IsUnused()) {
      // There is already a spare renderer.
      return false;
    }
  }

  return true;
}

}  // namespace

IsolatedPrerenderTabHelper::PrefetchMetrics::PrefetchMetrics() = default;
IsolatedPrerenderTabHelper::PrefetchMetrics::~PrefetchMetrics() = default;

IsolatedPrerenderTabHelper::AfterSRPMetrics::AfterSRPMetrics() = default;
IsolatedPrerenderTabHelper::AfterSRPMetrics::AfterSRPMetrics(
    const AfterSRPMetrics& other) = default;
IsolatedPrerenderTabHelper::AfterSRPMetrics::~AfterSRPMetrics() = default;

IsolatedPrerenderTabHelper::CurrentPageLoad::CurrentPageLoad(
    content::NavigationHandle* handle)
    : profile_(handle ? Profile::FromBrowserContext(
                            handle->GetWebContents()->GetBrowserContext())
                      : nullptr),
      navigation_start_(handle ? handle->NavigationStart() : base::TimeTicks()),
      srp_metrics_(
          base::MakeRefCounted<IsolatedPrerenderTabHelper::PrefetchMetrics>()) {
}

IsolatedPrerenderTabHelper::CurrentPageLoad::~CurrentPageLoad() {
  if (IsolatedPrerenderStartsSpareRenderer()) {
    UMA_HISTOGRAM_COUNTS_100(
        "IsolatedPrerender.SpareRenderer.CountStartedOnSRP",
        number_of_spare_renderers_started_);
  }

  if (!profile_)
    return;

  IsolatedPrerenderService* service =
      IsolatedPrerenderServiceFactory::GetForProfile(profile_);
  if (!service) {
    return;
  }

  for (const GURL& url : no_state_prefetched_urls_) {
    service->DestroySubresourceManagerForURL(url);
  }
  for (const GURL& url : urls_to_no_state_prefetch_) {
    service->DestroySubresourceManagerForURL(url);
  }
}

// static
const void* IsolatedPrerenderTabHelper::PrefetchingLikelyEventKey() {
  return &kPrefetchingLikelyEventKey;
}

static content::ServiceWorkerContext* g_service_worker_context_for_test =
    nullptr;

// static
void IsolatedPrerenderTabHelper::SetServiceWorkerContextForTest(
    content::ServiceWorkerContext* context) {
  g_service_worker_context_for_test = context;
}

IsolatedPrerenderTabHelper::IsolatedPrerenderTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  page_ = std::make_unique<CurrentPageLoad>(nullptr);
  profile_ = Profile::FromBrowserContext(web_contents->GetBrowserContext());

  NavigationPredictorKeyedService* navigation_predictor_service =
      NavigationPredictorKeyedServiceFactory::GetForProfile(profile_);
  if (navigation_predictor_service) {
    navigation_predictor_service->AddObserver(this);
  }

  // Make sure the global service is up and running so that the service worker
  // registrations can be queried before the first navigation prediction.
  IsolatedPrerenderServiceFactory::GetForProfile(profile_);
}

IsolatedPrerenderTabHelper::~IsolatedPrerenderTabHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NavigationPredictorKeyedService* navigation_predictor_service =
      NavigationPredictorKeyedServiceFactory::GetForProfile(profile_);
  if (navigation_predictor_service) {
    navigation_predictor_service->RemoveObserver(this);
  }
}

void IsolatedPrerenderTabHelper::AddObserverForTesting(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void IsolatedPrerenderTabHelper::RemoveObserverForTesting(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

network::mojom::NetworkContext*
IsolatedPrerenderTabHelper::GetIsolatedContextForTesting() const {
  return page_->isolated_network_context_.get();
}

base::Optional<IsolatedPrerenderTabHelper::AfterSRPMetrics>
IsolatedPrerenderTabHelper::after_srp_metrics() const {
  if (page_->after_srp_metrics_) {
    return *(page_->after_srp_metrics_);
  }
  return base::nullopt;
}

// static
bool IsolatedPrerenderTabHelper::IsProfileEligible(Profile* profile) {
  if (profile->IsOffTheRecord()) {
    return false;
  }

  if (IsolatedPrerenderOnlyForLiteMode()) {
    return data_reduction_proxy::DataReductionProxySettings::
        IsDataSaverEnabledByUser(profile->IsOffTheRecord(),
                                 profile->GetPrefs());
  }
  return true;
}

bool IsolatedPrerenderTabHelper::IsProfileEligible() const {
  return IsProfileEligible(profile_);
}

void IsolatedPrerenderTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!navigation_handle->IsInMainFrame()) {
    return;
  }

  // This check is only relevant for detecting AMP pages. For this feature, AMP
  // pages won't get sped up any so just ignore them.
  if (navigation_handle->IsSameDocument()) {
    return;
  }

  // Don't take any actions during a prerender since it was probably triggered
  // by another instance of this class and we don't want to interfere.
  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForBrowserContext(profile_);
  if (prerender_manager &&
      prerender_manager->IsWebContentsPrerendering(web_contents())) {
    return;
  }

  const GURL& url = navigation_handle->GetURL();

  if (page_->prefetched_responses_.find(url) !=
      page_->prefetched_responses_.end()) {
    // Start copying any needed cookies over to the main profile if this page
    // was prefetched.
    CopyIsolatedCookiesOnAfterSRPClick(url);
  }

  // User is navigating, don't bother prefetching further.
  page_->url_loaders_.clear();

  if (page_->srp_metrics_->prefetch_attempted_count_ > 0) {
    UMA_HISTOGRAM_COUNTS_100(
        "IsolatedPrerender.Prefetch.Mainframe.TotalRedirects",
        page_->srp_metrics_->prefetch_total_redirect_count_);
  }

  // Notify the subresource manager (if applicable)  that its page is being
  // navigated to so that the prefetched subresources can be used from cache.
  IsolatedPrerenderService* service =
      IsolatedPrerenderServiceFactory::GetForProfile(profile_);
  if (!service)
    return;

  IsolatedPrerenderSubresourceManager* subresource_manager =
      service->GetSubresourceManagerForURL(navigation_handle->GetURL());
  if (!subresource_manager)
    return;

  subresource_manager->NotifyPageNavigatedToAfterSRP();
}

void IsolatedPrerenderTabHelper::NotifyPrefetchProbeLatency(
    base::TimeDelta probe_latency) {
  page_->probe_latency_ = probe_latency;
}

void IsolatedPrerenderTabHelper::ReportProbeResult(
    const GURL& url,
    IsolatedPrerenderProbeResult result) {
  if (!page_->prefetch_metrics_collector_) {
    return;
  }
  page_->prefetch_metrics_collector_->OnMainframeNavigationProbeResult(url,
                                                                       result);
}

void IsolatedPrerenderTabHelper::OnPrefetchStatusUpdate(
    const GURL& url,
    IsolatedPrerenderPrefetchStatus usage) {
  page_->prefetch_status_by_url_[url] = usage;
}

IsolatedPrerenderPrefetchStatus
IsolatedPrerenderTabHelper::MaybeUpdatePrefetchStatusWithNSPContext(
    const GURL& url,
    IsolatedPrerenderPrefetchStatus status) const {
  switch (status) {
    // These are the statuses we want to update.
    case IsolatedPrerenderPrefetchStatus::kPrefetchUsedNoProbe:
    case IsolatedPrerenderPrefetchStatus::kPrefetchUsedProbeSuccess:
    case IsolatedPrerenderPrefetchStatus::kPrefetchNotUsedProbeFailed:
      break;
    // These statuses are not applicable since the prefetch was not used after
    // the click.
    case IsolatedPrerenderPrefetchStatus::kPrefetchNotStarted:
    case IsolatedPrerenderPrefetchStatus::kPrefetchNotEligibleGoogleDomain:
    case IsolatedPrerenderPrefetchStatus::kPrefetchNotEligibleUserHasCookies:
    case IsolatedPrerenderPrefetchStatus::
        kPrefetchNotEligibleUserHasServiceWorker:
    case IsolatedPrerenderPrefetchStatus::kPrefetchNotEligibleSchemeIsNotHttps:
    case IsolatedPrerenderPrefetchStatus::kPrefetchNotEligibleHostIsIPAddress:
    case IsolatedPrerenderPrefetchStatus::
        kPrefetchNotEligibleNonDefaultStoragePartition:
    case IsolatedPrerenderPrefetchStatus::kPrefetchNotFinishedInTime:
    case IsolatedPrerenderPrefetchStatus::kPrefetchFailedNetError:
    case IsolatedPrerenderPrefetchStatus::kPrefetchFailedNon2XX:
    case IsolatedPrerenderPrefetchStatus::kPrefetchFailedNotHTML:
    case IsolatedPrerenderPrefetchStatus::kPrefetchSuccessful:
    case IsolatedPrerenderPrefetchStatus::kNavigatedToLinkNotOnSRP:
    case IsolatedPrerenderPrefetchStatus::kSubresourceThrottled:
      return status;
    // These statuses we are going to update to, and this is the only place that
    // they are set so they are not expected to be passed in.
    case IsolatedPrerenderPrefetchStatus::kPrefetchUsedNoProbeWithNSP:
    case IsolatedPrerenderPrefetchStatus::kPrefetchUsedProbeSuccessWithNSP:
    case IsolatedPrerenderPrefetchStatus::kPrefetchNotUsedProbeFailedWithNSP:
    case IsolatedPrerenderPrefetchStatus::kPrefetchUsedNoProbeNSPAttemptDenied:
    case IsolatedPrerenderPrefetchStatus::
        kPrefetchUsedProbeSuccessNSPAttemptDenied:
    case IsolatedPrerenderPrefetchStatus::
        kPrefetchNotUsedProbeFailedNSPAttemptDenied:
    case IsolatedPrerenderPrefetchStatus::kPrefetchUsedNoProbeNSPNotStarted:
    case IsolatedPrerenderPrefetchStatus::
        kPrefetchUsedProbeSuccessNSPNotStarted:
    case IsolatedPrerenderPrefetchStatus::
        kPrefetchNotUsedProbeFailedNSPNotStarted:
      NOTREACHED();
      return status;
  }

  bool no_state_prefetch_not_started =
      base::Contains(page_->urls_to_no_state_prefetch_, url);

  bool no_state_prefetch_complete =
      base::Contains(page_->no_state_prefetched_urls_, url);

  bool no_state_prefetch_failed =
      base::Contains(page_->failed_no_state_prefetch_urls_, url);

  if (!no_state_prefetch_not_started && !no_state_prefetch_complete &&
      !no_state_prefetch_failed) {
    return status;
  }

  // At most one of those bools should be true.
  DCHECK(no_state_prefetch_not_started ^ no_state_prefetch_complete ^
         no_state_prefetch_failed);

  if (no_state_prefetch_complete) {
    switch (status) {
      case IsolatedPrerenderPrefetchStatus::kPrefetchUsedNoProbe:
        return IsolatedPrerenderPrefetchStatus::kPrefetchUsedNoProbeWithNSP;
      case IsolatedPrerenderPrefetchStatus::kPrefetchUsedProbeSuccess:
        return IsolatedPrerenderPrefetchStatus::
            kPrefetchUsedProbeSuccessWithNSP;
      case IsolatedPrerenderPrefetchStatus::kPrefetchNotUsedProbeFailed:
        return IsolatedPrerenderPrefetchStatus::
            kPrefetchNotUsedProbeFailedWithNSP;
      default:
        break;
    }
  }

  if (no_state_prefetch_failed) {
    switch (status) {
      case IsolatedPrerenderPrefetchStatus::kPrefetchUsedNoProbe:
        return IsolatedPrerenderPrefetchStatus::
            kPrefetchUsedNoProbeNSPAttemptDenied;
      case IsolatedPrerenderPrefetchStatus::kPrefetchUsedProbeSuccess:
        return IsolatedPrerenderPrefetchStatus::
            kPrefetchUsedProbeSuccessNSPAttemptDenied;
      case IsolatedPrerenderPrefetchStatus::kPrefetchNotUsedProbeFailed:
        return IsolatedPrerenderPrefetchStatus::
            kPrefetchNotUsedProbeFailedNSPAttemptDenied;
      default:
        break;
    }
  }

  if (no_state_prefetch_not_started) {
    switch (status) {
      case IsolatedPrerenderPrefetchStatus::kPrefetchUsedNoProbe:
        return IsolatedPrerenderPrefetchStatus::
            kPrefetchUsedNoProbeNSPNotStarted;
      case IsolatedPrerenderPrefetchStatus::kPrefetchUsedProbeSuccess:
        return IsolatedPrerenderPrefetchStatus::
            kPrefetchUsedProbeSuccessNSPNotStarted;
      case IsolatedPrerenderPrefetchStatus::kPrefetchNotUsedProbeFailed:
        return IsolatedPrerenderPrefetchStatus::
            kPrefetchNotUsedProbeFailedNSPNotStarted;
      default:
        break;
    }
  }

  NOTREACHED();
  return status;
}

std::unique_ptr<IsolatedPrerenderTabHelper::AfterSRPMetrics>
IsolatedPrerenderTabHelper::ComputeAfterSRPMetricsBeforeCommit(
    content::NavigationHandle* handle) const {
  if (page_->srp_metrics_->predicted_urls_count_ <= 0) {
    return nullptr;
  }

  auto metrics = std::make_unique<AfterSRPMetrics>();

  metrics->url_ = handle->GetURL();
  metrics->prefetch_eligible_count_ =
      page_->srp_metrics_->prefetch_eligible_count_;

  metrics->probe_latency_ = page_->probe_latency_;

  // Check every url in the redirect chain for a status, starting at the end
  // and working backwards. Note: When a redirect chain is eligible all the
  // way to the end, the status is already propagated. But if a redirect was
  // not eligible then this will find its last known status.
  DCHECK(!handle->GetRedirectChain().empty());
  base::Optional<IsolatedPrerenderPrefetchStatus> status;
  base::Optional<size_t> prediction_position;
  for (auto back_iter = handle->GetRedirectChain().rbegin();
       back_iter != handle->GetRedirectChain().rend(); ++back_iter) {
    GURL chain_url = *back_iter;
    auto status_iter = page_->prefetch_status_by_url_.find(chain_url);
    if (!status && status_iter != page_->prefetch_status_by_url_.end()) {
      status = MaybeUpdatePrefetchStatusWithNSPContext(chain_url,
                                                       status_iter->second);
    }

    // Same check for the original prediction ordering.
    auto position_iter = page_->original_prediction_ordering_.find(chain_url);
    if (!prediction_position &&
        position_iter != page_->original_prediction_ordering_.end()) {
      prediction_position = position_iter->second;
    }
  }

  if (status) {
    metrics->prefetch_status_ = *status;
  } else {
    metrics->prefetch_status_ =
        IsolatedPrerenderPrefetchStatus::kNavigatedToLinkNotOnSRP;
  }
  metrics->clicked_link_srp_position_ = prediction_position;

  return metrics;
}

void IsolatedPrerenderTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!navigation_handle->IsInMainFrame()) {
    return;
  }

  // This check is only relevant for detecting AMP pages. For this feature, AMP
  // pages won't get sped up any so just ignore them.
  if (navigation_handle->IsSameDocument()) {
    return;
  }

  if (!navigation_handle->HasCommitted()) {
    return;
  }

  // Don't take any actions during a prerender since it was probably triggered
  // by another instance of this class and we don't want to interfere.
  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForBrowserContext(profile_);
  if (prerender_manager &&
      prerender_manager->IsWebContentsPrerendering(web_contents())) {
    return;
  }

  DCHECK(!PrefetchingActive());

  GURL url = navigation_handle->GetURL();

  std::unique_ptr<CurrentPageLoad> new_page =
      std::make_unique<CurrentPageLoad>(navigation_handle);

  if (page_->srp_metrics_->predicted_urls_count_ > 0) {
    page_->prefetch_metrics_collector_->OnMainframeNavigatedTo(url);

    // If the previous page load was a Google SRP, the AfterSRPMetrics class
    // needs to be created now from the SRP's |page_| and then set on the new
    // one when we set it at the end of this method.
    new_page->after_srp_metrics_ =
        ComputeAfterSRPMetricsBeforeCommit(navigation_handle);

    // See if the page being navigated to was prerendered. If so, copy over its
    // subresource manager and networking pipes.
    IsolatedPrerenderService* service =
        IsolatedPrerenderServiceFactory::GetForProfile(profile_);
    std::unique_ptr<IsolatedPrerenderSubresourceManager> manager =
        service->TakeSubresourceManagerForURL(url);
    if (manager) {
      new_page->subresource_manager_ = std::move(manager);
      new_page->isolated_cookie_manager_ =
          std::move(page_->isolated_cookie_manager_);
      new_page->isolated_url_loader_factory_ =
          std::move(page_->isolated_url_loader_factory_);
      new_page->isolated_network_context_ =
          std::move(page_->isolated_network_context_);
    }
  }

  // |page_| is reset on commit so that any available cached prefetches that
  // result from a redirect get used.
  page_ = std::move(new_page);
}

void IsolatedPrerenderTabHelper::OnVisibilityChanged(
    content::Visibility visibility) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsolatedPrerenderIsEnabled()) {
    return;
  }

  // Start prefetching if the tab has become visible and prefetching is
  // inactive. Hidden and occluded visibility is ignored here so that pending
  // prefetches can finish.
  if (visibility == content::Visibility::VISIBLE && !PrefetchingActive())
    Prefetch();
}

std::unique_ptr<PrefetchedMainframeResponseContainer>
IsolatedPrerenderTabHelper::TakePrefetchResponse(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = page_->prefetched_responses_.find(url);
  if (it == page_->prefetched_responses_.end())
    return nullptr;

  std::unique_ptr<PrefetchedMainframeResponseContainer> response =
      std::move(it->second);
  page_->prefetched_responses_.erase(it);
  return response;
}

std::unique_ptr<PrefetchedMainframeResponseContainer>
IsolatedPrerenderTabHelper::CopyPrefetchResponseForNSP(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = page_->prefetched_responses_.find(url);
  if (it == page_->prefetched_responses_.end())
    return nullptr;

  return it->second->Clone();
}

bool IsolatedPrerenderTabHelper::PrefetchingActive() const {
  return page_ && !page_->url_loaders_.empty();
}

void IsolatedPrerenderTabHelper::Prefetch() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsolatedPrerenderIsEnabled());

  if (!page_->srp_metrics_->navigation_to_prefetch_start_.has_value()) {
    page_->srp_metrics_->navigation_to_prefetch_start_ =
        base::TimeTicks::Now() - page_->navigation_start_;
    DCHECK_GT(page_->srp_metrics_->navigation_to_prefetch_start_.value(),
              base::TimeDelta());
  }

  if (IsolatedPrerenderCloseIdleSockets() && page_->isolated_network_context_) {
    page_->isolated_network_context_->CloseIdleConnections(base::DoNothing());
  }

  if (web_contents()->GetVisibility() != content::Visibility::VISIBLE) {
    // |OnVisibilityChanged| will restart prefetching when the tab becomes
    // visible again.
    return;
  }

  DCHECK_GT(IsolatedPrerenderMaximumNumberOfConcurrentPrefetches(), 0U);

  while (
      // Checks that the total number of prefetches has not been met.
      !(IsolatedPrerenderMaximumNumberOfPrefetches().has_value() &&
        page_->srp_metrics_->prefetch_attempted_count_ >=
            IsolatedPrerenderMaximumNumberOfPrefetches().value()) &&

      // Checks that there are still urls to prefetch.
      !page_->urls_to_prefetch_.empty() &&

      // Checks that the max number of concurrent prefetches has not been met.
      page_->url_loaders_.size() <
          IsolatedPrerenderMaximumNumberOfConcurrentPrefetches()) {
    StartSinglePrefetch();
  }
}

void IsolatedPrerenderTabHelper::StartSinglePrefetch() {
  DCHECK(!page_->urls_to_prefetch_.empty());
  DCHECK(!(IsolatedPrerenderMaximumNumberOfPrefetches().has_value() &&
           page_->srp_metrics_->prefetch_attempted_count_ >=
               IsolatedPrerenderMaximumNumberOfPrefetches().value()));
  DCHECK(page_->url_loaders_.size() <
         IsolatedPrerenderMaximumNumberOfConcurrentPrefetches());

  page_->srp_metrics_->prefetch_attempted_count_++;

  GURL url = page_->urls_to_prefetch_[0];
  page_->urls_to_prefetch_.erase(page_->urls_to_prefetch_.begin());

  // The status is updated to be successful or failed when it finishes.
  OnPrefetchStatusUpdate(
      url, IsolatedPrerenderPrefetchStatus::kPrefetchNotFinishedInTime);

  url::Origin origin = url::Origin::Create(url);
  net::IsolationInfo isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RedirectMode::kUpdateTopFrame, origin, origin,
      net::SiteForCookies::FromOrigin(origin));
  network::ResourceRequest::TrustedParams trusted_params;
  trusted_params.isolation_info = isolation_info;

  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = url;
  request->method = "GET";
  request->enable_load_timing = true;
  request->load_flags = net::LOAD_DISABLE_CACHE | net::LOAD_PREFETCH;
  request->credentials_mode = network::mojom::CredentialsMode::kInclude;
  request->headers.SetHeader(content::kCorsExemptPurposeHeaderName, "prefetch");
  request->trusted_params = trusted_params;
  request->site_for_cookies = trusted_params.isolation_info.site_for_cookies();

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("navigation_predictor_srp_prefetch",
                                          R"(
          semantics {
            sender: "Navigation Predictor SRP Prefetch Loader"
            description:
              "Prefetches the mainframe HTML of a page linked from a Google "
              "Search Result Page (SRP). This is done out-of-band of normal "
              "prefetches to allow total isolation of this request from the "
              "rest of browser traffic and user state like cookies and cache."
            trigger:
              "Used for sites off of Google SRPs (Search Result Pages) only "
              "for Lite mode users when the feature is enabled."
            data: "None."
            destination: WEBSITE
          }
          policy {
            cookies_allowed: NO
            setting:
              "Users can control Lite mode on Android via the settings menu. "
              "Lite mode is not available on iOS, and on desktop only for "
              "developer testing."
            policy_exception_justification: "Not implemented."
        })");

  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);

  // base::Unretained is safe because |loader| is owned by |this|.
  loader->SetOnRedirectCallback(
      base::BindRepeating(&IsolatedPrerenderTabHelper::OnPrefetchRedirect,
                          base::Unretained(this), loader.get(), url));
  loader->SetAllowHttpErrorResults(true);
  loader->SetTimeoutDuration(IsolatedPrefetchTimeoutDuration());
  loader->DownloadToString(
      GetURLLoaderFactory(),
      base::BindOnce(&IsolatedPrerenderTabHelper::OnPrefetchComplete,
                     base::Unretained(this), loader.get(), url, isolation_info),
      IsolatedPrerenderMainframeBodyLengthLimit());

  page_->url_loaders_.emplace(std::move(loader));

  // Start a spare renderer now so that it will be ready by the time it is
  // useful to have.
  if (ShouldStartSpareRenderer()) {
    StartSpareRenderer();
  }
}

void IsolatedPrerenderTabHelper::OnPrefetchRedirect(
    network::SimpleURLLoader* loader,
    const GURL& original_url,
    const net::RedirectInfo& redirect_info,
    const network::mojom::URLResponseHead& response_head,
    std::vector<std::string>* removed_headers) {
  DCHECK(PrefetchingActive());

  page_->srp_metrics_->prefetch_total_redirect_count_++;

  // Copy the position ordering when there is a redirect so the metrics don't
  // miss out on redirects.
  auto position_iter = page_->original_prediction_ordering_.find(original_url);
  if (position_iter != page_->original_prediction_ordering_.end()) {
    page_->original_prediction_ordering_.emplace(redirect_info.new_url,
                                                 position_iter->second);
  }

  // Run the new URL through all the eligibility checks. In the mean time,
  // continue on with other Prefetches.
  CheckEligibilityOfURL(
      profile_, redirect_info.new_url,
      base::BindOnce(&IsolatedPrerenderTabHelper::OnGotEligibilityResult,
                     weak_factory_.GetWeakPtr()));

  // Cancels the current request.
  DCHECK(page_->url_loaders_.find(loader) != page_->url_loaders_.end());
  page_->url_loaders_.erase(page_->url_loaders_.find(loader));

  Prefetch();
}

void IsolatedPrerenderTabHelper::OnPrefetchComplete(
    network::SimpleURLLoader* loader,
    const GURL& url,
    const net::IsolationInfo& isolation_info,
    std::unique_ptr<std::string> body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(PrefetchingActive());

  base::UmaHistogramSparse("IsolatedPrerender.Prefetch.Mainframe.NetError",
                           std::abs(loader->NetError()));

  if (loader->CompletionStatus()) {
    page_->prefetch_metrics_collector_->OnMainframeResourcePrefetched(
        url, page_->original_prediction_ordering_.find(url)->second,
        loader->ResponseInfo() ? loader->ResponseInfo()->Clone() : nullptr,
        loader->CompletionStatus().value());
  }

  if (loader->NetError() != net::OK) {
    OnPrefetchStatusUpdate(
        url, IsolatedPrerenderPrefetchStatus::kPrefetchFailedNetError);

    for (auto& observer : observer_list_) {
      observer.OnPrefetchCompletedWithError(url, loader->NetError());
    }
  }

  if (loader->NetError() == net::OK && body && loader->ResponseInfo()) {
    network::mojom::URLResponseHeadPtr head = loader->ResponseInfo()->Clone();

    DCHECK(!head->proxy_server.is_direct());

    HandlePrefetchResponse(url, isolation_info, std::move(head),

                           std::move(body));
  }

  DCHECK(page_->url_loaders_.find(loader) != page_->url_loaders_.end());
  page_->url_loaders_.erase(page_->url_loaders_.find(loader));

  Prefetch();
}

void IsolatedPrerenderTabHelper::HandlePrefetchResponse(
    const GURL& url,
    const net::IsolationInfo& isolation_info,
    network::mojom::URLResponseHeadPtr head,
    std::unique_ptr<std::string> body) {
  DCHECK(!head->was_fetched_via_cache);

  if (!head->headers)
    return;

  UMA_HISTOGRAM_COUNTS_10M("IsolatedPrerender.Prefetch.Mainframe.BodyLength",
                           body->size());

  base::Optional<base::TimeDelta> total_time = GetTotalPrefetchTime(head.get());
  if (total_time) {
    UMA_HISTOGRAM_CUSTOM_TIMES("IsolatedPrerender.Prefetch.Mainframe.TotalTime",
                               *total_time,
                               base::TimeDelta::FromMilliseconds(10),
                               base::TimeDelta::FromSeconds(30), 100);
  }

  base::Optional<base::TimeDelta> connect_time =
      GetPrefetchConnectTime(head.get());
  if (connect_time) {
    UMA_HISTOGRAM_TIMES("IsolatedPrerender.Prefetch.Mainframe.ConnectTime",
                        *connect_time);
  }

  int response_code = head->headers->response_code();

  base::UmaHistogramSparse("IsolatedPrerender.Prefetch.Mainframe.RespCode",
                           response_code);

  if (response_code < 200 || response_code >= 300) {
    OnPrefetchStatusUpdate(
        url, IsolatedPrerenderPrefetchStatus::kPrefetchFailedNon2XX);
    for (auto& observer : observer_list_) {
      observer.OnPrefetchCompletedWithError(url, response_code);
    }
    return;
  }

  if (head->mime_type != "text/html") {
    OnPrefetchStatusUpdate(
        url, IsolatedPrerenderPrefetchStatus::kPrefetchFailedNotHTML);
    return;
  }

  std::unique_ptr<PrefetchedMainframeResponseContainer> response =
      std::make_unique<PrefetchedMainframeResponseContainer>(
          isolation_info, std::move(head), std::move(body));
  page_->prefetched_responses_.emplace(url, std::move(response));
  page_->srp_metrics_->prefetch_successful_count_++;

  OnPrefetchStatusUpdate(url,
                         IsolatedPrerenderPrefetchStatus::kPrefetchSuccessful);

  MaybeDoNoStatePrefetch(url);

  for (auto& observer : observer_list_) {
    observer.OnPrefetchCompletedSuccessfully(url);
  }
}

void IsolatedPrerenderTabHelper::MaybeDoNoStatePrefetch(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsolatedPrerenderNoStatePrefetchSubresources()) {
    return;
  }

  page_->urls_to_no_state_prefetch_.push_back(url);
  DoNoStatePrefetch();
}

void IsolatedPrerenderTabHelper::DoNoStatePrefetch() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (page_->urls_to_no_state_prefetch_.empty()) {
    return;
  }

  // Ensure there is not an active navigation.
  if (web_contents()->GetController().GetPendingEntry()) {
    return;
  }

  base::Optional<size_t> max_attempts =
      IsolatedPrerenderMaximumNumberOfNoStatePrefetchAttempts();
  if (max_attempts.has_value() &&
      page_->number_of_no_state_prefetch_attempts_ >= max_attempts.value()) {
    return;
  }

  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForBrowserContext(profile_);
  if (!prerender_manager) {
    return;
  }

  IsolatedPrerenderService* service =
      IsolatedPrerenderServiceFactory::GetForProfile(profile_);
  if (!service) {
    return;
  }

  GURL url = page_->urls_to_no_state_prefetch_[0];

  // Don't start another NSP until the previous one finishes.
  {
    IsolatedPrerenderSubresourceManager* manager =
        service->GetSubresourceManagerForURL(url);
    if (manager && manager->has_nsp_handle()) {
      return;
    }
  }

  // The manager must be created here so that the mainframe response can be
  // given to the URLLoaderInterceptor in this call stack, but may be destroyed
  // before the end of the method if the handle is not created.
  IsolatedPrerenderSubresourceManager* manager =
      service->OnAboutToNoStatePrefetch(url, CopyPrefetchResponseForNSP(url));
  DCHECK_EQ(manager, service->GetSubresourceManagerForURL(url));

  manager->SetPrefetchMetricsCollector(page_->prefetch_metrics_collector_);

  manager->SetCreateIsolatedLoaderFactoryCallback(base::BindRepeating(
      &IsolatedPrerenderTabHelper::CreateNewURLLoaderFactory,
      weak_factory_.GetWeakPtr()));

  content::SessionStorageNamespace* session_storage_namespace =
      web_contents()->GetController().GetDefaultSessionStorageNamespace();
  gfx::Size size = web_contents()->GetContainerBounds().size();

  std::unique_ptr<prerender::PrerenderHandle> handle =
      prerender_manager->AddIsolatedPrerender(url, session_storage_namespace,
                                              size);

  if (!handle) {
    // Clean up the prefetch response in |service| since it wasn't used.
    service->DestroySubresourceManagerForURL(url);
    // Don't use |manager| again!

    page_->failed_no_state_prefetch_urls_.push_back(url);

    // Try the next URL.
    page_->urls_to_no_state_prefetch_.erase(
        page_->urls_to_no_state_prefetch_.begin());
    DoNoStatePrefetch();
    return;
  }

  page_->number_of_no_state_prefetch_attempts_++;

  // It is possible for the manager to be destroyed during the NoStatePrefetch
  // navigation. If this happens, abort the NSP and try again.
  manager = service->GetSubresourceManagerForURL(url);
  if (!manager) {
    handle->OnCancel();
    handle.reset();

    page_->failed_no_state_prefetch_urls_.push_back(url);

    // Try the next URL.
    page_->urls_to_no_state_prefetch_.erase(
        page_->urls_to_no_state_prefetch_.begin());
    DoNoStatePrefetch();
    return;
  }

  manager->ManageNoStatePrefetch(
      std::move(handle),
      base::BindOnce(&IsolatedPrerenderTabHelper::OnPrerenderDone,
                     weak_factory_.GetWeakPtr(), url));
}

void IsolatedPrerenderTabHelper::OnPrerenderDone(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The completed NSP probably consumed a previously started spare renderer, so
  // kick off another one if needed.
  if (ShouldStartSpareRenderer()) {
    StartSpareRenderer();
  }

  // It is possible that this is run as a callback after a navigation has
  // already happened and |page_| is now a different instance than when the
  // prerender was started. In this case, just return.
  if (page_->urls_to_no_state_prefetch_.empty() ||
      url != page_->urls_to_no_state_prefetch_[0]) {
    return;
  }

  page_->no_state_prefetched_urls_.push_back(
      page_->urls_to_no_state_prefetch_[0]);

  for (auto& observer : observer_list_) {
    observer.OnNoStatePrefetchFinished();
  }

  page_->urls_to_no_state_prefetch_.erase(
      page_->urls_to_no_state_prefetch_.begin());

  DoNoStatePrefetch();
}

void IsolatedPrerenderTabHelper::StartSpareRenderer() {
  page_->number_of_spare_renderers_started_++;
  content::RenderProcessHost::WarmupSpareRenderProcessHost(profile_);
}

void IsolatedPrerenderTabHelper::OnPredictionUpdated(
    const base::Optional<NavigationPredictorKeyedService::Prediction>
        prediction) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsolatedPrerenderIsEnabled()) {
    return;
  }

  if (!IsProfileEligible()) {
    return;
  }

  // This checks whether the user has enabled pre* actions in the settings UI.
  if (!chrome_browser_net::CanPreresolveAndPreconnectUI(profile_->GetPrefs())) {
    return;
  }

  if (!prediction.has_value()) {
    return;
  }

  if (prediction->prediction_source() !=
      NavigationPredictorKeyedService::PredictionSource::
          kAnchorElementsParsedFromWebPage) {
    return;
  }

  if (prediction.value().web_contents() != web_contents()) {
    // We only care about predictions in this tab.
    return;
  }

  const base::Optional<GURL>& source_document_url =
      prediction->source_document_url();

  if (!source_document_url || source_document_url->is_empty())
    return;

  if (!google_util::IsGoogleSearchUrl(source_document_url.value())) {
    return;
  }

  if (!page_->prefetch_metrics_collector_) {
    page_->prefetch_metrics_collector_ =
        base::MakeRefCounted<IsolatedPrerenderPrefetchMetricsCollector>(
            page_->navigation_start_,
            web_contents()->GetMainFrame()->GetPageUkmSourceId());
  }

  // It's very likely we'll prefetch something at this point, so inform PLM to
  // start tracking metrics.
  InformPLMOfLikelyPrefetching(web_contents());

  page_->srp_metrics_->predicted_urls_count_ +=
      prediction.value().sorted_predicted_urls().size();

  // It is possible, since it is not stipulated by the API contract, that the
  // navigation predictor will issue multiple predictions during a single page
  // load. Additional predictions should be treated as appending to the ordering
  // of previous predictions.
  size_t original_prediction_ordering_starting_size =
      page_->original_prediction_ordering_.size();

  for (size_t i = 0; i < prediction.value().sorted_predicted_urls().size();
       ++i) {
    GURL url = prediction.value().sorted_predicted_urls()[i];

    size_t url_index = original_prediction_ordering_starting_size + i;
    page_->original_prediction_ordering_.emplace(url, url_index);

    CheckEligibilityOfURL(
        profile_, url,
        base::BindOnce(&IsolatedPrerenderTabHelper::OnGotEligibilityResult,
                       weak_factory_.GetWeakPtr()));
  }
}

// static
content::ServiceWorkerContext*
IsolatedPrerenderTabHelper::GetServiceWorkerContext(Profile* profile) {
  if (g_service_worker_context_for_test)
    return g_service_worker_context_for_test;
  return content::BrowserContext::GetDefaultStoragePartition(profile)
      ->GetServiceWorkerContext();
}

// static
void IsolatedPrerenderTabHelper::CheckEligibilityOfURL(
    Profile* profile,
    const GURL& url,
    OnEligibilityResultCallback result_callback) {
  if (!IsProfileEligible(profile)) {
    std::move(result_callback).Run(url, false, base::nullopt);
    return;
  }

  if (google_util::IsGoogleAssociatedDomainUrl(url)) {
    std::move(result_callback)
        .Run(url, false,
             IsolatedPrerenderPrefetchStatus::kPrefetchNotEligibleGoogleDomain);
    return;
  }

  if (url.HostIsIPAddress()) {
    std::move(result_callback)
        .Run(url, false,
             IsolatedPrerenderPrefetchStatus::
                 kPrefetchNotEligibleHostIsIPAddress);
    return;
  }

  if (!url.SchemeIs(url::kHttpsScheme)) {
    std::move(result_callback)
        .Run(url, false,
             IsolatedPrerenderPrefetchStatus::
                 kPrefetchNotEligibleSchemeIsNotHttps);
    return;
  }

  content::StoragePartition* default_storage_partition =
      content::BrowserContext::GetDefaultStoragePartition(profile);

  // Only the default storage partition is supported since that is the only
  // place where service workers are observed by
  // |IsolatedPrerenderServiceWorkersObserver|.
  if (default_storage_partition !=
      content::BrowserContext::GetStoragePartitionForSite(
          profile, url,
          /*can_create=*/false)) {
    std::move(result_callback)
        .Run(url, false,
             IsolatedPrerenderPrefetchStatus::
                 kPrefetchNotEligibleNonDefaultStoragePartition);
    return;
  }

  IsolatedPrerenderService* isolated_prerender_service =
      IsolatedPrerenderServiceFactory::GetForProfile(profile);
  if (!isolated_prerender_service) {
    std::move(result_callback).Run(url, false, base::nullopt);
    return;
  }

  content::ServiceWorkerContext* service_worker_context_ =
      GetServiceWorkerContext(profile);

  bool site_has_service_worker =
      service_worker_context_->MaybeHasRegistrationForOrigin(
          url::Origin::Create(url));
  if (site_has_service_worker) {
    std::move(result_callback)
        .Run(url, false,
             IsolatedPrerenderPrefetchStatus::
                 kPrefetchNotEligibleUserHasServiceWorker);
    return;
  }

  net::CookieOptions options = net::CookieOptions::MakeAllInclusive();
  options.set_return_excluded_cookies();
  default_storage_partition->GetCookieManagerForBrowserProcess()->GetCookieList(
      url, options,
      base::BindOnce(&OnGotCookieList, url, std::move(result_callback)));
}

void IsolatedPrerenderTabHelper::OnGotEligibilityResult(
    const GURL& url,
    bool eligible,
    base::Optional<IsolatedPrerenderPrefetchStatus> status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // It is possible that this callback is being run late. That is, after the
  // user has navigated away from the origin SRP. To detect this, check if the
  // url exists in the set of predicted urls. If it doesn't, do nothing.
  if (page_->original_prediction_ordering_.find(url) ==
      page_->original_prediction_ordering_.end()) {
    return;
  }

  if (!eligible) {
    if (status) {
      OnPrefetchStatusUpdate(url, status.value());

      if (page_->prefetch_metrics_collector_) {
        page_->prefetch_metrics_collector_->OnMainframeResourceNotEligible(
            url, page_->original_prediction_ordering_.find(url)->second,
            *status);
      }
    }
    return;
  }

  // TODO(robertogden): Consider adding redirect URLs to the front of the list.
  page_->urls_to_prefetch_.push_back(url);
  page_->srp_metrics_->prefetch_eligible_count_++;
  OnPrefetchStatusUpdate(url,
                         IsolatedPrerenderPrefetchStatus::kPrefetchNotStarted);

  if (page_->original_prediction_ordering_.find(url) !=
      page_->original_prediction_ordering_.end()) {
    size_t original_prediction_index =
        page_->original_prediction_ordering_.find(url)->second;
    // Check that we won't go above the allowable size.
    if (original_prediction_index <
        sizeof(page_->srp_metrics_->ordered_eligible_pages_bitmask_) * 8) {
      page_->srp_metrics_->ordered_eligible_pages_bitmask_ |=
          1 << original_prediction_index;
    }
  }

  Prefetch();

  for (auto& observer : observer_list_) {
    observer.OnNewEligiblePrefetchStarted();
  }
}

bool IsolatedPrerenderTabHelper::IsWaitingForAfterSRPCookiesCopy() const {
  switch (page_->cookie_copy_status_) {
    case CookieCopyStatus::kNoNavigation:
    case CookieCopyStatus::kCopyComplete:
      return false;
    case CookieCopyStatus::kWaitingForCopy:
      return true;
  }
}

void IsolatedPrerenderTabHelper::SetOnAfterSRPCookieCopyCompleteCallback(
    base::OnceClosure callback) {
  // We don't expect a callback unless there's something to wait on.
  DCHECK(IsWaitingForAfterSRPCookiesCopy());

  page_->on_after_srp_cookie_copy_complete_ = std::move(callback);
}

void IsolatedPrerenderTabHelper::CopyIsolatedCookiesOnAfterSRPClick(
    const GURL& url) {
  if (!page_->isolated_network_context_) {
    // Not set in unit tests.
    return;
  }

  page_->cookie_copy_status_ = CookieCopyStatus::kWaitingForCopy;

  if (!page_->isolated_cookie_manager_) {
    page_->isolated_network_context_->GetCookieManager(
        page_->isolated_cookie_manager_.BindNewPipeAndPassReceiver());
  }

  net::CookieOptions options = net::CookieOptions::MakeAllInclusive();
  page_->isolated_cookie_manager_->GetCookieList(
      url, options,
      base::BindOnce(
          &IsolatedPrerenderTabHelper::OnGotIsolatedCookiesToCopyAfterSRPClick,
          weak_factory_.GetWeakPtr(), url));
}

void IsolatedPrerenderTabHelper::OnGotIsolatedCookiesToCopyAfterSRPClick(
    const GURL& url,
    const net::CookieAccessResultList& cookie_list,
    const net::CookieAccessResultList& excluded_cookies) {
  DCHECK(IsWaitingForAfterSRPCookiesCopy());

  UMA_HISTOGRAM_COUNTS_100("IsolatedPrerender.Prefetch.Mainframe.CookiesToCopy",
                           cookie_list.size());

  if (cookie_list.empty()) {
    OnCopiedIsolatedCookiesAfterSRPClick();
    return;
  }

  // When |barrier| is run |cookie_list.size()| times, it will run
  // |OnCopiedIsolatedCookiesAfterSRPClick|.
  base::RepeatingClosure barrier = base::BarrierClosure(
      cookie_list.size(),
      base::BindOnce(
          &IsolatedPrerenderTabHelper::OnCopiedIsolatedCookiesAfterSRPClick,
          weak_factory_.GetWeakPtr()));

  content::StoragePartition* default_storage_partition =
      content::BrowserContext::GetDefaultStoragePartition(profile_);
  net::CookieOptions options = net::CookieOptions::MakeAllInclusive();

  for (const net::CookieWithAccessResult& cookie : cookie_list) {
    default_storage_partition->GetCookieManagerForBrowserProcess()
        ->SetCanonicalCookie(cookie.cookie, url, options,
                             base::BindOnce(&CookieSetHelper, barrier));
  }
}

void IsolatedPrerenderTabHelper::OnCopiedIsolatedCookiesAfterSRPClick() {
  DCHECK(IsWaitingForAfterSRPCookiesCopy());

  page_->cookie_copy_status_ = CookieCopyStatus::kCopyComplete;
  if (page_->on_after_srp_cookie_copy_complete_) {
    std::move(page_->on_after_srp_cookie_copy_complete_).Run();
  }
}

network::mojom::URLLoaderFactory*
IsolatedPrerenderTabHelper::GetURLLoaderFactory() {
  if (!page_->isolated_url_loader_factory_) {
    CreateIsolatedURLLoaderFactory();
  }
  DCHECK(page_->isolated_url_loader_factory_);
  return page_->isolated_url_loader_factory_.get();
}

void IsolatedPrerenderTabHelper::CreateNewURLLoaderFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> pending_receiver,
    base::Optional<net::IsolationInfo> isolation_info) {
  DCHECK(page_->isolated_network_context_);

  auto factory_params = network::mojom::URLLoaderFactoryParams::New();
  factory_params->process_id = network::mojom::kBrowserProcessId;
  factory_params->is_trusted = true;
  factory_params->is_corb_enabled = false;
  if (isolation_info) {
    factory_params->isolation_info = *isolation_info;
  }

  page_->isolated_network_context_->CreateURLLoaderFactory(
      std::move(pending_receiver), std::move(factory_params));
}

void IsolatedPrerenderTabHelper::CreateIsolatedURLLoaderFactory() {
  page_->isolated_network_context_.reset();
  page_->isolated_url_loader_factory_.reset();

  IsolatedPrerenderService* isolated_prerender_service =
      IsolatedPrerenderServiceFactory::GetForProfile(profile_);

  auto context_params = network::mojom::NetworkContextParams::New();
  context_params->context_name = "IsolatedPrerender";
  context_params->user_agent = ::GetUserAgent();
  context_params->accept_language = net::HttpUtil::GenerateAcceptLanguageHeader(
      profile_->GetPrefs()->GetString(language::prefs::kAcceptLanguages));
  context_params->initial_custom_proxy_config =
      isolated_prerender_service->proxy_configurator()
          ->CreateCustomProxyConfig();
  context_params->cert_verifier_params = content::GetCertVerifierParams(
      network::mojom::CertVerifierCreationParams::New());
  context_params->cors_exempt_header_list = {
      content::kCorsExemptPurposeHeaderName};
  context_params->cookie_manager_params =
      network::mojom::CookieManagerParams::New();

  context_params->http_cache_enabled = true;
  DCHECK(!context_params->http_cache_path);

  // Also register a client config receiver so that updates to the set of proxy
  // hosts or proxy headers will be updated.
  mojo::Remote<network::mojom::CustomProxyConfigClient> config_client;
  context_params->custom_proxy_config_client_receiver =
      config_client.BindNewPipeAndPassReceiver();
  isolated_prerender_service->proxy_configurator()->AddCustomProxyConfigClient(
      std::move(config_client));

  // Explicitly disallow network service features which could cause a privacy
  // leak.
  context_params->enable_certificate_reporting = false;
  context_params->enable_expect_ct_reporting = false;
  context_params->enable_domain_reliability = false;

  content::GetNetworkService()->CreateNetworkContext(
      page_->isolated_network_context_.BindNewPipeAndPassReceiver(),
      std::move(context_params));

  // Configure a context client to ensure Web Reports and other privacy leak
  // surfaces won't be enabled.
  mojo::PendingRemote<network::mojom::NetworkContextClient> client_remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<IsolatedPrerenderNetworkContextClient>(),
      client_remote.InitWithNewPipeAndPassReceiver());
  page_->isolated_network_context_->SetClient(std::move(client_remote));

  mojo::PendingRemote<network::mojom::URLLoaderFactory> isolated_factory_remote;

  CreateNewURLLoaderFactory(
      isolated_factory_remote.InitWithNewPipeAndPassReceiver(), base::nullopt);

  page_->isolated_url_loader_factory_ = network::SharedURLLoaderFactory::Create(
      std::make_unique<network::WrapperPendingSharedURLLoaderFactory>(
          std::move(isolated_factory_remote)));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(IsolatedPrerenderTabHelper)
