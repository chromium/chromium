// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/auxiliary_search/auxiliary_search_donation_service.h"

#include <jni.h>

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/android/application_status_listener.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "chrome/browser/auxiliary_search/auxiliary_search_provider.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/visited_url_ranking/visited_url_ranking_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/page_content_annotations/core/page_content_annotation_type.h"
#include "components/page_content_annotations/core/page_content_annotations_common.h"
#include "components/page_content_annotations/core/page_content_annotations_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/visited_url_ranking/public/features.h"
#include "components/visited_url_ranking/public/fetch_options.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "components/visited_url_ranking/public/visited_url_ranking_service.h"
#include "url/gurl.h"

namespace {

// The maximum time before "now" to fetch history from.
constexpr base::TimeDelta kHistoryAgeThreshold = base::Hours(24);

// Returns the maximum count of entries to donate.
int GetMaxDonationCount() {
  return chrome::android::kAppIntegrationMaxDonationCountParam.Get();
}

visited_url_ranking::FetchOptions CreateFetchOptionsForHistoryDonation(
    base::Time begin_time) {
  std::vector<visited_url_ranking::URLVisitAggregatesTransformType> transforms{
      // `kRecencyFilter` (i.e. `RecencyFilterTransformer`) is only useful if we
      // want have different age limits for different URL types, or if we want
      // additional signals from visits older than `begin_time`.
      // Neither of these are currently needed, so omit it here.

      // Filter out links commonly opened by default apps.
      visited_url_ranking::URLVisitAggregatesTransformType::
          kDefaultAppUrlFilter,

      // Filter out visits from AuthView.
      visited_url_ranking::URLVisitAggregatesTransformType::
          kHistoryBrowserTypeFilter,

      // Filter out URLs that may be sensitive to display on surfaces.
      visited_url_ranking::URLVisitAggregatesTransformType::
          kHistoryVisibilityScoreFilter,
  };

  return visited_url_ranking::FetchOptions(
      /*result_sources_arg=*/
      {{visited_url_ranking::URLVisitAggregate::URLType::kLocalVisit,
        visited_url_ranking::FetchOptions::ResultOption{
            // Only used in the `kRecencyFilter` transform which we don't use
            // (see above).
            .age_limit = kHistoryAgeThreshold,
            .visit_duration_limit = std::nullopt}}},
      /*fetcher_sources_arg=*/
      {{visited_url_ranking::Fetcher::kHistory,
        // Don't fetch from remote sources - their only use would be signals or
        // ranking which we don't use.
        {visited_url_ranking::FetchOptions::Source::kLocal}}},
      begin_time, std::move(transforms), GetMaxDonationCount());
}

}  // namespace

AuxiliarySearchDonationService::HistoryData::HistoryData(
    std::string url_key,
    GURL url,
    std::u16string title,
    base::Time last_visited)
    : url_key(std::move(url_key)),
      url(std::move(url)),
      title(std::move(title)),
      last_visited(last_visited) {}

AuxiliarySearchDonationService::HistoryData::HistoryData(const HistoryData&) =
    default;
AuxiliarySearchDonationService::HistoryData&
AuxiliarySearchDonationService::HistoryData::operator=(const HistoryData&) =
    default;
AuxiliarySearchDonationService::HistoryData::HistoryData(HistoryData&&) =
    default;
AuxiliarySearchDonationService::HistoryData&
AuxiliarySearchDonationService::HistoryData::operator=(HistoryData&&) = default;

AuxiliarySearchDonationService::HistoryData::~HistoryData() = default;

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
  // `VisitedURLRankingService` (specifically `HistoryURLVisitDataFetcher`,
  // which sets `history::QueryOptions`'s `visit_order` to `RECENT_FIRST`)
  // treats `begin_time` as inclusive, so add the smallest possible time unit
  // (1us) to the previous donation time to ensure we don't fetch the same entry
  // twice.
  const base::Time begin_time =
      std::max(pref_service_->GetTime(
                   prefs::kAuxiliarySearchLastDonatedHistoryEntryVisitTime) +
                   base::Microseconds(1),
               threshold_time);

  ranking_service_->FetchURLVisitAggregates(
      CreateFetchOptionsForHistoryDonation(begin_time),
      base::BindOnce(&AuxiliarySearchDonationService::OnHistoryFetched,
                     weak_factory_.GetWeakPtr()));
}

void AuxiliarySearchDonationService::OnHistoryFetched(
    visited_url_ranking::ResultStatus status,
    visited_url_ranking::URLVisitsMetadata url_visits_metadata,
    std::vector<visited_url_ranking::URLVisitAggregate> aggregates) {
  if (status != visited_url_ranking::ResultStatus::kSuccess) {
    // Halt if we fail to avoid erroneously updating the last donation time.
    return;
  }

  std::vector<HistoryData> entries;
  entries.reserve(aggregates.size());
  for (visited_url_ranking::URLVisitAggregate& aggregate : aggregates) {
    // In theory, we only use one `visited_url_ranking::Fetcher` which should
    // only return history data. Gracefully handle any unexpected data by taking
    // the "first" relevant fetcher data map entry.
    // Prioritise history data, then fall back to other sources.
    static constexpr visited_url_ranking::Fetcher kPriorityOrder[] = {
        visited_url_ranking::Fetcher::kHistory,
        visited_url_ranking::Fetcher::kSession,
        visited_url_ranking::Fetcher::kTabModel};

    for (visited_url_ranking::Fetcher fetcher : kPriorityOrder) {
      auto it = aggregate.fetcher_data_map.find(fetcher);
      if (it == aggregate.fetcher_data_map.end()) {
        continue;
      }

      if (auto* history_data =
              std::get_if<visited_url_ranking::URLVisitAggregate::HistoryData>(
                  &it->second)) {
        entries.emplace_back(std::move(aggregate.url_key),
                             std::move(history_data->visit.url),
                             std::move(history_data->visit.title),
                             history_data->last_visited.visit_row.visit_time);
        break;
      }
    }
  }

  DonateHistoryEntries(std::move(entries), url_visits_metadata);
}

void AuxiliarySearchDonationService::DonateHistoryEntries(
    std::vector<HistoryData> entries,
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
