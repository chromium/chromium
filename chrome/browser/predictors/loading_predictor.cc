// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/loading_predictor.h"

#include <algorithm>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor_util.h"
#include "chrome/browser/predictors/lcp_critical_path_predictor/prewarm_http_disk_cache_manager.h"
#include "chrome/browser/predictors/loading_data_collector.h"
#include "chrome/browser/predictors/loading_stats_collector.h"
#include "chrome/browser/predictors/predictors_features.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/origin_util.h"
#include "net/base/network_anonymization_key.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "services/network/public/cpp/request_destination.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/radio_utils.h"
#include "base/power_monitor/power_monitor.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace features {

// Don't preconnect on weak signal to save power.
BASE_FEATURE(kNoPreconnectToSearchOnWeakSignal,
             "NoPreconnectToSearchOnWeakSignal",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kNoNavigationPreconnectOnWeakSignal,
             "NoNavigationPreconnectOnWeakSignal",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, suppresses LoadingPredictor (https://crbug.com/350519234)
BASE_FEATURE(kSuppressesLoadingPredictorOnSlowNetwork,
             "SuppressesLoadingPredictorOnSlowNetwork",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta>
    kSuppressesLoadingPredictorOnSlowNetworkThreshold{
        &kSuppressesLoadingPredictorOnSlowNetwork, "slow_network_threshold",
        base::Milliseconds(208)};

}  // namespace features

namespace predictors {

namespace {

const base::TimeDelta kMinDelayBetweenPreresolveRequests = base::Seconds(60);
const base::TimeDelta kMinDelayBetweenPreconnectRequests = base::Seconds(10);

// Returns true iff |prediction| is not empty.
bool AddInitialUrlToPreconnectPrediction(const GURL& initial_url,
                                         PreconnectPrediction* prediction) {
  url::Origin initial_origin = url::Origin::Create(initial_url);
  // Open minimum 2 sockets to the main frame host to speed up the loading if a
  // main page has a redirect to the same host. This is because there can be a
  // race between reading the server redirect response and sending a new request
  // while the connection is still in use.
  static const int kMinSockets = 2;

  if (!prediction->requests.empty() &&
      prediction->requests.front().origin == initial_origin) {
    prediction->requests.front().num_sockets =
        std::max(prediction->requests.front().num_sockets, kMinSockets);
  } else if (!initial_origin.opaque() &&
             (initial_origin.scheme() == url::kHttpScheme ||
              initial_origin.scheme() == url::kHttpsScheme)) {
    prediction->requests.emplace(prediction->requests.begin(), initial_origin,
                                 kMinSockets,
                                 net::NetworkAnonymizationKey::CreateSameSite(
                                     net::SchemefulSite(initial_origin)));
  }

  return !prediction->requests.empty();
}

bool IsPreconnectExpensive() {
#if BUILDFLAG(IS_ANDROID)
  // Preconnecting is expensive while on battery power and cellular data and
  // the radio signal is weak.
  if (auto* power_monitor = base::PowerMonitor::GetInstance();
      (power_monitor->IsInitialized() && !power_monitor->IsOnBatteryPower()) ||
      (base::android::RadioUtils::GetConnectionType() !=
       base::android::RadioConnectionType::kCell)) {
    return false;
  }

  std::optional<base::android::RadioSignalLevel> maybe_level =
      base::android::RadioUtils::GetCellSignalLevel();
  return maybe_level.has_value() &&
         *maybe_level <= base::android::RadioSignalLevel::kModerate;
#else
  return false;
#endif
}

void MaybeWarmUpServiceWorker(const GURL& url, Profile* profile) {
  static const bool kEnabled =
      base::FeatureList::IsEnabled(
          blink::features::kSpeculativeServiceWorkerWarmUp) &&
      base::GetFieldTrialParamByFeatureAsBool(
          blink::features::kSpeculativeServiceWorkerWarmUp,
          "sw_warm_up_from_loading_predictor", true);
  if (!kEnabled) {
    return;
  }

  if (!profile) {
    return;
  }

  content::StoragePartition* storage_partition =
      profile->GetDefaultStoragePartition();

  if (!storage_partition) {
    return;
  }

  content::ServiceWorkerContext* service_worker_context =
      storage_partition->GetServiceWorkerContext();

  if (!service_worker_context) {
    return;
  }

  if (!content::OriginCanAccessServiceWorkers(url)) {
    return;
  }

  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(url));

  if (!service_worker_context->MaybeHasRegistrationForStorageKey(key)) {
    return;
  }

  service_worker_context->WarmUpServiceWorker(url, key, base::DoNothing());
}

}  // namespace

LoadingPredictor::LoadingPredictor(const LoadingPredictorConfig& config,
                                   Profile* profile)
    : config_(config),
      profile_(profile),
      resource_prefetch_predictor_(
          std::make_unique<ResourcePrefetchPredictor>(config, profile)),
      stats_collector_(std::make_unique<LoadingStatsCollector>(
          resource_prefetch_predictor_.get(),
          config)),
      loading_data_collector_(std::make_unique<LoadingDataCollector>(
          resource_prefetch_predictor_.get(),
          stats_collector_.get(),
          config)) {}

LoadingPredictor::~LoadingPredictor() {
  DCHECK(shutdown_);
}

bool LoadingPredictor::PrepareForPageLoad(
    const std::optional<url::Origin>& initiator_origin,
    const GURL& url,
    HintOrigin origin,
    bool preconnectable,
    std::optional<PreconnectPrediction> preconnect_prediction) {
  if (shutdown_)
    return true;

  // Suppresses network activities.
  static const bool kSuppressesLoadingPredictorOnSlowNetworkIsEnabled =
      base::FeatureList::IsEnabled(
          features::kSuppressesLoadingPredictorOnSlowNetwork);
  static const base::TimeDelta kSlowNetworkThreshold =
      features::kSuppressesLoadingPredictorOnSlowNetworkThreshold.Get();
  if (kSuppressesLoadingPredictorOnSlowNetworkIsEnabled && g_browser_process &&
      g_browser_process->network_quality_tracker()) {
    const bool is_slow_network =
        g_browser_process->network_quality_tracker()->GetHttpRTT() >
        kSlowNetworkThreshold;
    base::UmaHistogramBoolean("LoadingPredictor.IsSlowNetwork",
                              is_slow_network);
    if (is_slow_network) {
      return true;
    }
  }

  // Prewarm disk cache before preconnecting network.
  MaybePrewarmResources(initiator_origin, url);

  MaybeWarmUpServiceWorker(url, profile_);

  if (origin == HintOrigin::OMNIBOX) {
    // Omnibox hints are lightweight and need a special treatment.
    HandleHintByOrigin(url, preconnectable, /*only_allow_https=*/false,
                       omnibox_preconnect_data_);
    return true;
  }

  if (origin == HintOrigin::BOOKMARK_BAR) {
    // Bookmark hints are lightweight and need a special treatment.
    HandleHintByOrigin(url, /*preconnectable=*/true, /*only_allow_https=*/true,
                       bookmark_bar_preconnect_data_);
    return true;
  }

  if (origin == HintOrigin::NEW_TAB_PAGE) {
    // New Tab Page hints are lightweight and need a special treatment.
    HandleHintByOrigin(url, /*preconnectable=*/true, /*only_allow_https=*/true,
                       new_tab_page_preconnect_data_);
    return true;
  }

  PreconnectPrediction prediction;
  bool has_local_preconnect_prediction = false;
  if (origin == HintOrigin::OPTIMIZATION_GUIDE) {
    CHECK(preconnect_prediction);
    prediction = *preconnect_prediction;
  } else {
    CHECK(!preconnect_prediction);
    if (features::ShouldUseLocalPredictions()) {
      has_local_preconnect_prediction =
          resource_prefetch_predictor_->PredictPreconnectOrigins(url,
                                                                 &prediction);
    }
    if (active_hints_.find(url) != active_hints_.end() &&
        has_local_preconnect_prediction) {
      // We are currently preconnecting using the local preconnect prediction.
      // Do not proceed further.
      return true;
    }
    // Try to preconnect to the |url| even if the predictor has no
    // prediction.
    AddInitialUrlToPreconnectPrediction(url, &prediction);
  }

  // LCPP: AutoPreconnectLCPOrigins experiment (crbug.com/1518996)
  // Preconnect to LCPP predicted LCP origins in all platforms including those
  // without optimization guide.
  if (base::FeatureList::IsEnabled(
          blink::features::kLCPPAutoPreconnectLcpOrigin)) {
    std::optional<LcppStat> lcpp_stat =
        resource_prefetch_predictor()->GetLcppStat(initiator_origin, url);
    if (lcpp_stat) {
      size_t count = 0;
      std::vector<PreconnectRequest> additional_preconnects;
      auto anonymization_key =
          net::NetworkAnonymizationKey::CreateSameSite(net::SchemefulSite(url));
      for (const GURL& preconnect_origin :
           PredictPreconnectableOrigins(*lcpp_stat)) {
        additional_preconnects.emplace_back(
            url::Origin::Create(preconnect_origin), 1, anonymization_key);
        ++count;
      }

      if (count) {
        // The first preconnect record is usually to the url origin itself.
        // We want to prioritize LCP preconnects just after the page origin
        // preconnect, to minimize any performance regression. If no new
        // requests were identified, leave the existing set as-is.
        if (prediction.requests.empty()) {
          prediction.requests = std::move(additional_preconnects);
        } else {
          prediction.requests.reserve(count + prediction.requests.size());
          prediction.requests.insert(++prediction.requests.begin(),
                                     additional_preconnects.begin(),
                                     additional_preconnects.end());
        }
      }
      base::UmaHistogramCounts10000("Blink.LCPP.PreconnectPredictionCount",
                                    count);
    }
  }

  // LCPP: set fonts to be prefetched to prefetch_requests.
  // TODO(crbug.com/40285959): make prefetch work for platforms without the
  // optimization guide.
  static const bool kLCPPFontURLPredictorEnabled =
      base::FeatureList::IsEnabled(blink::features::kLCPPFontURLPredictor) &&
      blink::features::kLCPPFontURLPredictorEnablePrefetch.Get();
  static const bool kLoadingPredictorPrefetchEnabled =
      base::FeatureList::IsEnabled(features::kLoadingPredictorPrefetch) &&
      features::kLoadingPredictorPrefetchSubresourceType.Get() ==
          features::PrefetchSubresourceType::kAll;
  if (kLCPPFontURLPredictorEnabled && kLoadingPredictorPrefetchEnabled) {
    std::optional<LcppStat> lcpp_stat =
        resource_prefetch_predictor()->GetLcppStat(initiator_origin, url);
    if (lcpp_stat) {
      auto network_anonymization_key =
          net::NetworkAnonymizationKey::CreateSameSite(
              net::SchemefulSite(url::Origin::Create(url)));
      size_t count = 0;
      for (const GURL& font_url : PredictFetchedFontUrls(*lcpp_stat)) {
        prediction.prefetch_requests.emplace_back(
            font_url, network_anonymization_key,
            network::mojom::RequestDestination::kFont);
        ++count;
      }
      base::UmaHistogramCounts1000("Blink.LCPP.PrefetchFontCount", count);
    }
  }

  // Return early if we do not have any requests.
  if (prediction.requests.empty() && prediction.prefetch_requests.empty())
    return false;

  ++total_hints_activated_;
  active_hints_.emplace(url, base::TimeTicks::Now());
  if (IsPreconnectAllowed(profile_))
    MaybeAddPreconnect(url, std::move(prediction));
  return has_local_preconnect_prediction || preconnect_prediction;
}

void LoadingPredictor::CancelPageLoadHint(const GURL& url) {
  if (shutdown_)
    return;

  CancelActiveHint(active_hints_.find(url));
}

void LoadingPredictor::StartInitialization() {
  if (shutdown_)
    return;

  resource_prefetch_predictor_->StartInitialization();
}

LoadingDataCollector* LoadingPredictor::loading_data_collector() {
  return loading_data_collector_.get();
}

ResourcePrefetchPredictor* LoadingPredictor::resource_prefetch_predictor() {
  return resource_prefetch_predictor_.get();
}

PreconnectManager* LoadingPredictor::preconnect_manager() {
  CHECK(!shutdown_);
  if (!preconnect_manager_) {
    preconnect_manager_ =
        std::make_unique<PreconnectManager>(GetWeakPtr(), profile_);
  }

  return preconnect_manager_.get();
}

PrefetchManager* LoadingPredictor::prefetch_manager() {
  CHECK(base::FeatureList::IsEnabled(features::kLoadingPredictorPrefetch));
  CHECK(!shutdown_);

  if (!prefetch_manager_) {
    prefetch_manager_ =
        std::make_unique<PrefetchManager>(GetWeakPtr(), profile_);
  }

  return prefetch_manager_.get();
}

void LoadingPredictor::Shutdown() {
  DCHECK(!shutdown_);
  resource_prefetch_predictor_->Shutdown();
  preconnect_manager_.reset();
  shutdown_ = true;
}

bool LoadingPredictor::OnNavigationStarted(
    NavigationId navigation_id,
    ukm::SourceId ukm_source_id,
    const std::optional<url::Origin>& initiator_origin,
    const GURL& main_frame_url,
    base::TimeTicks creation_time) {
  if (shutdown_)
    return true;

  loading_data_collector()->RecordStartNavigation(
      navigation_id, ukm_source_id, main_frame_url, creation_time);
  CleanupAbandonedHintsAndNavigations(navigation_id);
  active_navigations_.emplace(navigation_id,
                              NavigationInfo{main_frame_url, creation_time});
  active_urls_to_navigations_[main_frame_url].insert(navigation_id);
  return PrepareForPageLoad(initiator_origin, main_frame_url,
                            HintOrigin::NAVIGATION);
}

void LoadingPredictor::OnNavigationFinished(NavigationId navigation_id,
                                            const GURL& old_main_frame_url,
                                            const GURL& new_main_frame_url,
                                            bool is_error_page) {
  if (shutdown_)
    return;

  loading_data_collector()->RecordFinishNavigation(
      navigation_id, new_main_frame_url, is_error_page);
  if (active_urls_to_navigations_.find(old_main_frame_url) !=
      active_urls_to_navigations_.end()) {
    active_urls_to_navigations_[old_main_frame_url].erase(navigation_id);
    if (active_urls_to_navigations_[old_main_frame_url].empty()) {
      active_urls_to_navigations_.erase(old_main_frame_url);
    }
  }
  active_navigations_.erase(navigation_id);
  CancelPageLoadHint(old_main_frame_url);
}

std::map<GURL, base::TimeTicks>::iterator LoadingPredictor::CancelActiveHint(
    std::map<GURL, base::TimeTicks>::iterator hint_it) {
  if (hint_it == active_hints_.end())
    return hint_it;

  const GURL& url = hint_it->first;
  MaybeRemovePreconnect(url);
  return active_hints_.erase(hint_it);
}

void LoadingPredictor::CleanupAbandonedHintsAndNavigations(
    NavigationId navigation_id) {
  base::TimeTicks time_now = base::TimeTicks::Now();
  const base::TimeDelta max_navigation_age =
      base::Seconds(config_.max_navigation_lifetime_seconds);

  // Hints.
  for (auto it = active_hints_.begin(); it != active_hints_.end();) {
    base::TimeDelta prefetch_age = time_now - it->second;
    if (prefetch_age > max_navigation_age) {
      // Will go to the last bucket in the duration reported in
      // CancelActiveHint() meaning that the duration was unlimited.
      it = CancelActiveHint(it);
    } else {
      ++it;
    }
  }

  // Navigations.
  for (auto it = active_navigations_.begin();
       it != active_navigations_.end();) {
    if ((it->first == navigation_id) ||
        (time_now - it->second.creation_time > max_navigation_age)) {
      CancelActiveHint(active_hints_.find(it->second.main_frame_url));
      it = active_navigations_.erase(it);
    } else {
      ++it;
    }
  }
}

void LoadingPredictor::MaybeAddPreconnect(const GURL& url,
                                          PreconnectPrediction prediction) {
  CHECK(!shutdown_);
  if (!prediction.prefetch_requests.empty() &&
      (AfterStartupTaskUtils::IsBrowserStartupComplete() ||
       !base::FeatureList::IsEnabled(
           features::kAvoidLoadingPredictorPrefetchDuringBrowserStartup))) {
    CHECK(base::FeatureList::IsEnabled(features::kLoadingPredictorPrefetch));
    prefetch_manager()->Start(url, std::move(prediction.prefetch_requests));
  }

  if (base::FeatureList::IsEnabled(
          features::kNoNavigationPreconnectOnWeakSignal) &&
      IsPreconnectExpensive()) {
    return;
  }

  if (!prediction.requests.empty())
    preconnect_manager()->Start(url, std::move(prediction.requests));
}

void LoadingPredictor::MaybeRemovePreconnect(const GURL& url) {
  DCHECK(!shutdown_);
  if (preconnect_manager_)
    preconnect_manager_->Stop(url);
  if (prefetch_manager_)
    prefetch_manager_->Stop(url);
}

bool LoadingPredictor::HandleHintByOrigin(const GURL& url,
                                          bool preconnectable,
                                          bool only_allow_https,
                                          PreconnectData& preconnect_data) {
  if (!url.is_valid() || !url.has_host() || !IsPreconnectAllowed(profile_) ||
      (only_allow_https && url.scheme() != url::kHttpsScheme)) {
    return false;
  }

  const url::Origin origin = url::Origin::Create(url);
  // When constructing an Origin from a GURL results in an opaque origin, the
  // resulting origin is guaranteed to be unique; trying to create another
  // origin from the same URL will result in a different unique opaque origin,
  // so any preconnect attempt would never be used anyway.
  if (origin.opaque()) {
    return false;
  }

  // Tracking whether this is a new origin request. If so, then
  // preconnect/presolve immediately. If the origins are the same, then
  // preconnect/presolve after a given threshold.
  const bool is_new_origin = origin != preconnect_data.last_origin_;
  preconnect_data.last_origin_ = origin;
  const net::SchemefulSite site = net::SchemefulSite(origin);
  const auto network_anonymization_key =
      net::NetworkAnonymizationKey::CreateSameSite(site);
  base::TimeTicks now = base::TimeTicks::Now();
  if (preconnectable) {
    if (is_new_origin || now - preconnect_data.last_preconnect_time_ >=
                             kMinDelayBetweenPreconnectRequests) {
      preconnect_data.last_preconnect_time_ = now;
      preconnect_manager()->StartPreconnectUrl(url, true,
                                               network_anonymization_key);
    }
    return true;
  }

  if (is_new_origin || now - preconnect_data.last_preresolve_time_ >=
                           kMinDelayBetweenPreresolveRequests) {
    preconnect_data.last_preresolve_time_ = now;
    preconnect_manager()->StartPreresolveHost(url, network_anonymization_key);
    return true;
  }

  return false;
}

void LoadingPredictor::PreconnectInitiated(const GURL& url,
                                           const GURL& preconnect_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (shutdown_)
    return;

  auto nav_id_set_it = active_urls_to_navigations_.find(url);
  if (nav_id_set_it == active_urls_to_navigations_.end())
    return;

  for (const auto& nav_id : nav_id_set_it->second)
    loading_data_collector_->RecordPreconnectInitiated(nav_id, preconnect_url);
}

void LoadingPredictor::PreconnectFinished(
    std::unique_ptr<PreconnectStats> stats) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (shutdown_)
    return;

  DCHECK(stats);
  active_hints_.erase(stats->url);
  stats_collector_->RecordPreconnectStats(std::move(stats));
}

void LoadingPredictor::PrefetchInitiated(const GURL& url,
                                         const GURL& prefetch_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (shutdown_)
    return;

  auto nav_id_set_it = active_urls_to_navigations_.find(url);
  if (nav_id_set_it == active_urls_to_navigations_.end())
    return;

  for (const auto& nav_id : nav_id_set_it->second)
    loading_data_collector_->RecordPrefetchInitiated(nav_id, prefetch_url);
}

void LoadingPredictor::PrefetchFinished(std::unique_ptr<PrefetchStats> stats) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (shutdown_)
    return;

  active_hints_.erase(stats->url);
}

