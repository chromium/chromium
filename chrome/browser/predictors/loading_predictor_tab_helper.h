// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_TAB_HELPER_H_
#define CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_TAB_HELPER_H_

#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/predictors/navigation_id.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace optimization_guide {
class OptimizationGuideDecider;
enum class OptimizationGuideDecision;
class OptimizationMetadata;
}  // namespace optimization_guide

namespace predictors {

class LoadingPredictor;

// Observes various page load events from the navigation start to onload
// completed and notifies the LoadingPredictor associated with the current
// profile.
//
// All methods must be called from the UI thread.
class LoadingPredictorTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<LoadingPredictorTabHelper> {
 public:
  ~LoadingPredictorTabHelper() override;

  // content::WebContentsObserver implementation
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void ResourceLoadComplete(
      content::RenderFrameHost* render_frame_host,
      const content::GlobalRequestID& request_id,
      const blink::mojom::ResourceLoadInfo& resource_load_info) override;
  void DidLoadResourceFromMemoryCache(
      const GURL& url,
      const std::string& mime_type,
      network::mojom::RequestDestination request_destination) override;
  void DocumentOnLoadCompletedInMainFrame() override;

  void SetLoadingPredictorForTesting(
      base::WeakPtr<LoadingPredictor> predictor) {
    predictor_ = predictor;
  }

 private:
  explicit LoadingPredictorTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<LoadingPredictorTabHelper>;

  // Callback invoked when |optimization_guide_decider_| has the information
  // required to decide if it has remote predictions for the page load.
  void OnOptimizationGuideDecision(
      const NavigationID& navigation_id,
      bool should_add_preconnects_to_prediction,
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  // Owned by profile.
  base::WeakPtr<LoadingPredictor> predictor_;

  NavigationID current_navigation_id_;

  bool has_local_preconnect_predictions_for_current_navigation_ = false;

  // The optimization guide decider to consult for remote predictions.
  optimization_guide::OptimizationGuideDecider* optimization_guide_decider_ =
      nullptr;

  // The optimization guide prediction for the current navigation. If set, this
  // will be cleared on |DocumentOnLoadCompletedInMainFrame|.
  base::Optional<OptimizationGuidePrediction>
      last_optimization_guide_prediction_;

  // Used to get a weak pointer to |this|.
  base::WeakPtrFactory<LoadingPredictorTabHelper> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(LoadingPredictorTabHelper);
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_TAB_HELPER_H_
