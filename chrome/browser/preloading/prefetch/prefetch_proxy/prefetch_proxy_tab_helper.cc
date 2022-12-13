// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_tab_helper.h"

#include <string>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/feature_list.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/data_saver/data_saver.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/prefetch/prefetch_headers.h"
#include "chrome/browser/prefetch/prefetch_prefs.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_features.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_network_context_client.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_origin_decider.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_origin_prober.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_params.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_prefetch_metrics_collector.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_proxy_configurator.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_service.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_service_factory.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_subresource_manager.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_type.h"
#include "chrome/browser/profiles/profile.h"
#include "components/google/core/common/google_util.h"
#include "components/language/core/browser/pref_names.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/frame_accept_header.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/user_agent.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/cookies/cookie_store.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace {

bool (*g_host_non_unique_filter)(base::StringPiece) = nullptr;

absl::optional<base::TimeDelta> GetTotalPrefetchTime(
    network::mojom::URLResponseHead* head) {
  DCHECK(head);

  base::Time start = head->request_time;
  base::Time end = head->response_time;

  if (start.is_null() || end.is_null())
    return absl::nullopt;

  return end - start;
}

absl::optional<base::TimeDelta> GetPrefetchConnectTime(
    network::mojom::URLResponseHead* head) {
  DCHECK(head);

  base::TimeTicks start = head->load_timing.connect_timing.connect_start;
  base::TimeTicks end = head->load_timing.connect_timing.connect_end;

  if (start.is_null() || end.is_null())
    return absl::nullopt;

  return end - start;
}

void InformPLMOfLikelyPrefetching(content::WebContents* web_contents) {
  page_load_metrics::MetricsWebContentsObserver* metrics_web_contents_observer =
      page_load_metrics::MetricsWebContentsObserver::FromWebContents(
          web_contents);
  if (!metrics_web_contents_observer)
    return;

  metrics_web_contents_observer->OnPrefetchLikely();
}

void OnGotCookieList(
    const GURL& url,
    PrefetchProxyTabHelper::OnEligibilityResultCallback result_callback,
    const net::CookieAccessResultList& cookie_list,
    const net::CookieAccessResultList& excluded_cookies) {
  if (!cookie_list.empty()) {
    std::move(result_callback)
        .Run(url, false,
             PrefetchProxyPrefetchStatus::kPrefetchNotEligibleUserHasCookies);
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
             PrefetchProxyPrefetchStatus::kPrefetchNotEligibleUserHasCookies);
    return;
  }

  std::move(result_callback).Run(url, true, absl::nullopt);
}

void CookieSetHelper(base::RepeatingClosure run_me,
                     net::CookieAccessResult access_result) {
  run_me.Run();
}

bool ShouldStartSpareRenderer() {
  if (!PrefetchProxyStartsSpareRenderer()) {
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

bool ShouldConsiderDecoyRequestForStatus(PrefetchProxyPrefetchStatus status) {
  switch (status) {
    case PrefetchProxyPrefetchStatus::kPrefetchNotEligibleUserHasCookies:
    case PrefetchProxyPrefetchStatus::kPrefetchNotEligibleUserHasServiceWorker:
      // If the prefetch is not eligible because of cookie or a service worker,
      // then maybe send a decoy.
      return true;
    case PrefetchProxyPrefetchStatus::kPrefetchNotEligibleGoogleDomain:
    case PrefetchProxyPrefetchStatus::kPrefetchNotEligibleSchemeIsNotHttps:
    case PrefetchProxyPrefetchStatus::
        kPrefetchNotEligibleNonDefaultStoragePartition:
    case PrefetchProxyPrefetchStatus::kPrefetchPositionIneligible:
    case PrefetchProxyPrefetchStatus::kPrefetchIneligibleRetryAfter:
    case PrefetchProxyPrefetchStatus::kPrefetchProxyNotAvailable:
    case PrefetchProxyPrefetchStatus::kPrefetchNotEligibleHostIsNonUnique:
    case PrefetchProxyPrefetchStatus::kPrefetchNotEligibleDataSaverEnabled:
    case PrefetchProxyPrefetchStatus::kPrefetchNotEligibleExistingProxy:
      // These statuses don't relate to any user state, so don't send a decoy
      // request.
      return false;
    case PrefetchProxyPrefetchStatus::kPrefetchUsedNoProbe:
    case PrefetchProxyPrefetchStatus::kPrefetchUsedProbeSuccess:
    case PrefetchProxyPrefetchStatus::kPrefetchNotUsedProbeFailed:
    case PrefetchProxyPrefetchStatus::kPrefetchNotStarted:
    case PrefetchProxyPrefetchStatus::kPrefetchNotFinishedInTime:
    case PrefetchProxyPrefetchStatus::kPrefetchFailedNetError:
    case PrefetchProxyPrefetchStatus::kPrefetchFailedNon2XX:
    case PrefetchProxyPrefetchStatus::kPrefetchFailedNotHTML:
    case PrefetchProxyPrefetchStatus::kPrefetchSuccessful:
    case PrefetchProxyPrefetchStatus::kNavigatedToLinkNotOnSRP:
    case PrefetchProxyPrefetchStatus::kSubresourceThrottled:
    case PrefetchProxyPrefetchStatus::kPrefetchUsedNoProbeWithNSP:
    case PrefetchProxyPrefetchStatus::kPrefetchUsedProbeSuccessWithNSP:
    case PrefetchProxyPrefetchStatus::kPrefetchNotUsedProbeFailedWithNSP:
    case PrefetchProxyPrefetchStatus::kPrefetchUsedNoProbeNSPAttemptDenied:
    case PrefetchProxyPrefetchStatus::kPrefetchUsedProbeSuccessNSPAttemptDenied:
    case PrefetchProxyPrefetchStatus::
        kPrefetchNotUsedProbeFailedNSPAttemptDenied:
    case PrefetchProxyPrefetchStatus::kPrefetchUsedNoProbeNSPNotStarted:
    case PrefetchProxyPrefetchStatus::kPrefetchUsedProbeSuccessNSPNotStarted:
    case PrefetchProxyPrefetchStatus::kPrefetchNotUsedProbeFailedNSPNotStarted:
    case PrefetchProxyPrefetchStatus::kPrefetchIsPrivacyDecoy:
    case PrefetchProxyPrefetchStatus::kPrefetchIsStale:
    case PrefetchProxyPrefetchStatus::kPrefetchIsStaleWithNSP:
    case PrefetchProxyPrefetchStatus::kPrefetchIsStaleNSPAttemptDenied:
    case PrefetchProxyPrefetchStatus::kPrefetchIsStaleNSPNotStarted:
    case PrefetchProxyPrefetchStatus::kPrefetchNotUsedCookiesChanged:
    case PrefetchProxyPrefetchStatus::kPrefetchFailedRedirectsDisabled:
      // These statuses should not be returned by the eligibility checks, and
      // thus not be passed in here.
      NOTREACHED();
      return false;
  }
}

void RecordPrefetchProxyPrefetchMainframeCookiesToCopy(
    size_t cookie_list_size) {
  UMA_HISTOGRAM_COUNTS_100("PrefetchProxy.Prefetch.Mainframe.CookiesToCopy",
                           cookie_list_size);
}

}  // namespace

PrefetchProxyTabHelper::PrefetchMetrics::PrefetchMetrics() = default;
PrefetchProxyTabHelper::PrefetchMetrics::~PrefetchMetrics() = default;

PrefetchProxyTabHelper::AfterSRPMetrics::AfterSRPMetrics() = default;
PrefetchProxyTabHelper::AfterSRPMetrics::AfterSRPMetrics(
    const AfterSRPMetrics& other) = default;
PrefetchProxyTabHelper::AfterSRPMetrics::~AfterSRPMetrics() = default;

PrefetchProxyTabHelper::CurrentPageLoad::CurrentPageLoad(
    content::NavigationHandle* handle)
    : profile_(handle ? Profile::FromBrowserContext(
                            handle->GetWebContents()->GetBrowserContext())
                      : nullptr),
      navigation_start_(handle ? handle->NavigationStart() : base::TimeTicks()),
      srp_metrics_(
          base::MakeRefCounted<PrefetchProxyTabHelper::PrefetchMetrics>()) {}

PrefetchProxyTabHelper::CurrentPageLoad::~CurrentPageLoad() {
  if (PrefetchProxyStartsSpareRenderer()) {
    UMA_HISTOGRAM_COUNTS_100("PrefetchProxy.SpareRenderer.CountStartedOnSRP",
                             number_of_spare_renderers_started_);
  }

  if (!profile_)
    return;

  PrefetchProxyService* service =
      PrefetchProxyServiceFactory::GetForProfile(profile_);
  if (!service) {
    return;
  }

  for (const auto& iter : prefetch_containers_) {
    PrefetchContainer::NoStatePrefetchStatus nsp_status =
        iter.second->GetNoStatePrefetchStatus();
    if (nsp_status != PrefetchContainer::NoStatePrefetchStatus::kNotStarted &&
        nsp_status != PrefetchContainer::NoStatePrefetchStatus::kFailed) {
      service->DestroySubresourceManagerForURL(iter.second->GetUrl());
    }
  }
}

static content::ServiceWorkerContext* g_service_worker_context_for_test =
    nullptr;

// static
void PrefetchProxyTabHelper::SetServiceWorkerContextForTest(
    content::ServiceWorkerContext* context) {
  g_service_worker_context_for_test = context;
}

// static
void PrefetchProxyTabHelper::SetHostNonUniqueFilterForTest(
    bool (*filter)(base::StringPiece)) {
  g_host_non_unique_filter = filter;
}

// static
void PrefetchProxyTabHelper::ResetHostNonUniqueFilterForTest() {
  g_host_non_unique_filter = nullptr;
}

PrefetchProxyTabHelper::PrefetchProxyTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<PrefetchProxyTabHelper>(*web_contents) {
  page_ = std::make_unique<CurrentPageLoad>(nullptr);
  profile_ = Profile::FromBrowserContext(web_contents->GetBrowserContext());

  NavigationPredictorKeyedService* navigation_predictor_service =
      NavigationPredictorKeyedServiceFactory::GetForProfile(profile_);
  if (navigation_predictor_service) {
    navigation_predictor_service->AddObserver(this);
  }

  // Make sure the global service is up and running so that the service worker
  // registrations can be queried before the first navigation prediction.
  PrefetchProxyServiceFactory::GetForProfile(profile_);
}

PrefetchProxyTabHelper::~PrefetchProxyTabHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NavigationPredictorKeyedService* navigation_predictor_service =
      NavigationPredictorKeyedServiceFactory::GetForProfile(profile_);
  if (navigation_predictor_service) {
    navigation_predictor_service->RemoveObserver(this);
  }
}

