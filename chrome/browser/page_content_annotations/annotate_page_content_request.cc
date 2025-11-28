// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_content_annotations/annotate_page_content_request.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_types.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history/core/browser/features.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/content/browser/page_context_eligibility.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "components/page_content_annotations/core/page_content_annotations_switches.h"
#include "components/pdf/common/constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "net/http/http_response_headers.h"
#include "pdf/buildflags.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#endif

#if BUILDFLAG(ENABLE_PDF)
#include "components/pdf/browser/pdf_document_helper.h"
#endif  // BUILDFLAG(ENABLE_PDF)

namespace page_content_annotations {

namespace {

#if BUILDFLAG(ENABLE_PDF)
void RecordPdfPageCountMetrics(
    ukm::SourceId source_id,
    pdf::mojom::PdfListener::GetPdfBytesStatus status,
    const std::vector<uint8_t>& bytes,
    uint32_t page_count) {
  if (status == pdf::mojom::PdfListener::GetPdfBytesStatus::kFailed) {
    return;
  }
  ukm::builders::OptimizationGuide_AnnotatedPdfContent(source_id)
      .SetPdfPageCount(ukm::GetExponentialBucketMinForCounts1000(page_count))
      .Record(ukm::UkmRecorder::Get());
}
#endif  // BUILDFLAG(ENABLE_PDF)

std::optional<int64_t> GetTabId(content::WebContents* web_contents) {
#if BUILDFLAG(IS_ANDROID)
  if (TabAndroid* tab = TabAndroid::FromWebContents(web_contents)) {
    return tab->GetAndroidId();
  }
#endif
  // TODO(440643544): Implement an usable tab ID for other platforms.
  return std::nullopt;
}

}  // namespace

// static
std::unique_ptr<AnnotatedPageContentRequest>
AnnotatedPageContentRequest::Create(content::WebContents* web_contents) {
  auto request = blink::mojom::AIPageContentOptions::New();
  request->mode =
      (page_content_annotations::features::AnnotatedPageContentMode() ==
       "actionable")
          ? blink::mojom::AIPageContentMode::kActionableElements
          : blink::mojom::AIPageContentMode::kDefault;
  request->on_critical_path = page_content_annotations::features::
      IsAnnotatedPageContentOnCriticalPath();

  return std::make_unique<AnnotatedPageContentRequest>(web_contents,
                                                       std::move(request));
}

AnnotatedPageContentRequest::AnnotatedPageContentRequest(
    content::WebContents* web_contents,
    blink::mojom::AIPageContentOptionsPtr request)
    : web_contents_(web_contents),
      request_(std::move(request)),
      delay_(features::GetAnnotatedPageContentCaptureDelay()),
      include_inner_text_(
          features::ShouldAnnotatedPageContentStudyIncludeInnerText()),
      fetch_page_context_callback_(
          base::BindRepeating(&page_content_annotations::FetchPageContext)) {
  // Post to a background thread to avoid blocking the set up of the overlay.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&optimization_guide::PageContextEligibility::Get),
      base::BindOnce(
          &AnnotatedPageContentRequest::OnPageContextEligibilityAPILoaded,
          weak_factory_.GetWeakPtr()));
}

AnnotatedPageContentRequest::~AnnotatedPageContentRequest() = default;

void AnnotatedPageContentRequest::PrimaryPageChanged() {
  ResetForNewNavigation();
}

void AnnotatedPageContentRequest::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  // Cross-document navigations are handled in PrimaryPageChanged.
  if (!navigation_handle->IsSameDocument() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  // This is a heuristic to tradeoff how frequently the content is updated and
  // ensuring we have coverage for single-page-apps in the data. If the
  // navigation will appear in the browser history, it's likely a significant
  // change in page state.
  if (!navigation_handle->ShouldUpdateHistory()) {
    return;
  }

  if (base::FeatureList::IsEnabled(history::kVisitedLinksOn404)) {
    // With the flag enabled, navigations with a 404 status code will be
    // eligible for History. We want to ignore 404s. At this point, we should
    // only be looking at committed same-document navigations. Same-document
    // navigations have no network request and therefore no response code, so we
    // should look at the response code for the request that brought us to the
    // current document instead of the `NavigationHandle`.
    const auto* document_response_head =
        navigation_handle->GetRenderFrameHost()->GetLastResponseHead();
    if (!document_response_head || !document_response_head->headers) {
      return;
    }
    const int status_code = document_response_head->headers->response_code();
    if (status_code == 404) {
      return;
    }
  }

  ResetForNewNavigation();

  // We don't have reliable load and FCP signals for same-document
  // navigations. So we assume the content is ready as soon as the navigation
  // commits.
  waiting_for_fcp_ = false;
  waiting_for_load_ = false;
  MaybeScheduleExtraction();
}