void LoadingPredictor::PreconnectURLIfAllowed(
    const GURL& url,
    bool allow_credentials,
    const net::NetworkAnonymizationKey& network_anonymization_key) {
  if (!url.is_valid() || !url.has_host() || !IsPreconnectAllowed(profile_))
    return;

  if (base::FeatureList::IsEnabled(
          features::kNoPreconnectToSearchOnWeakSignal) &&
      IsPreconnectExpensive()) {
    return;
  }

  preconnect_manager()->StartPreconnectUrl(url, allow_credentials,
                                           network_anonymization_key);
}

void LoadingPredictor::MaybePrewarmResources(
    const std::optional<url::Origin>& initiator_origin,
    const GURL& top_frame_main_resource_url) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kHttpDiskCachePrewarming)) {
    return;
  }

  if (shutdown_) {
    return;
  }

  if (!top_frame_main_resource_url.is_valid() ||
      !top_frame_main_resource_url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  std::optional<LcppStat> lcpp_stat =
      resource_prefetch_predictor()->GetLcppStat(initiator_origin,
                                                 top_frame_main_resource_url);

  if (!lcpp_stat || !IsValidLcppStat(*lcpp_stat)) {
    return;
  }

  if (!prewarm_http_disk_cache_manager_) {
    prewarm_http_disk_cache_manager_ =
        std::make_unique<PrewarmHttpDiskCacheManager>(
            profile_->GetDefaultStoragePartition()
                ->GetURLLoaderFactoryForBrowserProcess());
  }

  prewarm_http_disk_cache_manager_->MaybePrewarmResources(
      top_frame_main_resource_url, PredictFetchedSubresourceUrls(*lcpp_stat));
}

}  // namespace predictors