void PrefetchProxyTabHelper::AddObserverForTesting(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void PrefetchProxyTabHelper::RemoveObserverForTesting(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

network::mojom::NetworkContext*
PrefetchProxyTabHelper::GetIsolatedContextForTesting(const GURL& url) const {
  PrefetchProxyNetworkContext* network_context =
      page_->GetNetworkContextForUrl(url);
  if (!network_context)
    return nullptr;
  return network_context->GetNetworkContext();
}

absl::optional<PrefetchProxyTabHelper::AfterSRPMetrics>
PrefetchProxyTabHelper::after_srp_metrics() const {
  if (page_->after_srp_metrics_) {
    return *(page_->after_srp_metrics_);
  }
  return absl::nullopt;
}

// static
bool PrefetchProxyTabHelper::IsProfileEligible(Profile* profile) {
  if (profile->IsOffTheRecord()) {
    return false;
  }

  return true;
}

bool PrefetchProxyTabHelper::IsProfileEligible() const {
  return IsProfileEligible(profile_);
}

void PrefetchProxyTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  // This check is only relevant for detecting AMP pages. For this feature, AMP
  // pages won't get sped up any so just ignore them.
  if (navigation_handle->IsSameDocument()) {
    return;
  }

  // Don't take any actions during a prefetch since it was probably triggered
  // by another instance of this class and we don't want to interfere.
  prerender::NoStatePrefetchManager* no_state_prefetch_manager =
      prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(profile_);
  if (no_state_prefetch_manager &&
      no_state_prefetch_manager->IsWebContentsPrefetching(web_contents())) {
    return;
  }

  // User is navigating, don't bother prefetching further.
  page_->url_loaders_.clear();

  if (page_->srp_metrics_->prefetch_attempted_count_ > 0) {
    UMA_HISTOGRAM_COUNTS_100(
        "PrefetchProxy.Prefetch.Mainframe.TotalRedirects",
        page_->srp_metrics_->prefetch_total_redirect_count_);
  }

  const GURL& url = navigation_handle->GetURL();

  // If the cookies associated with |url| have changed since the initial
  // eligibility check, then we shouldn't serve prefetched resources.
  if (HaveCookiesChanged(url)) {
    OnPrefetchStatusUpdate(
        url, PrefetchProxyPrefetchStatus::kPrefetchNotUsedCookiesChanged);
    return;
  }

  PrepareToServe(url);
}

void PrefetchProxyTabHelper::PrepareToServe(const GURL& url) {
  // TODO(https://crbug.com/1238926): At this point in the navigation it's not
  // guaranteed that we serve the prefetch, so consider moving the cookies to
  // the interception path for robustness.
  auto prefetch_container_iter = page_->prefetch_containers_.find(url);
  if (prefetch_container_iter != page_->prefetch_containers_.end() &&
      prefetch_container_iter->second->HasPrefetchedResponse()) {
    // Content older than 5 minutes should not be served.
    if (prefetch_container_iter->second->IsPrefetchedResponseValid(
            PrefetchProxyCacheableDuration())) {
      // Start copying any needed cookies over to the main profile if this page
      // was prefetched.
      CopyIsolatedCookiesOnAfterSRPClick(prefetch_container_iter->second.get());
    }
  }

  // Notify the subresource manager (if applicable)  that its page is being
  // navigated to so that the prefetched subresources can be used from cache.
  PrefetchProxyService* service =
      PrefetchProxyServiceFactory::GetForProfile(profile_);
  if (!service)
    return;

  PrefetchProxySubresourceManager* subresource_manager =
      service->GetSubresourceManagerForURL(url);
  if (!subresource_manager)
    return;

  subresource_manager->NotifyPageNavigatedToAfterSRP();
}

void PrefetchProxyTabHelper::NotifyPrefetchProbeLatency(
    base::TimeDelta probe_latency) {
  page_->probe_latency_ = probe_latency;
}

void PrefetchProxyTabHelper::ReportProbeResult(
    const GURL& url,
    PrefetchProxyProbeResult result) {
  if (!page_->prefetch_metrics_collector_) {
    return;
  }
  page_->prefetch_metrics_collector_->OnMainframeNavigationProbeResult(url,
                                                                       result);
}

void PrefetchProxyTabHelper::OnPrefetchStatusUpdate(
    const GURL& url,
    PrefetchProxyPrefetchStatus usage) {
  auto prefetch_container_iter = page_->prefetch_containers_.find(url);
  if (prefetch_container_iter != page_->prefetch_containers_.end())
    prefetch_container_iter->second->SetPrefetchStatus(usage);
}

PrefetchProxyPrefetchStatus
PrefetchProxyTabHelper::MaybeUpdatePrefetchStatusWithNSPContext(
    PrefetchContainer* prefetch_container) const {
  DCHECK(prefetch_container);

  switch (prefetch_container->GetPrefetchStatus()) {
    // These are the statuses we want to update.
    case PrefetchProxyPrefetchStatus::kPrefetchUsedNoProbe:
    case PrefetchProxyPrefetchStatus::kPrefetchUsedProbeSuccess:
    case PrefetchProxyPrefetchStatus::kPrefetchNotUsedProbeFailed:
    case PrefetchProxyPrefetchStatus::kPrefetchIsStale:
      break;
    // These statuses are not applicable since the prefetch was not used after
    // the click.
    case PrefetchProxyPrefetchStatus::kPrefetchNotStarted:
    case PrefetchProxyPrefetchStatus::kPrefetchNotEligibleGoogleDomain:
    case PrefetchProxyPrefetchStatus::kPrefetchNotEligibleUserHasCookies:
    case PrefetchProxyPrefetchStatus::kPrefetchNotEligibleUserHasServiceWorker:
    case PrefetchProxyPrefetchStatus::kPrefetchNotEligibleSchemeIsNotHttps:
    case PrefetchProxyPrefetchStatus::
        kPrefetchNotEligibleNonDefaultStoragePartition:
    case PrefetchProxyPrefetchStatus::kPrefetchNotFinishedInTime:
    case PrefetchProxyPrefetchStatus::kPrefetchFailedNetError:
    case PrefetchProxyPrefetchStatus::kPrefetchFailedNon2XX:
    case PrefetchProxyPrefetchStatus::kPrefetchFailedNotHTML:
    case PrefetchProxyPrefetchStatus::kPrefetchSuccessful:
    case PrefetchProxyPrefetchStatus::kNavigatedToLinkNotOnSRP:
    case PrefetchProxyPrefetchStatus::kSubresourceThrottled:
    case PrefetchProxyPrefetchStatus::kPrefetchPositionIneligible:
    case PrefetchProxyPrefetchStatus::kPrefetchIneligibleRetryAfter:
    case PrefetchProxyPrefetchStatus::kPrefetchProxyNotAvailable:
    case PrefetchProxyPrefetchStatus::kPrefetchIsPrivacyDecoy:
    case PrefetchProxyPrefetchStatus::kPrefetchNotUsedCookiesChanged:
    case PrefetchProxyPrefetchStatus::kPrefetchFailedRedirectsDisabled:
    case PrefetchProxyPrefetchStatus::kPrefetchNotEligibleHostIsNonUnique:
    case PrefetchProxyPrefetchStatus::kPrefetchNotEligibleDataSaverEnabled:
    case PrefetchProxyPrefetchStatus::kPrefetchNotEligibleExistingProxy:
      return prefetch_container->GetPrefetchStatus();
    // These statuses we are going to update to, and this is the only place that
    // they are set so they are not expected to be passed in.
    case PrefetchProxyPrefetchStatus::kPrefetchUsedNoProbeWithNSP:
    case PrefetchProxyPrefetchStatus::kPrefetchUsedProbeSuccessWithNSP:
    case PrefetchProxyPrefetchStatus::kPrefetchNotUsedProbeFailedWithNSP:
    case PrefetchProxyPrefetchStatus::kPrefetchUsedNoProbeNSPAttemptDenied:
    case PrefetchProxyPrefetchStatus::kPrefetchUsedProbeSuccessNSPAttemptDenied:
    case PrefetchProxyPrefetchStatus::
        kPrefetchNotUsedProbeFailedNSPAttemptDenied:
    case PrefetchProxyPrefetchStatus::kPrefetchUsedNoProbeNSPNotStarted:
    case PrefetchProxyPrefetchStatus::kPrefetchUsedProbeSuccessNSPNotStarted:
    case PrefetchProxyPrefetchStatus::kPrefetchNotUsedProbeFailedNSPNotStarted:
    case PrefetchProxyPrefetchStatus::kPrefetchIsStaleWithNSP:
    case PrefetchProxyPrefetchStatus::kPrefetchIsStaleNSPAttemptDenied:
    case PrefetchProxyPrefetchStatus::kPrefetchIsStaleNSPNotStarted:
      NOTREACHED();
      return prefetch_container->GetPrefetchStatus();
  }

  PrefetchContainer::NoStatePrefetchStatus nsp_status =
      prefetch_container->GetNoStatePrefetchStatus();

  if (nsp_status == PrefetchContainer::NoStatePrefetchStatus::kNotStarted) {
    return prefetch_container->GetPrefetchStatus();
  }

  if (nsp_status == PrefetchContainer::NoStatePrefetchStatus::kSucceeded) {
    switch (prefetch_container->GetPrefetchStatus()) {
      case PrefetchProxyPrefetchStatus::kPrefetchUsedNoProbe:
        return PrefetchProxyPrefetchStatus::kPrefetchUsedNoProbeWithNSP;
      case PrefetchProxyPrefetchStatus::kPrefetchUsedProbeSuccess:
        return PrefetchProxyPrefetchStatus::kPrefetchUsedProbeSuccessWithNSP;
      case PrefetchProxyPrefetchStatus::kPrefetchNotUsedProbeFailed:
        return PrefetchProxyPrefetchStatus::kPrefetchNotUsedProbeFailedWithNSP;
      case PrefetchProxyPrefetchStatus::kPrefetchIsStale:
        return PrefetchProxyPrefetchStatus::kPrefetchIsStaleWithNSP;
      default:
        break;
    }
  }

  if (nsp_status == PrefetchContainer::NoStatePrefetchStatus::kFailed) {
    switch (prefetch_container->GetPrefetchStatus()) {
      case PrefetchProxyPrefetchStatus::kPrefetchUsedNoProbe:
        return PrefetchProxyPrefetchStatus::
            kPrefetchUsedNoProbeNSPAttemptDenied;
      case PrefetchProxyPrefetchStatus::kPrefetchUsedProbeSuccess:
        return PrefetchProxyPrefetchStatus::
            kPrefetchUsedProbeSuccessNSPAttemptDenied;
      case PrefetchProxyPrefetchStatus::kPrefetchNotUsedProbeFailed:
        return PrefetchProxyPrefetchStatus::
            kPrefetchNotUsedProbeFailedNSPAttemptDenied;
      case PrefetchProxyPrefetchStatus::kPrefetchIsStale:
        return PrefetchProxyPrefetchStatus::kPrefetchIsStaleNSPAttemptDenied;
      default:
        break;
    }
  }

  if (nsp_status == PrefetchContainer::NoStatePrefetchStatus::kInProgress) {
    switch (prefetch_container->GetPrefetchStatus()) {
      case PrefetchProxyPrefetchStatus::kPrefetchUsedNoProbe:
        return PrefetchProxyPrefetchStatus::kPrefetchUsedNoProbeNSPNotStarted;
      case PrefetchProxyPrefetchStatus::kPrefetchUsedProbeSuccess:
        return PrefetchProxyPrefetchStatus::
            kPrefetchUsedProbeSuccessNSPNotStarted;
      case PrefetchProxyPrefetchStatus::kPrefetchNotUsedProbeFailed:
        return PrefetchProxyPrefetchStatus::
            kPrefetchNotUsedProbeFailedNSPNotStarted;
      case PrefetchProxyPrefetchStatus::kPrefetchIsStale:
        return PrefetchProxyPrefetchStatus::kPrefetchIsStaleNSPNotStarted;
      default:
        break;
    }
  }

  NOTREACHED();
  return prefetch_container->GetPrefetchStatus();
}

std::unique_ptr<PrefetchProxyTabHelper::AfterSRPMetrics>
PrefetchProxyTabHelper::ComputeAfterSRPMetricsBeforeCommit(
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
  absl::optional<PrefetchProxyPrefetchStatus> status;
  absl::optional<size_t> prediction_position;
  for (const GURL& chain_url : base::Reversed(handle->GetRedirectChain())) {
    auto container_iter = page_->prefetch_containers_.find(chain_url);
    if (!status && container_iter != page_->prefetch_containers_.end() &&
        container_iter->second->HasPrefetchStatus()) {
      status =
          MaybeUpdatePrefetchStatusWithNSPContext(container_iter->second.get());
    }

    // Same check for the original prediction ordering.
    if (!prediction_position &&
        container_iter != page_->prefetch_containers_.end()) {
      prediction_position =
          container_iter->second->GetOriginalPredictionIndex();
    }
  }

  if (status) {
    metrics->prefetch_status_ = *status;
  } else {
    metrics->prefetch_status_ =
        PrefetchProxyPrefetchStatus::kNavigatedToLinkNotOnSRP;
  }
  metrics->clicked_link_srp_position_ = prediction_position;

  return metrics;
}

void PrefetchProxyTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!navigation_handle->IsInPrimaryMainFrame()) {
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

  // Don't take any actions during a prefetch since it was probably triggered
  // by another instance of this class and we don't want to interfere.
  prerender::NoStatePrefetchManager* no_state_prefetch_manager =
      prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(profile_);
  if (no_state_prefetch_manager &&
      no_state_prefetch_manager->IsWebContentsPrefetching(web_contents())) {
    return;
  }

  // Ensure there's no ongoing prefetches.
  page_->url_loaders_.clear();

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

    // See if the page being navigated to was prefetched. If so, copy over its
    // subresource manager and networking pipes.
    PrefetchProxyService* service =
        PrefetchProxyServiceFactory::GetForProfile(profile_);
    std::unique_ptr<PrefetchProxySubresourceManager> manager =
        service->TakeSubresourceManagerForURL(url);
    if (manager) {
      new_page->subresource_manager_ = std::move(manager);

      if (PrefetchProxyUseIndividualNetworkContextsForEachPrefetch()) {
        auto prefetch_container_iter = page_->prefetch_containers_.find(url);
        if (prefetch_container_iter != page_->prefetch_containers_.end() &&
            prefetch_container_iter->second->GetNetworkContext()) {
          new_page->previous_network_context_ =
              prefetch_container_iter->second->ReleaseNetworkContext();
        }
      } else {
        new_page->previous_network_context_ =
            std::move(page_->network_context_);
      }
    }
  }

  // |page_| is reset on commit so that any available cached prefetches that
  // result from a redirect get used.
  page_ = std::move(new_page);
}

