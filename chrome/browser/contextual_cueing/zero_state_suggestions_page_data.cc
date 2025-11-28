// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/zero_state_suggestions_page_data.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_helper.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_types.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_extraction/content/browser/inner_text.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
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
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"

namespace {

// Returns true if eligible for contextual suggestions.
bool IsEligibleForContextualSuggestions(
    optimization_guide::OptimizationGuideDecision decision,
    optimization_guide::OptimizationMetadata& metadata) {
  if (decision != optimization_guide::OptimizationGuideDecision::kTrue) {
    return true;
  }

  auto suggestions_metadata = metadata.ParsedMetadata<
      optimization_guide::proto::GlicZeroStateSuggestionsMetadata>();
  if (!suggestions_metadata) {
    return true;
  }
  return suggestions_metadata->contextual_suggestions_eligible();
}

void GetEligibilityAndRunCallback(
    const GURL& url,
    optimization_guide::PageContextEligibility* page_context_eligibility,
    base::OnceCallback<
        void(std::optional<optimization_guide::proto::AnnotatedPageContent>)>
        callback,
    optimization_guide::AIPageContentResultOrError content) {
  bool is_eligible =
      content.has_value() &&
      (!page_context_eligibility ||
       optimization_guide::IsPageContextEligible(
           url.GetHost(), url.GetPath(),
           optimization_guide::GetFrameMetadataFromPageContent(*content),
           page_context_eligibility));
  std::move(callback).Run(is_eligible ? std::make_optional(content->proto)
                                      : std::nullopt);
}

}  // namespace

namespace contextual_cueing {

ZeroStateSuggestionsPageData::ZeroStateSuggestionsPageData(content::Page& page)
    : content::PageUserData<ZeroStateSuggestionsPageData>(page) {
  CHECK(IsZeroStateSuggestionsEnabled());
  CHECK(kExtractInnerTextForZeroStateSuggestions.Get() ||
        kExtractAnnotatedPageContentForZeroStateSuggestions.Get());

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&(page.GetMainDocument()));
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  optimization_guide_keyed_service_ =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  page_content_extraction_service_ = page_content_annotations::
      PageContentExtractionServiceFactory::GetForProfile(profile);

  MODEL_EXECUTION_LOG(base::StringPrintf(
      "ZeroStateSuggestionsPageData: Creating page data for %s.",
      web_contents->GetLastCommittedURL().spec()));

  base::TimeDelta initiate_page_content_extraction_delay;
  if (auto* helper = ContextualCueingHelper::FromWebContents(web_contents)) {
    std::optional<base::TimeTicks> last_same_doc_navigation_time =
        helper->last_same_doc_navigation_committed();
    if (last_same_doc_navigation_time) {
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

  // Post to a background thread to avoid blocking the set up of the overlay.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&optimization_guide::PageContextEligibility::Get),
      base::BindOnce(
          &ZeroStateSuggestionsPageData::OnPageContextEligibilityAPILoaded,
          weak_ptr_factory_.GetWeakPtr()));
}

ZeroStateSuggestionsPageData::~ZeroStateSuggestionsPageData() {
  MODEL_EXECUTION_LOG(base::StringPrintf(
      "ZeroStateSuggestionsPageData: Destructing page data for %s.",
      GetUrl().spec()));
  if (!work_done()) {
    MODEL_EXECUTION_LOG(base::StringPrintf(
        "ZeroStateSuggestionsPageData: %s: Destroying before content extracted",
        GetUrl().spec()));
    GiveUp();
  }
}

void ZeroStateSuggestionsPageData::InitiatePageContentExtraction() {
  const GURL url = GetUrl();

  if (content_extraction_initiated_) {
    // Do not re-fetch content.
    MODEL_EXECUTION_LOG(
        base::StringPrintf("ZeroStateSuggestionsPageData: Content extraction "
                           "already initiated for %s. Not trying again",
                           url.spec()));
    return;
  }

  if (!timeout_scheduled_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ZeroStateSuggestionsPageData::GiveUp, AsWeakPtr()),
        kZSSPageContextTimeout.Get());
    timeout_scheduled_ = true;
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

