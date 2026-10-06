// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_CUE_TAB_STATE_H_
#define CHROME_BROWSER_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_CUE_TAB_STATE_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/contextual_cueing/cue_target.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "url/gurl.h"

class OptimizationGuideKeyedService;

namespace content {
class NavigationHandle;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace contextual_search {

// Tab-scoped state helper for ContextualSearchCueTarget. Manages the per-tab
// PageContentAnnotationsObserver, caches the latest classification result for
// the current navigation, and manages the pending CheckEligibility callback and
// timeout timer.
// Note: This class is an intentional duplicate of `glic::GlicCueTabState`.
class ContextualSearchCueTabState
    : public content::WebContentsObserver,
      public page_content_annotations::PageContentAnnotationsService::
          PageContentAnnotationsObserver {
 public:
  DECLARE_USER_DATA(ContextualSearchCueTabState);

  // The state is owned by `tab`'s TabFeatures.
  explicit ContextualSearchCueTabState(tabs::TabInterface& tab);

  // Returns the state owned by `tab`'s TabFeatures, or nullptr if it was
  // not created.
  static ContextualSearchCueTabState* From(tabs::TabInterface* tab);

  ContextualSearchCueTabState(const ContextualSearchCueTabState&) = delete;
  ContextualSearchCueTabState& operator=(const ContextualSearchCueTabState&) =
      delete;
  ~ContextualSearchCueTabState() override;

  // Evaluates eligibility for this tab. If the classification result is already
  // cached for the current navigation, resolves immediately. Otherwise, stores
  // the callback and starts the timeout timer.
  void CheckEligibility(
      contextual_cueing::CueIntrusiveness intrusiveness,
      contextual_cueing::CueTarget::EligibilityCallback callback,
      base::WeakPtr<contextual_cueing::CueTarget> target);

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // page_content_annotations::PageContentAnnotationsService::
  // PageContentAnnotationsObserver:
  void OnPageContentAnnotated(
      const page_content_annotations::HistoryVisit& visit,
      const page_content_annotations::PageContentAnnotationsResult& result)
      override;

  void SetAnnotationServiceForTesting(
      page_content_annotations::PageContentAnnotationsService* service) {
    annotation_service_ = service;
  }

 private:
  // Cancels `pending_check_` by firing its callback with false.
  void CancelPendingCheck();

  // Resolves `pending_check_` using `cached_result_`. No-op if either is
  // missing.
  void ResolvePendingCheck();

  // Called if the annotation timeout expires before an annotation arrives.
  void OnAnnotationTimeout();

  // Tracked parameters to match incoming annotations to the current navigation.
  GURL last_committed_url_;

  // The latest classification result for the current navigation.
  std::optional<page_content_annotations::PageContentAnnotationsResult>
      cached_result_;

  struct PendingCheck {
    contextual_cueing::CueIntrusiveness intrusiveness;
    contextual_cueing::CueTarget::EligibilityCallback callback;
    base::WeakPtr<contextual_cueing::CueTarget> target;
  };
  std::optional<PendingCheck> pending_check_;

  raw_ptr<page_content_annotations::PageContentAnnotationsService>
      annotation_service_ = nullptr;
  // Used for logging to chrome://optimization-guide-internals with the
  // CUEING_LOG macro.
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_keyed_service_ =
      nullptr;

  base::OneShotTimer annotation_timeout_timer_;

  ui::ScopedUnownedUserData<ContextualSearchCueTabState>
      scoped_unowned_user_data_;
};

}  // namespace contextual_search

#endif  // CHROME_BROWSER_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_CUE_TAB_STATE_H_