void PrefetchProxyTabHelper::OnVisibilityChanged(
    content::Visibility visibility) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!PrefetchProxyIsEnabled()) {
    return;
  }

  // Start prefetching if the tab has become visible and prefetching is
  // inactive. Hidden and occluded visibility is ignored here so that pending
  // prefetches can finish.
  if (visibility == content::Visibility::VISIBLE && !PrefetchingActive())
    Prefetch();
}

std::unique_ptr<PrefetchedMainframeResponseContainer>
PrefetchProxyTabHelper::TakePrefetchResponse(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto prefetch_container_iter = page_->prefetch_containers_.find(url);
  if (prefetch_container_iter == page_->prefetch_containers_.end())
    return nullptr;

  if (!prefetch_container_iter->second->HasPrefetchedResponse())
    return nullptr;

  // Content older than 5 minutes should not be served.
  if (!prefetch_container_iter->second->IsPrefetchedResponseValid(
          PrefetchProxyCacheableDuration())) {
    prefetch_container_iter->second->SetPrefetchStatus(
        PrefetchProxyPrefetchStatus::kPrefetchIsStale);
    return nullptr;
  }

  return prefetch_container_iter->second->ReleasePrefetchedResponse();
}

std::unique_ptr<PrefetchedMainframeResponseContainer>
PrefetchProxyTabHelper::CopyPrefetchResponseForNSP(
    PrefetchContainer* prefetch_container) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(prefetch_container);

  if (!prefetch_container->HasPrefetchedResponse())
    return nullptr;

  return prefetch_container->ClonePrefetchedResponse();
}

