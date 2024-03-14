// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_LOADING_DATA_COLLECTOR_H_
#define CHROME_BROWSER_PREDICTORS_LOADING_DATA_COLLECTOR_H_

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/types/id_type.h"
#include "chrome/browser/predictors/loading_predictor_config.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/fetch_api.mojom-forward.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace predictors {

class LoadingStatsCollector;
struct OptimizationGuidePrediction;
class ResourcePrefetchPredictor;
using NavigationId = base::IdType64<content::NavigationHandle>;

// Data collected for origin-based prediction, for a single origin during a
// page load (see PageRequestSummary).
struct OriginRequestSummary {
  OriginRequestSummary();
  OriginRequestSummary(const OriginRequestSummary& other);
  ~OriginRequestSummary();

  url::Origin origin;
  bool always_access_network = false;
  bool accessed_network = false;
  int first_occurrence = 0;
};

// Stores the data learned from a single navigation.
struct PageRequestSummary {
  PageRequestSummary(ukm::SourceId ukm_source_id,
                     const GURL& main_frame_url,
                     base::TimeTicks navigation_started);
  PageRequestSummary(const PageRequestSummary& other);
  ~PageRequestSummary();
  void UpdateOrAddResource(
      const blink::mojom::ResourceLoadInfo& resource_load_info);
  void AddPreconnectAttempt(const GURL& preconnect_url);
  void AddPrefetchAttempt(const GURL& prefetch_url);
  void MainFrameLoadComplete();

  const ukm::SourceId ukm_source_id;
  GURL main_frame_url;
  const GURL initial_url;
  const base::TimeTicks navigation_started;
  std::optional<base::TimeTicks> navigation_committed;
  bool main_frame_load_complete{false};

  // Map of origin -> OriginRequestSummary. Only one instance of each origin
  // is kept per navigation, but the summary is updated several times.
  std::map<url::Origin, OriginRequestSummary> origins;

  // Set of origins for which preconnects were initiated.
  base::flat_set<url::Origin> preconnect_origins;

  // Set of seen resource URLs.
  base::flat_set<GURL> subresource_urls;

  // Set of resource URLs for which prefetches were initiated.
  base::flat_set<GURL> prefetch_urls;

  // The time for which the first resource prefetch was initiated for the
  // navigation.
  std::optional<base::TimeTicks> first_prefetch_initiated;

  // Equivalents of |origins_| and |subresource_urls| that store URLs/origins
  // that are not learned by LoadingPredictor. These are from low-priority
  // subresource loads, or subresource loads after the load event for the page
  // has been dispatched. They are used to record metrics to understand the
  // precision of optimization guide predictions.
  base::flat_set<url::Origin> low_priority_origins;
  base::flat_set<GURL> low_priority_subresource_urls;

 private:
  FRIEND_TEST_ALL_PREFIXES(LoadingDataCollectorTest, HandledResourceTypes);
  FRIEND_TEST_ALL_PREFIXES(LoadingDataCollectorTest, ShouldRecordMainFrameLoad);
  FRIEND_TEST_ALL_PREFIXES(LoadingDataCollectorTest,
                           ShouldRecordSubresourceLoadAfterFCP);
  FRIEND_TEST_ALL_PREFIXES(LoadingDataCollectorTest,
                           ShouldRecordSubresourceLoad);
  FRIEND_TEST_ALL_PREFIXES(LoadingDataCollectorTest,
                           ShouldRecordSubresourceLoadAfterLoadComplete);

  enum ShouldRecordResourceLoadResult { kNo, kLowPriority, kYes };
  ShouldRecordResourceLoadResult ShouldRecordResourceLoad(
      const blink::mojom::ResourceLoadInfo& resource_load_info) const;
  // Returns true if the resource has a supported type.
  static bool IsHandledResourceType(
      network::mojom::RequestDestination destination,
      const std::string& mime_type);
  void UpdateOrAddToOrigins(
      const url::Origin& origin,
      const blink::mojom::CommonNetworkInfoPtr& network_info,
      bool is_low_priority);
};

