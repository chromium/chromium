// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_ZERO_STATE_SUGGESTIONS_PAGE_DATA_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_ZERO_STATE_SUGGESTIONS_PAGE_DATA_H_

#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/time/time.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "content/public/browser/page_user_data.h"

class OptimizationGuideKeyedService;

namespace content_extraction {
struct InnerTextResult;
}  // namespace content_extraction

namespace optimization_guide {
class ModelQualityLogEntry;
}  // namespace optimization_guide

namespace optimization_guide::proto {
class ZeroStateSuggestionsRequest;
}  // namespace optimization_guide::proto

namespace contextual_cueing {

using GlicSuggestionsCallbackList =
    base::OnceCallbackList<void(std::optional<std::vector<std::string>>)>;
using GlicSuggestionsCallback = GlicSuggestionsCallbackList::CallbackType;

// Processes zero state suggestions for GLIC, scoped to the given page.
class ZeroStateSuggestionsPageData
    : public content::PageUserData<ZeroStateSuggestionsPageData> {
 public:
  ZeroStateSuggestionsPageData(const ZeroStateSuggestionsPageData&) = delete;
  ZeroStateSuggestionsPageData& operator=(const ZeroStateSuggestionsPageData&) =
      delete;
  ~ZeroStateSuggestionsPageData() override;

  // Initiates page content extraction.
  void InitiatePageContentExtraction();

  // Explicitly fetch suggestions for this page.
  void FetchSuggestions(bool is_fre, GlicSuggestionsCallback callback);

 private:
  friend class content::PageUserData<ZeroStateSuggestionsPageData>;
  friend class ZeroStateSuggestionsPageDataTest;

  // Note that this constructor initiates extracting page content.
  explicit ZeroStateSuggestionsPageData(content::Page& page);

  // Returns the URL of the primary main frame associated with this page.
  const GURL GetUrl();

  // Called when inner text is extracted.
  void OnReceivedInnerText(
      std::unique_ptr<content_extraction::InnerTextResult> result);

  // Called when annotated page content is received.
  void OnReceivedAnnotatedPageContent(
      std::optional<optimization_guide::AIPageContentResult> content);

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

  // Send out suggestions request, if all necessary fetches are complete.
  void RequestSuggestionsIfComplete();

  // If `optimization_metadata_` contains everything necessary to determine a
  // suggestions result, run `suggestions_callbacks_` to return those
  // suggestions. This method itself also returns true if suggestions are sent
  // via the callbacks as a result of execution.
  bool ReturnSuggestionsFromOptimizationMetadataIfPossible();

  // Called when a zero state suggestions server response is received.
  void OnModelExecutionResponse(
      base::TimeTicks mes_begin_time,
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  // Tracks the status of fetches needed in `RequestSuggestionsIfComplete()`:
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
  std::optional<optimization_guide::AIPageContentResult>
      annotated_page_content_;
  bool optimization_metadata_done_ = false;
  optimization_guide::OptimizationGuideDecision optimization_decision_;
  optimization_guide::OptimizationMetadata optimization_metadata_;

  // Tracks the state for a request.
  base::TimeTicks begin_time_;
  std::optional<optimization_guide::proto::ZeroStateSuggestionsRequest>
      suggestions_request_;
  GlicSuggestionsCallbackList suggestions_callbacks_;

  std::optional<std::vector<std::string>> cached_suggestions_;

  // Not owned and guaranteed to outlive `this`.
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_keyed_service_ =
      nullptr;
  base::WeakPtrFactory<ZeroStateSuggestionsPageData> weak_ptr_factory_{this};

  PAGE_USER_DATA_KEY_DECL();
};

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_ZERO_STATE_SUGGESTIONS_PAGE_DATA_H_