bool PrefetchProxyTabHelper::PrefetchingActive() const {
  return page_ && !page_->url_loaders_.empty();
}

void PrefetchProxyTabHelper::Prefetch() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(PrefetchProxyIsEnabled());

  if (!page_->srp_metrics_->navigation_to_prefetch_start_.has_value()) {
    page_->srp_metrics_->navigation_to_prefetch_start_ =
        base::TimeTicks::Now() - page_->navigation_start_;
    DCHECK_GT(page_->srp_metrics_->navigation_to_prefetch_start_.value(),
              base::TimeDelta());
  }

  if (PrefetchProxyCloseIdleSockets()) {
    if (page_->network_context_) {
      page_->network_context_->CloseIdleConnections();
    }

    for (const auto& iter : page_->prefetch_containers_) {
      if (iter.second->GetNetworkContext()) {
        iter.second->GetNetworkContext()->CloseIdleConnections();
      }
    }
  }

  if (web_contents()->GetVisibility() != content::Visibility::VISIBLE) {
    // |OnVisibilityChanged| will restart prefetching when the tab becomes
    // visible again.
    return;
  }

  DCHECK_GT(PrefetchProxyMaximumNumberOfConcurrentPrefetches(), 0U);

  while (
      // Checks that the total number of prefetches has not been met.
      !(PrefetchProxyMaximumNumberOfPrefetches().has_value() &&
        page_->decoy_requests_attempted_ +
                page_->srp_metrics_->prefetch_attempted_count_ >=
            PrefetchProxyMaximumNumberOfPrefetches().value()) &&

      // Checks that there are still urls to prefetch.
      !page_->urls_to_prefetch_.empty() &&

      // Checks that the max number of concurrent prefetches has not been met.
      page_->url_loaders_.size() <
          PrefetchProxyMaximumNumberOfConcurrentPrefetches()) {
    StartSinglePrefetch();
  }
}

void PrefetchProxyTabHelper::StartSinglePrefetch() {
  DCHECK(!page_->urls_to_prefetch_.empty());
  DCHECK(!(PrefetchProxyMaximumNumberOfPrefetches().has_value() &&
           page_->decoy_requests_attempted_ +
                   page_->srp_metrics_->prefetch_attempted_count_ >=
               PrefetchProxyMaximumNumberOfPrefetches().value()));
  DCHECK(page_->url_loaders_.size() <
         PrefetchProxyMaximumNumberOfConcurrentPrefetches());

  PrefetchContainer* prefetch_container = page_->urls_to_prefetch_[0];
  page_->urls_to_prefetch_.erase(page_->urls_to_prefetch_.begin());

  // Only update these metrics on normal prefetches.
  if (!prefetch_container->IsDecoy()) {
    page_->srp_metrics_->prefetch_attempted_count_++;
    // The status is updated to be successful or failed when it finishes.
    prefetch_container->SetPrefetchStatus(
        PrefetchProxyPrefetchStatus::kPrefetchNotFinishedInTime);
  } else {
    page_->decoy_requests_attempted_++;
    prefetch_container->SetPrefetchStatus(
        PrefetchProxyPrefetchStatus::kPrefetchIsPrivacyDecoy);
  }

  url::Origin origin = url::Origin::Create(prefetch_container->GetUrl());
  net::IsolationInfo isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kMainFrame, origin, origin,
      net::SiteForCookies::FromOrigin(origin));
  network::ResourceRequest::TrustedParams trusted_params;
  trusted_params.isolation_info = isolation_info;

  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = prefetch_container->GetUrl();
  request->method = "GET";
  request->enable_load_timing = true;
  request->load_flags = net::LOAD_DISABLE_CACHE | net::LOAD_PREFETCH;
  request->credentials_mode = network::mojom::CredentialsMode::kInclude;
  request->headers.SetHeader(content::kCorsExemptPurposeHeaderName, "prefetch");
  request->headers.SetHeader(
      prefetch::headers::kSecPurposeHeaderName,
      prefetch_container->GetPrefetchType().IsProxyRequired()
          ? prefetch::headers::kSecPurposePrefetchAnonymousClientIpHeaderValue
          : prefetch::headers::kSecPurposePrefetchHeaderValue);
  request->headers.SetHeader(
      net::HttpRequestHeaders::kAccept,
      content::FrameAcceptHeaderValue(/*allow_sxg_responses=*/true, profile_));
  request->headers.SetHeader("Upgrade-Insecure-Requests", "1");
  // Remove the user agent header if it was set so that the network context's
  // default is used.
  request->headers.RemoveHeader("User-Agent");
  request->trusted_params = trusted_params;
  request->site_for_cookies = trusted_params.isolation_info.site_for_cookies();
  request->devtools_request_id = prefetch_container->RequestId();

  const auto& devtools_observer = prefetch_container->GetDevToolsObserver();
  if (devtools_observer && !prefetch_container->IsDecoy()) {
    request->trusted_params->devtools_observer =
        devtools_observer->MakeSelfOwnedNetworkServiceDevToolsObserver();
    devtools_observer->OnStartSinglePrefetch(prefetch_container->RequestId(),
                                             *request);
  }

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
  loader->SetOnRedirectCallback(base::BindRepeating(
      &PrefetchProxyTabHelper::OnPrefetchRedirect, base::Unretained(this),
      loader.get(), prefetch_container->GetUrl()));
  loader->SetAllowHttpErrorResults(true);
  loader->SetTimeoutDuration(PrefetchProxyTimeoutDuration());
  loader->SetURLLoaderFactoryOptions(
      network::mojom::kURLLoadOptionSendSSLInfoWithResponse |
      network::mojom::kURLLoadOptionSniffMimeType |
      network::mojom::kURLLoadOptionSendSSLInfoForCertificateError);
  loader->DownloadToString(
      GetURLLoaderFactory(prefetch_container->GetUrl()),
      base::BindOnce(&PrefetchProxyTabHelper::OnPrefetchComplete,
                     base::Unretained(this), loader.get(),
                     prefetch_container->GetUrl(), isolation_info),
      PrefetchProxyMainframeBodyLengthLimit());

  page_->url_loaders_.emplace(std::move(loader));

  if (!prefetch_container->IsDecoy() &&
      page_->srp_metrics_->prefetch_attempted_count_ == 1) {
    // Make sure canary checks have run so we know the result by the time we
    // want to use the prefetch. Checking the canary cache can be a slow and
    // blocking operation (see crbug.com/1266018), so we only do this for the
    // first non-decoy prefetch we make on the page.
    // TODO(crbug.com/1266018): once this bug is fixed, fire off canary check
    // regardless of whether the request is a decoy or not.
    PrefetchProxyService* service =
        PrefetchProxyServiceFactory::GetForProfile(profile_);
    service->origin_prober()->RunCanaryChecksIfNeeded();
  }

  // Start a spare renderer now so that it will be ready by the time it is
  // useful to have.
  if (ShouldStartSpareRenderer()) {
    StartSpareRenderer();
  }
}

