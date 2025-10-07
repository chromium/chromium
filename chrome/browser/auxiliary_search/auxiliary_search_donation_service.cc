// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/auxiliary_search/auxiliary_search_donation_service.h"

#include "components/page_content_annotations/core/page_content_annotation_type.h"
#include "components/page_content_annotations/core/page_content_annotations_common.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"
#include "url/gurl.h"

AuxiliarySearchDonationService::AuxiliarySearchDonationService(
    page_content_annotations::PageContentAnnotationsService*
        page_content_annotations_service)
    : page_content_annotations_service_(page_content_annotations_service) {
  CHECK(page_content_annotations_service_);
  page_content_annotations_service_->AddObserver(
      page_content_annotations::AnnotationType::kContentVisibility, this);
}

AuxiliarySearchDonationService::~AuxiliarySearchDonationService() {
  page_content_annotations_service_->RemoveObserver(
      page_content_annotations::AnnotationType::kContentVisibility, this);
}

void AuxiliarySearchDonationService::OnPageContentAnnotated(
    const GURL& url,
    const page_content_annotations::PageContentAnnotationsResult& result) {
  // TODO: b/432359106 - Implement this using FetchAndRankHelper and
  // AuxiliarySearchDonor
}
