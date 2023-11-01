// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/loading_predictor.h"

#include <algorithm>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor_util.h"
#include "chrome/browser/predictors/loading_data_collector.h"
#include "chrome/browser/predictors/loading_stats_collector.h"
#include "chrome/browser/predictors/predictors_features.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/network_anonymization_key.h"
#include "services/network/public/cpp/request_destination.h"
#include "third_party/blink/public/common/features.h"
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
  if ((base::PowerMonitor::IsInitialized() &&
       !base::PowerMonitor::IsOnBatteryPower()) ||
      (base::android::RadioUtils::GetConnectionType() !=
       base::android::RadioConnectionType::kCell)) {
    return false;
  }

  absl::optional<base::android::RadioSignalLevel> maybe_level =
      base::android::RadioUtils::GetCellSignalLevel();
  return maybe_level.has_value() &&
         *maybe_level <= base::android::RadioSignalLevel::kModerate;
#else
  return false;
#endif
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
    const GURL& url,
    HintOrigin origin,
    bool preconnectable,
    absl::optional<PreconnectPrediction> preconnect_prediction) {
  if (shutdown_)
    return true;

  if (origin == HintOrigin::OMNIBOX) {
    // Omnibox hints are lightweight and need a special treatment.
    HandleOmniboxHint(url, preconnectable);
    return true;
  }

  if (origin == HintOrigin::BOOKMARK_BAR) {
    // Bookmark hints are lightweight and need a special treatment.
    HandleBookmarkBarHint(url, preconnectable);
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

  // LCPP: set fonts to be prefetched to prefetch_requests.
  // TODO(crbug.com/1493768): make prefetch work for platforms without the
  // optimization guide.
  if (base::FeatureList::IsEnabled(blink::features::kLCPPFontURLPredictor) &&
      base::FeatureList::IsEnabled(features::kLoadingPredictorPrefetch) &&
      features::kLoadingPredictorPrefetchSubresourceType.Get() ==
          features::PrefetchSubresourceType::kAll) {
    absl::optional<LcppData> lcpp_data =
        resource_prefetch_predictor()->GetLcppData(url);
    if (lcpp_data) {
      auto network_anonymization_key =
          net::NetworkAnonymizationKey::CreateSameSite(
              net::SchemefulSite(url::Origin::Create(url)));
      size_t count = 0;
      for (const GURL& font_url : PredictFetchedFontUrls(*lcpp_data)) {
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
  if (shutdown_) {
    return nullptr;
  }

  if (!preconnect_manager_) {
    preconnect_manager_ =
        std::make_unique<PreconnectManager>(GetWeakPtr(), profile_);
  }

  return preconnect_manager_.get();
}

PrefetchManager* LoadingPredictor::prefetch_manager() {
  if (!base::FeatureList::IsEnabled(features::kLoadingPredictorPrefetch))
    return nullptr;

  if (shutdown_) {
    return nullptr;
  }

  if (!prefetch_manager_) {
    prefetch_manager_ =
        std::make_unique<PrefetchManager>(GetWeakPtr(), profile_);
  }

  return prefetch_manager_.get();
}

void LoadingPredictor::Shutdown() {
  DCHECK(!shutdown_);
  resource_prefetch_predictor_->Shutdown();
  shutdown_ = true;
}

bool LoadingPredictor::OnNavigationStarted(NavigationId navigation_id,
                                           ukm::SourceId ukm_source_id,
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
  return PrepareForPageLoad(main_frame_url, HintOrigin::NAVIGATION);
}

void LoadingPredictor::OnNavigationFinished(NavigationId navigation_id,
                                            const GURL& old_main_frame_url,
                                            const GURL& new_main_frame_url,
                                            bool is_error_page) {
  if (shutdown_)
    return;

  loading_data_collector()->RecordFinishNavigation(
      navigation_id, old_main_frame_url, new_main_frame_url, is_error_page);
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
  DCHECK(!shutdown_);
  if (!prediction.prefetch_requests.empty()) {
    DCHECK(base::FeatureList::IsEnabled(features::kLoadingPredictorPrefetch));
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

void LoadingPredictor::HandleOmniboxHint(const GURL& url, bool preconnectable) {
  if (!url.is_valid() || !url.has_host() || !IsPreconnectAllowed(profile_))
    return;

  url::Origin origin = url::Origin::Create(url);
  bool is_new_origin = origin != last_omnibox_origin_;
  last_omnibox_origin_ = origin;
  net::SchemefulSite site = net::SchemefulSite(origin);
  auto network_anonymization_key =
      net::NetworkAnonymizationKey::CreateSameSite(site);
  base::TimeTicks now = base::TimeTicks::Now();
  if (preconnectable) {
    if (is_new_origin || now - last_omnibox_preconnect_time_ >=
                             kMinDelayBetweenPreconnectRequests) {
      last_omnibox_preconnect_time_ = now;
      preconnect_manager()->StartPreconnectUrl(url, true,
                                               network_anonymization_key);
    }
    return;
  }

  if (is_new_origin || now - last_omnibox_preresolve_time_ >=
                           kMinDelayBetweenPreresolveRequests) {
    last_omnibox_preresolve_time_ = now;
    preconnect_manager()->StartPreresolveHost(url, network_anonymization_key);
  }
}

void LoadingPredictor::HandleBookmarkBarHint(const GURL& url,
                                             bool preconnectable) {
  if (!url.is_valid() || !url.has_host() || !IsPreconnectAllowed(profile_)) {
    return;
  }

  url::Origin origin = url::Origin::Create(url);
  bool is_new_origin = origin != last_bookmark_bar_origin_;
  last_bookmark_bar_origin_ = origin;
  net::SchemefulSite site = net::SchemefulSite(origin);
  auto network_anonymization_key =
      net::NetworkAnonymizationKey::CreateSameSite(site);
  base::TimeTicks now = base::TimeTicks::Now();
  if (preconnectable && url.SchemeIs("https")) {
    if (is_new_origin || now - last_bookmark_bar_preconnect_time_ >=
                             kMinDelayBetweenPreconnectRequests) {
      last_bookmark_bar_preconnect_time_ = now;
      preconnect_manager()->StartPreconnectUrl(url, true,
                                               network_anonymization_key);
    }
    return;
  }

  if (is_new_origin || now - last_bookmark_bar_preresolve_time_ >=
                           kMinDelayBetweenPreresolveRequests) {
    last_bookmark_bar_preresolve_time_ = now;
    preconnect_manager()->StartPreresolveHost(url, network_anonymization_key);
  }
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

}  // namespace predictors