// Records navigation events as reported by various observers to the database
// and stats collection classes. All the non-static methods of this class need
// to be called on the UI thread.
class LoadingDataCollector {
 public:
  explicit LoadingDataCollector(
      predictors::ResourcePrefetchPredictor* predictor,
      predictors::LoadingStatsCollector* stats_collector,
      const LoadingPredictorConfig& config);
  virtual ~LoadingDataCollector();

  // |LoadingPredictorTabHelper| calls the below functions to inform the
  // collector of navigation and resource load events.
  virtual void RecordStartNavigation(NavigationId navigation_id,
                                     ukm::SourceId ukm_source_id,
                                     const GURL& main_frame_url,
                                     base::TimeTicks creation_time);
  virtual void RecordFinishNavigation(NavigationId navigation_id,
                                      const GURL& new_main_frame_url,
                                      bool is_error_page);
  virtual void RecordResourceLoadComplete(
      NavigationId navigation_id,
      const blink::mojom::ResourceLoadInfo& resource_load_info);

  // Called when a preconnect is initiated for the navigation.
  virtual void RecordPreconnectInitiated(NavigationId navigation_id,
                                         const GURL& preconnect_url);
  // Called when a prefetch is initiated for the navigation.
  virtual void RecordPrefetchInitiated(NavigationId navigation_id,
                                       const GURL& prefetch_url);

  // Called when the main frame of a page completes loading. We treat this point
  // as the "completion" of the navigation. The resources requested by the page
  // up to this point are the only ones considered by |predictor_|.
  virtual void RecordMainFrameLoadComplete(NavigationId navigation_id);

  // Called when the page committed by the navigation is destroyed.
  virtual void RecordPageDestroyed(
      NavigationId navigation_id,
      const std::optional<OptimizationGuidePrediction>& prediction);

 private:
  using NavigationMap =
      std::map<NavigationId, std::unique_ptr<PageRequestSummary>>;

  friend class LoadingDataCollectorTest;

  FRIEND_TEST_ALL_PREFIXES(LoadingDataCollectorTest, ShouldRecordMainFrameLoad);
  FRIEND_TEST_ALL_PREFIXES(LoadingDataCollectorTest,
                           ShouldRecordSubresourceLoad);
  FRIEND_TEST_ALL_PREFIXES(LoadingDataCollectorTest, SimpleNavigation);
  FRIEND_TEST_ALL_PREFIXES(LoadingDataCollectorTest, SimpleRedirect);
  FRIEND_TEST_ALL_PREFIXES(LoadingDataCollectorTest,
                           RecordStartNavigationMissing);
  FRIEND_TEST_ALL_PREFIXES(LoadingDataCollectorTest, RecordFailedNavigation);
  FRIEND_TEST_ALL_PREFIXES(LoadingDataCollectorTest, ManyNavigations);
  FRIEND_TEST_ALL_PREFIXES(LoadingDataCollectorTest,
                           RecordResourceLoadComplete);
  FRIEND_TEST_ALL_PREFIXES(LoadingDataCollectorTest,
                           RecordPreconnectInitiatedNoInflightNavigation);
  FRIEND_TEST_ALL_PREFIXES(LoadingDataCollectorTest,
                           RecordPrefetchInitiatedNoInflightNavigation);
  FRIEND_TEST_ALL_PREFIXES(LoadingDataCollectorTest,
                           ShouldRecordSubresourceLoadAfterLoadComplete);
  FRIEND_TEST_ALL_PREFIXES(LoadingDataCollectorTest, RecordPageDestroyed);

  static void SetAllowPortInUrlsForTesting(bool state);

  // Cleanup inflight_navigations_ and call a cleanup for stats_collector_.
  void CleanupAbandonedNavigations();

  const raw_ptr<ResourcePrefetchPredictor, DanglingUntriaged> predictor_;
  const raw_ptr<LoadingStatsCollector> stats_collector_;
  const LoadingPredictorConfig config_;

  NavigationMap inflight_navigations_;
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_LOADING_DATA_COLLECTOR_H_
