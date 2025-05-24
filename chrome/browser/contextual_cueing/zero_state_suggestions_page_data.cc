// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/zero_state_suggestions_page_data.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/content_extraction/inner_text.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_helper.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/content/browser/page_context_eligibility.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/optimization_guide_common.mojom.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_permissions_util.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/contextual_cueing_metadata.pb.h"
#include "components/optimization_guide/proto/features/zero_state_suggestions.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "content/public/browser/web_contents.h"

namespace {

// Parse the given metadata and return a std::pair containing:
// 1. bool: true if eligible for contextual suggestions.
// 2. std::vector<std::string>: suggestions in the metadata, if present.
std::pair<bool, std::vector<std::string>> ParseOptimizationMetadata(
    optimization_guide::OptimizationGuideDecision decision,
    optimization_guide::OptimizationMetadata& metadata) {
  if (decision != optimization_guide::OptimizationGuideDecision::kTrue) {
    return std::make_pair(true, std::vector<std::string>());
  }

  auto suggestions_metadata = metadata.ParsedMetadata<
      optimization_guide::proto::GlicZeroStateSuggestionsMetadata>();
  if (!suggestions_metadata.has_value()) {
    return std::make_pair(true, std::vector<std::string>());
  }
  if (!suggestions_metadata->contextual_suggestions_eligible()) {
    return std::make_pair(false, std::vector<std::string>());
  }
  if (!suggestions_metadata->contextual_suggestions().empty()) {
    return std::make_pair(
        true, std::vector<std::string>(
                  suggestions_metadata->contextual_suggestions().begin(),
                  suggestions_metadata->contextual_suggestions().end()));
  }

  return std::make_pair(true, std::vector<std::string>());
}

}  // namespace

namespace contextual_cueing {

ZeroStateSuggestionsPageData::ZeroStateSuggestionsPageData(content::Page& page)
    : content::PageUserData<ZeroStateSuggestionsPageData>(page) {
  CHECK(base::FeatureList::IsEnabled(kGlicZeroStateSuggestions));
  CHECK(kExtractInnerTextForZeroStateSuggestions.Get() ||
        kExtractAnnotatedPageContentForZeroStateSuggestions.Get());

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&(page.GetMainDocument()));
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  optimization_guide_keyed_service_ =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);

  OPTIMIZATION_GUIDE_LOG(
      optimization_guide_common::mojom::LogSource::MODEL_EXECUTION,
      optimization_guide_keyed_service_->GetOptimizationGuideLogger(),
      base::StringPrintf(
          "ZeroStateSuggestionsPageData: Creating page data for %s.",
          web_contents->GetLastCommittedURL().spec()));

  base::TimeDelta initiate_page_content_extraction_delay;
  if (auto* helper = ContextualCueingHelper::FromWebContents(web_contents)) {
    std::optional<base::TimeTicks> last_same_doc_navigation_time =
        helper->last_same_doc_navigation_committed();
    if (last_same_doc_navigation_time) {
      if (kReturnEmptyForSameDocumentNavigation.Get()) {
        cached_suggestions_ = std::make_optional(std::vector<std::string>({}));
        return;
      }

      initiate_page_content_extraction_delay =
          (kPageContentExtractionDelayForSameDocumentNavigation.Get() +
           *last_same_doc_navigation_time) -
          base::TimeTicks::Now();
    }
  }
  if (initiate_page_content_extraction_delay.is_positive()) {
    LOCAL_HISTOGRAM_BOOLEAN(
        "ContextualCueing.ZeroStateSuggestions.ContentExtractionSameDocDelay",
        true);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &ZeroStateSuggestionsPageData::InitiatePageContentExtraction,
            weak_ptr_factory_.GetWeakPtr()),
        initiate_page_content_extraction_delay);
  } else {
    InitiatePageContentExtraction();
  }
}

