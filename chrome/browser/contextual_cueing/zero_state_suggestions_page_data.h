// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_ZERO_STATE_SUGGESTIONS_PAGE_DATA_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_ZERO_STATE_SUGGESTIONS_PAGE_DATA_H_

#include "base/time/time.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "content/public/browser/page_user_data.h"

class OptimizationGuideKeyedService;

namespace optimization_guide {
class ModelQualityLogEntry;
}  // namespace optimization_guide

namespace optimization_guide::proto {
class ZeroStateSuggestionsRequest;
}  // namespace optimization_guide::proto

namespace content_extraction {
struct InnerTextResult;
}  // namespace content_extraction

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
                               const GURL& page_url,
                               std::string page_title,
                               OptimizationGuideKeyedService* ogks,
                               GlicSuggestionsCallback callback);

  // Called when inner text is extracted.
  void OnReceivedInnerText(
      base::TimeTicks begin_time,
      std::unique_ptr<optimization_guide::proto::ZeroStateSuggestionsRequest>
          request,
      std::unique_ptr<content_extraction::InnerTextResult> result);

  // Called when a zero state suggestions server response is received.
  void OnModelExecutionResponse(
      base::TimeTicks begin_time,
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  // Not owned and guaranteed to outlive `this`.
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_keyed_service_ =
      nullptr;
  GlicSuggestionsCallback suggestions_callback_;
  base::WeakPtrFactory<ZeroStateSuggestionsPageData> weak_ptr_factory_{this};

  PAGE_USER_DATA_KEY_DECL();
};

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_ZERO_STATE_SUGGESTIONS_PAGE_DATA_H_
