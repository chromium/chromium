// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_ZERO_STATE_SUGGESTIONS_REQUEST_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_ZERO_STATE_SUGGESTIONS_REQUEST_H_

#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/optimization_guide/proto/features/zero_state_suggestions.pb.h"

class OptimizationGuideKeyedService;

namespace content {
class WebContents;
}  // namespace content

namespace optimization_guide {
class ModelQualityLogEntry;
struct OptimizationGuideModelExecutionResult;
}  // namespace optimization_guide

namespace contextual_cueing {

enum class PageContextIneligibilityType;
class ZeroStateSuggestionsPageData;

// Encapsulates logic for a single zero-state suggestions request.
class ZeroStateSuggestionsRequest {
 public:
  ZeroStateSuggestionsRequest(
      OptimizationGuideKeyedService* optimization_guide_keyed_service,
      const optimization_guide::proto::ZeroStateSuggestionsRequest&
          pending_base_request,
      const std::vector<content::WebContents*>& requested_tabs,
      const content::WebContents* focused_tab);

  ~ZeroStateSuggestionsRequest();

  static void Destroy(std::unique_ptr<ZeroStateSuggestionsRequest>);

  // Adds a callback for this pending request that gets invoked when suggestions
  // have been returned.
  void AddCallback(base::OnceCallback<void(std::vector<std::string>)>);

  // Returns the tabs that were requested to get suggestions for.
  std::vector<content::WebContents*> GetRequestedTabs() const;

  base::WeakPtr<ZeroStateSuggestionsRequest> AsWeakPtr();

 private:
  friend class ContextualCueingServiceTestZeroStateSuggestions;

  // Callback invoked when all requested zero state page contexts have been
  // extracted.
  void OnAllPageContextExtracted(
      const std::vector<
          base::expected<optimization_guide::proto::ZeroStatePageContext,
                         PageContextIneligibilityType>>&
          zero_state_page_contexts);

  // Callback invoked when model execution has completed.
  void OnModelExecutionResponse(
      base::TimeTicks mes_begin_time,
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  // Caches `suggestions_to_cache` in `focused_tab_page_data_` if the request is
  // for focused tab suggestions.
  void CacheFocusedTabSuggestions(
      const std::vector<std::string>& suggestions_to_cache);

  // The time when this request was initiated.
  base::TimeTicks begin_time_;

  // The request proto to build off of to ultimately send for model execution.
  optimization_guide::proto::ZeroStateSuggestionsRequest pending_base_request_;

  // The list of callbacks to invoke when model execution has completed.
  base::OnceCallbackList<void(std::vector<std::string>)> pending_callbacks_;

  // Weak pointer to focused tab page data to cache suggestions later.
  base::WeakPtr<ZeroStateSuggestionsPageData> focused_tab_page_data_;

  // The tabs requested to get suggestions for.
  std::vector<content::WebContents*> requested_tabs_;

  // Not owned. Guaranteed to outlive `this`.
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_keyed_service_;

  base::WeakPtrFactory<ZeroStateSuggestionsRequest> weak_ptr_factory_{this};
};

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_ZERO_STATE_SUGGESTIONS_REQUEST_H_
