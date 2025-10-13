// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/auxiliary_search/auxiliary_search_donation_service.h"

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/auxiliary_search/fetch_and_rank_helper.h"
#include "components/page_content_annotations/core/page_content_annotation_type.h"
#include "components/page_content_annotations/core/page_content_annotations_common.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"
#include "url/gurl.h"

namespace {

// The delay between a new content annotation and when a history donation is
// triggered, in order to batch together multiple annotations.
constexpr base::TimeDelta kDonationDelay = base::Minutes(5);

}  // namespace

AuxiliarySearchDonationService::AuxiliarySearchDonationService(
    page_content_annotations::PageContentAnnotationsService*
        page_content_annotations_service,
    visited_url_ranking::VisitedURLRankingService* ranking_service)
    : page_content_annotations_service_(page_content_annotations_service),
      ranking_service_(ranking_service) {
  CHECK(page_content_annotations_service_);
  CHECK(ranking_service_);
  page_content_annotations_service_->AddObserver(
      page_content_annotations::AnnotationType::kContentVisibility, this);
}

AuxiliarySearchDonationService::~AuxiliarySearchDonationService() {
  page_content_annotations_service_->RemoveObserver(
      page_content_annotations::AnnotationType::kContentVisibility, this);
}

void AuxiliarySearchDonationService::OnPageContentAnnotated(
    const page_content_annotations::HistoryVisit& visit,
    const page_content_annotations::PageContentAnnotationsResult& result) {
  // Ignore annotations from remote visits (navigation ID is 0).
  if (result.GetType() !=
          page_content_annotations::AnnotationType::kContentVisibility ||
      visit.navigation_id == 0) {
    return;
  }

  if (!donation_timer_.IsRunning()) {
    donation_timer_.Start(
        FROM_HERE, kDonationDelay, this,
        &AuxiliarySearchDonationService::FetchHistoryAndDonate);
  }
}

base::TimeDelta AuxiliarySearchDonationService::GetDonationDelayForTesting()
    const {
  return kDonationDelay;
}

void AuxiliarySearchDonationService::FetchHistoryAndDonate() {
  // TODO: https://crbug.com/432359106 - Set `begin_time` to the time of the
  // most recent visit that was donated.
  scoped_refptr<FetchAndRankHelper> helper =
      base::MakeRefCounted<FetchAndRankHelper>(
          ranking_service_,
          base::BindOnce(&AuxiliarySearchDonationService::DonateHistoryEntries,
                         weak_factory_.GetWeakPtr()),
          /*custom_tab_url=*/std::nullopt,
          /*begin_time=*/std::nullopt);

  helper->StartFetching();
}

void AuxiliarySearchDonationService::DonateHistoryEntries(
    std::vector<jni_zero::ScopedJavaLocalRef<jobject>> entries) {
  // TODO: https://crbug.com/432359106 - Use AuxiliarySearchDonor to donate the
  // entries.
}
