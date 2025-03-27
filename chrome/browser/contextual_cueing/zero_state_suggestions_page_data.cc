// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/zero_state_suggestions_page_data.h"

#include "chrome/browser/content_extraction/inner_text.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/proto/features/zero_state_suggestions.pb.h"

namespace contextual_cueing {

ZeroStateSuggestionsPageData::ZeroStateSuggestionsPageData(
    content::Page& page,
    const GURL& page_url,
    std::string page_title,
    OptimizationGuideKeyedService* ogks,
    GlicSuggestionsCallback suggestions_callback)
    : content::PageUserData<ZeroStateSuggestionsPageData>(page),
      optimization_guide_keyed_service_(ogks),
      suggestions_callback_(std::move(suggestions_callback)) {
  CHECK(base::FeatureList::IsEnabled(kGlicZeroStateSuggestions));

  std::unique_ptr<optimization_guide::proto::ZeroStateSuggestionsRequest>
      request = std::make_unique<
          optimization_guide::proto::ZeroStateSuggestionsRequest>();
  optimization_guide::proto::PageContext* page_context =
      request->mutable_page_context();
  if (!page_url.is_empty() && page_url.is_valid()) {
    page_context->set_url(page_url.spec());
  }
  page_context->set_title(page_title);

  content::RenderFrameHost& frame = page.GetMainDocument();
  content_extraction::GetInnerText(
      frame,
      /*node_id=*/std::nullopt,
      base::BindOnce(&ZeroStateSuggestionsPageData::OnReceivedInnerText,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now(),
                     std::move(request)));
}

ZeroStateSuggestionsPageData::~ZeroStateSuggestionsPageData() = default;

void ZeroStateSuggestionsPageData::OnReceivedInnerText(
    base::TimeTicks begin_time,
    std::unique_ptr<optimization_guide::proto::ZeroStateSuggestionsRequest>
        request,
    std::unique_ptr<content_extraction::InnerTextResult> result) {
  if (!result) {
    std::move(suggestions_callback_).Run(std::nullopt);
    return;
  }

  request->mutable_page_context()->set_inner_text(result->inner_text);
  optimization_guide_keyed_service_->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kZeroStateSuggestions,
      *request,
      /*execution_timeout=*/std::nullopt,
      base::BindOnce(&ZeroStateSuggestionsPageData::OnModelExecutionResponse,
                     weak_ptr_factory_.GetWeakPtr(), begin_time));
}

void ZeroStateSuggestionsPageData::OnModelExecutionResponse(
    base::TimeTicks begin_time,
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  base::TimeDelta suggestions_duration = base::TimeTicks::Now() - begin_time;
  if (!result.response.has_value()) {
    VLOG(3) << "ZeroStateSuggestionsPageData: Failed to get suggestions after "
            << suggestions_duration.InMilliseconds() << "ms. Error: "
            << static_cast<int>(result.response.error().error());
    std::move(suggestions_callback_).Run(std::nullopt);
    return;
  }

  VLOG(3) << "ZeroStateSuggestionsPageData: Received valid suggestions after "
          << suggestions_duration.InMilliseconds() << "ms.";

  std::optional<optimization_guide::proto::ZeroStateSuggestionsResponse>
      response = optimization_guide::ParsedAnyMetadata<
          optimization_guide::proto::ZeroStateSuggestionsResponse>(
          result.response.value());
  if (!response) {
    std::move(suggestions_callback_).Run(std::nullopt);
    return;
  }

  std::vector<std::string> suggestions;
  for (int i = 0; i < response->suggestions_size(); ++i) {
    suggestions.push_back(response->suggestions(i).label());
    VLOG(3) << "ZeroStateSuggestionsPageData: Suggestion " << (i + 1) << ": "
            << response->suggestions(i).label();
  }
  std::move(suggestions_callback_).Run(suggestions);
}

PAGE_USER_DATA_KEY_IMPL(ZeroStateSuggestionsPageData);

}  // namespace contextual_cueing
