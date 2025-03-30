// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/zero_state_suggestions_page_data.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/content_extraction/inner_text.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/proto/features/zero_state_suggestions.pb.h"
#include "content/public/browser/web_contents.h"

namespace contextual_cueing {

ZeroStateSuggestionsPageData::ZeroStateSuggestionsPageData(
    content::Page& page,
    content::WebContents* web_contents,
    OptimizationGuideKeyedService* ogks,
    bool is_fre,
    GlicSuggestionsCallback suggestions_callback)
    : content::PageUserData<ZeroStateSuggestionsPageData>(page),
      begin_time_(base::TimeTicks::Now()),
      optimization_guide_keyed_service_(ogks),
      suggestions_callback_(std::move(suggestions_callback)) {
  CHECK(base::FeatureList::IsEnabled(kGlicZeroStateSuggestions));

  suggestions_request_.set_is_fre(is_fre);
  optimization_guide::proto::PageContext* page_context =
      suggestions_request_.mutable_page_context();
  const GURL& page_url = web_contents->GetLastCommittedURL();
  if (!page_url.is_empty() && page_url.is_valid()) {
    page_context->set_url(page_url.spec());
  }
  page_context->set_title(base::UTF16ToUTF8(web_contents->GetTitle()));

  blink::mojom::AIPageContentOptionsPtr ai_page_content_options;
  ai_page_content_options = optimization_guide::DefaultAIPageContentOptions();
  ai_page_content_options->include_geometry = false;
  ai_page_content_options->on_critical_path = true;
  ai_page_content_options->include_hidden_searchable_content = false;
  optimization_guide::GetAIPageContent(
      web_contents, std::move(ai_page_content_options),
      base::BindOnce(
          &ZeroStateSuggestionsPageData::OnReceivedAnnotatedPageContent,
          weak_ptr_factory_.GetWeakPtr()));

  // TODO(crbug.com/407121627): remove inner text fetch once server is ready to
  // take annotated page content.
  content::RenderFrameHost& frame = page.GetMainDocument();
  content_extraction::GetInnerText(
      frame,
      /*node_id=*/std::nullopt,
      base::BindOnce(&ZeroStateSuggestionsPageData::OnReceivedInnerText,
                     weak_ptr_factory_.GetWeakPtr()));
}

ZeroStateSuggestionsPageData::~ZeroStateSuggestionsPageData() = default;

void ZeroStateSuggestionsPageData::OnReceivedAnnotatedPageContent(
    std::optional<optimization_guide::AIPageContentResult> content) {
  annotated_page_content_ = std::move(content);
  annotated_page_content_done_ = true;
  RequestSuggestionsIfComplete();
}

void ZeroStateSuggestionsPageData::OnReceivedInnerText(
    std::unique_ptr<content_extraction::InnerTextResult> result) {
  inner_text_result_ = std::move(result);
  inner_text_done_ = true;
  RequestSuggestionsIfComplete();
}

void ZeroStateSuggestionsPageData::RequestSuggestionsIfComplete() {
  bool work_done = inner_text_done_ && annotated_page_content_done_;
  bool has_page_context = inner_text_result_ || annotated_page_content_;
  if (!work_done) {
    return;
  }
  if (!has_page_context) {
    std::move(suggestions_callback_).Run(std::nullopt);
    return;
  }

  if (annotated_page_content_) {
    *suggestions_request_.mutable_page_context()
         ->mutable_annotated_page_content() = annotated_page_content_->proto;
  }
  if (inner_text_result_) {
    suggestions_request_.mutable_page_context()->set_inner_text(
        inner_text_result_->inner_text);
  }

  optimization_guide_keyed_service_->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kZeroStateSuggestions,
      suggestions_request_,
      /*execution_timeout=*/std::nullopt,
      base::BindOnce(&ZeroStateSuggestionsPageData::OnModelExecutionResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ZeroStateSuggestionsPageData::OnModelExecutionResponse(
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  base::TimeDelta suggestions_duration = base::TimeTicks::Now() - begin_time_;
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
