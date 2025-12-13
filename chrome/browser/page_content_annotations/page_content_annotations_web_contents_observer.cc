// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_content_annotations/page_content_annotations_web_contents_observer.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/page_content_annotations/annotate_page_content_request.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/content_extraction/content/browser/inner_text.h"
#include "components/google/core/common/google_util.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"
#include "components/pdf/common/constants.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
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
  page_content_annotations_service_->AddObserver(
      AnnotationType::kContentVisibility, this);
}

AnnotatedPageContentRequest*
PageContentAnnotationsWebContentsObserver::GetAnnotatedPageContentRequest() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  auto* page_content_extraction_service =
      PageContentExtractionServiceFactory::GetForProfile(profile);
  bool should_enable =
      page_content_extraction_service &&
      page_content_extraction_service->ShouldEnablePageContentExtraction();

  if (should_enable) {
    if (!annotated_page_content_request_) {
      annotated_page_content_request_ =
          AnnotatedPageContentRequest::Create(web_contents());
    }
  } else {
    annotated_page_content_request_.reset();
  }
  return annotated_page_content_request_.get();
}

PageContentAnnotationsWebContentsObserver::
    ~PageContentAnnotationsWebContentsObserver() {
  page_content_annotations_service_->RemoveObserver(
      AnnotationType::kContentVisibility, this);
}

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
  if (auto* annotated_page_content_request = GetAnnotatedPageContentRequest()) {
    annotated_page_content_request->DidStopLoading();
  }
}

void PageContentAnnotationsWebContentsObserver::PrimaryPageChanged(
    content::Page& page) {
  if (auto* annotated_page_content_request = GetAnnotatedPageContentRequest()) {
    annotated_page_content_request->PrimaryPageChanged();
  }
}

void PageContentAnnotationsWebContentsObserver::
    OnFirstContentfulPaintInPrimaryMainFrame() {
  if (auto* annotated_page_content_request = GetAnnotatedPageContentRequest()) {
    annotated_page_content_request->OnFirstContentfulPaintInPrimaryMainFrame();
  }
}

void PageContentAnnotationsWebContentsObserver::OnVisibilityChanged(
    content::Visibility visibility) {
  if (auto* annotated_page_content_request = GetAnnotatedPageContentRequest()) {
    annotated_page_content_request->OnVisibilityChanged(visibility);
  }
}

void PageContentAnnotationsWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (auto* annotated_page_content_request = GetAnnotatedPageContentRequest()) {
    annotated_page_content_request->DidFinishNavigation(navigation_handle);
  }

  // New navigation. Reset the content visibility score.
  content_visibility_score_ = std::nullopt;
}

void PageContentAnnotationsWebContentsObserver::OnRelatedSearchesExtracted(
    const HistoryVisit& visit,
    continuous_search::SearchResultExtractorClientStatus status,
    continuous_search::mojom::CategoryResultsPtr results) {
  page_content_annotations_service_->OnRelatedSearchesExtracted(
      visit, status, std::move(results));
}

void PageContentAnnotationsWebContentsObserver::OnPageContentAnnotated(
    const HistoryVisit& annotated_visit,
    const PageContentAnnotationsResult& result) {
  HistoryVisit history_visit =
      CreateHistoryVisitFromWebContents(web_contents());
  if (history_visit.nav_entry_timestamp !=
          annotated_visit.nav_entry_timestamp ||
      history_visit.url != annotated_visit.url) {
    return;
  }

  content_visibility_score_ = result.GetContentVisibilityScore();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PageContentAnnotationsWebContentsObserver);

}  // namespace page_content_annotations
