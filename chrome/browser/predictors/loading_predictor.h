// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_H_
#define CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/predictors/loading_data_collector.h"
#include "chrome/browser/predictors/navigation_id.h"
#include "chrome/browser/predictors/preconnect_manager.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

class Profile;

namespace predictors {

class ResourcePrefetchPredictor;
class LoadingStatsCollector;

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
                         public PreconnectManager::Delegate {
 public:
  LoadingPredictor(const LoadingPredictorConfig& config, Profile* profile);
  ~LoadingPredictor() override;

  // Hints that a page load is expected for |url|, with the hint coming from a
  // given |origin|. May trigger actions, such as prefetch and/or preconnect.
  void PrepareForPageLoad(const GURL& url,
                          HintOrigin origin,
                          bool preconnectable = false);

  // Indicates that a page load hint is no longer active.
  void CancelPageLoadHint(const GURL& url);

  // Starts initialization, will complete asynchronously.
  void StartInitialization();

  // Don't use, internal only.
  ResourcePrefetchPredictor* resource_prefetch_predictor();
  LoadingDataCollector* loading_data_collector();
  PreconnectManager* preconnect_manager();

  // KeyedService:
  void Shutdown() override;

  void OnNavigationStarted(const NavigationID& navigation_id);
  void OnNavigationFinished(const NavigationID& old_navigation_id,
                            const NavigationID& new_navigation_id,
                            bool is_error_page);

  base::WeakPtr<LoadingPredictor> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // PreconnectManager::Delegate:
  void PreconnectFinished(std::unique_ptr<PreconnectStats> stats) override;

  size_t GetActiveHintsSizeForTesting() { return active_hints_.size(); }
  size_t GetTotalHintsActivatedForTesting() { return total_hints_activated_; }
  size_t GetActiveNavigationsSizeForTesting() {
    return active_navigations_.size();
  }

  const std::map<GURL, base::TimeTicks>& active_hints_for_testing() const {
    return active_hints_;
  }

 private:
  // Cancels an active hint, from its iterator inside |active_hints_|. If the
  // iterator is .end(), does nothing. Returns the iterator after deletion of
  // the entry.
  std::map<GURL, base::TimeTicks>::iterator CancelActiveHint(
      std::map<GURL, base::TimeTicks>::iterator hint_it);
  void CleanupAbandonedHintsAndNavigations(const NavigationID& navigation_id);

  // May start preconnect and preresolve jobs according to |requests| for |url|
  // with a given hint |origin|.
  void MaybeAddPreconnect(const GURL& url,
                          std::vector<PreconnectRequest> requests,
                          HintOrigin origin);
  // If a preconnect exists for |url|, stop it.
  void MaybeRemovePreconnect(const GURL& url);

  // May start a preconnect or a preresolve for |url|. |preconnectable|
  // indicates if preconnect is possible.
  void HandleOmniboxHint(const GURL& url, bool preconnectable);

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
  Profile* profile_;
  std::unique_ptr<ResourcePrefetchPredictor> resource_prefetch_predictor_;
  std::unique_ptr<LoadingStatsCollector> stats_collector_;
  std::unique_ptr<LoadingDataCollector> loading_data_collector_;
  std::unique_ptr<PreconnectManager> preconnect_manager_;
  std::map<GURL, base::TimeTicks> active_hints_;
  std::set<NavigationID> active_navigations_;
  bool shutdown_ = false;
  size_t total_hints_activated_ = 0;

  GURL last_omnibox_origin_;
  base::TimeTicks last_omnibox_preconnect_time_;
  base::TimeTicks last_omnibox_preresolve_time_;

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

  base::WeakPtrFactory<LoadingPredictor> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LoadingPredictor);
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_H_