void PrefetchProxyTabHelper::OnPrefetchRedirect(
    network::SimpleURLLoader* loader,
    const GURL& original_url,
    const net::RedirectInfo& redirect_info,
    const network::mojom::URLResponseHead& response_head,
    std::vector<std::string>* removed_headers) {
  DCHECK(PrefetchingActive());

  // Currently all redirects are disabled when using the prefetch proxy. See
  // crbug.com/1266876 for more details.
  OnPrefetchStatusUpdate(
      original_url,
      PrefetchProxyPrefetchStatus::kPrefetchFailedRedirectsDisabled);

  auto prefetch_container_iter = page_->prefetch_containers_.find(original_url);
  if (prefetch_container_iter != page_->prefetch_containers_.end()) {
    const auto& devtools_observer =
        prefetch_container_iter->second->GetDevToolsObserver();
    if (devtools_observer) {
      devtools_observer->OnPrefetchResponseReceived(
          original_url, prefetch_container_iter->second->RequestId(),
          response_head);

      devtools_observer->OnPrefetchRequestComplete(
          prefetch_container_iter->second->RequestId(),
          network::URLLoaderCompletionStatus{net::ERR_NOT_IMPLEMENTED});
    }
  }

  // Cancels the current request.
  DCHECK(page_->url_loaders_.find(loader) != page_->url_loaders_.end());
  page_->url_loaders_.erase(page_->url_loaders_.find(loader));

  // Continue prefetching other urls.
  Prefetch();
}

void PrefetchProxyTabHelper::OnPrefetchComplete(
    network::SimpleURLLoader* loader,
    const GURL& url,
    const net::IsolationInfo& isolation_info,
    std::unique_ptr<std::string> body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(PrefetchingActive());

  auto prefetch_container_iter = page_->prefetch_containers_.find(url);
  DCHECK(prefetch_container_iter != page_->prefetch_containers_.end());

  if (prefetch_container_iter->second->IsDecoy()) {
    if (loader->CompletionStatus()) {
      page_->prefetch_metrics_collector_->OnDecoyPrefetchComplete(
          url, prefetch_container_iter->second->GetOriginalPredictionIndex(),
          loader->ResponseInfo() ? loader->ResponseInfo()->Clone() : nullptr,
          loader->CompletionStatus().value());
    }

    for (auto& observer : observer_list_) {
      observer.OnDecoyPrefetchCompleted(url);
    }

    // Do nothing with the response, i.e.: don't cache it.

    // Cancels the current request.
    DCHECK(page_->url_loaders_.find(loader) != page_->url_loaders_.end());
    page_->url_loaders_.erase(page_->url_loaders_.find(loader));

    Prefetch();
    return;
  }

  base::UmaHistogramSparse("PrefetchProxy.Prefetch.Mainframe.NetError",
                           std::abs(loader->NetError()));
  const auto& devtools_observer =
      prefetch_container_iter->second->GetDevToolsObserver();
  if (devtools_observer) {
    if (loader->ResponseInfo()) {
      devtools_observer->OnPrefetchResponseReceived(
          url, prefetch_container_iter->second->RequestId(),
          *loader->ResponseInfo());
    }

    if (body) {
      devtools_observer->OnPrefetchBodyDataReceived(
          prefetch_container_iter->second->RequestId(), *body,
          /*is_base64_encoded=*/false);
    }

    devtools_observer->OnPrefetchRequestComplete(
        prefetch_container_iter->second->RequestId(),
        loader->CompletionStatus().value_or(
            network::URLLoaderCompletionStatus(loader->NetError())));
  }

  if (loader->CompletionStatus()) {
    page_->prefetch_metrics_collector_->OnMainframeResourcePrefetched(
        url, prefetch_container_iter->second->GetOriginalPredictionIndex(),
        loader->ResponseInfo() ? loader->ResponseInfo()->Clone() : nullptr,
        loader->CompletionStatus().value());
  }

  if (loader->NetError() != net::OK) {
    prefetch_container_iter->second->SetPrefetchStatus(
        PrefetchProxyPrefetchStatus::kPrefetchFailedNetError);

    for (auto& observer : observer_list_) {
      observer.OnPrefetchCompletedWithError(url, loader->NetError());
    }
  }

  if (loader->NetError() == net::OK && body && loader->ResponseInfo()) {
    network::mojom::URLResponseHeadPtr head = loader->ResponseInfo()->Clone();

    // Verifies that the request was made using the prefetch proxy if required,
    // or made directly if the proxy was not required.
    DCHECK(prefetch_container_iter->second->GetPrefetchType()
               .IsProxyBypassedForTesting() ||
           !head->proxy_server.is_direct() ==
               prefetch_container_iter->second->GetPrefetchType()
                   .IsProxyRequired());

    HandlePrefetchResponse(prefetch_container_iter->second.get(),
                           isolation_info, std::move(head), std::move(body));
  }

  DCHECK(page_->url_loaders_.find(loader) != page_->url_loaders_.end());
  page_->url_loaders_.erase(page_->url_loaders_.find(loader));

  Prefetch();
}

void PrefetchProxyTabHelper::HandlePrefetchResponse(
    PrefetchContainer* prefetch_container,
    const net::IsolationInfo& isolation_info,
    network::mojom::URLResponseHeadPtr head,
    std::unique_ptr<std::string> body) {
  DCHECK(!head->was_fetched_via_cache);
  DCHECK(prefetch_container);

  if (!head->headers)
    return;

  UMA_HISTOGRAM_COUNTS_10M("PrefetchProxy.Prefetch.Mainframe.BodyLength",
                           body->size());

  absl::optional<base::TimeDelta> total_time = GetTotalPrefetchTime(head.get());
  if (total_time) {
    UMA_HISTOGRAM_CUSTOM_TIMES("PrefetchProxy.Prefetch.Mainframe.TotalTime",
                               *total_time, base::Milliseconds(10),
                               base::Seconds(30), 100);
  }

  absl::optional<base::TimeDelta> connect_time =
      GetPrefetchConnectTime(head.get());
  if (connect_time) {
    UMA_HISTOGRAM_TIMES("PrefetchProxy.Prefetch.Mainframe.ConnectTime",
                        *connect_time);
  }

  int response_code = head->headers->response_code();

  base::UmaHistogramSparse("PrefetchProxy.Prefetch.Mainframe.RespCode",
                           response_code);

  if (response_code < 200 || response_code >= 300) {
    prefetch_container->SetPrefetchStatus(
        PrefetchProxyPrefetchStatus::kPrefetchFailedNon2XX);
    for (auto& observer : observer_list_) {
      observer.OnPrefetchCompletedWithError(prefetch_container->GetUrl(),
                                            response_code);
    }

    if (response_code == net::HTTP_SERVICE_UNAVAILABLE) {
      base::TimeDelta retry_after;
      std::string retry_after_string;
      if (head->headers->EnumerateHeader(nullptr, "Retry-After",
                                         &retry_after_string) &&
          net::HttpUtil::ParseRetryAfterHeader(
              retry_after_string, base::Time::Now(), &retry_after)) {
        PrefetchProxyService* service =
            PrefetchProxyServiceFactory::GetForProfile(profile_);
        service->origin_decider()->ReportOriginRetryAfter(
            prefetch_container->GetUrl(), retry_after);
      }
    }

    return;
  }

  if (PrefetchProxyHTMLOnly() && head->mime_type != "text/html") {
    prefetch_container->SetPrefetchStatus(
        PrefetchProxyPrefetchStatus::kPrefetchFailedNotHTML);
    return;
  }

  prefetch_container->SetPrefetchedResponse(
      std::make_unique<PrefetchedMainframeResponseContainer>(
          isolation_info, std::move(head), std::move(body)));
  page_->srp_metrics_->prefetch_successful_count_++;

  prefetch_container->SetPrefetchStatus(
      PrefetchProxyPrefetchStatus::kPrefetchSuccessful);

  MaybeDoNoStatePrefetch(prefetch_container);

  for (auto& observer : observer_list_) {
    observer.OnPrefetchCompletedSuccessfully(prefetch_container->GetUrl());
  }
}

void PrefetchProxyTabHelper::MaybeDoNoStatePrefetch(
    PrefetchContainer* prefetch_container) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(prefetch_container);

  if (!PrefetchProxyNoStatePrefetchSubresources()) {
    return;
  }

  // Not all prefetches are eligible for NSP, which fetches subresources.
  if (!prefetch_container->GetPrefetchType().AllowedToPrefetchSubresources())
    return;

  page_->urls_to_no_state_prefetch_.push_back(prefetch_container);
  prefetch_container->SetNoStatePrefetchStatus(
      PrefetchContainer::NoStatePrefetchStatus::kInProgress);
  DoNoStatePrefetch();
}

