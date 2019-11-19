// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_WEB_CONTENTS_OBSERVER_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/optimization_guide/optimization_guide_navigation_data.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
}  // namespace content

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

  // Captures the timing information at the time of FCP for the current
  // navigation to be used by the Optimization Guide to make decisions. Other
  // timing metric information may be missing (e.g., LCP, FMP).
  void UpdateSessionTimingStatistics(
      const page_load_metrics::mojom::PageLoadTiming& timing);

 private:
  friend class content::WebContentsUserData<
      OptimizationGuideWebContentsObserver>;

  explicit OptimizationGuideWebContentsObserver(
      content::WebContents* web_contents);

  // Overridden from content::WebContentsObserver.
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Synchronously flushes the metrics for the navigation with ID
  // |navigation_id| and then removes any data associated with it, including its
  // entry in |inflight_optimization_guide_navigation_datas_|.
  void FlushMetricsAndRemoveOptimizationGuideNavigationData(
      int64_t navigation_id,
      bool has_committed);

  // The data related to a given navigation ID.
  std::map<int64_t, OptimizationGuideNavigationData>
      inflight_optimization_guide_navigation_datas_;

  // Initialized in constructor. It may be null if the
  // OptimizationGuideKeyedService feature is not enabled.
  OptimizationGuideKeyedService* optimization_guide_keyed_service_ = nullptr;

  base::WeakPtrFactory<OptimizationGuideWebContentsObserver> weak_factory_{
      this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(OptimizationGuideWebContentsObserver);
};

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_WEB_CONTENTS_OBSERVER_H_
