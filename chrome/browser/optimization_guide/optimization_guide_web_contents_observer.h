// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_WEB_CONTENTS_OBSERVER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/core/insertion_ordered_set.h"
#include "components/optimization_guide/core/optimization_guide_navigation_data.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "content/public/browser/page_user_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace optimization_guide {
class ChromeHintsManager;
class ChromeHintsManagerFetchingTest;
}  // namespace optimization_guide

class OptimizationGuideKeyedService;

// Observes navigation events.
class OptimizationGuideWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<
          OptimizationGuideWebContentsObserver> {
 public:
  OptimizationGuideWebContentsObserver(
      const OptimizationGuideWebContentsObserver&) = delete;
  OptimizationGuideWebContentsObserver& operator=(
      const OptimizationGuideWebContentsObserver&) = delete;

  ~OptimizationGuideWebContentsObserver() override;

  // Notifies |this| to flush |last_navigation_data| so metrics are recorded.
  void FlushLastNavigationData();

  // Tell the observer that hints for this URL should be fetched once we reach
  // onload in this |web_contents|. Note that |web_contents| must be the
  // WebContents being observed by this object.
  void AddURLsToBatchFetchBasedOnPrediction(std::vector<GURL> urls,
                                            content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<
      OptimizationGuideWebContentsObserver>;
  friend class optimization_guide::ChromeHintsManagerFetchingTest;

  explicit OptimizationGuideWebContentsObserver(
      content::WebContents* web_contents);

  // Gets the OptimizationGuideNavigationData associated with the
  // |navigation_handle|. If one does not exist already, one will be created for
  // it.
  OptimizationGuideNavigationData* GetOrCreateOptimizationGuideNavigationData(
      content::NavigationHandle* navigation_handle);

  // content::WebContentsObserver implementation:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;

  void DocumentOnLoadCompletedInPrimaryMainFrame() override;

  // Ask |hints_manager| to fetch hints for navigations that were predicted for
  // the current page load.
  void FetchHintsUsingManager(
      optimization_guide::ChromeHintsManager* hints_manager,
      base::WeakPtr<content::Page> page);

  // Notifies |optimization_guide_keyed_service_| that the navigation has
  // finished.
  void NotifyNavigationFinish(
      std::unique_ptr<OptimizationGuideNavigationData> navigation_data,
      const std::vector<GURL>& navigation_redirect_chain);

  // Data scoped to a single page. PageData has the same lifetime as the page's
  // main document.
  class PageData : public content::PageUserData<PageData> {
   public:
    explicit PageData(content::Page& page);
    PageData(const PageData&) = delete;
    PageData& operator=(const PageData&) = delete;
    ~PageData() override;

    bool is_sent_batched_hints_request() const {
      return sent_batched_hints_request_;
    }
    void set_sent_batched_hints_request() {
      sent_batched_hints_request_ = true;
    }
    void InsertHintTargetUrls(const std::vector<GURL>& urls);
    std::vector<GURL> GetHintsTargetUrls();

    void SetNavigationData(
        std::unique_ptr<OptimizationGuideNavigationData> navigation_data) {
      navigation_data_ = std::move(navigation_data);
    }

    PAGE_USER_DATA_KEY_DECL();

   private:
    // List of predicted URLs to fetch hints for once the page reaches onload.
    optimization_guide::InsertionOrderedSet<GURL> hints_target_urls_;

    // Whether a hints request for predicted URLs has been fired off for this
    // page loads. Used to avoid sending more than one predicted URLs hints
    // request per page load.
    bool sent_batched_hints_request_ = false;

    // The navigation data for the completed navigation.
    std::unique_ptr<OptimizationGuideNavigationData> navigation_data_;
  };

  class NavigationHandleData
      : public content::NavigationHandleUserData<NavigationHandleData> {
   public:
    explicit NavigationHandleData(content::NavigationHandle&);
    NavigationHandleData(const NavigationHandleData&) = delete;
    NavigationHandleData& operator=(const NavigationHandleData&) = delete;
    ~NavigationHandleData() override;

    std::unique_ptr<OptimizationGuideNavigationData>
    TakeOptimizationGuideNavigationData() {
      return std::move(optimization_guide_navigation_data_);
    }

    OptimizationGuideNavigationData* GetOptimizationGuideNavigationData() {
      return optimization_guide_navigation_data_.get();
    }

    void SetOptimizationGuideNavigationData(
        std::unique_ptr<OptimizationGuideNavigationData> navigation_data) {
      optimization_guide_navigation_data_ = std::move(navigation_data);
    }

    NAVIGATION_HANDLE_USER_DATA_KEY_DECL();

   private:
    // The data related to a given navigation ID.
    std::unique_ptr<OptimizationGuideNavigationData>
        optimization_guide_navigation_data_;
  };

  // Returns the PageData for the specified |page|.
  PageData& GetPageData(content::Page& page);

  // Initialized in constructor. It may be null if the
  // OptimizationGuideKeyedService feature is not enabled.
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_keyed_service_ =
      nullptr;

  base::WeakPtrFactory<OptimizationGuideWebContentsObserver> weak_factory_{
      this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_WEB_CONTENTS_OBSERVER_H_
