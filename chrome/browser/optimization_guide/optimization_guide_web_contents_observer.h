// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_WEB_CONTENTS_OBSERVER_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/optimization_guide/optimization_guide_navigation_data.h"
#include "components/optimization_guide/core/insertion_ordered_set.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
}  // namespace content

class OptimizationGuideHintsManager;
class OptimizationGuideKeyedService;

// Observes navigation events.
class OptimizationGuideWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<
          OptimizationGuideWebContentsObserver> {
 public:
  ~OptimizationGuideWebContentsObserver() override;

  // Gets the OptimizationGuideNavigationData associated with
  // |navigation_handle|. If one does not exist already, one will be created for
  // it.
  OptimizationGuideNavigationData* GetOrCreateOptimizationGuideNavigationData(
      content::NavigationHandle* navigation_handle);

  // Notifies |this| to flush |last_navigation_data| so metrics are recorded.
  void FlushLastNavigationData();

  // Tell the observer that hints for this URL should be fetched once we reach
  // onload in this |web_contents|. Note that |web_contents| must be the
  // WebContents being observed by this object.
  void AddURLsToBatchFetchBasedOnPrediction(std::vector<GURL> urls,
                                            content::WebContents* web_contents);

 private:
  friend class OptimizationGuideHintsManagerFetchingTest;

  friend class content::WebContentsUserData<
      OptimizationGuideWebContentsObserver>;

  explicit OptimizationGuideWebContentsObserver(
      content::WebContents* web_contents);

  // Clears the state related to hints to be fetched at onload due to navigation
  // predictions.
  void ClearHintsToFetchBasedOnPredictions(
      content::NavigationHandle* navigation_handle);

  // content::WebContentsObserver implementation:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DocumentOnLoadCompletedInMainFrame(
      content::RenderFrameHost* render_frame_host) override;

  // Ask OptimizationGuideHintsManager to fetch hints for navigations that were
  // predicted for the current page load.
  void FetchHints();

  // For testing.
  void FetchHintsUsingManagerForTesting(
      OptimizationGuideHintsManager* hints_manager);

  // Notifies |optimization_guide_keyed_service_| that the navigation has
  // finished.
  void NotifyNavigationFinish(
      int64_t navigation_id,
      const std::vector<GURL>& navigation_redirect_chain);

  // The data related to a given navigation ID.
  base::flat_map<int64_t, std::unique_ptr<OptimizationGuideNavigationData>>
      inflight_optimization_guide_navigation_datas_;

  // The navigation data for the last completed navigation.
  std::unique_ptr<OptimizationGuideNavigationData> last_navigation_data_;

  // Initialized in constructor. It may be null if the
  // OptimizationGuideKeyedService feature is not enabled.
  OptimizationGuideKeyedService* optimization_guide_keyed_service_ = nullptr;

  // List of predicted URLs to fetch hints for once the page reaches onload.
  InsertionOrderedSet<GURL> hints_target_urls_;

  // Whether a hints request for predicted URLs has been fired off for this page
  // loads. Used to avoid sending more than one predicted URLs hints request per
  // page load.
  bool sent_batched_hints_request_ = false;

  base::WeakPtrFactory<OptimizationGuideWebContentsObserver> weak_factory_{
      this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(OptimizationGuideWebContentsObserver);
};

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_WEB_CONTENTS_OBSERVER_H_
