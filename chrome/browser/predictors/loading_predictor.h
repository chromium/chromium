// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_H_
#define CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/predictors/loading_data_collector.h"
#include "chrome/browser/predictors/preconnect_manager.h"
#include "chrome/browser/predictors/prefetch_manager.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"

class Profile;

namespace features {

BASE_DECLARE_FEATURE(kSuppressesLoadingPredictorOnSlowNetwork);

extern const base::FeatureParam<base::TimeDelta>
    kSuppressesLoadingPredictorOnSlowNetworkThreshold;

}  // namespace features

namespace predictors {

class ResourcePrefetchPredictor;
class LoadingStatsCollector;
class PrewarmHttpDiskCacheManager;

// Entry point for the Loading predictor.
// From a high-level request (GURL and motivation) and a database of historical
// data, initiates predictive actions to speed up page loads.
//
// Also listens to main frame requests/redirects/responses to initiate and
// cancel page load hints.
//
// See ResourcePrefetchPredictor for a description of the resource prefetch
// predictor.
//
// All methods must be called from the UI thread.
class LoadingPredictor : public KeyedService,
                         public PreconnectManager::Delegate,
                         public PrefetchManager::Delegate {
 public:
  LoadingPredictor(const LoadingPredictorConfig& config, Profile* profile);

  LoadingPredictor(const LoadingPredictor&) = delete;
  LoadingPredictor& operator=(const LoadingPredictor&) = delete;

  ~LoadingPredictor() override;

  // Hints that a page load is expected for |url|, with the hint coming from a
  // given |origin|. If |preconnect_prediction| is provided, this will use it
  // over local predictions to trigger actions, such as prefetch and/or
  // preconnect. Returns true if no more preconnect actions should be taken by
  // the caller.
  bool PrepareForPageLoad(
      const std::optional<url::Origin>& initiator_origin,
      const GURL& url,
      HintOrigin origin,
      bool preconnectable = false,
      std::optional<PreconnectPrediction> preconnect_prediction = std::nullopt);

  // Indicates that a page load hint is no longer active.
  void CancelPageLoadHint(const GURL& url);

  // Starts initialization, will complete asynchronously.
  void StartInitialization();

  // Don't use, internal only.
  ResourcePrefetchPredictor* resource_prefetch_predictor();
  LoadingDataCollector* loading_data_collector();
  PreconnectManager* preconnect_manager();
  PrefetchManager* prefetch_manager();

  // KeyedService:
  void Shutdown() override;

  // OnNavigationStarted is invoked when navigation |navigation_id| with
  // |main_frame_url| has started navigating. It returns whether any actions
  // were taken, such as preconnecting to known resource hosts, at that time.
  bool OnNavigationStarted(NavigationId navigation_id,
                           ukm::SourceId ukm_source_id,
                           const std::optional<url::Origin>& initiator_origin,
                           const GURL& main_frame_url,
                           base::TimeTicks creation_time);
  void OnNavigationFinished(NavigationId navigation_id,
                            const GURL& old_main_frame_url,
                            const GURL& new_main_frame_url,
                            bool is_error_page);

  base::WeakPtr<LoadingPredictor> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // PreconnectManager::Delegate:
  void PreconnectInitiated(const GURL& url,
                           const GURL& preconnect_url) override;
  void PreconnectFinished(std::unique_ptr<PreconnectStats> stats) override;

  // PrefetchManager::Delegate:
  void PrefetchInitiated(const GURL& url, const GURL& prefetch_url) override;
  void PrefetchFinished(std::unique_ptr<PrefetchStats> stats) override;

  size_t GetActiveHintsSizeForTesting() { return active_hints_.size(); }
  size_t GetTotalHintsActivatedForTesting() { return total_hints_activated_; }
  size_t GetActiveNavigationsSizeForTesting() {
    return active_navigations_.size();
  }

  const std::map<GURL, base::TimeTicks>& active_hints_for_testing() const {
    return active_hints_;
  }

  // May start a preconnect for |url|, if the current profile settings allow to
  // perform preresolve and preconnect actions.
  void PreconnectURLIfAllowed(
      const GURL& url,
      bool allow_credentials,
      const net::NetworkAnonymizationKey& network_anonymization_key);

  void MaybePrewarmResources(const std::optional<url::Origin>& initiator_origin,
                             const GURL& top_frame_main_resource_url);

 private:
  // Stores the information necessary to keep track of the active navigations.
  struct NavigationInfo {
    GURL main_frame_url;
    base::TimeTicks creation_time;
  };

  struct PreconnectData {
    url::Origin last_origin_;
    base::TimeTicks last_preconnect_time_;
    base::TimeTicks last_preresolve_time_;
  };

  // Cancels an active hint, from its iterator inside |active_hints_|. If the
  // iterator is .end(), does nothing. Returns the iterator after deletion of
  // the entry.
  std::map<GURL, base::TimeTicks>::iterator CancelActiveHint(
      std::map<GURL, base::TimeTicks>::iterator hint_it);
  void CleanupAbandonedHintsAndNavigations(NavigationId navigation_id);

  // May start preconnect and preresolve jobs according to `prediction` for
  // `url`.
  //
  // When LoadingPredictorPrefetch is enabled, starts prefetch jobs if
  // `prediction` has prefetch requests.
  void MaybeAddPreconnect(const GURL& url, PreconnectPrediction prediction);
  // If a preconnect or prefetch exists for `url`, stop it.
  void MaybeRemovePreconnect(const GURL& url);

  // May start a preconnect or a preresolve for `url`. `preconnectable`
  // indicates if preconnect is possible, or only preresolve will be performed.
  bool HandleHintByOrigin(const GURL& url,
                          bool preconnectable,
                          bool only_allow_https,
                          PreconnectData& preconnect_data);

  // For testing.
  void set_mock_resource_prefetch_predictor(
      std::unique_ptr<ResourcePrefetchPredictor> predictor) {
    resource_prefetch_predictor_ = std::move(predictor);
  }

  // For testing.
  void set_mock_preconnect_manager(
      std::unique_ptr<PreconnectManager> preconnect_manager) {
    preconnect_manager_ = std::move(preconnect_manager);
  }

  // For testing.
  void set_mock_loading_data_collector(
      std::unique_ptr<LoadingDataCollector> loading_data_collector) {
    loading_data_collector_ = std::move(loading_data_collector);
  }

  LoadingPredictorConfig config_;
  raw_ptr<Profile, DanglingUntriaged> profile_;
  std::unique_ptr<ResourcePrefetchPredictor> resource_prefetch_predictor_;
  std::unique_ptr<LoadingStatsCollector> stats_collector_;
  std::unique_ptr<LoadingDataCollector> loading_data_collector_;
  std::unique_ptr<PreconnectManager> preconnect_manager_;
  std::unique_ptr<PrefetchManager> prefetch_manager_;
  std::unique_ptr<PrewarmHttpDiskCacheManager> prewarm_http_disk_cache_manager_;
  std::map<GURL, base::TimeTicks> active_hints_;
  std::map<NavigationId, NavigationInfo> active_navigations_;
  std::map<GURL, std::set<NavigationId>> active_urls_to_navigations_;
  bool shutdown_ = false;
  size_t total_hints_activated_ = 0;

  PreconnectData omnibox_preconnect_data_;

  PreconnectData bookmark_bar_preconnect_data_;

  PreconnectData new_tab_page_preconnect_data_;

  friend class LoadingPredictorTest;
  friend class LoadingPredictorPreconnectTest;
  friend class LoadingPredictorTabHelperTest;
  friend class LoadingPredictorTabHelperTestCollectorTest;
  FRIEND_TEST_ALL_PREFIXES(LoadingPredictorTest,
                           TestMainFrameResponseCancelsHint);
  FRIEND_TEST_ALL_PREFIXES(LoadingPredictorTest,
                           TestMainFrameRequestCancelsStaleNavigations);
  FRIEND_TEST_ALL_PREFIXES(LoadingPredictorTest,
                           TestMainFrameResponseClearsNavigations);
  FRIEND_TEST_ALL_PREFIXES(LoadingPredictorTest,
                           TestMainFrameRequestDoesntCancelExternalHint);
  FRIEND_TEST_ALL_PREFIXES(LoadingPredictorTest,
                           TestDuplicateHintAfterPreconnectCompleteCalled);
  FRIEND_TEST_ALL_PREFIXES(LoadingPredictorTest,
                           TestDuplicateHintAfterPreconnectCompleteNotCalled);
  FRIEND_TEST_ALL_PREFIXES(LoadingPredictorTest,
                           TestDontTrackNonPrefetchableUrls);
  FRIEND_TEST_ALL_PREFIXES(LoadingPredictorTest, TestDontPredictOmniboxHints);
  FRIEND_TEST_ALL_PREFIXES(LoadingPredictorPreconnectTest,
                           TestHandleHintWithOpaqueOrigins);
  FRIEND_TEST_ALL_PREFIXES(LoadingPredictorPreconnectTest,
                           TestHandleHintWhenOnlyHttpsAllowed);
  FRIEND_TEST_ALL_PREFIXES(LoadingPredictorPreconnectTest,
                           TestHandleHintPreresolveWhenOnlyHttpsAllowed);

  base::WeakPtrFactory<LoadingPredictor> weak_factory_{this};
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_H_