void PrefetchProxyTabHelper::DoNoStatePrefetch() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (page_->urls_to_no_state_prefetch_.empty()) {
    return;
  }

  // Ensure there is not an active navigation.
  if (web_contents()->GetController().GetPendingEntry()) {
    return;
  }

  absl::optional<size_t> max_attempts =
      PrefetchProxyMaximumNumberOfNoStatePrefetchAttempts();
  if (max_attempts.has_value() &&
      page_->number_of_no_state_prefetch_attempts_ >= max_attempts.value()) {
    return;
  }

  prerender::NoStatePrefetchManager* no_state_prefetch_manager =
      prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(profile_);
  if (!no_state_prefetch_manager) {
    return;
  }

  PrefetchProxyService* service =
      PrefetchProxyServiceFactory::GetForProfile(profile_);
  if (!service) {
    return;
  }

  PrefetchContainer* prefetch_container = page_->urls_to_no_state_prefetch_[0];

  // Don't start another NSP until the previous one finishes.
  {
    PrefetchProxySubresourceManager* manager =
        service->GetSubresourceManagerForURL(prefetch_container->GetUrl());
    if (manager && manager->has_nsp_handle()) {
      return;
    }
  }

  // The manager must be created here so that the mainframe response can be
  // given to the URLLoaderInterceptor in this call stack, but may be destroyed
  // before the end of the method if the handle is not created.
  PrefetchProxySubresourceManager* manager = service->OnAboutToNoStatePrefetch(
      prefetch_container->GetUrl(),
      CopyPrefetchResponseForNSP(prefetch_container));
  DCHECK_EQ(manager,
            service->GetSubresourceManagerForURL(prefetch_container->GetUrl()));

  manager->SetPrefetchMetricsCollector(page_->prefetch_metrics_collector_);

  DCHECK(page_->GetNetworkContextForUrl(prefetch_container->GetUrl()));
  manager->SetCreateIsolatedLoaderFactoryCallback(base::BindRepeating(
      &PrefetchProxyNetworkContext::CreateNewUrlLoaderFactory,
      page_->GetNetworkContextForUrl(prefetch_container->GetUrl())
          ->GetWeakPtr()));

  content::SessionStorageNamespace* session_storage_namespace =
      web_contents()->GetController().GetDefaultSessionStorageNamespace();
  gfx::Size size = web_contents()->GetContainerBounds().size();

  std::unique_ptr<prerender::NoStatePrefetchHandle> handle =
      no_state_prefetch_manager->AddIsolatedPrerender(
          prefetch_container->GetUrl(), session_storage_namespace, size);

  if (!handle) {
    // Clean up the prefetch response in |service| since it wasn't used.
    service->DestroySubresourceManagerForURL(prefetch_container->GetUrl());
    // Don't use |manager| again!

    prefetch_container->SetNoStatePrefetchStatus(
        PrefetchContainer::NoStatePrefetchStatus::kFailed);

    // Try the next URL.
    page_->urls_to_no_state_prefetch_.erase(
        page_->urls_to_no_state_prefetch_.begin());
    DoNoStatePrefetch();
    return;
  }

  page_->number_of_no_state_prefetch_attempts_++;

  // It is possible for the manager to be destroyed during the NoStatePrefetch
  // navigation. If this happens, abort the NSP and try again.
  manager = service->GetSubresourceManagerForURL(prefetch_container->GetUrl());
  if (!manager) {
    handle->OnCancel();
    handle.reset();

    prefetch_container->SetNoStatePrefetchStatus(
        PrefetchContainer::NoStatePrefetchStatus::kFailed);

    // Try the next URL.
    page_->urls_to_no_state_prefetch_.erase(
        page_->urls_to_no_state_prefetch_.begin());
    DoNoStatePrefetch();
    return;
  }

  manager->ManageNoStatePrefetch(
      std::move(handle),
      base::BindOnce(&PrefetchProxyTabHelper::OnPrerenderDone,
                     weak_factory_.GetWeakPtr(), prefetch_container->GetUrl()));
}

void PrefetchProxyTabHelper::OnPrerenderDone(const GURL& url) {
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
      url != page_->urls_to_no_state_prefetch_[0]->GetUrl()) {
    return;
  }

  auto prefetch_container_iter = page_->prefetch_containers_.find(url);
  if (prefetch_container_iter == page_->prefetch_containers_.end())
    return;

  prefetch_container_iter->second->SetNoStatePrefetchStatus(
      PrefetchContainer::NoStatePrefetchStatus::kSucceeded);

  for (auto& observer : observer_list_) {
    observer.OnNoStatePrefetchFinished();
  }

  page_->urls_to_no_state_prefetch_.erase(
      page_->urls_to_no_state_prefetch_.begin());

  DoNoStatePrefetch();
}

void PrefetchProxyTabHelper::StartSpareRenderer() {
  page_->number_of_spare_renderers_started_++;
  content::RenderProcessHost::WarmupSpareRenderProcessHost(profile_);
}

void PrefetchProxyTabHelper::PrefetchSpeculationCandidates(
    const std::vector<std::pair<GURL, PrefetchType>>& prefetches,
    const GURL& source_document_url,
    base::WeakPtr<content::SpeculationHostDevToolsObserver> devtools_observer) {
  // Use navigation predictor by default.
  if (!PrefetchProxyUseSpeculationRules())
    return;

  // For IP-private prefetches, using the Google proxy needs to be restricted to
  // first party sites unless users opted-in to extended preloading.
  std::vector<std::pair<GURL, PrefetchType>> filtered_prefetches = prefetches;
  const bool allow_all_domains =
      PrefetchProxyAllowAllDomains() ||
      (PrefetchProxyAllowAllDomainsForExtendedPreloading() &&
       prefetch::GetPreloadPagesState(*profile_->GetPrefs()) ==
           prefetch::PreloadPagesState::kExtendedPreloading);
  if (!allow_all_domains &&
      !IsGoogleDomainUrl(source_document_url, google_util::ALLOW_SUBDOMAIN,
                         google_util::ALLOW_NON_STANDARD_PORTS) &&
      !IsYoutubeDomainUrl(source_document_url, google_util::ALLOW_SUBDOMAIN,
                          google_util::ALLOW_NON_STANDARD_PORTS)) {
    // Filter out prefetches that require the Google proxy.
    auto new_end =
        std::remove_if(filtered_prefetches.begin(), filtered_prefetches.end(),
                       [](const std::pair<GURL, PrefetchType>& prefetch) {
                         return prefetch.second.IsProxyRequired();
                       });
    filtered_prefetches.erase(new_end, filtered_prefetches.end());
  }

  PrefetchUrls(filtered_prefetches, std::move(devtools_observer));
}

void PrefetchProxyTabHelper::OnPredictionUpdated(
    const absl::optional<NavigationPredictorKeyedService::Prediction>
        prediction) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Use speculation rules API instead of navigation predictor.
  if (PrefetchProxyUseSpeculationRules())
    return;

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

  const absl::optional<GURL>& source_document_url =
      prediction->source_document_url();

  if (!source_document_url || source_document_url->is_empty())
    return;

  if (!google_util::IsGoogleSearchUrl(source_document_url.value())) {
    return;
  }

  // For the navigation predictor approach, we assume all predicted URLs are
  // eligible for NSP.
  std::vector<std::pair<GURL, PrefetchType>> prefetches;
  for (const auto& url : prediction.value().sorted_predicted_urls()) {
    prefetches.emplace_back(url,
                            PrefetchType(/*use_isolated_network_context=*/true,
                                         /*use_prefetch_proxy=*/true,
                                         /*can_prefetch_subresources=*/true));
  }
  // TODO: we need to pass devtools observer here
  PrefetchUrls(prefetches, nullptr);
}

