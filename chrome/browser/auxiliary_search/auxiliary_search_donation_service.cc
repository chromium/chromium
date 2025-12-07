// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/auxiliary_search/auxiliary_search_donation_service.h"

#include "base/android/application_status_listener.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/auxiliary_search/fetch_and_rank_helper.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "components/page_content_annotations/core/page_content_annotation_type.h"
#include "components/page_content_annotations/core/page_content_annotations_common.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"
#include "url/gurl.h"

namespace {

// The maximum time before "now" to fetch history from.
constexpr base::TimeDelta kHistoryAgeThreshold = base::Hours(24);

}  // namespace

AuxiliarySearchDonationService::AuxiliarySearchDonationService(
    page_content_annotations::PageContentAnnotationsService*
        page_content_annotations_service,
    visited_url_ranking::VisitedURLRankingService* ranking_service)
    : page_content_annotations_service_(page_content_annotations_service),
      ranking_service_(ranking_service),
      application_status_listener_(
          base::android::ApplicationStatusListener::New(base::BindRepeating(
              &AuxiliarySearchDonationService::OnApplicationStateChanged,
              // Listener is destroyed at destructor, and
              // object will be alive for any callback.
              base::Unretained(this)))) {
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
        FROM_HERE, GetDonationDelay(), this,
        &AuxiliarySearchDonationService::FetchHistoryAndDonate);
  }
}

base::TimeDelta AuxiliarySearchDonationService::GetDonationDelay() const {
  return base::Seconds(
      chrome::android::kAuxiliarySearchHistoryDonationDelayInSeconds.Get());
}

base::TimeDelta
AuxiliarySearchDonationService::GetHistoryAgeThresholdForTesting() const {
  return kHistoryAgeThreshold;
}

void AuxiliarySearchDonationService::FetchHistoryAndDonate() {
  // Only fetch history entries newer than the most recent visit from the
  // previous fetch. If that is too old (more than `kHistoryAgeThreshold` ago),
  // then start from `kHistoryAgeThreshold`.
  const base::Time threshold_time = base::Time::Now() - kHistoryAgeThreshold;
  const base::Time begin_time =
      last_donated_history_entry_visit_time_.has_value()
          ? std::max(*last_donated_history_entry_visit_time_, threshold_time)
          : threshold_time;

  scoped_refptr<FetchAndRankHelper> helper =
      base::MakeRefCounted<FetchAndRankHelper>(
          ranking_service_,
          base::BindOnce(&AuxiliarySearchDonationService::DonateHistoryEntries,
                         weak_factory_.GetWeakPtr()),
          /*custom_tab_url=*/std::nullopt, begin_time);

  helper->StartFetching();
}

void AuxiliarySearchDonationService::DonateHistoryEntries(
    std::vector<jni_zero::ScopedJavaLocalRef<jobject>> entries,
    const visited_url_ranking::URLVisitsMetadata& metadata) {
  // TODO: https://crbug.com/432359106 - Use AuxiliarySearchDonor to donate the
  // entries.
  // TODO: https://crbug.com/432359106 - Write the visit time to prefs so it
  // persists across sessions.
  if (!metadata.most_recent_timestamp.has_value()) {
    return;
  }

  last_donated_history_entry_visit_time_ =
      last_donated_history_entry_visit_time_.has_value()
          ? std::max(*last_donated_history_entry_visit_time_,
                     *metadata.most_recent_timestamp)
          : *metadata.most_recent_timestamp;
}

void AuxiliarySearchDonationService::OnApplicationStateChanged(
    base::android::ApplicationState state) {
  if (state == base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES &&
      donation_timer_.IsRunning()) {
    donation_timer_.FireNow();
  }
}
