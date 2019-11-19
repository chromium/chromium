// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_LOADING_DATA_COLLECTOR_H_
#define CHROME_BROWSER_PREDICTORS_LOADING_DATA_COLLECTOR_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/predictors/loading_predictor_config.h"
#include "chrome/browser/predictors/navigation_id.h"
#include "content/public/common/resource_load_info.mojom.h"
#include "content/public/common/resource_type.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace predictors {

class LoadingStatsCollector;
class ResourcePrefetchPredictor;

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
  explicit PageRequestSummary(const GURL& main_frame_url);
  PageRequestSummary(const PageRequestSummary& other);
  ~PageRequestSummary();
  void UpdateOrAddToOrigins(
      const content::mojom::ResourceLoadInfo& resource_load_info);

  GURL main_frame_url;
  GURL initial_url;
  base::TimeTicks first_contentful_paint;

  // Map of origin -> OriginRequestSummary. Only one instance of each origin
  // is kept per navigation, but the summary is updated several times.
  std::map<url::Origin, OriginRequestSummary> origins;

 private:
  void UpdateOrAddToOrigins(
      const url::Origin& origin,
      const content::mojom::CommonNetworkInfoPtr& network_info);
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
  virtual void RecordStartNavigation(const NavigationID& navigation_id);
  virtual void RecordFinishNavigation(const NavigationID& old_navigation_id,
                                      const NavigationID& new_navigation_id,
                                      bool is_error_page);
  virtual void RecordResourceLoadComplete(
      const NavigationID& navigation_id,
      const content::mojom::ResourceLoadInfo& resource_load_info);

  // Called when the main frame of a page completes loading. We treat this point
  // as the "completion" of the navigation. The resources requested by the page
  // up to this point are the only ones considered.
  virtual void RecordMainFrameLoadComplete(const NavigationID& navigation_id);

  // Called after the main frame's first contentful paint.
  virtual void RecordFirstContentfulPaint(
      const NavigationID& navigation_id,
      const base::TimeTicks& first_contentful_paint);

 private:
  using NavigationMap =
      std::map<NavigationID, std::unique_ptr<PageRequestSummary>>;

  friend class LoadingDataCollectorTest;

  FRIEND_TEST_ALL_PREFIXES(LoadingDataCollectorTest, HandledResourceTypes);
  FRIEND_TEST_ALL_PREFIXES(LoadingDataCollectorTest, ShouldRecordMainFrameLoad);
  FRIEND_TEST_ALL_PREFIXES(LoadingDataCollectorTest,
                           ShouldRecordSubresourceLoadAfterFCP);
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

  static void SetAllowPortInUrlsForTesting(bool state);

  bool ShouldRecordResourceLoad(
      const NavigationID& navigation_id,
      const content::mojom::ResourceLoadInfo& resource_load_info) const;

  // Returns true if the subresource has a supported type.
  static bool IsHandledResourceType(content::ResourceType resource_type,
                                    const std::string& mime_type);

  // Cleanup inflight_navigations_ and call a cleanup for stats_collector_.
  void CleanupAbandonedNavigations(const NavigationID& navigation_id);

  ResourcePrefetchPredictor* const predictor_;
  LoadingStatsCollector* const stats_collector_;
  const LoadingPredictorConfig config_;

  NavigationMap inflight_navigations_;
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_LOADING_DATA_COLLECTOR_H_