void PrefetchProxyTabHelper::PrefetchUrls(
    const std::vector<std::pair<GURL, PrefetchType>>& prefetch_targets,
    base::WeakPtr<content::SpeculationHostDevToolsObserver> devtools_observer) {
  if (!PrefetchProxyIsEnabled()) {
    return;
  }

  if (!IsProfileEligible()) {
    return;
  }

  // This checks whether the user has disabled pre* actions in the settings UI.
  if (prefetch::IsSomePreloadingEnabled(*profile_->GetPrefs()) !=
      content::PreloadingEligibility::kEligible) {
    return;
  }

  if (!page_->prefetch_metrics_collector_) {
    page_->prefetch_metrics_collector_ =
        base::MakeRefCounted<PrefetchProxyPrefetchMetricsCollector>(
            page_->navigation_start_,
            web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId());
  }

  // Add new prefetches, and update the type for any existing prefetches.
  std::vector<std::pair<GURL, PrefetchType>> new_targets;
  for (const auto& prefetch_with_type : prefetch_targets) {
    auto prefetch_container_iter =
        page_->prefetch_containers_.find(prefetch_with_type.first);
    if (prefetch_container_iter == page_->prefetch_containers_.end()) {
      new_targets.push_back(prefetch_with_type);

      // It is possible, since it is not stipulated by the API contract, that
      // the navigation predictor will issue multiple predictions during a
      // single page load. Additional predictions should be treated as appending
      // to the ordering of previous predictions.
      auto prefetch_container = std::make_unique<PrefetchContainer>(
          prefetch_with_type.first, prefetch_with_type.second,
          page_->prefetch_containers_.size());
      prefetch_container->SetDevToolsObserver(devtools_observer);
      page_->prefetch_containers_[prefetch_with_type.first] =
          std::move(prefetch_container);
    } else {
      if (prefetch_with_type.second !=
          prefetch_container_iter->second->GetPrefetchType()) {
        prefetch_container_iter->second->ChangePrefetchType(
            prefetch_with_type.second);
      }
      prefetch_container_iter->second->SetDevToolsObserver(devtools_observer);
    }
  }

  // It's very likely we'll prefetch something at this point, so inform PLM to
  // start tracking metrics.
  InformPLMOfLikelyPrefetching(web_contents());

  page_->srp_metrics_->predicted_urls_count_ += new_targets.size();

  for (const auto& prefetch_with_type : new_targets) {
    CheckEligibilityOfURL(
        profile_, prefetch_with_type.first, prefetch_with_type.second,
        base::BindOnce(&PrefetchProxyTabHelper::OnGotEligibilityResult,
                       weak_factory_.GetWeakPtr()));
  }
}

// static
content::ServiceWorkerContext* PrefetchProxyTabHelper::GetServiceWorkerContext(
    Profile* profile) {
  if (g_service_worker_context_for_test)
    return g_service_worker_context_for_test;
  return profile->GetDefaultStoragePartition()->GetServiceWorkerContext();
}

// static
std::pair<bool, absl::optional<PrefetchProxyPrefetchStatus>>
PrefetchProxyTabHelper::CheckEligibilityOfURLSansUserData(
    Profile* profile,
    const GURL& url,
    const PrefetchType& prefetch_type) {
  if (!IsProfileEligible(profile)) {
    return std::make_pair(false, absl::nullopt);
  }

  if (data_saver::IsDataSaverEnabled(profile)) {
    return std::make_pair(
        false,
        PrefetchProxyPrefetchStatus::kPrefetchNotEligibleDataSaverEnabled);
  }

  if (!PrefetchProxyUseSpeculationRules() &&
      google_util::IsGoogleAssociatedDomainUrl(url)) {
    return std::make_pair(
        false, PrefetchProxyPrefetchStatus::kPrefetchNotEligibleGoogleDomain);
  }

  // While a registry-controlled domain could still resolve to a non-publicly
  // routable IP, this allows hosts which are very unlikely to work via the
  // proxy to be discarded immediately.
  if (!prefetch_type.IsProxyBypassedForTesting() &&
      prefetch_type.IsProxyRequired() &&
      (g_host_non_unique_filter
           ? g_host_non_unique_filter(url.HostNoBracketsPiece())
           : net::IsHostnameNonUnique(url.HostNoBrackets()))) {
    return std::make_pair(
        false,
        PrefetchProxyPrefetchStatus::kPrefetchNotEligibleHostIsNonUnique);
  }

  // Only HTTP(S) URLs which are believed to be secure are eligible.
  // For proxied prefetches, we only want HTTPS URLs.
  // For non-proxied prefetches, other URLs (notably localhost HTTP) is also
  // acceptable. This is common during development.
  const bool is_secure_http = prefetch_type.IsProxyRequired()
                                  ? url.SchemeIs(url::kHttpsScheme)
                                  : (url.SchemeIsHTTPOrHTTPS() &&
                                     network::IsUrlPotentiallyTrustworthy(url));
  if (!is_secure_http) {
    return std::make_pair(
        false,
        PrefetchProxyPrefetchStatus::kPrefetchNotEligibleSchemeIsNotHttps);
  }

  PrefetchProxyService* prefetch_proxy_service =
      PrefetchProxyServiceFactory::GetForProfile(profile);
  if (!prefetch_proxy_service) {
    return std::make_pair(false, absl::nullopt);
  }

  if (prefetch_type.IsProxyRequired() &&
      !prefetch_proxy_service->proxy_configurator()
           ->IsPrefetchProxyAvailable()) {
    return std::make_pair(
        false, PrefetchProxyPrefetchStatus::kPrefetchProxyNotAvailable);
  }

  return std::make_pair(true, absl::nullopt);
}

// static
void PrefetchProxyTabHelper::CheckEligibilityOfURL(
    Profile* profile,
    const GURL& url,
    const PrefetchType& prefetch_type,
    OnEligibilityResultCallback result_callback) {
  auto no_user_data_check =
      CheckEligibilityOfURLSansUserData(profile, url, prefetch_type);
  if (!no_user_data_check.first) {
    std::move(result_callback).Run(url, false, no_user_data_check.second);
    return;
  }

  content::StoragePartition* default_storage_partition =
      profile->GetDefaultStoragePartition();

  // Only the default storage partition is supported since that is the only
  // place where service workers are observed by
  // |PrefetchProxyServiceWorkersObserver|.
  if (default_storage_partition !=
      profile->GetStoragePartitionForUrl(url,
                                         /*can_create=*/false)) {
    std::move(result_callback)
        .Run(url, false,
             PrefetchProxyPrefetchStatus::
                 kPrefetchNotEligibleNonDefaultStoragePartition);
    return;
  }

  PrefetchProxyService* prefetch_proxy_service =
      PrefetchProxyServiceFactory::GetForProfile(profile);
  if (!prefetch_proxy_service) {
    std::move(result_callback).Run(url, false, absl::nullopt);
    return;
  }

  if (!prefetch_proxy_service->origin_decider()
           ->IsOriginOutsideRetryAfterWindow(url)) {
    std::move(result_callback)
        .Run(url, false,
             PrefetchProxyPrefetchStatus::kPrefetchIneligibleRetryAfter);
    return;
  }

  content::ServiceWorkerContext* service_worker_context_ =
      GetServiceWorkerContext(profile);

  // This service worker check assumes that the prefetch will only ever be
  // performed in a first-party context (main frame prefetch). At the moment
  // that is true but if it ever changes then the StorageKey will need to be
  // constructed with the top-level site to ensure correct partitioning.
  bool site_has_service_worker =
      service_worker_context_->MaybeHasRegistrationForStorageKey(
          blink::StorageKey(url::Origin::Create(url)));
  if (site_has_service_worker) {
    std::move(result_callback)
        .Run(url, false,
             PrefetchProxyPrefetchStatus::
                 kPrefetchNotEligibleUserHasServiceWorker);
    return;
  }

  // We don't have to check the cookies for prefetches that use the default
  // network context instead of an isolated network context.
  if (!prefetch_type.IsIsolatedNetworkContextRequired()) {
    std::move(result_callback).Run(url, true, absl::nullopt);
    return;
  }

  net::CookieOptions options = net::CookieOptions::MakeAllInclusive();
  options.set_return_excluded_cookies();
  default_storage_partition->GetCookieManagerForBrowserProcess()->GetCookieList(
      url, options, net::CookiePartitionKeyCollection::Todo(),
      base::BindOnce(&OnGotCookieList, url, std::move(result_callback)));
}

