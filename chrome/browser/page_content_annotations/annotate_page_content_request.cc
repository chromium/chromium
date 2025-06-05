// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_content_annotations/annotate_page_content_request.h"

#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "components/pdf/common/constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "pdf/buildflags.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"

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

}  // namespace

// static
std::unique_ptr<AnnotatedPageContentRequest>
AnnotatedPageContentRequest::Create(content::WebContents* web_contents) {
  auto request = blink::mojom::AIPageContentOptions::New();
  request->on_critical_path = page_content_annotations::features::
      IsAnnotatedPageContentOnCriticalPath();
  request->include_geometry = page_content_annotations::features::
      ShouldAnnotatedPageContentIncludeGeometry();
  request->include_hidden_searchable_content = page_content_annotations::
      features::ShouldIncludeHiddenButSearchableContent();

  return std::make_unique<AnnotatedPageContentRequest>(web_contents,
                                                       std::move(request));
}

AnnotatedPageContentRequest::AnnotatedPageContentRequest(
    content::WebContents* web_contents,
    blink::mojom::AIPageContentOptionsPtr request)
    : web_contents_(web_contents),
      request_(std::move(request)),
      delay_(page_content_annotations::features::
                 GetAnnotatedPageContentCaptureDelay()),
      include_inner_text_(
          page_content_annotations::features::
              ShouldAnnotatedPageContentStudyIncludeInnerText()) {}

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

  if (web_contents_->GetContentsMimeType() == pdf::kPDFMimeType) {
    // Pdfs don't provide a FirstContentfulPaint signal, so skip waiting for
    // it for these Documents.
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

  // Drop pending extraction request for the previous page, if any.
  weak_factory_.InvalidateWeakPtrs();
}

void AnnotatedPageContentRequest::MaybeScheduleExtraction() {
  if (!ShouldScheduleExtraction()) {
    return;
  }

  lifecycle_ = Lifecycle::kScheduled;
  if (web_contents_->GetContentsMimeType() == pdf::kPDFMimeType) {
#if BUILDFLAG(ENABLE_PDF)
    content::GetUIThreadTaskRunner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AnnotatedPageContentRequest::RequestPdfPageCount,
                       weak_factory_.GetWeakPtr()),
        page_content_annotations::features::
            GetAnnotatedPageContentCaptureDelay());
#endif  // BUILDFLAG(ENABLE_PDF)
  } else {
    content::GetUIThreadTaskRunner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &AnnotatedPageContentRequest::RequestAnnotatedPageContentSync,
            weak_factory_.GetWeakPtr()),
        delay_);
  }
}

void AnnotatedPageContentRequest::RequestAnnotatedPageContentSync() {
  TRACE_EVENT0("browser",
               "AnnotatedPageContentRequest::RequestAnnotatedPageContentSync");
  optimization_guide::GetAIPageContent(
      web_contents_, request_.Clone(),
      base::BindOnce(&AnnotatedPageContentRequest::OnPageContentReceived,
                     weak_factory_.GetWeakPtr()));

  if (include_inner_text_) {
    content_extraction::GetInnerText(
        *web_contents_->GetPrimaryMainFrame(), std::nullopt,
        base::BindOnce(&AnnotatedPageContentRequest::OnInnerTextReceived,
                       weak_factory_.GetWeakPtr(), base::TimeTicks::Now()));
  }
}

bool AnnotatedPageContentRequest::ShouldScheduleExtraction() const {
  if (lifecycle_ != Lifecycle::kPending) {
    return false;
  }

  return !waiting_for_fcp_ && !waiting_for_load_;
}

void AnnotatedPageContentRequest::OnPageContentReceived(
    std::optional<optimization_guide::AIPageContentResult> page_content) {
  lifecycle_ = Lifecycle::kDone;
  if (!page_content) {
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  auto* page_content_extraction_service = page_content_annotations::
      PageContentExtractionServiceFactory::GetForProfile(profile);
  page_content_extraction_service->OnPageContentExtracted(
      web_contents_->GetPrimaryPage(), page_content->proto);
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

}  // namespace page_content_annotations
