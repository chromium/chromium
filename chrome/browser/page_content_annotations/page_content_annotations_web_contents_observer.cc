// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_content_annotations/page_content_annotations_web_contents_observer.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/content_extraction/inner_text.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/google/core/common/google_util.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_user_data.h"
#include "third_party/blink/public/mojom/opengraph/metadata.mojom.h"

namespace page_content_annotations {

namespace {

// Creates a HistoryVisit based on the current state of |web_contents|.
HistoryVisit CreateHistoryVisitFromWebContents(
    content::WebContents* web_contents) {
  HistoryVisit visit(
      web_contents->GetController().GetLastCommittedEntry()->GetTimestamp(),
      web_contents->GetLastCommittedURL());
  return visit;
}

}  // namespace

class PageContentAnnotationsWebContentsObserver::AnnotatedPageContentRequest {
 public:
  static std::unique_ptr<AnnotatedPageContentRequest> MaybeCreate(
      content::WebContents* web_contents) {
    if (!base::FeatureList::IsEnabled(page_content_annotations::features::
                                          kAnnotatedPageContentExtraction)) {
      return nullptr;
    }

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

  AnnotatedPageContentRequest(content::WebContents* web_contents,
                              blink::mojom::AIPageContentOptionsPtr request)
      : web_contents_(web_contents),
        request_(std::move(request)),
        delay_(page_content_annotations::features::
                   GetAnnotatedPageContentCaptureDelay()),
        include_inner_text_(
            page_content_annotations::features::
                ShouldAnnotatedPageContentStudyIncludeInnerText()) {}

  ~AnnotatedPageContentRequest() = default;

  void PrimaryPageChanged() { ResetForNewNavigation(); }

  void DidFinishNavigation(content::NavigationHandle* navigation_handle) {
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
    RequestContentIfReady();
  }

  void DidStopLoading() {
    // Ensure that the main frame's Document has finished loading.
    if (!web_contents_->IsDocumentOnLoadCompletedInPrimaryMainFrame()) {
      return;
    }

    // Once the main Document has fired the `load` event, wait for all subframes
    // currently in the FrameTree to also finish loading.
    if (web_contents_->IsLoading()) {
      return;
    }

    waiting_for_load_ = false;
    RequestContentIfReady();
  }

  void OnFirstContentfulPaintInPrimaryMainFrame() {
    waiting_for_fcp_ = false;
    RequestContentIfReady();
  }

 private:
  void ResetForNewNavigation() {
    page_content_pending_ = true;
    waiting_for_fcp_ = true;
    waiting_for_load_ = true;

    // Drop pending extraction request for the previous page, if any.
    weak_factory_.InvalidateWeakPtrs();
  }

  void RequestContentIfReady() {
    if (!Ready()) {
      return;
    }

    if (delay_.is_zero()) {
      RequestContentIfReadySync();
      return;
    }

    content::GetUIThreadTaskRunner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AnnotatedPageContentRequest::RequestContentIfReadySync,
                       weak_factory_.GetWeakPtr()),
        delay_);
  }

  void RequestContentIfReadySync() {
    if (!Ready()) {
      return;
    }

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

  bool Ready() const {
    if (!page_content_pending_) {
      return false;
    }

    return !waiting_for_fcp_ && !waiting_for_load_;
  }

  void OnPageContentReceived(
      std::optional<optimization_guide::proto::AnnotatedPageContent> proto) {}

  void OnInnerTextReceived(
      base::TimeTicks start_time,
      std::unique_ptr<content_extraction::InnerTextResult> result) {
    if (!result) {
      return;
    }
    UMA_HISTOGRAM_TIMES("OptimizationGuide.InnerText.TotalLatency",
                        base::TimeTicks::Now() - start_time);
    UMA_HISTOGRAM_CUSTOM_COUNTS("OptimizationGuide.InnerText.TotalSize2",
                                result->inner_text.length() / 1024, 10, 5000,
                                50);
  }

  const raw_ptr<content::WebContents> web_contents_;
  const blink::mojom::AIPageContentOptionsPtr request_;
  const base::TimeDelta delay_;
  const bool include_inner_text_;

  // Set if a new page was committed and querying it's content is pending.
  bool page_content_pending_ = false;

  bool waiting_for_load_ = false;
  bool waiting_for_fcp_ = false;

  base::WeakPtrFactory<AnnotatedPageContentRequest> weak_factory_{this};
};

PageContentAnnotationsWebContentsObserver::
    PageContentAnnotationsWebContentsObserver(
        content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<PageContentAnnotationsWebContentsObserver>(
          *web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  page_content_annotations_service_ =
      PageContentAnnotationsServiceFactory::GetForProfile(profile);
  CHECK(page_content_annotations_service_);
  no_state_prefetch_manager_ =
      prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(profile);
  template_url_service_ = TemplateURLServiceFactory::GetForProfile(profile);
  annotated_page_content_request_ =
      AnnotatedPageContentRequest::MaybeCreate(web_contents);
}

PageContentAnnotationsWebContentsObserver::
    ~PageContentAnnotationsWebContentsObserver() = default;

void PageContentAnnotationsWebContentsObserver::
    DocumentOnLoadCompletedInPrimaryMainFrame() {
  if (!features::ShouldExtractRelatedSearches()) {
    return;
  }
  if (!google_util::IsGoogleSearchUrl(web_contents()->GetLastCommittedURL())) {
    return;
  }

  HistoryVisit history_visit =
      CreateHistoryVisitFromWebContents(web_contents());
  search_result_extractor_client_.RequestData(
      web_contents(), {continuous_search::mojom::ResultType::kRelatedSearches},
      base::BindOnce(&PageContentAnnotationsWebContentsObserver::
                         OnRelatedSearchesExtracted,
                     weak_ptr_factory_.GetWeakPtr(), history_visit));
  LOCAL_HISTOGRAM_BOOLEAN(
      "OptimizationGuide.PageContentAnnotationsWebContentsObserver."
      "RelatedSearchesExtractRequest",
      true);
}

void PageContentAnnotationsWebContentsObserver::DidStopLoading() {
  if (annotated_page_content_request_) {
    annotated_page_content_request_->DidStopLoading();
  }
}

void PageContentAnnotationsWebContentsObserver::PrimaryPageChanged(
    content::Page& page) {
  if (annotated_page_content_request_) {
    annotated_page_content_request_->PrimaryPageChanged();
  }
}

void PageContentAnnotationsWebContentsObserver::
    OnFirstContentfulPaintInPrimaryMainFrame() {
  if (annotated_page_content_request_) {
    annotated_page_content_request_->OnFirstContentfulPaintInPrimaryMainFrame();
  }
}

void PageContentAnnotationsWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (annotated_page_content_request_) {
    annotated_page_content_request_->DidFinishNavigation(navigation_handle);
  }
}

void PageContentAnnotationsWebContentsObserver::OnRelatedSearchesExtracted(
    const HistoryVisit& visit,
    continuous_search::SearchResultExtractorClientStatus status,
    continuous_search::mojom::CategoryResultsPtr results) {
  page_content_annotations_service_->OnRelatedSearchesExtracted(
      visit, status, std::move(results));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PageContentAnnotationsWebContentsObserver);

}  // namespace page_content_annotations
