// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/zero_state_suggestions_request.h"

#include <string>
#include <vector>

#include "base/barrier_callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/contextual_cueing/zero_state_suggestions_page_data.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "content/public/browser/web_contents.h"

namespace contextual_cueing {

ZeroStateSuggestionsRequest::ZeroStateSuggestionsRequest(
    OptimizationGuideKeyedService* optimization_guide_keyed_service,
    const optimization_guide::proto::ZeroStateSuggestionsRequest&
        pending_base_request,
    const std::vector<content::WebContents*>& requested_tabs,
    const content::WebContents* focused_tab)
    : begin_time_(base::TimeTicks::Now()),
      pending_base_request_(pending_base_request),
      requested_tabs_(requested_tabs),
      optimization_guide_keyed_service_(optimization_guide_keyed_service) {
  MODEL_EXECUTION_LOG(base::StringPrintf(
      "ZeroStateSuggestionsRequest: Creating new zero state suggestions "
      "request for %llu tabs",
      requested_tabs.size()));
  auto barrier_callback = base::BarrierCallback<
      base::expected<optimization_guide::proto::ZeroStatePageContext,
                     PageContextIneligibilityType>>(
      requested_tabs.size(),
      base::BindOnce(&ZeroStateSuggestionsRequest::OnAllPageContextExtracted,
                     weak_ptr_factory_.GetWeakPtr()));

  for (auto* tab : requested_tabs) {
    auto* zss_data =
        ZeroStateSuggestionsPageData::GetOrCreateForPage(tab->GetPrimaryPage());

    // Capture the page data for the focused tab to cache suggestions into if we
    // are in that mode.
    if (pending_base_request_.has_page_context()) {
      CHECK_EQ(requested_tabs.size(), 1u);
      focused_tab_page_data_ = zss_data->AsWeakPtr();
    }

    // Do not fetch page context if there are already cached suggestions for the
    // focused tab and we are in focused tab mode.
    if (focused_tab_page_data_ &&
        focused_tab_page_data_->cached_suggestions_for_focused_tab()) {
      return;
    }

    // If we're in multitab mode, store the information about focused tab.
    if (focused_tab && tab == focused_tab) {
      zss_data->set_is_focused_tab(true);
    } else {
      zss_data->set_is_focused_tab(false);
    }
    // Otherwise, start grabbing the page context.
    zss_data->GetPageContext(barrier_callback);
  }
}

ZeroStateSuggestionsRequest::~ZeroStateSuggestionsRequest() {
  MODEL_EXECUTION_LOG(
      "ZeroStateSuggestionsRequest: Destructing zero state suggestions "
      "request");
}

// static
void ZeroStateSuggestionsRequest::Destroy(
    std::unique_ptr<ZeroStateSuggestionsRequest> request) {
  // The unique_ptr deletes automatically.
}

void ZeroStateSuggestionsRequest::AddCallback(
    base::OnceCallback<void(std::vector<std::string>)> callback) {
  // Check if we have cached suggestions if we are in focused tab mode.
  if (focused_tab_page_data_) {
    if (auto cached_suggestions =
            focused_tab_page_data_->cached_suggestions_for_focused_tab()) {
      // Post cached suggestions to back of UI thread.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), *cached_suggestions));
      return;
    }
  }

  pending_callbacks_.AddUnsafe(std::move(callback));
}

std::vector<content::WebContents*>
ZeroStateSuggestionsRequest::GetRequestedTabs() const {
  return requested_tabs_;
}

void ZeroStateSuggestionsRequest::OnAllPageContextExtracted(
    const std::vector<
        base::expected<optimization_guide::proto::ZeroStatePageContext,
                       PageContextIneligibilityType>>&
        zero_state_page_contexts) {
  // Filter for page contexts that are available.
  std::vector<optimization_guide::proto::ZeroStatePageContext>
      filtered_page_contexts;
  PageContextIneligibilityType latest_ineligibility_type =
      PageContextIneligibilityType::kNone;
  for (const auto& zero_state_page_context : zero_state_page_contexts) {
    if (zero_state_page_context.has_value()) {
      filtered_page_contexts.push_back(*zero_state_page_context);
    } else {
      latest_ineligibility_type = zero_state_page_context.error();
    }
  }

  std::string engagement_type =
      pending_base_request_.is_fre() ? "FRE" : "Reengagement";
  base::UmaHistogramEnumeration(
      "ContextualCueing.GlicSuggestions.PageContextIneligibilityReason",
      latest_ineligibility_type);
  base::UmaHistogramEnumeration(
      "ContextualCueing.GlicSuggestions.PageContextIneligibilityReason." +
          engagement_type,
      latest_ineligibility_type);

  // No content to generate suggestions. Return empty.
  if (filtered_page_contexts.empty()) {
    MODEL_EXECUTION_LOG(
        "ZeroStateSuggestionsRequest: No page context to fetch suggestions "
        "for.");
    CacheFocusedTabSuggestions({});
    pending_callbacks_.Notify(std::vector<std::string>({}));
    return;
  }

  // Add page context to request.
  if (pending_base_request_.has_page_context()) {
    CHECK_EQ(filtered_page_contexts.size(), 1u);
    *pending_base_request_.mutable_page_context() =
        filtered_page_contexts.front().page_context();
  } else if (pending_base_request_.has_page_context_list()) {
    pending_base_request_.mutable_page_context()->Clear();
    *pending_base_request_.mutable_page_context_list()
         ->mutable_page_contexts() = {filtered_page_contexts.begin(),
                                      filtered_page_contexts.end()};
  }

  // Initiate model execution fetch.
  MODEL_EXECUTION_LOG(base::StringPrintf(
      "ZeroStateSuggestionsRequest: Starting fetch for "
      "suggestions. Is-mulitab request: %s",
      pending_base_request_.has_page_context_list() ? "true" : "false"));
  optimization_guide_keyed_service_->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kZeroStateSuggestions,
      pending_base_request_,
      /*options=*/{},
      base::BindOnce(&ZeroStateSuggestionsRequest::OnModelExecutionResponse,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

void ZeroStateSuggestionsRequest::OnModelExecutionResponse(
    base::TimeTicks mes_begin_time,
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  base::UmaHistogramTimes("ContextualCueing.GlicSuggestions.MesFetchLatency",
                          base::TimeTicks::Now() - mes_begin_time);

  base::TimeDelta suggestions_duration = base::TimeTicks::Now() - begin_time_;
  if (!result.response.has_value()) {
    MODEL_EXECUTION_LOG(
        base::StringPrintf("ZeroStateSuggestionsRequest: Failed to get "
                           "suggestions after %ld ms. Error: %d",
                           suggestions_duration.InMilliseconds(),
                           static_cast<int>(result.response.error().error())));

    pending_callbacks_.Notify(std::vector<std::string>({}));
    CacheFocusedTabSuggestions({});
    return;
  }

  MODEL_EXECUTION_LOG(
      base::StringPrintf("ZeroStateSuggestionsRequest: Received valid "
                         "suggestions after %ld ms.",
                         suggestions_duration.InMilliseconds()));

  std::optional<optimization_guide::proto::ZeroStateSuggestionsResponse>
      response = optimization_guide::ParsedAnyMetadata<
          optimization_guide::proto::ZeroStateSuggestionsResponse>(
          result.response.value());
  if (!response) {
    MODEL_EXECUTION_LOG("ZeroStateSuggestionsRequest: No response available.");
    pending_callbacks_.Notify(std::vector<std::string>({}));
    // Treat this as a transient error that server returned bad data
    // momentarily. Do not cache.
    return;
  }

  std::vector<std::string> suggestions;
  for (int i = 0; i < response->suggestions_size(); ++i) {
    suggestions.push_back(response->suggestions(i).label());
    MODEL_EXECUTION_LOG(
        base::StringPrintf("ZeroStateSuggestionsRequest: Suggestion %d: %s",
                           i + 1, response->suggestions(i).label()));
  }
  CacheFocusedTabSuggestions(suggestions);
  pending_callbacks_.Notify(suggestions);
}

void ZeroStateSuggestionsRequest::CacheFocusedTabSuggestions(
    const std::vector<std::string>& suggestions_to_cache) {
  if (!focused_tab_page_data_) {
    return;
  }

  focused_tab_page_data_->set_cached_suggestions_for_focused_tab(
      suggestions_to_cache);
}

base::WeakPtr<ZeroStateSuggestionsRequest>
ZeroStateSuggestionsRequest::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace contextual_cueing
