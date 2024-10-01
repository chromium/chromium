// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/most_relevant_tab_resumption/most_relevant_tab_resumption_page_handler.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <set>
#include <string>

#include "base/hash/hash.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/new_tab_page/modules/v2/most_relevant_tab_resumption/most_relevant_tab_resumption.mojom.h"
#include "chrome/browser/new_tab_page/modules/v2/most_relevant_tab_resumption/url_visit_types.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/visited_url_ranking/visited_url_ranking_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/history/core/browser/history_types.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search/ntp_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_device_info/device_info.h"
#include "components/visited_url_ranking/public/features.h"
#include "components/visited_url_ranking/public/fetch_options.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "components/visited_url_ranking/public/url_visit_util.h"
#include "components/visited_url_ranking/public/visited_url_ranking_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "url/gurl.h"

using visited_url_ranking::Fetcher;
using visited_url_ranking::FetchOptions;
using visited_url_ranking::URLVisit;
using visited_url_ranking::URLVisitAggregate;
using visited_url_ranking::URLVisitAggregatesTransformType;
using Source = visited_url_ranking::URLVisit::Source;

namespace {
// Name of preference to track list of dismissed visits.
const char kDismissedVisitsPrefName[] =
    "NewTabPage.MostRelevantTabResumption.DismissedVisits";

std::u16string FormatRelativeTime(const base::Time& time) {
  // Return a time like "1 hour ago", "2 days ago", etc.
  base::Time now = base::Time::Now();
  // TimeFormat does not support negative TimeDelta values, so then we use 0.
  return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_ELAPSED,
                                ui::TimeFormat::LENGTH_SHORT,
                                now < time ? base::TimeDelta() : now - time);
}

// Helper method to create mojom tab objects from Tab objects.
ntp::most_relevant_tab_resumption::mojom::URLVisitPtr TabToMojom(
    const URLVisitAggregate::Tab& tab,
    base::Time last_active = base::Time()) {
  auto url_visit_mojom =
      ntp::most_relevant_tab_resumption::mojom::URLVisit::New();
  url_visit_mojom->form_factor = tab.visit.device_type;
  url_visit_mojom->session_name = tab.session_name;

  base::Value::Dict dictionary;
  NewTabUI::SetUrlTitleAndDirection(&dictionary, tab.visit.title,
                                    tab.visit.url);
  url_visit_mojom->title = *dictionary.FindString("title");

  return url_visit_mojom;
}

// Helper method to create mojom tab objects from HistoryEntry objects.
ntp::most_relevant_tab_resumption::mojom::URLVisitPtr HistoryEntryVisitToMojom(
    const history::AnnotatedVisit& visit,
    const std::optional<std::string>& client_name,
    syncer::DeviceInfo::FormFactor device_type) {
  auto url_visit_mojom =
      ntp::most_relevant_tab_resumption::mojom::URLVisit::New();
  url_visit_mojom->form_factor = device_type;
  if (client_name) {
    url_visit_mojom->session_name = client_name.value();
  }

  base::Value::Dict dictionary;
  NewTabUI::SetUrlTitleAndDirection(&dictionary, visit.url_row.title(),
                                    visit.url_row.url());
  url_visit_mojom->title = *dictionary.FindString("title");

  return url_visit_mojom;
}

URLVisitAggregate::Tab CreateSampleURLVisitAggregateTab(
    const GURL& url,
    base::Time time = base::Time::Now() - base::Minutes(5)) {
  const std::u16string kSampleTitle(u"sample_title");
  return URLVisitAggregate::Tab(
      1,
      URLVisit(url, kSampleTitle, time,
               syncer::DeviceInfo::FormFactor::kDesktop,
               URLVisit::Source::kLocal),
      "Sample Session Tag", "Test Session");
}

