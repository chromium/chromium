// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_ZERO_STATE_SUGGESTIONS_PAGE_DATA_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_ZERO_STATE_SUGGESTIONS_PAGE_DATA_H_

#include "base/time/time.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "content/public/browser/page_user_data.h"

class OptimizationGuideKeyedService;

namespace content {
class WebContents;
}  // namespace content

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

using GlicSuggestionsCallback =
    base::OnceCallback<void(std::optional<std::vector<std::string>>)>;

// Processes zero state suggestions for GLIC, scoped to the given page.
class ZeroStateSuggestionsPageData
    : public content::PageUserData<ZeroStateSuggestionsPageData> {
 public:
  ZeroStateSuggestionsPageData(const ZeroStateSuggestionsPageData&) = delete;
  ZeroStateSuggestionsPageData& operator=(const ZeroStateSuggestionsPageData&) =
      delete;
  ~ZeroStateSuggestionsPageData() override;

 private:
  friend class content::PageUserData<ZeroStateSuggestionsPageData>;
  friend class ZeroStateSuggestionsPageDataTest;

  ZeroStateSuggestionsPageData(content::Page& page,
                               content::WebContents* web_contents,
                               OptimizationGuideKeyedService* ogks,
                               bool is_fre,
                               GlicSuggestionsCallback callback);

  // Called when inner text is extracted.
  void OnReceivedInnerText(
      std::unique_ptr<content_extraction::InnerTextResult> result);

  // Called when annotated page content is received.
  void OnReceivedAnnotatedPageContent(
      std::optional<optimization_guide::AIPageContentResult> content);

  // Send out suggestions request, if all necessary fetches are complete.
  void RequestSuggestionsIfComplete();

  // Called when a zero state suggestions server response is received.
  void OnModelExecutionResponse(
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  // Tracks the status of inner text and annotated page content fetches.
  bool inner_text_done_ = false;
  std::unique_ptr<content_extraction::InnerTextResult> inner_text_result_;
  bool annotated_page_content_done_ = false;
  std::optional<optimization_guide::AIPageContentResult>
      annotated_page_content_;
  optimization_guide::proto::ZeroStateSuggestionsRequest suggestions_request_;

  // Timestamp of when `this` is created, i.e. before any fetch or request
  // is sent.
  base::TimeTicks begin_time_;

  // Not owned and guaranteed to outlive `this`.
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_keyed_service_ =
      nullptr;
  GlicSuggestionsCallback suggestions_callback_;
  base::WeakPtrFactory<ZeroStateSuggestionsPageData> weak_ptr_factory_{this};

  PAGE_USER_DATA_KEY_DECL();
};

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_ZERO_STATE_SUGGESTIONS_PAGE_DATA_H_
