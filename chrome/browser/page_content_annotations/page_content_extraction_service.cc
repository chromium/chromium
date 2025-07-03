// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_content_annotations/page_content_extraction_service.h"

#include "chrome/browser/page_content_annotations/annotate_page_content_request.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_web_contents_observer.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_types.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"

namespace page_content_annotations {

PageContentExtractionService::PageContentExtractionService() = default;

PageContentExtractionService::~PageContentExtractionService() = default;

void PageContentExtractionService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PageContentExtractionService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool PageContentExtractionService::ShouldEnablePageContentExtraction() const {
  if (base::FeatureList::IsEnabled(page_content_annotations::features::
                                       kAnnotatedPageContentExtraction)) {
    return true;
  }
  return !observers_.empty();
}

void PageContentExtractionService::OnPageContentExtracted(
    content::Page& page,
    const optimization_guide::proto::AnnotatedPageContent& page_content) {
  for (auto& observer : observers_) {
    observer.OnPageContentExtracted(page, page_content);
  }
}

std::optional<ExtractedPageContentResult>
PageContentExtractionService::GetExtractedPageContentAndEligibilityForPage(
    content::Page& page) {
  // Get web contents for page.
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&page.GetMainDocument());
  PageContentAnnotationsWebContentsObserver* observer =
      PageContentAnnotationsWebContentsObserver::FromWebContents(web_contents);
  if (!observer) {
    return std::nullopt;
  }
  AnnotatedPageContentRequest* request =
      observer->GetAnnotatedPageContentRequest();
  if (!request) {
    return std::nullopt;
  }
  return request->GetCachedContentAndEligibility();
}

}  // namespace page_content_annotations