FetchOptions::URLTypeSet AsURLTypeSet(
    const std::vector<std::string>& url_type_entries) {
  FetchOptions::URLTypeSet result_url_types = {};
  for (const auto& url_type_entry : url_type_entries) {
    int url_type;
    if (base::StringToInt(url_type_entry, &url_type)) {
      result_url_types.Put(static_cast<FetchOptions::URLType>(url_type));
    }
  }

  return result_url_types;
}

// Return the desired fetch result types as specified via feature params or the
// defaults if not specified.
FetchOptions::URLTypeSet GetFetchResultURLTypes() {
  const std::string module_data_param = base::GetFieldTrialParamValueByFeature(
      ntp_features::kNtpMostRelevantTabResumptionModule,
      ntp_features::kNtpMostRelevantTabResumptionModuleDataParam);
  if (!module_data_param.empty() && module_data_param != "Fake Data") {
    const std::vector<std::string> data_param_url_types =
        base::SplitString(module_data_param, ",:;", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);

    FetchOptions::URLTypeSet result_url_types =
        AsURLTypeSet(data_param_url_types);
    if (!result_url_types.empty()) {
      return result_url_types;
    }
  }

  auto url_type_entries = base::SplitString(
      base::GetFieldTrialParamValueByFeature(
          ntp_features::kNtpMostRelevantTabResumptionModule,
          ntp_features::kNtpTabResumptionModuleResultTypesParam),
      ",:;", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
  if (url_type_entries.empty()) {
    return {FetchOptions::URLType::kActiveRemoteTab,
            FetchOptions::URLType::kRemoteVisit};
  }

  return AsURLTypeSet(url_type_entries);
}
}  // namespace

MostRelevantTabResumptionPageHandler::MostRelevantTabResumptionPageHandler(
    mojo::PendingReceiver<ntp::most_relevant_tab_resumption::mojom::PageHandler>
        pending_page_handler,
    content::WebContents* web_contents)
    : profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      web_contents_(web_contents),
      result_url_types_(GetFetchResultURLTypes()),
      dismissal_duration_days_(base::GetFieldTrialParamByFeatureAsInt(
          ntp_features::kNtpMostRelevantTabResumptionModule,
          ntp_features::kNtpTabResumptionModuleDismissalDurationParam,
          90)),
      page_handler_(this, std::move(pending_page_handler)) {
  DCHECK(profile_);
  DCHECK(web_contents_);
}

MostRelevantTabResumptionPageHandler::~MostRelevantTabResumptionPageHandler() {
  RemoveOldDismissedTabs();
}