ZeroStateSuggestionsPageData::~ZeroStateSuggestionsPageData() {
  OPTIMIZATION_GUIDE_LOG(
      optimization_guide_common::mojom::LogSource::MODEL_EXECUTION,
      optimization_guide_keyed_service_->GetOptimizationGuideLogger(),
      base::StringPrintf(
          "ZeroStateSuggestionsPageData: Destructing page data for %s.",
          GetUrl().spec()));
}

void ZeroStateSuggestionsPageData::InitiatePageContentExtraction() {
  const GURL url = GetUrl();

  if (content_extraction_initiated_) {
    // Do not re-fetch content.
    OPTIMIZATION_GUIDE_LOG(
        optimization_guide_common::mojom::LogSource::MODEL_EXECUTION,
        optimization_guide_keyed_service_->GetOptimizationGuideLogger(),
        base::StringPrintf("ZeroStateSuggestionsPageData: Content extraction "
                           "already initiated for %s. Not trying again",
                           url.spec()));
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&(page().GetMainDocument()));
  bool has_first_contentful_paint = false;
  if (auto* helper = ContextualCueingHelper::FromWebContents(web_contents)) {
    has_first_contentful_paint = helper->has_first_contentful_paint();
  }
  if (!has_first_contentful_paint &&
      !page().GetMainDocument().IsDocumentOnLoadCompletedInMainFrame()) {
    LOCAL_HISTOGRAM_BOOLEAN(
        "ContextualCueing.ZeroStateSuggestions.ContentExtractionWait", true);
    // Wait for signal from tab helper to initiate content extraction if not
    // loaded yet.

    OPTIMIZATION_GUIDE_LOG(
        optimization_guide_common::mojom::LogSource::MODEL_EXECUTION,
        optimization_guide_keyed_service_->GetOptimizationGuideLogger(),
        base::StringPrintf("ZeroStateSuggestionsPageData: Page not "
                           "sufficiently loaded for %s. Waiting until ready",
                           url.spec()));
    return;
  }
  content_extraction_initiated_ = true;
  page_context_begin_time_ = base::TimeTicks::Now();

  OPTIMIZATION_GUIDE_LOG(
      optimization_guide_common::mojom::LogSource::MODEL_EXECUTION,
      optimization_guide_keyed_service_->GetOptimizationGuideLogger(),
      base::StringPrintf("ZeroStateSuggestionsPageData: Initiating page "
                         "content extraction for %s.",
                         url.spec()));

  if (kExtractAnnotatedPageContentForZeroStateSuggestions.Get()) {
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
  } else {
    OnReceivedAnnotatedPageContent(/*content=*/std::nullopt);
  }

  if (kExtractInnerTextForZeroStateSuggestions.Get()) {
    // TODO(crbug.com/407121627): remove inner text fetch once server is ready
    // to take annotated page content.
    content::RenderFrameHost& frame = page().GetMainDocument();
    content_extraction::GetInnerText(
        frame,
        /*node_id=*/std::nullopt,
        base::BindOnce(&ZeroStateSuggestionsPageData::OnReceivedInnerText,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    OnReceivedInnerText(/*result=*/nullptr);
  }

  OPTIMIZATION_GUIDE_LOG(
      optimization_guide_common::mojom::LogSource::MODEL_EXECUTION,
      optimization_guide_keyed_service_->GetOptimizationGuideLogger(),
      base::StringPrintf("ZeroStateSuggestionsPageData: Starting request for "
                         "optimization metadata for %s.",
                         url.spec()));
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  bool can_request_metadata =
      optimization_guide::IsUserPermittedToFetchFromRemoteOptimizationGuide(
          profile->IsOffTheRecord(), profile->GetPrefs());
  if (can_request_metadata) {
    optimization_guide_keyed_service_->CanApplyOptimization(
        web_contents->GetLastCommittedURL(),
        optimization_guide::proto::GLIC_ZERO_STATE_SUGGESTIONS,
        base::BindOnce(
            &ZeroStateSuggestionsPageData::OnReceivedOptimizationMetadata,
            weak_ptr_factory_.GetWeakPtr()));
  } else {
    optimization_guide_keyed_service_->CanApplyOptimizationOnDemand(
        {web_contents->GetLastCommittedURL()},
        {optimization_guide::proto::GLIC_ZERO_STATE_SUGGESTIONS},
        optimization_guide::proto::RequestContext::
            CONTEXT_GLIC_ZERO_STATE_SUGGESTIONS,
        base::BindRepeating(&ZeroStateSuggestionsPageData::
                                OnReceivedOptimizationMetadataOnDemand,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

void ZeroStateSuggestionsPageData::FetchSuggestions(
    bool is_fre,
    GlicSuggestionsCallback callback) {
  if (cached_suggestions_) {
    OPTIMIZATION_GUIDE_LOG(
        optimization_guide_common::mojom::LogSource::MODEL_EXECUTION,
        optimization_guide_keyed_service_->GetOptimizationGuideLogger(),
        base::StringPrintf("ZeroStateSuggestionsPageData: Returning cached "
                           "suggestions for %s.",
                           GetUrl().spec()));
    std::move(callback).Run(cached_suggestions_->empty()
                                ? std::nullopt
                                : std::make_optional(*cached_suggestions_));
    return;
  }

  begin_time_ = base::TimeTicks::Now();

  // Request for page already in flight - just notify when it comes back.
  if (suggestions_request_) {
    suggestions_callbacks_.AddUnsafe(std::move(callback));
    return;
  }

  suggestions_request_ = optimization_guide::proto::
      ZeroStateSuggestionsRequest::default_instance();
  suggestions_request_->set_is_fre(is_fre);
  suggestions_callbacks_.AddUnsafe(std::move(callback));
  RequestSuggestionsIfComplete();
}

void ZeroStateSuggestionsPageData::OnReceivedAnnotatedPageContent(
    std::optional<optimization_guide::AIPageContentResult> content) {
  GURL url = GetUrl();
  auto* pce = optimization_guide::PageContextEligibility::Get();
  bool is_eligible =
      content &&
      (!pce || pce->api().IsPageContextEligible(
                   url.host(), url.path(),
                   optimization_guide::GetFrameMetadataFromPageContent(
                       content.value())));

  if (is_eligible) {
    annotated_page_content_ = std::move(content);
    base::UmaHistogramTimes(
        "ContextualCueing.GlicSuggestions.PageContextFetchlatency."
        "AnnotatedPageContent",
        base::TimeTicks::Now() - page_context_begin_time_);
  } else {
    annotated_page_content_ = std::nullopt;
  }
  annotated_page_content_done_ = true;
  RequestSuggestionsIfComplete();
}

void ZeroStateSuggestionsPageData::OnReceivedInnerText(
    std::unique_ptr<content_extraction::InnerTextResult> result) {
  inner_text_result_ = std::move(result);
  inner_text_done_ = true;
  if (inner_text_result_) {
    base::UmaHistogramTimes(
        "ContextualCueing.GlicSuggestions.PageContextFetchlatency.InnerText",
        base::TimeTicks::Now() - page_context_begin_time_);
  }
  RequestSuggestionsIfComplete();
}

void ZeroStateSuggestionsPageData::OnReceivedOptimizationMetadataOnDemand(
    const GURL& url,
    const base::flat_map<
        optimization_guide::proto::OptimizationType,
        optimization_guide::OptimizationGuideDecisionWithMetadata>& decisions) {
  auto it =
      decisions.find(optimization_guide::proto::GLIC_ZERO_STATE_SUGGESTIONS);
  if (it == decisions.end()) {
    // If not found, treat it as no metadata.
    OnReceivedOptimizationMetadata(
        optimization_guide::OptimizationGuideDecision::kFalse, {});
  } else {
    OnReceivedOptimizationMetadata(it->second.decision, it->second.metadata);
  }
}

void ZeroStateSuggestionsPageData::OnReceivedOptimizationMetadata(
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  optimization_metadata_done_ = true;
  optimization_decision_ = decision;
  optimization_metadata_ = metadata;

  RequestSuggestionsIfComplete();
}

bool ZeroStateSuggestionsPageData::
    ReturnSuggestionsFromOptimizationMetadataIfPossible() {
  if (!optimization_metadata_done_) {
    OPTIMIZATION_GUIDE_LOG(
        optimization_guide_common::mojom::LogSource::MODEL_EXECUTION,
        optimization_guide_keyed_service_->GetOptimizationGuideLogger(),
        base::StringPrintf(
            "ZeroStateSuggestionsPageData: Waiting on "
            "optimization metadata for %s. Not returning suggestions yet.",
            GetUrl().spec()));
    return false;
  }

  std::pair<bool, std::vector<std::string>> pair =
      ParseOptimizationMetadata(optimization_decision_, optimization_metadata_);
  if (!pair.first) {
    suggestions_callbacks_.Notify(std::nullopt);
    cached_suggestions_ = std::make_optional(std::vector<std::string>({}));
    return true;
  }
  if (!pair.second.empty()) {
    suggestions_callbacks_.Notify(pair.second);
    cached_suggestions_ = pair.second;
    return true;
  }
  return false;
}

void ZeroStateSuggestionsPageData::RequestSuggestionsIfComplete() {
  bool work_done = inner_text_done_ && annotated_page_content_done_ &&
                   optimization_metadata_done_;
  bool has_page_context = inner_text_result_ || annotated_page_content_;
  if (!work_done) {
    return;
  }

  if (has_page_context && !page_context_duration_logged_) {
    page_context_duration_logged_ = true;
    base::UmaHistogramTimes(
        "ContextualCueing.GlicSuggestions.PageContextFetchlatency.Total",
        base::TimeTicks::Now() - page_context_begin_time_);
    LOCAL_HISTOGRAM_BOOLEAN(
        "ContextualCueing.ZeroStateSuggestions.ContextExtractionDone", true);
  }

  if (!suggestions_request_) {
    return;
  }

  const GURL url = GetUrl();
  if (ReturnSuggestionsFromOptimizationMetadataIfPossible()) {
    OPTIMIZATION_GUIDE_LOG(
        optimization_guide_common::mojom::LogSource::MODEL_EXECUTION,
        optimization_guide_keyed_service_->GetOptimizationGuideLogger(),
        base::StringPrintf("ZeroStateSuggestionsPageData: Suggestions for %s "
                           "returned from optimization metadata.",
                           url.spec()));
    return;
  }

  if (!has_page_context) {
    OPTIMIZATION_GUIDE_LOG(
        optimization_guide_common::mojom::LogSource::MODEL_EXECUTION,
        optimization_guide_keyed_service_->GetOptimizationGuideLogger(),
        base::StringPrintf(
            "ZeroStateSuggestionsPageData: No page content for %s "
            ". Returning no suggestions available",
            url.spec()));
    suggestions_callbacks_.Notify(std::nullopt);
    cached_suggestions_ = std::make_optional(std::vector<std::string>({}));
    return;
  }

  OPTIMIZATION_GUIDE_LOG(
      optimization_guide_common::mojom::LogSource::MODEL_EXECUTION,
      optimization_guide_keyed_service_->GetOptimizationGuideLogger(),
      base::StringPrintf("ZeroStateSuggestionsPageData: Starting fetch for "
                         "suggestions for %s.",
                         url.spec()));

  optimization_guide::proto::PageContext* page_context =
      suggestions_request_->mutable_page_context();
  if (!url.is_empty() && url.is_valid()) {
    page_context->set_url(url.spec());
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&(page().GetMainDocument()));
  page_context->set_title(base::UTF16ToUTF8(web_contents->GetTitle()));

  if (annotated_page_content_) {
    *page_context->mutable_annotated_page_content() =
        annotated_page_content_->proto;
  }
  if (inner_text_result_) {
    page_context->set_inner_text(inner_text_result_->inner_text);
  }

  optimization_guide_keyed_service_->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kZeroStateSuggestions,
      *suggestions_request_,
      /*execution_timeout=*/std::nullopt,
      base::BindOnce(&ZeroStateSuggestionsPageData::OnModelExecutionResponse,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

void ZeroStateSuggestionsPageData::OnModelExecutionResponse(
    base::TimeTicks mes_begin_time,
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  base::UmaHistogramTimes("ContextualCueing.GlicSuggestions.MesFetchLatency",
                          base::TimeTicks::Now() - mes_begin_time);
  const GURL url = GetUrl();

  // Clear out suggestions request as it's been fulfilled.
  suggestions_request_ = std::nullopt;

  base::TimeDelta suggestions_duration = base::TimeTicks::Now() - begin_time_;
  if (!result.response.has_value()) {
    OPTIMIZATION_GUIDE_LOG(
        optimization_guide_common::mojom::LogSource::MODEL_EXECUTION,
        optimization_guide_keyed_service_->GetOptimizationGuideLogger(),
        base::StringPrintf("ZeroStateSuggestionsPageData: Failed to get "
                           "suggestions for %s after %ld ms. Error: %d",
                           url.spec(), suggestions_duration.InMilliseconds(),
                           static_cast<int>(result.response.error().error())));
    suggestions_callbacks_.Notify(std::nullopt);

    if (!result.response.error().transient()) {
      // Cache empty suggestions if error is not transient.
      cached_suggestions_ = std::make_optional(std::vector<std::string>({}));
    }

    return;
  }

  OPTIMIZATION_GUIDE_LOG(
      optimization_guide_common::mojom::LogSource::MODEL_EXECUTION,
      optimization_guide_keyed_service_->GetOptimizationGuideLogger(),
      base::StringPrintf("ZeroStateSuggestionsPageData: Received valid "
                         "suggestions for %s after %ld ms.",
                         url.spec(), suggestions_duration.InMilliseconds()));

  std::optional<optimization_guide::proto::ZeroStateSuggestionsResponse>
      response = optimization_guide::ParsedAnyMetadata<
          optimization_guide::proto::ZeroStateSuggestionsResponse>(
          result.response.value());
  if (!response) {
    OPTIMIZATION_GUIDE_LOG(
        optimization_guide_common::mojom::LogSource::MODEL_EXECUTION,
        optimization_guide_keyed_service_->GetOptimizationGuideLogger(),
        base::StringPrintf(
            "ZeroStateSuggestionsPageData: No response available for %s.",
            url.spec()));
    suggestions_callbacks_.Notify(std::nullopt);
    // Treat this as a transient error that server returned bad data
    // momentarily.
    return;
  }

  std::vector<std::string> suggestions;
  for (int i = 0; i < response->suggestions_size(); ++i) {
    suggestions.push_back(response->suggestions(i).label());
    OPTIMIZATION_GUIDE_LOG(
        optimization_guide_common::mojom::LogSource::MODEL_EXECUTION,
        optimization_guide_keyed_service_->GetOptimizationGuideLogger(),
        base::StringPrintf("ZeroStateSuggestionsPageData: Suggestion %d: %s",
                           i + 1, response->suggestions(i).label()));
  }
  suggestions_callbacks_.Notify(suggestions);
  cached_suggestions_ = suggestions;
}

const GURL ZeroStateSuggestionsPageData::GetUrl() {
  return content::WebContents::FromRenderFrameHost(&(page().GetMainDocument()))
      ->GetLastCommittedURL();
}

PAGE_USER_DATA_KEY_IMPL(ZeroStateSuggestionsPageData);

}  // namespace contextual_cueing
