// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_content_annotations/page_content_annotations_web_contents_observer.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/strings/utf_string_conversions.h"
#include "components/google/core/common/google_util.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_user_data.h"
#include "third_party/blink/public/mojom/opengraph/metadata.mojom.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/profiles/profile.h"

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

void PageContentAnnotationsWebContentsObserver::OnRelatedSearchesExtracted(
    const HistoryVisit& visit,
    continuous_search::SearchResultExtractorClientStatus status,
    continuous_search::mojom::CategoryResultsPtr results) {
  page_content_annotations_service_->OnRelatedSearchesExtracted(
      visit, status, std::move(results));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PageContentAnnotationsWebContentsObserver);

}  // namespace page_content_annotations