void AnnotatedPageContentRequest::DidStopLoading() {
  // Ensure that the main frame's Document has finished loading.
  if (!web_contents_->IsDocumentOnLoadCompletedInPrimaryMainFrame()) {
    return;
  }

  // Once the main Document has fired the `load` event, wait for all subframes
  // currently in the FrameTree to also finish loading.
  if (web_contents_->IsLoading()) {
    return;
  }

  if (web_contents_->GetContentsMimeType() == pdf::kPDFMimeType ||
      web_contents_->GetVisibility() == content::Visibility::HIDDEN ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kPageContentAnnotationsSkipFCPWaitForTesting)) {
    // Pdfs and hidden tabs don't provide a reliable FirstContentfulPaint
    // signal, so skip waiting for it for these Documents.
    waiting_for_fcp_ = false;
  }

  waiting_for_load_ = false;
  MaybeScheduleExtraction();
}

void AnnotatedPageContentRequest::OnFirstContentfulPaintInPrimaryMainFrame() {
  waiting_for_fcp_ = false;
  MaybeScheduleExtraction();
}

void AnnotatedPageContentRequest::ResetForNewNavigation() {
  lifecycle_ = Lifecycle::kPending;
  waiting_for_fcp_ = true;
  waiting_for_load_ = true;

  cached_content_ = std::nullopt;

  // Drop pending extraction request for the previous page, if any.
  weak_factory_.InvalidateWeakPtrs();

  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  auto* page_content_extraction_service =
      PageContentExtractionServiceFactory::GetForProfile(profile);
  if (page_content_extraction_service) {
    page_content_extraction_service->OnNewNavigation(GetTabId(web_contents_),
                                                     web_contents_);
  }
}

void AnnotatedPageContentRequest::MaybeScheduleExtraction() {
  if (!ShouldScheduleExtraction()) {
    return;
  }

  lifecycle_ = Lifecycle::kScheduled;

  content::GetUIThreadTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AnnotatedPageContentRequest::ExtractPageContent,
                     weak_factory_.GetWeakPtr()),
      delay_);
}

void AnnotatedPageContentRequest::ExtractPageContent() {
  // If there was a navigation in between the delay, skip extraction.
  if (lifecycle_ != Lifecycle::kScheduled) {
    return;
  }

  if (web_contents_->GetContentsMimeType() == pdf::kPDFMimeType) {
#if BUILDFLAG(ENABLE_PDF)
    RequestPdfPageCount();
#endif  // BUILDFLAG(ENABLE_PDF)
  } else {
    RequestAnnotatedPageContentSync();
  }
}

void AnnotatedPageContentRequest::RequestAnnotatedPageContentSync() {
  TRACE_EVENT0("browser",
               "AnnotatedPageContentRequest::RequestAnnotatedPageContentSync");

  // Note: This is not fetching pdfs since we do not want to cache pdfs in disk
  // in PageContentCache.
  FetchPageContextOptions options;
  options.annotated_page_content_options = request_->Clone();
  if (features::kPageContentCacheEnableScreenshot.Get()) {
    options.screenshot_options = ScreenshotOptions::ViewportOnly(std::nullopt);
  }
  fetch_page_context_callback_.Run(
      *web_contents_, options, /*progress_listener=*/nullptr,
      base::BindOnce(&AnnotatedPageContentRequest::OnPageContextFetched,
                     weak_factory_.GetWeakPtr()));

  if (include_inner_text_) {
    content_extraction::GetInnerText(
        *web_contents_->GetPrimaryMainFrame(), std::nullopt,
        base::BindOnce(&AnnotatedPageContentRequest::OnInnerTextReceived,
                       weak_factory_.GetWeakPtr(), base::TimeTicks::Now()));
  }
}

void AnnotatedPageContentRequest::SetFetchPageContextCallbackForTesting(
    FetchPageContextCallback callback) {
  fetch_page_context_callback_ = std::move(callback);
}

bool AnnotatedPageContentRequest::ShouldScheduleExtraction() const {
  auto triggering_mode = features::GetPageContentExtractionTriggeringMode();

  // If the page is not loaded, the extraction would not work.
  if (waiting_for_fcp_ || waiting_for_load_) {
    return false;
  }

  if (triggering_mode ==
          features::PageContentExtractionTriggeringMode::kOnLoadAndHidden &&
      is_hidden_) {
    // Allow extraction if:
    // 1. Page loaded while hidden (kPending) - This is the *first* extraction.
    // 2. Page loaded while visible, and is now hidden
    //      (kExtractedAtPageLoad). This is the *second* extraction.
    if (lifecycle_ == Lifecycle::kPending ||
        lifecycle_ == Lifecycle::kExtractedAtPageLoad) {
      return true;
    }
  }

  if (lifecycle_ != Lifecycle::kPending) {
    return false;
  }

  if (triggering_mode ==
          features::PageContentExtractionTriggeringMode::kOnLoad ||
      triggering_mode ==
          features::PageContentExtractionTriggeringMode::kOnLoadAndHidden) {
    return true;
  }
  if (triggering_mode ==
      features::PageContentExtractionTriggeringMode::kOnHidden) {
    return is_hidden_;
  }
  return false;
}

