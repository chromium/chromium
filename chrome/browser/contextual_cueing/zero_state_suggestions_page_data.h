// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_ZERO_STATE_SUGGESTIONS_PAGE_DATA_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_ZERO_STATE_SUGGESTIONS_PAGE_DATA_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/time/time.h"
#include "chrome/browser/contextual_cueing/zero_state_suggestions_request.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/core/hints/optimization_guide_decision.h"
#include "components/optimization_guide/core/hints/optimization_metadata.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "content/public/browser/page_user_data.h"

class OptimizationGuideKeyedService;

namespace content_extraction {
struct InnerTextResult;
}  // namespace content_extraction

namespace optimization_guide {
class PageContextEligibility;
namespace proto {
class ZeroStatePageContext;
}  // namespace proto
}  // namespace optimization_guide

namespace page_content_annotations {
class PageContentExtractionService;
}  // namespace page_content_annotations

namespace contextual_cueing {

using PageContextCallbackList = base::OnceCallbackList<void(
    std::optional<optimization_guide::proto::ZeroStatePageContext>)>;
using PageContextCallback = PageContextCallbackList::CallbackType;

// Processes necessary information about the page to generate zero state
// suggestions.
class ZeroStateSuggestionsPageData
    : public content::PageUserData<ZeroStateSuggestionsPageData> {
 public:
  ZeroStateSuggestionsPageData(const ZeroStateSuggestionsPageData&) = delete;
  ZeroStateSuggestionsPageData& operator=(const ZeroStateSuggestionsPageData&) =
      delete;
  ~ZeroStateSuggestionsPageData() override;

  // Initiates page content extraction.
  void InitiatePageContentExtraction();

  // Gets the page context for this page. Will return synchronously if page
  // context is already ready.
  void GetPageContext(PageContextCallback callback);

  void set_cached_suggestions_for_focused_tab(
      std::optional<std::vector<std::string>>
          cached_suggestions_for_focused_tab) {
    cached_suggestions_for_focused_tab_ = cached_suggestions_for_focused_tab;
  }
  std::optional<std::vector<std::string>> cached_suggestions_for_focused_tab()
      const {
    return cached_suggestions_for_focused_tab_;
  }

  void set_focused_tab_request(
      std::unique_ptr<ZeroStateSuggestionsRequest> focused_tab_request) {
    focused_tab_request_ = std::move(focused_tab_request);
  }
  ZeroStateSuggestionsRequest* focused_tab_request() {
    return focused_tab_request_.get();
  }

  base::WeakPtr<ZeroStateSuggestionsPageData> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void set_is_focused_tab() { is_focused_ = true; }

 private:
  friend class content::PageUserData<ZeroStateSuggestionsPageData>;
  friend class ContextualCueingServiceTestZeroStateSuggestions;
  friend class ZeroStateSuggestionsPageDataTest;

  // Note that this constructor initiates extracting page content.
  explicit ZeroStateSuggestionsPageData(content::Page& page);

  // Returns the URL of the primary main frame associated with this page.
  const GURL GetUrl() const;

  // Called when inner text is extracted.
  void OnReceivedInnerText(
      std::unique_ptr<content_extraction::InnerTextResult> result);

  // Called when annotated page content is received.
  void OnReceivedAnnotatedPageContent(
      std::optional<optimization_guide::proto::AnnotatedPageContent> content);

  // Called when on-demand metadata is received.
  void OnReceivedOptimizationMetadataOnDemand(
      const GURL& url,
      const base::flat_map<
          optimization_guide::proto::OptimizationType,
          optimization_guide::OptimizationGuideDecisionWithMetadata>&
          decisions);

  // Called when optimization metadata is received.
  void OnReceivedOptimizationMetadata(
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  // Notifies all page context callbacks that page context has been collected
  // for the page.
  void InvokePageContextCallbacksIfComplete();

  // If `optimization_metadata_` contains everything necessary to determine a
  // suggestions result, run `suggestions_callbacks_` to return those
  // suggestions. This method itself also returns true if suggestions are sent
  // via the callbacks as a result of execution.
  bool ReturnSuggestionsFromOptimizationMetadataIfPossible();

  bool work_done() const {
    return inner_text_done_ && annotated_page_content_done_ &&
           optimization_metadata_done_;
  }

  // Returns the page context collected for this page.
  optimization_guide::proto::ZeroStatePageContext ConstructPageContextProto()
      const;

  void OnPageContextEligibilityAPILoaded(
      optimization_guide::PageContextEligibility* page_context_eligibility);

  // Tracks the status of page context needed to fetch suggestions:
  // 1. inner text
  // 2. annotated page content
  // 3. optimization metadata
  bool content_extraction_initiated_ = false;
  base::TimeTicks page_context_begin_time_;
  // Tracks if `this` has logged to page context extraction duration histogram.
  bool page_context_duration_logged_ = false;
  bool inner_text_done_ = false;
  std::unique_ptr<content_extraction::InnerTextResult> inner_text_result_;
  bool annotated_page_content_done_ = false;
  std::optional<optimization_guide::proto::AnnotatedPageContent>
      annotated_page_content_;
  bool optimization_metadata_done_ = false;
  optimization_guide::OptimizationGuideDecision optimization_decision_;
  optimization_guide::OptimizationMetadata optimization_metadata_;

  // The suggestions that were computed for this page when suggestions were
  // requested for the focused tab.
  std::optional<std::vector<std::string>> cached_suggestions_for_focused_tab_;

  std::unique_ptr<ZeroStateSuggestionsRequest> focused_tab_request_;

  // Tracks the state for a page context request.
  PageContextCallbackList page_context_callbacks_;

  bool is_focused_ = false;
  // Not owned and guaranteed to outlive `this`.
  raw_ptr<optimization_guide::PageContextEligibility> page_context_eligibility_;
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_keyed_service_ =
      nullptr;
  raw_ptr<page_content_annotations::PageContentExtractionService>
      page_content_extraction_service_ = nullptr;
  base::WeakPtrFactory<ZeroStateSuggestionsPageData> weak_ptr_factory_{this};

  PAGE_USER_DATA_KEY_DECL();
};

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_ZERO_STATE_SUGGESTIONS_PAGE_DATA_H_
