// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/auxiliary_search/auxiliary_search_donation_service.h"

#include <jni.h>

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include "base/android/application_status_listener.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "chrome/browser/auxiliary_search/fetch_and_rank_helper.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/common/pref_names.h"
#include "components/page_content_annotations/core/page_content_annotation_type.h"
#include "components/page_content_annotations/core/page_content_annotations_common.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/jni_zero/jni_zero.h"

namespace {

// The maximum time before "now" to fetch history from.
constexpr base::TimeDelta kHistoryAgeThreshold = base::Hours(24);

}  // namespace

AuxiliarySearchDonationService::AuxiliarySearchDonationService(
    page_content_annotations::PageContentAnnotationsService*
        page_content_annotations_service,
    visited_url_ranking::VisitedURLRankingService* ranking_service,
    PrefService* pref_service,
    DonateCallback donate_callback)
    : page_content_annotations_service_(
          raw_ref<page_content_annotations::PageContentAnnotationsService>::
              from_ptr(page_content_annotations_service)),
      ranking_service_(
          raw_ref<visited_url_ranking::VisitedURLRankingService>::from_ptr(
              ranking_service)),
      pref_service_(raw_ref<PrefService>::from_ptr(pref_service)),
      donate_callback_(std::move(donate_callback)),
      application_status_listener_(
          base::android::ApplicationStatusListener::New(base::BindRepeating(
              &AuxiliarySearchDonationService::OnApplicationStateChanged,
              // Listener is destroyed at destructor, and
              // object will be alive for any callback.
              base::Unretained(this)))) {
  page_content_annotations_service_->AddObserver(
      page_content_annotations::AnnotationType::kContentVisibility, this);
}

AuxiliarySearchDonationService::~AuxiliarySearchDonationService() {
  page_content_annotations_service_->RemoveObserver(
      page_content_annotations::AnnotationType::kContentVisibility, this);
}

void AuxiliarySearchDonationService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterTimePref(
      prefs::kAuxiliarySearchLastDonatedHistoryEntryVisitTime, base::Time());
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
  // Only fetch history entries strictly newer than the most recent visit from
  // the previous fetch. If that is too old (more than `kHistoryAgeThreshold`
  // ago), then start from `kHistoryAgeThreshold`.
  const base::Time threshold_time = base::Time::Now() - kHistoryAgeThreshold;
  // `FetchAndRankHelper` treats `begin_time` as inclusive, so add the smallest
  // possible time unit (1us) to the previous donation time to ensure we don't
  // fetch the same entry twice.
  const base::Time begin_time =
      std::max(pref_service_->GetTime(
                   prefs::kAuxiliarySearchLastDonatedHistoryEntryVisitTime) +
                   base::Microseconds(1),
               threshold_time);

  scoped_refptr<FetchAndRankHelper> helper =
      base::MakeRefCounted<FetchAndRankHelper>(
          &ranking_service_.get(),
          base::BindOnce(&AuxiliarySearchDonationService::DonateHistoryEntries,
                         weak_factory_.GetWeakPtr()),
          /*custom_tab_url=*/std::nullopt, begin_time);

  helper->StartFetching();
}

void AuxiliarySearchDonationService::DonateHistoryEntries(
    std::vector<jni_zero::ScopedJavaLocalRef<jobject>> entries,
    const visited_url_ranking::URLVisitsMetadata& metadata) {
  if (!entries.empty()) {
    donate_callback_.Run(std::move(entries));
  }

  if (!metadata.most_recent_timestamp.has_value()) {
    return;
  }

  pref_service_->SetTime(
      prefs::kAuxiliarySearchLastDonatedHistoryEntryVisitTime,
      std::max(pref_service_->GetTime(
                   prefs::kAuxiliarySearchLastDonatedHistoryEntryVisitTime),
               *metadata.most_recent_timestamp));
}

void AuxiliarySearchDonationService::OnApplicationStateChanged(
    base::android::ApplicationState state) {
  if (state == base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES &&
      donation_timer_.IsRunning()) {
    donation_timer_.FireNow();
  }
}