void PrefetchProxyTabHelper::OnGotEligibilityResult(
    const GURL& url,
    bool eligible,
    absl::optional<PrefetchProxyPrefetchStatus> status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // It is possible that this callback is being run late. That is, after the
  // user has navigated away from the origin SRP. To detect this, check if the
  // url exists in the set of predicted urls. If it doesn't, do nothing.
  auto prefetch_container_iter = page_->prefetch_containers_.find(url);
  if (prefetch_container_iter == page_->prefetch_containers_.end()) {
    return;
  }
  PrefetchContainer* prefetch_container = prefetch_container_iter->second.get();

  if (!eligible) {
    if (status) {
      prefetch_container->SetPrefetchStatus(*status);
      if (page_->prefetch_metrics_collector_) {
        page_->prefetch_metrics_collector_->OnMainframeResourceNotEligible(
            url, prefetch_container->GetOriginalPredictionIndex(), *status);
      }

      // Consider whether to send a decoy request to mask any user state (i.e.:
      // cookies), and if so randomly decide whether to send a decoy request.
      if (prefetch_container->GetPrefetchType().IsProxyRequired() &&
          ShouldConsiderDecoyRequestForStatus(*status) &&
          PrefetchProxySendDecoyRequestForIneligiblePrefetch(
              profile_->GetPrefs())) {
        prefetch_container->SetIsDecoy(true);
        page_->urls_to_prefetch_.push_back(prefetch_container);
        prefetch_container->SetPrefetchStatus(
            PrefetchProxyPrefetchStatus::kPrefetchIsPrivacyDecoy);
        Prefetch();
      }
    }

    return;
  }

  // TODO(robertogden): Consider adding redirect URLs to the front of the list.
  page_->urls_to_prefetch_.push_back(prefetch_container);
  page_->srp_metrics_->prefetch_eligible_count_++;
  prefetch_container->SetPrefetchStatus(
      PrefetchProxyPrefetchStatus::kPrefetchNotStarted);

  if (!PrefetchProxyShouldPrefetchPosition(
          prefetch_container->GetOriginalPredictionIndex())) {
    prefetch_container->SetPrefetchStatus(
        PrefetchProxyPrefetchStatus::kPrefetchPositionIneligible);
    return;
  }

  Prefetch();

  // Registers a cookie listener for this prefetch if it is using an isolated
  // network context. If the cookies in the default partition associated with
  // this URL change after this point, then the prefetched resources should not
  // be served.
  if (prefetch_container->GetPrefetchType()
          .IsIsolatedNetworkContextRequired()) {
    prefetch_container->RegisterCookieListener(
        base::BindOnce(
            &PrefetchProxyTabHelper::OnCookiesChangedAfterInitialCheck,
            weak_factory_.GetWeakPtr()),
        profile_->GetDefaultStoragePartition()
            ->GetCookieManagerForBrowserProcess());
  }

  for (auto& observer : observer_list_) {
    observer.OnNewEligiblePrefetchStarted();
  }
}

bool PrefetchProxyTabHelper::IsWaitingForAfterSRPCookiesCopy() const {
  switch (page_->cookie_copy_status_) {
    case CookieCopyStatus::kNoNavigation:
    case CookieCopyStatus::kCopyComplete:
      return false;
    case CookieCopyStatus::kWaitingForCopy:
      return true;
  }
}

void PrefetchProxyTabHelper::SetOnAfterSRPCookieCopyCompleteCallback(
    base::OnceClosure callback) {
  // We don't expect a callback unless there's something to wait on.
  DCHECK(IsWaitingForAfterSRPCookiesCopy());

  page_->on_after_srp_cookie_copy_complete_ = std::move(callback);
}

void PrefetchProxyTabHelper::CopyIsolatedCookiesOnAfterSRPClick(
    PrefetchContainer* prefetch_container) {
  DCHECK(prefetch_container);

  if (!page_->GetNetworkContextForUrl(prefetch_container->GetUrl())) {
    // Not set in unit tests.
    return;
  }

  // We only need to copy cookies if the prefetch used an isolated network
  // context.
  if (!prefetch_container->GetPrefetchType()
           .IsIsolatedNetworkContextRequired()) {
    RecordPrefetchProxyPrefetchMainframeCookiesToCopy(0U);
    return;
  }

  // We don't want the cookie listener for this URL to get the changes from the
  // copy.
  prefetch_container->StopCookieListener();

  page_->cookie_copy_status_ = CookieCopyStatus::kWaitingForCopy;
  page_->cookie_copy_start_time_ = base::TimeTicks::Now();

  net::CookieOptions options = net::CookieOptions::MakeAllInclusive();
  page_->GetNetworkContextForUrl(prefetch_container->GetUrl())
      ->GetCookieManager()
      ->GetCookieList(
          prefetch_container->GetUrl(), options,
          net::CookiePartitionKeyCollection::Todo(),
          base::BindOnce(
              &PrefetchProxyTabHelper::OnGotIsolatedCookiesToCopyAfterSRPClick,
              weak_factory_.GetWeakPtr(), prefetch_container->GetUrl()));
}

void PrefetchProxyTabHelper::OnGotIsolatedCookiesToCopyAfterSRPClick(
    const GURL& url,
    const net::CookieAccessResultList& cookie_list,
    const net::CookieAccessResultList& excluded_cookies) {
  DCHECK(IsWaitingForAfterSRPCookiesCopy());
  DCHECK(page_->prefetch_containers_.find(url) !=
         page_->prefetch_containers_.end());

  RecordPrefetchProxyPrefetchMainframeCookiesToCopy(cookie_list.size());

  if (cookie_list.empty()) {
    OnCopiedIsolatedCookiesAfterSRPClick();
    return;
  }

  // When |barrier| is run |cookie_list.size()| times, it will run
  // |OnCopiedIsolatedCookiesAfterSRPClick|.
  base::RepeatingClosure barrier = base::BarrierClosure(
      cookie_list.size(),
      base::BindOnce(
          &PrefetchProxyTabHelper::OnCopiedIsolatedCookiesAfterSRPClick,
          weak_factory_.GetWeakPtr()));

  content::StoragePartition* default_storage_partition =
      profile_->GetDefaultStoragePartition();
  net::CookieOptions options = net::CookieOptions::MakeAllInclusive();

  for (const net::CookieWithAccessResult& cookie : cookie_list) {
    default_storage_partition->GetCookieManagerForBrowserProcess()
        ->SetCanonicalCookie(cookie.cookie, url, options,
                             base::BindOnce(&CookieSetHelper, barrier));
  }
}

void PrefetchProxyTabHelper::OnCopiedIsolatedCookiesAfterSRPClick() {
  DCHECK(IsWaitingForAfterSRPCookiesCopy());

  if (page_->cookie_copy_start_time_) {
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "PrefetchProxy.AfterClick.Mainframe.CookieCopyTime",
        base::TimeTicks::Now() - page_->cookie_copy_start_time_.value(),
        base::TimeDelta(), base::Seconds(5), 50);
  }

  page_->cookie_copy_status_ = CookieCopyStatus::kCopyComplete;
  if (page_->on_after_srp_cookie_copy_complete_) {
    std::move(page_->on_after_srp_cookie_copy_complete_).Run();
  }
}

void PrefetchProxyTabHelper::OnInterceptorCheckCookieCopy() {
  if (!page_->cookie_copy_start_time_)
    return;

  UMA_HISTOGRAM_CUSTOM_TIMES(
      "PrefetchProxy.AfterClick.Mainframe.CookieCopyStartToInterceptorCheck",
      base::TimeTicks::Now() - page_->cookie_copy_start_time_.value(),
      base::TimeDelta(), base::Seconds(5), 50);
}

network::mojom::URLLoaderFactory* PrefetchProxyTabHelper::GetURLLoaderFactory(
    const GURL& url) {
  if (!page_->GetNetworkContextForUrl(url))
    page_->CreateNetworkContextForUrl(url);
  DCHECK(page_->GetNetworkContextForUrl(url));
  return page_->GetNetworkContextForUrl(url)->GetUrlLoaderFactory();
}

bool PrefetchProxyTabHelper::HaveCookiesChanged(const GURL& url) const {
  auto prefetch_container_iter = page_->prefetch_containers_.find(url);
  if (prefetch_container_iter == page_->prefetch_containers_.end())
    return false;
  return prefetch_container_iter->second->HaveCookiesChanged();
}

void PrefetchProxyTabHelper::OnCookiesChangedAfterInitialCheck(
    const GURL& url) {
  for (auto& observer : observer_list_) {
    observer.OnCookiesChangedForPrefetchAfterInitialCheck(url);
  }
}

void PrefetchProxyTabHelper::CurrentPageLoad::CreateNetworkContextForUrl(
    const GURL& url) {
  if (PrefetchProxyUseIndividualNetworkContextsForEachPrefetch()) {
    auto prefetch_container_iter = prefetch_containers_.find(url);
    if (prefetch_container_iter != prefetch_containers_.end())
      prefetch_container_iter->second->CreateNetworkContextForPrefetch(
          profile_);
    return;
  }
  network_context_ = std::make_unique<PrefetchProxyNetworkContext>(
      profile_, /*is_isolated=*/true, /*use_proxy=*/true);
}

PrefetchProxyNetworkContext*
PrefetchProxyTabHelper::CurrentPageLoad::GetNetworkContextForUrl(
    const GURL& url) const {
  if (PrefetchProxyUseIndividualNetworkContextsForEachPrefetch()) {
    auto prefetch_container_iter = prefetch_containers_.find(url);
    if (prefetch_container_iter == prefetch_containers_.end())
      return nullptr;
    return prefetch_container_iter->second->GetNetworkContext();
  }
  return network_context_.get();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PrefetchProxyTabHelper);