void AnnotatedPageContentRequest::OnPageContextFetched(
    FetchPageContextResultCallbackArg result) {
  if (is_hidden_) {
    lifecycle_ = Lifecycle::kFinal;
  } else {
    // Set to kExtractedAtPageLoad, which could potentially trigger another
    // extraction when backgrounded when background trigger is also needed.
    lifecycle_ = Lifecycle::kExtractedAtPageLoad;
  }

  if (!result.has_value() || !result.value() ||
      !result.value()->annotated_page_content_result.has_value()) {
    return;
  }
  base::Time extraction_time = base::Time::Now();
  std::vector<uint8_t> screenshot_data;
  if (result.value()->screenshot_result.has_value()) {
    screenshot_data =
        std::move(result.value()->screenshot_result.value().screenshot_data);
  }

  auto page_content_result =
      std::move(result.value()->annotated_page_content_result);

  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  auto* page_content_extraction_service =
      PageContentExtractionServiceFactory::GetForProfile(profile);
  page_content_extraction_service->OnPageContentExtracted(
      web_contents_->GetPrimaryPage(), page_content_result->proto,
      screenshot_data, GetTabId(web_contents_));

  GURL url = web_contents_->GetLastCommittedURL();
  bool is_eligible_for_server_upload =
      !page_context_eligibility_ ||
      optimization_guide::IsPageContextEligible(
          url.GetHost(), url.GetPath(),
          optimization_guide::GetFrameMetadataFromPageContent(
              *page_content_result),
          page_context_eligibility_);
  cached_content_ = ExtractedPageContentResult(
      std::move(page_content_result->proto), extraction_time,
      is_eligible_for_server_upload, std::move(screenshot_data));
}

void AnnotatedPageContentRequest::OnInnerTextReceived(
    base::TimeTicks start_time,
    std::unique_ptr<content_extraction::InnerTextResult> result) {
  if (!result) {
    return;
  }
  UMA_HISTOGRAM_TIMES("OptimizationGuide.InnerText.TotalLatency",
                      base::TimeTicks::Now() - start_time);
  UMA_HISTOGRAM_CUSTOM_COUNTS("OptimizationGuide.InnerText.TotalSize2",
                              result->inner_text.length() / 1024, 10, 5000, 50);
}

#if BUILDFLAG(ENABLE_PDF)
void AnnotatedPageContentRequest::RequestPdfPageCount() {
  CHECK_EQ(pdf::kPDFMimeType, web_contents_->GetContentsMimeType());
  auto* pdf_helper =
      pdf::PDFDocumentHelper::MaybeGetForWebContents(web_contents_);
  if (pdf_helper) {
    pdf_helper->RegisterForDocumentLoadComplete(
        base::BindOnce(&AnnotatedPageContentRequest::OnPdfDocumentLoadComplete,
                       weak_factory_.GetWeakPtr()));
  }
}

void AnnotatedPageContentRequest::OnPdfDocumentLoadComplete() {
  CHECK_EQ(pdf::kPDFMimeType, web_contents_->GetContentsMimeType());
  // Do not need to set to kExtractedAtPageLoad since PDFs contents will
  // never change.
  lifecycle_ = Lifecycle::kFinal;

  auto* pdf_helper =
      pdf::PDFDocumentHelper::MaybeGetForWebContents(web_contents_);
  if (pdf_helper) {
    // Fetch zero PDF bytes to just receive the total page count.
    pdf_helper->GetPdfBytes(
        /*size_limit=*/0,
        base::BindOnce(
            &RecordPdfPageCountMetrics,
            web_contents_->GetPrimaryMainFrame()->GetPageUkmSourceId()));
  }
}
#endif  // BUILDFLAG(ENABLE_PDF)

void AnnotatedPageContentRequest::OnPageContextEligibilityAPILoaded(
    optimization_guide::PageContextEligibility* page_context_eligibility) {
  page_context_eligibility_ = page_context_eligibility;
}

std::optional<ExtractedPageContentResult>
AnnotatedPageContentRequest::GetCachedContentAndEligibility() {
  return cached_content_;
}

void AnnotatedPageContentRequest::OnVisibilityChanged(
    content::Visibility visibility) {
  bool was_hidden = is_hidden_;
  is_hidden_ = visibility == content::Visibility::HIDDEN;
  if (is_hidden_ == was_hidden) {
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  auto* page_content_extraction_service =
      PageContentExtractionServiceFactory::GetForProfile(profile);
  if (page_content_extraction_service) {
    page_content_extraction_service->OnVisibilityChanged(
        GetTabId(web_contents_), web_contents_, visibility);
  }

  auto triggering_mode = features::GetPageContentExtractionTriggeringMode();
  bool trigger_on_hide =
      triggering_mode ==
          features::PageContentExtractionTriggeringMode::kOnHidden ||
      triggering_mode ==
          features::PageContentExtractionTriggeringMode::kOnLoadAndHidden;

  if (!trigger_on_hide) {
    return;
  }

  if (is_hidden_) {
    MaybeScheduleExtraction();
  } else {
    // When extraction runs on hide, reset the lifecycle, so it can be extracted
    // on hide again. Keep the cached contents to be used as needed.
    lifecycle_ = Lifecycle::kPending;
  }
}

}  // namespace page_content_annotations