    MODEL_EXECUTION_LOG(
        base::StringPrintf("ZeroStateSuggestionsPageData: Page not "
                           "sufficiently loaded for %s. Waiting until ready",
                           url.spec()));
    return;
  }
  content_extraction_initiated_ = true;
  page_context_begin_time_ = base::TimeTicks::Now();

  MODEL_EXECUTION_LOG(
      base::StringPrintf("ZeroStateSuggestionsPageData: Initiating page "
                         "content extraction for %s.",
                         url.spec()));

  if (kExtractAnnotatedPageContentForZeroStateSuggestions.Get()) {
    bool should_extract_apc = true;
    if (page_content_extraction_service_) {
      // Use cached APC if available.
      auto extracted_page_content_result =
          page_content_extraction_service_
              ->GetExtractedPageContentAndEligibilityForPage(page());
      if (extracted_page_content_result) {
        // Only cache the page content if the content is eligible for server
        // upload.
        OnReceivedAnnotatedPageContent(
            extracted_page_content_result->is_eligible_for_server_upload
                ? std::make_optional(
                      extracted_page_content_result->page_content)
                : std::nullopt);
        should_extract_apc = false;
      }
    }

    // Otherwise, extract fresh APC.
    if (should_extract_apc) {
      blink::mojom::AIPageContentOptionsPtr ai_page_content_options;
      ai_page_content_options = optimization_guide::DefaultAIPageContentOptions(
          /*on_critical_path =*/true);

      optimization_guide::GetAIPageContent(
          web_contents, std::move(ai_page_content_options),
          base::BindOnce(
              &GetEligibilityAndRunCallback, GetUrl(),
              page_context_eligibility_,
              base::BindOnce(
                  &ZeroStateSuggestionsPageData::OnReceivedAnnotatedPageContent,
                  weak_ptr_factory_.GetWeakPtr())));
    }
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

  MODEL_EXECUTION_LOG(
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

void ZeroStateSuggestionsPageData::GetPageContext(
    PageContextCallback callback) {
  if (work_done()) {
    std::move(callback).Run(ConstructPageContextProto());
    return;
  }

  // Page content extraction should already be initiated with construction of
  // this object. Add callback to list to get fired.
  page_context_callbacks_.AddUnsafe(std::move(callback));
}

void ZeroStateSuggestionsPageData::OnReceivedAnnotatedPageContent(
    std::optional<optimization_guide::proto::AnnotatedPageContent> content) {
  if (annotated_page_content_done_) {
    return;
  }

  if (content) {
    base::UmaHistogramTimes(
        "ContextualCueing.GlicSuggestions.PageContextFetchlatency."
        "AnnotatedPageContent",
        base::TimeTicks::Now() - page_context_begin_time_);
  }
  annotated_page_content_ = std::move(content);
  annotated_page_content_done_ = true;
  InvokePageContextCallbacksIfComplete();
}

void ZeroStateSuggestionsPageData::OnReceivedInnerText(
    std::unique_ptr<content_extraction::InnerTextResult> result) {
  if (inner_text_done_) {
    return;
  }
  inner_text_result_ = std::move(result);
  inner_text_done_ = true;
  if (inner_text_result_) {
    base::UmaHistogramTimes(
        "ContextualCueing.GlicSuggestions.PageContextFetchlatency.InnerText",
        base::TimeTicks::Now() - page_context_begin_time_);
  }
  InvokePageContextCallbacksIfComplete();
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
  if (optimization_metadata_done_) {
    return;
  }

  optimization_metadata_done_ = true;
  optimization_decision_ = decision;
  optimization_metadata_ = metadata;

  InvokePageContextCallbacksIfComplete();
}

void ZeroStateSuggestionsPageData::GiveUp() {
  if (work_done()) {
    return;
  }

  MODEL_EXECUTION_LOG(
      base::StringPrintf("ZeroStateSuggestionsPageData: Timed out or page "
                         "destroyed while waiting for "
                         "annotated page content from %s.",
                         GetUrl().spec()));

  // Each OnReceived* method may try to construct the page context proto and
  // access the (maybe already destroyed) page if partial results are available,
  // so clear both of these first.
  inner_text_result_.reset();
  annotated_page_content_.reset();

  // Finish with failure and run the page context callbacks.
  OnReceivedInnerText(nullptr);
  OnReceivedOptimizationMetadata(
      optimization_guide::OptimizationGuideDecision::kUnknown, {});
  OnReceivedAnnotatedPageContent(/*content=*/std::nullopt);
}

void ZeroStateSuggestionsPageData::InvokePageContextCallbacksIfComplete() {
  if (!work_done()) {
    return;
  }

  // Check if we are allowed to request suggestions for this page.
  if (!IsEligibleForContextualSuggestions(optimization_decision_,
                                          optimization_metadata_)) {
    page_context_callbacks_.Notify(
        base::unexpected(PageContextIneligibilityType::kOptimizationMetadata));
    return;
  }

  bool has_page_context = inner_text_result_ || annotated_page_content_;
  if (has_page_context && !page_context_duration_logged_) {
    page_context_duration_logged_ = true;
    base::UmaHistogramTimes(
        "ContextualCueing.GlicSuggestions.PageContextFetchlatency.Total",
        base::TimeTicks::Now() - page_context_begin_time_);
    LOCAL_HISTOGRAM_BOOLEAN(
        "ContextualCueing.ZeroStateSuggestions.ContextExtractionDone", true);
  }

  if (has_page_context) {
    page_context_callbacks_.Notify(base::ok(ConstructPageContextProto()));
  } else {
    page_context_callbacks_.Notify(
        base::unexpected(PageContextIneligibilityType::kPageContext));
  }
}

const GURL ZeroStateSuggestionsPageData::GetUrl() const {
  return content::WebContents::FromRenderFrameHost(&(page().GetMainDocument()))
      ->GetLastCommittedURL();
}

optimization_guide::proto::ZeroStatePageContext
ZeroStateSuggestionsPageData::ConstructPageContextProto() const {
  GURL url = GetUrl();

  optimization_guide::proto::PageContext page_context;
  if (!url.is_empty() && url.is_valid()) {
    page_context.set_url(url.spec());
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&(page().GetMainDocument()));
  page_context.set_title(base::UTF16ToUTF8(web_contents->GetTitle()));

  if (annotated_page_content_) {
    *page_context.mutable_annotated_page_content() = *annotated_page_content_;
  }
  if (inner_text_result_) {
    page_context.set_inner_text(inner_text_result_->inner_text);
  }

  optimization_guide::proto::ZeroStatePageContext zero_state_page_context;
  *zero_state_page_context.mutable_page_context() = page_context;
  zero_state_page_context.set_is_focused(is_focused_);

  return zero_state_page_context;
}

void ZeroStateSuggestionsPageData::OnPageContextEligibilityAPILoaded(
    optimization_guide::PageContextEligibility* page_context_eligibility) {
  page_context_eligibility_ = page_context_eligibility;
}

PAGE_USER_DATA_KEY_IMPL(ZeroStateSuggestionsPageData);

}  // namespace contextual_cueing
