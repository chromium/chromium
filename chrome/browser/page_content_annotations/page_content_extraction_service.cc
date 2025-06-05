// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_content_annotations/page_content_extraction_service.h"

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

}  // namespace page_content_annotations