void MostRelevantTabResumptionPageHandler::GetURLVisits(
    GetURLVisitsCallback callback) {
  const std::string data_type_param = base::GetFieldTrialParamValueByFeature(
      ntp_features::kNtpMostRelevantTabResumptionModule,
      ntp_features::kNtpMostRelevantTabResumptionModuleDataParam);

  if (data_type_param.find("Fake Data") != std::string::npos) {
    std::vector<ntp::most_relevant_tab_resumption::mojom::URLVisitPtr>
        url_visits_mojom;
    const int kSampleVisitsCount = 3;
    for (int i = 0; i < kSampleVisitsCount; i++) {
      auto url_visit_mojom = TabToMojom(
          CreateSampleURLVisitAggregateTab(GURL("https://www.google.com")));
      url_visit_mojom->source =
          ntp::most_relevant_tab_resumption::mojom::VisitSource::kTab;
      url_visit_mojom->url = GURL("https://www.google.com");
      url_visit_mojom->url_key = "https://www.google.com";
      url_visit_mojom->training_request_id = 0;
      url_visit_mojom->decoration =
          ntp::most_relevant_tab_resumption::mojom::Decoration::New();
      if (data_type_param.find("Most Recent Decorator") != std::string::npos) {
        url_visit_mojom->decoration->type = ntp::most_relevant_tab_resumption::
            mojom::DecorationType::kMostRecent;
        url_visit_mojom->decoration->display_string =
            l10n_util::GetStringUTF8(IDS_TAB_RESUME_DECORATORS_MOST_RECENT);
      } else if (data_type_param.find("Frequently Visited At Time Decorator") !=
                 std::string::npos) {
        url_visit_mojom->decoration->type = ntp::most_relevant_tab_resumption::
            mojom::DecorationType::kFrequentlyVisitedAtTime;
        url_visit_mojom->decoration->display_string = l10n_util::GetStringUTF8(
            IDS_TAB_RESUME_DECORATORS_FREQUENTLY_VISITED);
      } else if (data_type_param.find("Just Visited") != std::string::npos) {
        url_visit_mojom->decoration->type = ntp::most_relevant_tab_resumption::
            mojom::DecorationType::kVisitedXAgo;
        url_visit_mojom->decoration->display_string = l10n_util::GetStringUTF8(
            IDS_TAB_RESUME_DECORATORS_VISITED_RECENTLY);
      } else {
        url_visit_mojom->decoration->type = ntp::most_relevant_tab_resumption::
            mojom::DecorationType::kVisitedXAgo;
        url_visit_mojom->decoration->display_string = base::UTF16ToUTF8(
            l10n_util::GetStringUTF16(IDS_TAB_RESUME_DECORATORS_VISITED_X_AGO) +
            u" " + FormatRelativeTime(base::Time::Now() - base::Minutes(5)));
      }
      url_visits_mojom.push_back(std::move(url_visit_mojom));
    }
    std::move(callback).Run(std::move(url_visits_mojom));
    return;
  }

  FetchOptions fetch_options =
      FetchOptions::CreateFetchOptionsForTabResumption(result_url_types_);
  // Filter certain content categories, generally for use cases where a device
  // and profile may be shared by multiple family members.
  fetch_options.transforms.insert(
      fetch_options.transforms.begin(),
      URLVisitAggregatesTransformType::kHistoryCategoriesFilter);
  auto* visited_url_ranking_service =
      visited_url_ranking::VisitedURLRankingServiceFactory::GetForProfile(
          profile_);
  visited_url_ranking_service->FetchURLVisitAggregates(
      fetch_options,
      base::BindOnce(
          &MostRelevantTabResumptionPageHandler::OnURLVisitAggregatesFetched,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  base::UmaHistogramSparse("NewTabPage.Modules.DataRequest",
                           base::PersistentHash("tab_resumption"));
}

void MostRelevantTabResumptionPageHandler::DismissModule(
    const std::vector<ntp::most_relevant_tab_resumption::mojom::URLVisitPtr>
        url_visits) {
  DismissURLVisits(url_visits);
}

void MostRelevantTabResumptionPageHandler::DismissURLVisit(
    ntp::most_relevant_tab_resumption::mojom::URLVisitPtr url_visit) {
  std::vector<ntp::most_relevant_tab_resumption::mojom::URLVisitPtr>
      url_visits_mojom;
  url_visits_mojom.push_back(std::move(url_visit));
  DismissURLVisits(url_visits_mojom);
}

void MostRelevantTabResumptionPageHandler::DismissURLVisits(
    const std::vector<ntp::most_relevant_tab_resumption::mojom::URLVisitPtr>&
        url_visits) {
  RemoveOldDismissedTabs();
  ScopedDictPrefUpdate url_visit_dict(profile_->GetPrefs(),
                                      kDismissedVisitsPrefName);
  auto* visited_url_ranking_service =
      visited_url_ranking::VisitedURLRankingServiceFactory::GetForProfile(
          profile_);
  for (const auto& url_visit : url_visits) {
    url_visit_dict->Set(
        url_visit->url_key,
        static_cast<double>(
            url_visit->timestamp->ToDeltaSinceWindowsEpoch().InMicroseconds()));
    visited_url_ranking_service->RecordAction(
        visited_url_ranking::ScoredURLUserAction::kDismissed,
        url_visit->url_key,
        segmentation_platform::TrainingRequestId(
            url_visit->training_request_id));
  }
}

void MostRelevantTabResumptionPageHandler::RestoreModule(
    const std::vector<ntp::most_relevant_tab_resumption::mojom::URLVisitPtr>
        url_visits) {
  RestoreURLVisits(std::move(url_visits));
}

void MostRelevantTabResumptionPageHandler::RestoreURLVisit(
    ntp::most_relevant_tab_resumption::mojom::URLVisitPtr url_visit) {
  std::vector<ntp::most_relevant_tab_resumption::mojom::URLVisitPtr>
      url_visits_mojom;
  url_visits_mojom.push_back(std::move(url_visit));
  RestoreURLVisits(url_visits_mojom);
}

void MostRelevantTabResumptionPageHandler::RestoreURLVisits(
    const std::vector<ntp::most_relevant_tab_resumption::mojom::URLVisitPtr>&
        url_visits) {
  ScopedDictPrefUpdate url_visit_dict(profile_->GetPrefs(),
                                      kDismissedVisitsPrefName);
  auto* visited_url_ranking_service =
      visited_url_ranking::VisitedURLRankingServiceFactory::GetForProfile(
          profile_);
  for (const auto& url_visit : url_visits) {
    if (url_visit_dict->Find(url_visit->url_key) &&
        static_cast<long>(
            url_visit_dict->Find(url_visit->url_key)->GetDouble()) ==
            url_visit->timestamp->ToDeltaSinceWindowsEpoch().InMicroseconds()) {
      url_visit_dict->Remove(url_visit->url_key);
      visited_url_ranking_service->RecordAction(
          visited_url_ranking::ScoredURLUserAction::kSeen, url_visit->url_key,
          segmentation_platform::TrainingRequestId(
              url_visit->training_request_id));
    }
  }
}

void MostRelevantTabResumptionPageHandler::OnURLVisitAggregatesFetched(
    GetURLVisitsCallback callback,
    visited_url_ranking::ResultStatus status,
    visited_url_ranking::URLVisitsMetadata url_visits_metadata,
    std::vector<visited_url_ranking::URLVisitAggregate> url_visit_aggregates) {
  if (status == visited_url_ranking::ResultStatus::kError) {
    std::move(callback).Run({});
    return;
  }

  auto* visited_url_ranking_service =
      visited_url_ranking::VisitedURLRankingServiceFactory::GetForProfile(
          profile_);
  visited_url_ranking_service->RankURLVisitAggregates(
      {.key = visited_url_ranking::kTabResumptionRankerKey},
      std::move(url_visit_aggregates),
      base::BindOnce(
          &MostRelevantTabResumptionPageHandler::OnGotRankedURLVisitAggregates,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback),
          std::move(url_visits_metadata)));
}

void MostRelevantTabResumptionPageHandler::OnGotRankedURLVisitAggregates(
    GetURLVisitsCallback callback,
    visited_url_ranking::URLVisitsMetadata url_visits_metadata,
    visited_url_ranking::ResultStatus status,
    std::vector<visited_url_ranking::URLVisitAggregate> url_visit_aggregates) {
  base::UmaHistogramEnumeration("NewTabPage.TabResumption.ResultStatus",
                                status);
  if (status == visited_url_ranking::ResultStatus::kError) {
    std::move(callback).Run({});
    return;
  }

  int max_visits = base::GetFieldTrialParamByFeatureAsInt(
      ntp_features::kNtpMostRelevantTabResumptionModule,
      ntp_features::kNtpMostRelevantTabResumptionModuleMaxVisitsParam, 5);

  const size_t num_visits =
      std::min(static_cast<size_t>(max_visits), url_visit_aggregates.size());

  if (num_visits < url_visit_aggregates.size()) {
    url_visit_aggregates.erase(url_visit_aggregates.begin() + num_visits,
                               url_visit_aggregates.end());
    url_visit_aggregates.shrink_to_fit();
  }

  auto* visited_url_ranking_service =
      visited_url_ranking::VisitedURLRankingServiceFactory::GetForProfile(
          profile_);
  visited_url_ranking_service->DecorateURLVisitAggregates(
      {.key = visited_url_ranking::kTabResumptionRankerKey},
      std::move(url_visits_metadata), std::move(url_visit_aggregates),
      base::BindOnce(&MostRelevantTabResumptionPageHandler::
                         OnGotDecoratedURLVisitAggregates,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void MostRelevantTabResumptionPageHandler::OnGotDecoratedURLVisitAggregates(
    GetURLVisitsCallback callback,
    visited_url_ranking::ResultStatus status,
    std::vector<visited_url_ranking::URLVisitAggregate> url_visit_aggregates) {
  std::vector<ntp::most_relevant_tab_resumption::mojom::URLVisitPtr>
      url_visits_mojom;
  for (const auto& url_visit_aggregate : url_visit_aggregates) {
    const URLVisitAggregate::TabData* tab_data =
        GetTabDataIfExists(url_visit_aggregate);
    if (tab_data) {
      const URLVisitAggregate::Tab* tab = &tab_data->last_active_tab;
      auto url_visit_mojom = TabToMojom(*tab, tab_data->last_active);
      url_visit_mojom->source =
          ntp::most_relevant_tab_resumption::mojom::VisitSource::kTab;
      url_visit_mojom->url = **url_visit_aggregate.GetAssociatedURLs().begin();
      url_visit_mojom->url_key = url_visit_aggregate.url_key;
      url_visit_mojom->training_request_id =
          url_visit_aggregate.request_id.GetUnsafeValue();
      url_visit_mojom->decoration =
          ntp::most_relevant_tab_resumption::mojom::Decoration::New();
      if (base::FeatureList::IsEnabled(
              visited_url_ranking::features::kVisitedURLRankingDecorations)) {
        auto decoration = GetMostRelevantDecoration(url_visit_aggregate);
        url_visit_mojom->decoration->type =
            ntp::most_relevant_tab_resumption::mojom::DecorationType(
                static_cast<int>(decoration.GetType()));
        url_visit_mojom->decoration->display_string =
            base::UTF16ToUTF8(decoration.GetDisplayString());
      } else {
        url_visit_mojom->decoration->type = ntp::most_relevant_tab_resumption::
            mojom::DecorationType::kVisitedXAgo;
        url_visit_mojom->decoration->display_string = base::UTF16ToUTF8(
            visited_url_ranking::GetStringForRecencyDecorationWithTime(
                url_visit_aggregate.GetLastVisitTime()));
      }

      url_visit_mojom->timestamp = url_visit_aggregate.GetLastVisitTime();
      if (IsNewURL(url_visit_mojom)) {
        url_visits_mojom.push_back(std::move(url_visit_mojom));
        base::UmaHistogramEnumeration(
            "NewTabPage.TabResumption.URLVisitAggregateDataTypeDisplayed",
            URLVisitAggregateDataType::kTab);
      }
    } else {
      const URLVisitAggregate::HistoryData* history_data =
          GetHistoryDataIfExists(url_visit_aggregate);
      if (history_data) {
        auto history_url_visit_mojom = HistoryEntryVisitToMojom(
            history_data->last_visited, history_data->visit.client_name,
            history_data->visit.device_type);
        history_url_visit_mojom->source =
            ntp::most_relevant_tab_resumption::mojom::VisitSource::kHistory;
        history_url_visit_mojom->url =
            **url_visit_aggregate.GetAssociatedURLs().begin();
        history_url_visit_mojom->url_key = url_visit_aggregate.url_key;
        history_url_visit_mojom->training_request_id =
            url_visit_aggregate.request_id.GetUnsafeValue();
        history_url_visit_mojom->decoration =
            ntp::most_relevant_tab_resumption::mojom::Decoration::New();
        if (base::FeatureList::IsEnabled(
                visited_url_ranking::features::kVisitedURLRankingDecorations)) {
          auto decoration = GetMostRelevantDecoration(url_visit_aggregate);
          history_url_visit_mojom->decoration->type =
              ntp::most_relevant_tab_resumption::mojom::DecorationType(
                  static_cast<int>(decoration.GetType()));
          history_url_visit_mojom->decoration->display_string =
              base::UTF16ToUTF8(decoration.GetDisplayString());
        } else {
          history_url_visit_mojom->decoration->type = ntp::
              most_relevant_tab_resumption::mojom::DecorationType::kVisitedXAgo;
          history_url_visit_mojom->decoration->display_string =
              base::UTF16ToUTF8(
                  visited_url_ranking::GetStringForRecencyDecorationWithTime(
                      url_visit_aggregate.GetLastVisitTime()));
        }

        history_url_visit_mojom->timestamp =
            url_visit_aggregate.GetLastVisitTime();
        if (IsNewURL(history_url_visit_mojom)) {
          url_visits_mojom.push_back(std::move(history_url_visit_mojom));
          base::UmaHistogramEnumeration(
              "NewTabPage.TabResumption.URLVisitAggregateDataTypeDisplayed",
              URLVisitAggregateDataType::kHistory);
        }
      }
    }
  }

  std::move(callback).Run(std::move(url_visits_mojom));
}

// static
void MostRelevantTabResumptionPageHandler::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kDismissedVisitsPrefName,
                                   base::Value::Dict());
}

bool MostRelevantTabResumptionPageHandler::IsNewURL(
    ntp::most_relevant_tab_resumption::mojom::URLVisitPtr& url_visit) {
  const base::Value::Dict& cached_urls =
      profile_->GetPrefs()->GetDict(kDismissedVisitsPrefName);
  if (cached_urls.Find(url_visit->url_key) == nullptr) {
    return true;
  } else {
    return static_cast<long>(
               cached_urls.Find(url_visit->url_key)->GetDouble()) !=
           url_visit->timestamp->ToDeltaSinceWindowsEpoch().InMicroseconds();
  }
}

void MostRelevantTabResumptionPageHandler::RemoveOldDismissedTabs() {
  ScopedDictPrefUpdate visit_dict(profile_->GetPrefs(),
                                  kDismissedVisitsPrefName);
  for (auto it = visit_dict->begin(); it != visit_dict->end(); ++it) {
    base::Time timestamp = base::Time::FromDeltaSinceWindowsEpoch(
        base::Microseconds(it->second.GetDouble()));
    if (base::Time::Now() - timestamp > base::Days(dismissal_duration_days_)) {
      visit_dict->Remove(it->first);
    }
  }
}

void MostRelevantTabResumptionPageHandler::RecordAction(
    ntp::most_relevant_tab_resumption::mojom::ScoredURLUserAction action,
    const std::string& url_key,
    int64_t visit_request_id) {
  auto* visited_url_ranking_service =
      visited_url_ranking::VisitedURLRankingServiceFactory::GetForProfile(
          profile_);
  visited_url_ranking::ScoredURLUserAction user_action;
  switch (action) {
    case ntp::most_relevant_tab_resumption::mojom::ScoredURLUserAction::
        kUnknown:
      user_action = visited_url_ranking::ScoredURLUserAction::kUnknown;
      break;
    case ntp::most_relevant_tab_resumption::mojom::ScoredURLUserAction::kSeen:
      user_action = visited_url_ranking::ScoredURLUserAction::kSeen;
      break;
    case ntp::most_relevant_tab_resumption::mojom::ScoredURLUserAction::
        kActivated:
      user_action = visited_url_ranking::ScoredURLUserAction::kActivated;
      break;
    case ntp::most_relevant_tab_resumption::mojom::ScoredURLUserAction::
        kDismissed:
      user_action = visited_url_ranking::ScoredURLUserAction::kDismissed;
      break;
  }
  visited_url_ranking_service->RecordAction(
      user_action, url_key,
      segmentation_platform::TrainingRequestId(visit_request_id));
}
