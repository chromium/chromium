// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_TAB_HELPER_H_
#define CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_TAB_HELPER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/id_type.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/navigation_handle_user_data.h"
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
using NavigationId = base::IdType64<content::NavigationHandle>;

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
  LoadingPredictorTabHelper(const LoadingPredictorTabHelper&) = delete;
  LoadingPredictorTabHelper& operator=(const LoadingPredictorTabHelper&) =
      delete;

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
      content::RenderFrameHost* render_frame_host,
      const GURL& url,
      const std::string& mime_type,
      network::mojom::RequestDestination request_destination) override;
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;

  void SetLoadingPredictorForTesting(
      base::WeakPtr<LoadingPredictor> predictor) {
    predictor_ = predictor;
  }

 private:
  // The PageData stores the state needed for each page. It is primarily owned
  // by the DocumentPageDataHolder or NavigationPageDataHolder, depending on
  // whether the navigation has committed or not. The PageData is RefCounted
  // because it is passed to a callback that might outlive the holders, and the
  // holders still need to access the PageData after passing it to the callback.
  class DocumentPageDataHolder;
  class NavigationPageDataHolder;
  class PageData : public base::RefCounted<PageData> {
   public:
    PageData();

    static PageData* GetForNavigationHandle(
        content::NavigationHandle& navigation_handle);
    static PageData& CreateForNavigationHandle(
        content::NavigationHandle& navigation_handle);
    static PageData* GetForDocument(
        content::RenderFrameHost& render_frame_host);
    static void TransferFromNavigationHandleToDocument(
        content::NavigationHandle& navigation_handle,
        content::RenderFrameHost& render_frame_host);

    // Uniquely identifies this navigation.
    NavigationId navigation_id_;

    // True if the navigation has committed.
    bool has_committed_ = false;

    bool has_local_preconnect_predictions_for_current_navigation_ = false;

    // The optimization guide prediction for the current navigation.
    std::optional<OptimizationGuidePrediction>
        last_optimization_guide_prediction_;

    // Stores weak ptrs to the document and navigation page data holders, in
    // order to determine the current state of the navigation.
    base::WeakPtr<DocumentPageDataHolder> document_page_data_holder_;
    base::WeakPtr<NavigationPageDataHolder> navigation_page_data_holder_;

    base::WeakPtr<LoadingPredictor> predictor_;

   private:
    friend class base::RefCounted<PageData>;
    ~PageData();
  };

  // The DocumentPageDataHolder is used to store the state after the navigation
  // has committed.
  class DocumentPageDataHolder
      : public content::DocumentUserData<DocumentPageDataHolder> {
   public:
    explicit DocumentPageDataHolder(
        content::RenderFrameHost* render_frame_host);
    ~DocumentPageDataHolder() override;
    DOCUMENT_USER_DATA_KEY_DECL();

    scoped_refptr<PageData> page_data_;
    base::WeakPtrFactory<DocumentPageDataHolder> weak_factory_{this};
  };

  // The NavigationPageDataHolder is used to store the PageData while a
  // navigation is still in progress, and has not yet committed.
  class NavigationPageDataHolder
      : public content::NavigationHandleUserData<NavigationPageDataHolder> {
   public:
    explicit NavigationPageDataHolder(
        content::NavigationHandle& navigation_handle);
    ~NavigationPageDataHolder() override;
    NAVIGATION_HANDLE_USER_DATA_KEY_DECL();

    scoped_refptr<PageData> page_data_;
    base::SafeRef<content::NavigationHandle> navigation_handle_;
    base::WeakPtrFactory<NavigationPageDataHolder> weak_factory_{this};
  };

  explicit LoadingPredictorTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<LoadingPredictorTabHelper>;

  // Callback invoked when |optimization_guide_decider_| has the information
  // required to decide if it has remote predictions for the page load.
  void OnOptimizationGuideDecision(
      scoped_refptr<PageData> page_helper,
      const std::optional<url::Origin>& initiator_origin,
      const GURL& main_frame_url,
      bool should_add_preconnects_to_prediction,
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  // Owned by profile.
  base::WeakPtr<LoadingPredictor> predictor_;

  // The optimization guide decider to consult for remote predictions.
  raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_ = nullptr;

  // Used to get a weak pointer to |this|.
  base::WeakPtrFactory<LoadingPredictorTabHelper> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_TAB_HELPER_H_
